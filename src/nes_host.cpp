#include "nes_host.h"

#include "APU/Constants.h"
#include "Log.h"

namespace scaler
{
using clock = std::chrono::high_resolution_clock;

NesHost::NesHost()
  : m_audioPlayer(static_cast<int>(1.0 / sn::apu_clock_period_s.count()))
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
