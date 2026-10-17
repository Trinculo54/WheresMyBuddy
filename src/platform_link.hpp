#pragma once

// The FH2 Mod Platform (d3dcompiler_47.dll) calls our fh2mod_init right
// after loading us. With an old FH2 Mod Loader (1.x) nobody does, and the
// mod runs without the platform's services: no AI driver names, no update
// notices.

#include "fh2mod.h"

#include <windows.h>

#include <string>
#include <vector>

namespace wmb {

// Waits (up to `ms`) for fh2mod_init; null if there's no platform.
const FH2ModApi* wait_for_platform(DWORD ms);
// Our handle, once registered.
FH2Mod* platform_mod();
// The platform, if it's new enough for file overrides and interfaces (API 2).
const FH2ModApi* platform_v2();
// The platform, if it's new enough for hooks (API 3, platform 2.2).
const FH2ModApi* platform_v3();

// What the WheresMyBuddy.Friends interface hands out (wheresmybuddy.h).
void publish_drivatar_names(const std::vector<std::string>& names);
void publish_signed_in_gamertag(const std::string& gamertag);

} // namespace wmb
