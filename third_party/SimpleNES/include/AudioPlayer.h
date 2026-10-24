#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <miniaudio.h>
#include <vector>

#include "APU/spsc.hpp"

namespace sn
{

// Converts the APU output (one sample per APU tick, ~894 kHz) to the device rate with band-limited step synthesis
// (the BLEP technique popularised by blargg's blip_buf): every change in the APU level is written as a windowed-sinc
// step at its exact sub-sample time, so the output has no aliasing and no interpolation blur. The result goes through
// the NES output filters and miniaudio (WASAPI / CoreAudio / PulseAudio / ALSA).
//
// The emulator runs on its own 60.1 Hz clock, not the sound card's, so the resampling ratio is nudged (max +-0.2%,
// smoothed over many frames so it never warbles) to hold the queue near a small target fill. Underruns (pause, menus,
// hitches) fade to silence instead of replaying stale audio.
class AudioPlayer
{
public:
    explicit AudioPlayer(int input_rate)
      : input_sample_rate(input_rate)
    {
    }
    ~AudioPlayer();

    bool      start();
    void      mute();

    // Emulation thread: one sample per APU tick.
    void      push(float sample)
    {
        if (!m_enabled)
            return;
        if (sample != m_level)
        {
            addStep(sample - m_level);
            m_level = sample;
        }
        m_time += m_ratio;
        if (m_time >= 1.0)
            emitReady();
    }

    // Emulation thread: stop/start producing audio (e.g. while fast-forwarding).
    void      setEnabled(bool enabled) { m_enabled = enabled && m_queue; }
    // Any thread: drop queued audio and fade out (pause, load state, reset).
    void      flush() { m_flushRequested = true; }

    // Device callback.
    void      render(float* out, std::uint32_t frames);

    const int input_sample_rate;

private:
    void                                     addStep(float delta);
    void                                     emitReady();
    void                                     updateRate();

    std::unique_ptr<spsc::RingBuffer<float>> m_queue;
    ma_device                                m_device {};
    bool                                     m_initialized = false;
    bool                                     m_enabled     = false;
    std::atomic<bool>                        m_muted { false };
    std::atomic<bool>                        m_flushRequested { false };

    // Synthesis (emulation thread). m_time is the current position in output samples, relative to m_readPos.
    std::vector<float>                       m_accum;     // pending band-limited impulses, ring of output samples
    std::size_t                              m_readPos = 0;
    double                                   m_time    = 0.0;
    double                                   m_baseRatio = 1.0; // output samples per input sample
    double                                   m_ratio     = 1.0;
    double                                   m_adjust    = 0.0; // smoothed rate correction
    float                                    m_level     = 0.f; // last APU level
    double                                   m_integrator = 0.0;
    float                                    m_hpPrevIn = 0.f, m_hpPrevOut = 0.f, m_hpA = 1.f;
    float                                    m_lpOut = 0.f, m_lpK = 1.f;
    double                                   m_fillSum = 0.0;
    int                                      m_fillCount = 0;
    std::size_t                              m_target = 0; // queue fill to hold, in output samples

    // Consumer state (device thread).
    float                                    m_last   = 0.f;
    bool                                     m_primed = false;
};
}
