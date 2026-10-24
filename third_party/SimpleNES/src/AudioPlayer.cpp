#include "AudioPlayer.h"
#include "Log.h"
#include "miniaudio.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace sn
{
namespace
{
constexpr double      kPi            = 3.14159265358979323846;
constexpr double      kTargetSeconds = 0.040; // steady-state queue latency
constexpr double      kMaxSeconds    = 0.120; // above this, skip ahead to the target
constexpr double      kMaxAdjust     = 0.002; // rate correction range (+-0.2%)
constexpr int         kFillWindow    = 2048;  // output samples averaged per rate update (> one frame of ripple)
constexpr double      kHighpassHz    = 90;    // NES output filter; also removes DC so silence is 0
constexpr double      kLowpassHz     = 14000; // NES output filter

// Band-limited impulse table: kTaps output samples wide, kPhases sub-sample positions.
constexpr int         kTaps          = 48;
constexpr int         kPhases        = 256;
constexpr std::size_t kAccumSize     = 8192; // power of two
constexpr std::size_t kAccumMask     = kAccumSize - 1;

using Kernel = std::array<std::array<float, kTaps>, kPhases + 1>;

// Windowed-sinc low-pass impulse (cutoff 0.43 * output rate, Blackman window), one row per fractional offset.
// Each row sums to 1, so integrating the impulses reproduces each step exactly.
const Kernel& kernel()
{
    static const Kernel table = [] {
        Kernel       t {};
        const double cutoff = 0.43;
        for (int p = 0; p <= kPhases; ++p)
        {
            const double frac = static_cast<double>(p) / kPhases;
            double       sum  = 0;
            for (int k = 0; k < kTaps; ++k)
            {
                const double x    = k - kTaps / 2 + 1 - frac; // centre of the window at kTaps/2 - 1 + frac
                const double sinc = x == 0 ? 1.0 : std::sin(2 * kPi * cutoff * x) / (2 * kPi * cutoff * x);
                const double w    = (x + kTaps / 2.0) / kTaps;  // 0..1 across the window
                const double win  = 0.42 - 0.5 * std::cos(2 * kPi * w) + 0.08 * std::cos(4 * kPi * w);
                t[p][k]           = static_cast<float>(sinc * std::max(0.0, win));
                sum += t[p][k];
            }
            for (int k = 0; k < kTaps; ++k)
                t[p][k] = static_cast<float>(t[p][k] / sum);
        }
        return t;
    }();
    return table;
}

void data_callback(ma_device* device, void* output, const void*, ma_uint32 frames)
{
    static_cast<AudioPlayer*>(device->pUserData)->render(static_cast<float*>(output), frames);
}
}

bool AudioPlayer::start()
{
    ma_device_config config         = ma_device_config_init(ma_device_type_playback);
    config.playback.format          = ma_format_f32;
    config.playback.channels        = 1;
    config.sampleRate               = 0; // device native rate: no second resampler inside miniaudio
    config.dataCallback             = data_callback;
    config.pUserData                = this;
    config.performanceProfile       = ma_performance_profile_low_latency;
    config.periodSizeInMilliseconds = 10;

    if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS)
    {
        LOG(Error) << "Failed to open playback device" << std::endl;
        return false;
    }

    const double outRate = m_device.sampleRate;
    const double dt      = 1.0 / outRate;
    m_baseRatio          = outRate / input_sample_rate;
    m_ratio              = m_baseRatio;
    const double hpRC    = 1.0 / (2 * kPi * kHighpassHz);
    const double lpRC    = 1.0 / (2 * kPi * kLowpassHz);
    m_hpA                = static_cast<float>(hpRC / (hpRC + dt));
    m_lpK                = static_cast<float>(dt / (lpRC + dt));
    m_target             = static_cast<std::size_t>(outRate * kTargetSeconds);
    m_accum.assign(kAccumSize, 0.f);
    kernel();
    m_queue = std::make_unique<spsc::RingBuffer<float>>(static_cast<int>(outRate * 0.25));

    if (ma_device_start(&m_device) != MA_SUCCESS)
    {
        LOG(Error) << "Failed to start playback device" << std::endl;
        ma_device_uninit(&m_device);
        m_queue.reset();
        return false;
    }
    m_initialized = true;
    m_enabled     = true;
    return true;
}

void AudioPlayer::addStep(float delta)
{
    // Spread the step's impulse over kTaps output samples starting at the current output sample.
    const double pos   = m_time;
    const auto   whole = static_cast<std::size_t>(pos);
    const double frac  = pos - whole;
    const auto&  row   = kernel()[static_cast<int>(frac * kPhases + 0.5)];
    const auto   base  = m_readPos + whole;
    for (int k = 0; k < kTaps; ++k)
        m_accum[(base + k) & kAccumMask] += delta * row[k];
}

void AudioPlayer::emitReady()
{
    // Output samples before the current position can no longer receive impulses: finish them.
    while (m_time >= 1.0)
    {
        float& slot = m_accum[m_readPos & kAccumMask];
        m_integrator += slot;
        slot = 0.f;
        ++m_readPos;
        m_time -= 1.0;

        // NES output filters: first-order high-pass (90 Hz) then low-pass (14 kHz).
        const float x  = static_cast<float>(m_integrator);
        const float hp = m_hpA * (m_hpPrevOut + x - m_hpPrevIn);
        m_hpPrevIn     = x;
        m_hpPrevOut    = hp;
        m_lpOut += m_lpK * (hp - m_lpOut);

        m_queue->push(m_lpOut); // full queue: drop (render() skips ahead anyway)

        m_fillSum += static_cast<double>(m_queue->size());
        if (++m_fillCount == kFillWindow)
            updateRate();
    }
}

void AudioPlayer::updateRate()
{
    // Averaging over kFillWindow samples hides the per-frame fill sawtooth; the extra smoothing keeps the
    // ratio from moving faster than a slow drift, so pitch never wobbles.
    const double mean  = m_fillSum / m_fillCount;
    const double error = std::clamp((mean - m_target) / m_target, -1.0, 1.0);
    m_adjust += 0.2 * (error * kMaxAdjust - m_adjust);
    m_ratio     = m_baseRatio * (1.0 - m_adjust); // too full -> fewer output samples
    m_fillSum   = 0.0;
    m_fillCount = 0;
}

void AudioPlayer::render(float* out, std::uint32_t frames)
{
    auto&             q       = *m_queue;
    const std::size_t maxFill = static_cast<std::size_t>(m_target * (kMaxSeconds / kTargetSeconds));
    float             sink[512];
    auto              discard = [&](std::size_t n) {
        while (n > 0)
        {
            const std::size_t got = q.pop(sink, std::min<std::size_t>(n, 512));
            if (got == 0)
                break;
            n -= got;
        }
    };

    if (m_flushRequested.exchange(false))
    {
        discard(q.size());
        m_primed = false;
    }

    std::size_t avail = q.size();
    if (avail > maxFill) // after a hitch or fast-forward: jump back to low latency
    {
        discard(avail - m_target);
        avail = m_target;
    }

    std::size_t got = 0;
    if (m_primed || avail >= m_target / 2)
    {
        m_primed = true;
        got      = q.pop(out, frames);
    }
    if (got > 0)
        m_last = out[got - 1];
    if (got < frames)
    {
        // Underrun: glide to silence (no clicks, no looping), then wait to re-buffer.
        for (std::uint32_t i = static_cast<std::uint32_t>(got); i < frames; ++i)
        {
            m_last *= 0.995f;
            out[i] = m_last;
        }
        m_primed = false;
    }

    if (m_muted)
        std::fill(out, out + frames, 0.f);
}

AudioPlayer::~AudioPlayer()
{
    if (m_initialized)
        ma_device_uninit(&m_device);
}

void AudioPlayer::mute()
{
    m_muted = true;
}

}
