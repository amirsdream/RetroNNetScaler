#pragma once

#include <string>

namespace scaler
{
enum class ScaleMode
{
    Nearest = 0,
    Bilinear,
    NIS,
    TinySR, // Clarity: pure integer nearest
    Smooth, // Hyllian xBR-sharp (clean diagonals)
    Neural, // Trained TinySR CNN (4x residual)
    DLSS,   // falls back to NIS until Streamline linked
};

enum class OutputRes
{
    Res1080p = 0,
    Res1440p,
    Res4K,
};

struct Prefs
{
    ScaleMode scaleMode     = ScaleMode::Smooth;
    OutputRes outputRes     = OutputRes::Res4K;
    float     sharpness     = 0.0f;
    int       phaseX        = 0;
    int       phaseY        = 0;
    bool      vsync         = false;
    bool      fullscreen    = false;
    bool        mute         = false;
    int         windowWidth  = 3840;
    int         windowHeight = 2160;
    std::string lastRomPath;
};

const char* scaleModeName(ScaleMode mode);
const char* outputResName(OutputRes res);
ScaleMode   nextScaleMode(ScaleMode mode);
OutputRes   nextOutputRes(OutputRes res);
void        applyOutputRes(Prefs& prefs);

bool loadPrefs(Prefs& prefs);
bool savePrefs(const Prefs& prefs);
std::string prefsPath();
}
