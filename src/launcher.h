#pragma once

#include "prefs.h"

#include <string>

namespace scaler
{
struct LaunchConfig
{
    std::string romPath;
    Prefs       prefs;
    bool        cancelled = true;
};

/// Modal start screen. Returns false if user cancelled/closed.
bool showLauncher(LaunchConfig& cfg);
}
