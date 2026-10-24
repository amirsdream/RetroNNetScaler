// Headless NES core for the Electron UI (ui/). Portable: no Win32/D3D.
//
// stdout: raw 256x240 RGBA frames, back to back (245760 bytes each).
// stderr: log text, plus replies as lines starting with '@':
//   @ok save <path> | @ok load <path> | @err save <path> | @err load <path>
// stdin:  one command per line:
//   pad <mask>    NES button bitmask (A=0 B=1 Select=2 Start=3 Up Down Left Right)
//   save <path>   write a save state (rest of the line is the path)
//   load <path>   restore a save state
//   speed <n>     emulation speed multiplier (1 = normal, e.g. 4 for fast-forward)
//   pause | resume | reset | quit

#include "nes_host.h"

#include "Log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
std::atomic<std::uint8_t> g_pad { 0 };
std::atomic<bool>         g_paused { false };
std::atomic<bool>         g_reset { false };
std::atomic<bool>         g_running { true };
std::atomic<int>          g_speed { 1 };

// Save/load requests are handed to the emulation thread between frames.
std::mutex               g_fileMutex;
std::vector<std::string> g_fileCmds;

std::string restOfLine(std::istringstream& in)
{
    std::string rest;
    std::getline(in >> std::ws, rest);
    if (!rest.empty() && rest.back() == '\r')
        rest.pop_back();
    return rest;
}

void runFileCommand(scaler::NesHost& nes, const std::string& line)
{
    std::istringstream in(line);
    std::string        cmd;
    in >> cmd;
    const std::string path = restOfLine(in);
    bool              ok   = false;
    if (cmd == "save")
    {
        std::vector<std::uint8_t> data;
        if (nes.saveState(data))
        {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            ok = static_cast<bool>(out);
        }
    }
    else if (cmd == "load")
    {
        std::ifstream             f(path, std::ios::binary);
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        ok = f.good() || f.eof() ? nes.loadState(data) : false;
        if (ok)
            nes.flushAudio();
    }
    std::cerr << (ok ? "@ok " : "@err ") << cmd << ' ' << path << std::endl;
}

void readCommands()
{
    std::string line;
    while (g_running && std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string        cmd;
        in >> cmd;
        if (cmd == "pad")
        {
            int mask = 0;
            in >> mask;
            g_pad = static_cast<std::uint8_t>(mask);
        }
        else if (cmd == "speed")
        {
            int n = 1;
            in >> n;
            g_speed = n < 1 ? 1 : (n > 8 ? 8 : n);
        }
        else if (cmd == "pause")
            g_paused = true;
        else if (cmd == "resume")
            g_paused = false;
        else if (cmd == "reset")
            g_reset = true;
        else if (cmd == "save" || cmd == "load")
        {
            std::lock_guard<std::mutex> lock(g_fileMutex);
            g_fileCmds.push_back(line);
        }
        else if (cmd == "quit")
            break;
    }
    // stdin closed (UI gone) or quit: stop.
    g_running = false;
}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    sn::Log::get().setLogStream(std::cerr);
    sn::Log::get().setCpuTraceStream(std::cerr);
    sn::Log::get().setLevel(sn::Error);

    std::string romPath;
    bool        mute = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "--rom" || arg == "-r") && i + 1 < argc)
            romPath = argv[++i];
        else if (arg == "--mute")
            mute = true;
        else if (romPath.empty() && arg[0] != '-')
            romPath = arg;
    }
    if (romPath.empty())
    {
        std::cerr << "Usage: 4kscaler-core --rom <path.nes> [--mute]\n";
        return 1;
    }

    scaler::NesHost nes;
    if (!nes.loadRom(romPath))
    {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        return 1;
    }
    nes.setMute(mute);

    std::thread input(readCommands);
    input.detach();

    using clock = std::chrono::steady_clock;
    constexpr auto kFramePeriod =
      std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / 60.0988));
    constexpr std::size_t kFrameBytes = scaler::NesHost::kWidth * scaler::NesHost::kHeight * 4;
    auto nextFrame  = clock::now();
    bool wasPaused  = false;
    int  lastSpeed  = 1;

    while (g_running)
    {
        std::this_thread::sleep_until(nextFrame);
        const auto now    = clock::now();
        const int  speed  = g_speed.load();
        const auto period = kFramePeriod / speed;
        nextFrame += period;
        if (now - nextFrame > period * 2)
            nextFrame = now + period;

        // Audio follows the emulation state: clear on pause/resume, silent while fast-forwarding.
        if (g_paused != wasPaused)
        {
            wasPaused = g_paused;
            nes.flushAudio();
        }
        if (speed != lastSpeed)
        {
            lastSpeed = speed;
            nes.setAudioEnabled(speed == 1);
            nes.flushAudio();
        }
        if (g_reset.exchange(false))
        {
            nes.reset();
            nes.flushAudio();
        }
        {
            std::vector<std::string> cmds;
            {
                std::lock_guard<std::mutex> lock(g_fileMutex);
                cmds.swap(g_fileCmds);
            }
            for (const auto& c : cmds)
                runFileCommand(nes, c);
        }
        if (g_paused)
            continue;

        nes.setController1(g_pad);
        nes.stepFrame();
        if (std::fwrite(nes.frameRGBA(), 1, kFrameBytes, stdout) != kFrameBytes)
            break; // UI closed the pipe
        std::fflush(stdout);
    }
    return 0;
}
