#pragma once

#include "AudioPlayer.h"
#include "CPU.h"
#include "Cartridge.h"
#include "Controller.h"
#include "MainBus.h"
#include "Mapper.h"
#include "PPU.h"
#include "PictureBus.h"
#include "VirtualScreen.h"
#include "APU/APU.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scaler
{
class NesHost
{
public:
    static constexpr int kWidth  = sn::ScanlineVisibleDots;
    static constexpr int kHeight = sn::VisibleScanlines;

    NesHost();

    bool loadRom(const std::string& path);
    void reset();

    // Snapshot of the whole machine; load fails on a different ROM or bad data.
    bool saveState(std::vector<std::uint8_t>& out);
    bool loadState(const std::vector<std::uint8_t>& in);
    void setController1(std::uint8_t buttons);
    void setMute(bool mute);
    // Drop queued audio (pause, load, reset) / stop producing it (fast-forward).
    void flushAudio() { m_audioPlayer.flush(); }
    void setAudioEnabled(bool enabled) { m_audioPlayer.setEnabled(enabled); }

    // Advance emulation; returns true when a new frame is ready.
    bool stepFrame();

    const std::uint8_t* frameRGBA() const { return m_screen.rgba(); }

private:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration  = std::chrono::high_resolution_clock::duration;

    void     OAMDMA(sn::Byte page);
    sn::Byte DMCDMA(sn::Address addr);

    sn::VirtualScreen           m_screen;
    sn::PictureBus              m_pictureBus;
    sn::AudioPlayer             m_audioPlayer;
    sn::CPU                     m_cpu;
    sn::PPU                     m_ppu;
    sn::APU                     m_apu;
    sn::Cartridge               m_cartridge;
    std::unique_ptr<sn::Mapper> m_mapper;
    sn::Controller              m_controller1;
    sn::Controller              m_controller2;
    sn::MainBus                 m_bus;

    TimePoint m_lastWakeup {};
    Duration  m_elapsedTime {};
    bool      m_loaded = false;
};
}
