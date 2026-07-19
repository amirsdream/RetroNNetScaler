#include "d3d11_renderer.h"
#include "input.h"
#include "launcher.h"
#include "nes_host.h"
#include "nis_upscaler.h"
#include "prefs.h"
#include "tiny_sr_upscaler.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

namespace
{
struct AppState
{
    scaler::NesHost*         nes      = nullptr;
    scaler::D3D11Renderer*   renderer = nullptr;
    scaler::NisUpscaler*     nis      = nullptr;
    scaler::TinySrUpscaler*  tinySr   = nullptr;
    scaler::Prefs*           prefs    = nullptr;
    bool                     running  = true;
    bool                     dlssWarned = false;
    double                 fps      = 0.0;
    std::string            romName;
};

AppState* g_app = nullptr;

void updateWindowTitle(HWND hwnd)
{
    if (!g_app || !g_app->prefs)
        return;
    char title[256];
    const int scale = g_app->renderer ? g_app->renderer->lastIntegerScale() : 1;
    std::snprintf(title,
                  sizeof(title),
                  "RetroNNetScaler | %s | %s | %s | %dx | phase %d,%d | sharp %.2f | %.0f FPS  [F1][F3]",
                  g_app->romName.c_str(),
                  scaler::scaleModeName(g_app->prefs->scaleMode),
                  scaler::outputResName(g_app->prefs->outputRes),
                  scale,
                  g_app->prefs->phaseX,
                  g_app->prefs->phaseY,
                  g_app->prefs->sharpness,
                  g_app->fps);
    SetWindowTextA(hwnd, title);
}

void applyWindowClientSize(HWND hwnd, int w, int h)
{
    RECT rc { 0, 0, w, h };
    AdjustWindowRect(&rc, GetWindowLong(hwnd, GWL_STYLE), FALSE);
    SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 rc.right - rc.left,
                 rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

void toggleFullscreen(HWND hwnd, scaler::Prefs& prefs)
{
    static WINDOWPLACEMENT g_wp = { sizeof(g_wp) };
    prefs.fullscreen            = !prefs.fullscreen;
    if (prefs.fullscreen)
    {
        GetWindowPlacement(hwnd, &g_wp);
        MONITORINFO mi { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd,
                     HWND_TOP,
                     mi.rcMonitor.left,
                     mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(hwnd, &g_wp);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        if (g_app)
            g_app->running = false;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (g_app && g_app->renderer && wParam != SIZE_MINIMIZED)
        {
            g_app->renderer->resize(LOWORD(lParam), HIWORD(lParam));
            if (g_app->prefs)
            {
                g_app->prefs->windowWidth  = LOWORD(lParam);
                g_app->prefs->windowHeight = HIWORD(lParam);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (!g_app || !g_app->prefs)
            break;
        if (wParam == VK_ESCAPE)
        {
            g_app->running = false;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_F11)
        {
            toggleFullscreen(hwnd, *g_app->prefs);
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == VK_F3)
        {
            g_app->prefs->outputRes = scaler::nextOutputRes(g_app->prefs->outputRes);
            scaler::applyOutputRes(*g_app->prefs);
            if (!g_app->prefs->fullscreen)
                applyWindowClientSize(hwnd, g_app->prefs->windowWidth, g_app->prefs->windowHeight);
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == 'V')
        {
            g_app->prefs->vsync = !g_app->prefs->vsync;
            g_app->renderer->setVsync(g_app->prefs->vsync);
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == '1' || wParam == VK_F1)
        {
            g_app->prefs->scaleMode = scaler::nextScaleMode(g_app->prefs->scaleMode);
            if (g_app->prefs->scaleMode == scaler::ScaleMode::DLSS && !g_app->dlssWarned)
            {
                g_app->dlssWarned = true;
                MessageBoxA(hwnd,
                            "Real DLSS is not linked in this build (needs Streamline/NGX).\n"
                            "NES also has no motion vectors/depth for proper DLSS.\n"
                            "Mode label DLSS(NIS): using NVIDIA Image Scaling instead.\n"
                            "Use TinySR for the open AI/FPGA path.",
                            "4kscaler DLSS",
                            MB_OK | MB_ICONINFORMATION);
            }
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == VK_OEM_4) // [
        {
            g_app->prefs->sharpness = (std::max)(0.f, g_app->prefs->sharpness - 0.05f);
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == VK_OEM_6) // ]
        {
            g_app->prefs->sharpness = (std::min)(1.f, g_app->prefs->sharpness + 0.05f);
            updateWindowTitle(hwnd);
            return 0;
        }
        // OSSC-style sampling phase: nudge integer placement by 1 output pixel.
        if (wParam == VK_LEFT)
        {
            g_app->prefs->phaseX -= 1;
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == VK_RIGHT)
        {
            g_app->prefs->phaseX += 1;
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == VK_UP)
        {
            g_app->prefs->phaseY -= 1;
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == VK_DOWN)
        {
            g_app->prefs->phaseY += 1;
            updateWindowTitle(hwnd);
            return 0;
        }
        if (wParam == '0')
        {
            g_app->prefs->phaseX = 0;
            g_app->prefs->phaseY = 0;
            updateWindowTitle(hwnd);
            return 0;
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

std::string exeDir()
{
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    auto slash = dir.find_last_of("\\/");
    if (slash != std::string::npos)
        dir.resize(slash);
    return dir;
}

std::string findExisting(const std::vector<std::string>& candidates, const char* file)
{
    for (const auto& c : candidates)
    {
        const std::string probe = c + "\\" + file;
        if (GetFileAttributesA(probe.c_str()) != INVALID_FILE_ATTRIBUTES)
            return c;
    }
    return candidates.empty() ? std::string() : candidates.front();
}

std::string findNisDir()
{
    const std::string dir = exeDir();
    return findExisting(
        {
            dir + "\\NIS",
            dir + "\\..\\..\\third_party\\NIS\\NIS",
            dir + "\\..\\third_party\\NIS\\NIS",
            "third_party\\NIS\\NIS",
            "..\\third_party\\NIS\\NIS",
        },
        "NIS_Main.hlsl");
}

std::string findTinySrShader()
{
    const std::string dir = exeDir();
    const std::vector<std::string> candidates = {
        dir + "\\shaders\\tiny_sr.hlsl",
        dir + "\\..\\..\\shaders\\tiny_sr.hlsl",
        dir + "\\..\\shaders\\tiny_sr.hlsl",
        "shaders\\tiny_sr.hlsl",
        "..\\shaders\\tiny_sr.hlsl",
    };
    for (const auto& c : candidates)
    {
        if (GetFileAttributesA(c.c_str()) != INVALID_FILE_ATTRIBUTES)
            return c;
    }
    return "shaders\\tiny_sr.hlsl";
}

void printUsage()
{
    std::cout
      << "Usage: 4kscaler --rom <path.nes> [options]\n"
         "  --scale nearest|bilinear|nis|clarity|smooth|neural|dlss\n"
         "  --res 1080|1440|4k\n"
         "  --dump-frames N --dump-dir DIR   write raw 256x240 RGBA (+ quit)\n"
         "  clarity=integer  smooth=xBR-sharp  neural=trained TinySR\n"
         "Controls: F1 mode  F3 res  [/] sharp  V vsync  F11 fullscreen  Esc quit\n";
}
}

static void enableDpiAwareness()
{
    // Prevent DWM/DPI soft-scaling of the swapchain (looks blurry on 125%/150% desktops).
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32)
    {
        using Fn = BOOL(WINAPI*)(HANDLE);
        auto setCtx = reinterpret_cast<Fn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setCtx)
            setCtx(reinterpret_cast<HANDLE>(-4)); // PER_MONITOR_AWARE_V2
    }
}

static void clampResToMonitor(scaler::Prefs& prefs)
{
    const int monW = GetSystemMetrics(SM_CXSCREEN);
    const int monH = GetSystemMetrics(SM_CYSCREEN);
    if (monW <= 0 || monH <= 0)
        return;
    // Prefer native panel pixels; a 4K buffer on a 1440p 165Hz screen is DWM-scaled → blurry.
    if (prefs.windowWidth > monW || prefs.windowHeight > monH)
    {
        prefs.windowWidth  = monW;
        prefs.windowHeight = monH;
        if (monH >= 2100)
            prefs.outputRes = scaler::OutputRes::Res4K;
        else if (monH >= 1400)
            prefs.outputRes = scaler::OutputRes::Res1440p;
        else
            prefs.outputRes = scaler::OutputRes::Res1080p;
        std::cout << "Clamped output to monitor native " << monW << "x" << monH
                  << " (avoids DWM downscale blur)\n";
    }
}

int main(int argc, char** argv)
{
    enableDpiAwareness();
    timeBeginPeriod(1); // 1ms sleep granularity for 60.1 Hz pacing

    std::string romPath;
    scaler::Prefs prefs;
    scaler::loadPrefs(prefs);
    bool        resFromCli  = false;
    int         dumpFrames  = 0;
    std::string dumpDir;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "--rom" || arg == "-r") && i + 1 < argc)
            romPath = argv[++i];
        else if (arg == "--mute")
            prefs.mute = true;
        else if (arg == "--dump-frames" && i + 1 < argc)
            dumpFrames = std::atoi(argv[++i]);
        else if (arg == "--dump-dir" && i + 1 < argc)
            dumpDir = argv[++i];
        else if (arg == "--scale" && i + 1 < argc)
        {
            std::string m = argv[++i];
            if (m == "nearest")
                prefs.scaleMode = scaler::ScaleMode::Nearest;
            else if (m == "bilinear")
                prefs.scaleMode = scaler::ScaleMode::Bilinear;
            else if (m == "nis")
                prefs.scaleMode = scaler::ScaleMode::NIS;
            else if (m == "tinysr" || m == "ai" || m == "clarity")
                prefs.scaleMode = scaler::ScaleMode::TinySR;
            else if (m == "smooth" || m == "depixel" || m == "xbr")
                prefs.scaleMode = scaler::ScaleMode::Smooth;
            else if (m == "neural" || m == "cnn" || m == "tinycnn")
                prefs.scaleMode = scaler::ScaleMode::Neural;
            else if (m == "dlss")
                prefs.scaleMode = scaler::ScaleMode::DLSS;
        }
        else if (arg == "--res" && i + 1 < argc)
        {
            std::string r = argv[++i];
            resFromCli    = true;
            if (r == "1080" || r == "1080p")
                prefs.outputRes = scaler::OutputRes::Res1080p;
            else if (r == "1440" || r == "1440p")
                prefs.outputRes = scaler::OutputRes::Res1440p;
            else if (r == "4k" || r == "2160" || r == "2160p")
                prefs.outputRes = scaler::OutputRes::Res4K;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }
        else if (romPath.empty() && arg[0] != '-')
            romPath = arg;
    }

    if (dumpFrames > 0 && dumpDir.empty())
        dumpDir = "data/nes/raw";

    if (resFromCli || prefs.windowWidth < 640)
        scaler::applyOutputRes(prefs);
    clampResToMonitor(prefs);

    // GUI launcher when no ROM on CLI (skip for dump / automation)
    if (romPath.empty() && dumpFrames <= 0)
    {
        scaler::LaunchConfig launch;
        launch.prefs             = prefs;
        launch.romPath           = prefs.lastRomPath;
        launch.cancelled         = true;
        if (!scaler::showLauncher(launch))
        {
            timeEndPeriod(1);
            return 0;
        }
        prefs   = launch.prefs;
        romPath = launch.romPath;
        scaler::applyOutputRes(prefs);
        clampResToMonitor(prefs);
    }

    if (romPath.empty())
    {
        printUsage();
        timeEndPeriod(1);
        return 1;
    }

    scaler::NesHost nes;
    if (!nes.loadRom(romPath))
    {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        MessageBoxA(nullptr, ("Failed to load ROM:\n" + romPath).c_str(), "RetroNNetScaler", MB_ICONERROR);
        return 1;
    }
    prefs.lastRomPath = romPath;
    nes.setMute(prefs.mute);

    HINSTANCE hInst = GetModuleHandle(nullptr);
    WNDCLASSEXA wc {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "FourKScalerWindow";
    RegisterClassExA(&wc);

    RECT rc { 0, 0, prefs.windowWidth, prefs.windowHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0,
                                wc.lpszClassName,
                                "RetroNNetScaler",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                rc.right - rc.left,
                                rc.bottom - rc.top,
                                nullptr,
                                nullptr,
                                hInst,
                                nullptr);
    if (!hwnd)
    {
        std::cerr << "CreateWindow failed\n";
        return 1;
    }

    scaler::D3D11Renderer renderer;
    if (!renderer.create(hwnd, prefs.windowWidth, prefs.windowHeight, prefs.vsync))
    {
        std::cerr << "Failed to create D3D11 renderer\n";
        return 1;
    }

    scaler::NisUpscaler nis;
    const std::string   nisDir = findNisDir();
    if (!nis.init(renderer.device(), renderer.context(), nisDir))
        std::cerr << "NIS init failed (nearest/bilinear still work). Shader dir: " << nisDir << std::endl;

    scaler::TinySrUpscaler tinySr;
    const std::string      tinyPath = findTinySrShader();
    if (!tinySr.init(renderer.device(), renderer.context(), tinyPath))
        std::cerr << "TinySR init failed. Shader: " << tinyPath << std::endl;

    AppState app;
    app.nes      = &nes;
    app.renderer = &renderer;
    app.nis      = &nis;
    app.tinySr   = &tinySr;
    app.prefs    = &prefs;
    auto slash   = romPath.find_last_of("\\/");
    app.romName  = slash == std::string::npos ? romPath : romPath.substr(slash + 1);
    g_app        = &app;
    updateWindowTitle(hwnd);

    if (prefs.fullscreen)
        toggleFullscreen(hwnd, prefs);

    using clock = std::chrono::steady_clock;
    // Lock gameplay to NES NTSC rate — independent of 165 Hz panel refresh.
    constexpr auto kFramePeriod =
      std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / 60.0988));
    auto nextFrame = clock::now();
    auto lastTitle = nextFrame;
    int  frames    = 0;
    int  dumpIndex = 0;

    if (dumpFrames > 0)
    {
        CreateDirectoryA("data", nullptr);
        CreateDirectoryA("data\\nes", nullptr);
        CreateDirectoryA(dumpDir.c_str(), nullptr);
        prefs.mute = true;
        std::cout << "Dumping " << dumpFrames << " frames to " << dumpDir << std::endl;
    }

    MSG msg {};
    while (app.running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                app.running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!app.running)
            break;

        // Wait until the next NES frame slot (never run unbound to display Hz).
        auto now = clock::now();
        if (now < nextFrame && dumpFrames <= 0)
            std::this_thread::sleep_until(nextFrame);
        now = clock::now();

        // Auto-press Start a few times so title→game for dumps
        std::uint8_t pad = scaler::pollController1();
        if (dumpFrames > 0 && dumpIndex > 30 && dumpIndex < 90 && ((dumpIndex / 10) % 2) == 0)
            pad |= (1u << 3); // Start (Controller::Start)
        nes.setController1(pad);
        nes.stepFrame();

        if (dumpFrames > 0 && dumpIndex < dumpFrames)
        {
            char path[512];
            std::snprintf(path, sizeof(path), "%s\\frame_%06d.rgba", dumpDir.c_str(), dumpIndex);
            std::ofstream out(path, std::ios::binary);
            const int bytes = scaler::NesHost::kWidth * scaler::NesHost::kHeight * 4;
            out.write(reinterpret_cast<const char*>(nes.frameRGBA()), bytes);
            ++dumpIndex;
            if (dumpIndex >= dumpFrames)
            {
                std::cout << "Dump complete: " << dumpFrames << " frames\n";
                app.running = false;
                break;
            }
        }

        renderer.uploadFrame(nes.frameRGBA(), scaler::NesHost::kWidth, scaler::NesHost::kHeight);
        renderer.present(prefs.scaleMode, prefs.sharpness, &nis, &tinySr, prefs.phaseX, prefs.phaseY);
        ++frames;

        nextFrame += kFramePeriod;
        // If we fell far behind (hitch), reschedule from now — do NOT catch up with burst frames.
        if (now - nextFrame > kFramePeriod * 2)
            nextFrame = now + kFramePeriod;

        if (now - lastTitle >= std::chrono::seconds(1))
        {
            app.fps    = frames / std::chrono::duration<double>(now - lastTitle).count();
            frames     = 0;
            lastTitle  = now;
            updateWindowTitle(hwnd);
        }
    }

    scaler::savePrefs(prefs);
    g_app = nullptr;
    timeEndPeriod(1);
    return 0;
}
