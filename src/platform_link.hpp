#pragma once

// The FH2 Mod Platform (d3dcompiler_47.dll) calls our fh2mod_init right
// after loading us. With an old FH2 Mod Loader (1.x) nobody does, and the
// mod runs without the platform's services: no AI driver names, no update
// notices.

#include "fh2mod.h"

#include <windows.h>

namespace wmb {

// Waits (up to `ms`) for fh2mod_init; null if there's no platform.
const FH2ModApi* wait_for_platform(DWORD ms);
// Our handle, once registered.
FH2Mod* platform_mod();

} // namespace wmb
