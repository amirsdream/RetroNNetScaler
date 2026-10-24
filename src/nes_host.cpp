#include "nes_host.h"

#include "APU/Constants.h"
#include "Log.h"

namespace scaler
{
using clock = std::chrono::high_resolution_clock;

namespace
{
// One APU sample every 2 CPU cycles, at the rate stepFrame() is actually paced (29781 cycles x 60.0988 Hz).
constexpr int kApuSampleRate = static_cast<int>(29781 * 60.0988 / 2);
}

NesHost::NesHost()
  : m_audioPlayer(kApuSampleRate)
  , m_cpu(m_bus)
  , m_ppu(m_pictureBus, m_screen)
  , m_apu(m_audioPlayer, m_cpu.createIRQHandler(), [&](sn::Address addr) { return DMCDMA(addr); })
  , m_bus(m_ppu, m_apu, m_controller1, m_controller2, [&](sn::Byte b) { OAMDMA(b); })
{
    m_ppu.setInterruptCallback([&]() { m_cpu.nmiInterrupt(); });
    m_screen.create(kWidth, kHeight);
}

bool NesHost::loadRom(const std::string& path)
{
    if (!m_cartridge.loadFromFile(path))
        return false;

    m_mapper = sn::Mapper::createMapper(static_cast<sn::Mapper::Type>(m_cartridge.getMapper()),
                                        m_cartridge,
                                        m_cpu.createIRQHandler(),
                                        [&]() { m_pictureBus.updateMirroring(); });
    if (!m_mapper)
    {
        LOG(sn::Error) << "Creating Mapper failed. Probably unsupported." << std::endl;
        return false;
    }

    if (!m_bus.setMapper(m_mapper.get()) || !m_pictureBus.setMapper(m_mapper.get()))
        return false;

    m_cpu.reset();
    m_ppu.reset();
    m_lastWakeup  = clock::now();
    m_elapsedTime = Duration::zero();
    m_audioPlayer.start();
    m_loaded = true;
    return true;
}

void NesHost::reset()
{
    if (!m_loaded)
        return;
    m_cpu.reset();
    m_ppu.reset();
}

namespace
{
constexpr std::uint32_t kStateMagic   = 0x31534E52; // "RNS1"
constexpr std::uint32_t kStateVersion = 2; // 2: DMC sample-buffer flag

std::uint64_t fnv1a(const std::vector<sn::Byte>& data)
{
    std::uint64_t h = 1469598103934665603ull;
    for (sn::Byte b : data)
        h = (h ^ b) * 1099511628211ull;
    return h;
}
}

bool NesHost::saveState(std::vector<std::uint8_t>& out)
{
    if (!m_loaded)
        return false;
    out.clear();
    sn::StateIO   s(out, true);
    std::uint32_t magic = kStateMagic, version = kStateVersion;
    std::uint64_t romHash = fnv1a(m_cartridge.getROM());
    s.pod(magic);
    s.pod(version);
    s.pod(romHash);
    m_cpu.state(s);
    m_ppu.state(s);
    m_pictureBus.state(s);
    m_bus.state(s);
    m_apu.state(s);
    m_controller1.state(s);
    m_controller2.state(s);
    m_mapper->state(s);
    return true;
}

bool NesHost::loadState(const std::vector<std::uint8_t>& in)
{
    if (!m_loaded)
        return false;

    // Validate the header before touching the machine.
    std::vector<std::uint8_t> buf = in;
    sn::StateIO               s(buf, false);
    std::uint32_t             magic = 0, version = 0;
    std::uint64_t             romHash = 0;
    s.pod(magic);
    s.pod(version);
    s.pod(romHash);
    if (!s.ok() || magic != kStateMagic || version != kStateVersion || romHash != fnv1a(m_cartridge.getROM()))
        return false;

    std::vector<std::uint8_t> backup;
    saveState(backup);

    m_cpu.state(s);
    m_ppu.state(s);
    m_pictureBus.state(s);
    m_bus.state(s);
    m_apu.state(s);
    m_controller1.state(s);
    m_controller2.state(s);
    m_mapper->state(s);
    m_pictureBus.updateMirroring();
    if (!s.ok())
    {
        loadState(backup);
        return false;
    }
    return true;
}

void NesHost::setController1(std::uint8_t buttons)
{
    m_controller1.setButtons(buttons);
}

void NesHost::setMute(bool mute)
{
    if (mute)
        m_audioPlayer.mute();
}

bool NesHost::stepFrame()
{
    if (!m_loaded)
        return false;

    // Run roughly one frame worth of CPU cycles (~29781).
    constexpr int kCyclesPerFrame = 29781;
    for (int i = 0; i < kCyclesPerFrame; ++i)
    {
        m_ppu.step();
        m_ppu.step();
        m_ppu.step();
        m_cpu.step();
        m_apu.step();
    }
    return true;
}

void NesHost::OAMDMA(sn::Byte page)
{
    m_cpu.skipOAMDMACycles();
    auto page_ptr = m_bus.getPagePtr(page);
    if (page_ptr != nullptr)
        m_ppu.doDMA(page_ptr);
    else
        LOG(sn::Error) << "Can't get pageptr for DMA" << std::endl;
}

sn::Byte NesHost::DMCDMA(sn::Address addr)
{
    m_cpu.skipDMCDMACycles();
    return m_bus.read(addr);
}
}
