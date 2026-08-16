#include "prefs.h"

#include <fstream>
#include <string>

#include <shlobj.h>
#include <windows.h>

namespace scaler
{
const char* scaleModeName(ScaleMode mode)
{
    switch (mode)
    {
    case ScaleMode::Nearest:
        return "Nearest";
    case ScaleMode::Bilinear:
        return "Bilinear";
    case ScaleMode::NIS:
        return "NIS";
    case ScaleMode::TinySR:
        return "Clarity";
    case ScaleMode::Smooth:
        return "Smooth(sharp)";
    case ScaleMode::Neural:
        return "Contour"; // hard fills + continuous diagonals (no cream)
    case ScaleMode::DLSS:
        return "DLSS(NIS)";
    }
    return "Unknown";
}

const char* outputResName(OutputRes res)
{
    switch (res)
    {
    case OutputRes::Res1080p:
        return "1080p";
    case OutputRes::Res1440p:
        return "1440p";
    case OutputRes::Res4K:
        return "4K";
    }
    return "?";
}

ScaleMode nextScaleMode(ScaleMode mode)
{
    switch (mode)
    {
    case ScaleMode::Nearest:
        return ScaleMode::Bilinear;
    case ScaleMode::Bilinear:
        return ScaleMode::NIS;
    case ScaleMode::NIS:
        return ScaleMode::TinySR;
    case ScaleMode::TinySR:
        return ScaleMode::Smooth;
    case ScaleMode::Smooth:
        return ScaleMode::Neural;
    case ScaleMode::Neural:
        return ScaleMode::DLSS;
    case ScaleMode::DLSS:
        return ScaleMode::Nearest;
    }
    return ScaleMode::Smooth;
}

OutputRes nextOutputRes(OutputRes res)
{
    switch (res)
    {
    case OutputRes::Res1080p:
        return OutputRes::Res1440p;
    case OutputRes::Res1440p:
        return OutputRes::Res4K;
    case OutputRes::Res4K:
        return OutputRes::Res1080p;
    }
    return OutputRes::Res4K;
}

void applyOutputRes(Prefs& prefs)
{
    switch (prefs.outputRes)
    {
    case OutputRes::Res1080p:
        prefs.windowWidth  = 1920;
        prefs.windowHeight = 1080;
        break;
    case OutputRes::Res1440p:
        prefs.windowWidth  = 2560;
        prefs.windowHeight = 1440;
        break;
    case OutputRes::Res4K:
        prefs.windowWidth  = 3840;
        prefs.windowHeight = 2160;
        break;
    }
}

std::string prefsPath()
{
    char appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)))
        return "4kscaler_prefs.ini";

    std::string dir = std::string(appData) + "\\4kscaler";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\prefs.ini";
}

bool loadPrefs(Prefs& prefs)
{
    std::ifstream in(prefsPath());
    if (!in)
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "scaleMode")
            prefs.scaleMode = static_cast<ScaleMode>(std::stoi(val));
        else if (key == "outputRes")
            prefs.outputRes = static_cast<OutputRes>(std::stoi(val));
        else if (key == "sharpness")
            prefs.sharpness = std::stof(val);
        else if (key == "phaseX")
            prefs.phaseX = std::stoi(val);
        else if (key == "phaseY")
            prefs.phaseY = std::stoi(val);
        else if (key == "vsync")
            prefs.vsync = val == "1";
        else if (key == "fullscreen")
            prefs.fullscreen = val == "1";
        else if (key == "mute")
            prefs.mute = val == "1";
        else if (key == "windowWidth")
            prefs.windowWidth = std::stoi(val);
        else if (key == "windowHeight")
            prefs.windowHeight = std::stoi(val);
        else if (key == "lastRomPath")
            prefs.lastRomPath = val;
    }
    return true;
}

bool savePrefs(const Prefs& prefs)
{
    std::ofstream out(prefsPath(), std::ios::trunc);
    if (!out)
        return false;
    out << "scaleMode=" << static_cast<int>(prefs.scaleMode) << '\n';
    out << "outputRes=" << static_cast<int>(prefs.outputRes) << '\n';
    out << "sharpness=" << prefs.sharpness << '\n';
    out << "phaseX=" << prefs.phaseX << '\n';
    out << "phaseY=" << prefs.phaseY << '\n';
    out << "vsync=" << (prefs.vsync ? 1 : 0) << '\n';
    out << "fullscreen=" << (prefs.fullscreen ? 1 : 0) << '\n';
    out << "mute=" << (prefs.mute ? 1 : 0) << '\n';
    out << "windowWidth=" << prefs.windowWidth << '\n';
    out << "windowHeight=" << prefs.windowHeight << '\n';
    out << "lastRomPath=" << prefs.lastRomPath << '\n';
    return true;
}
}
