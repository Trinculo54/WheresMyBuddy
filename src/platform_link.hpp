#pragma once

// FH2ML loads WheresMyBuddy from mods\wheresmybuddy and calls our Initialize
// (dll_main.cpp) as Azimuth starts. Initialize hands FH2ML's services to
// connect_fh2ml before it starts anything else, so every thread of ours has
// them.

#include "FH2ML_v4.h"

#include <windows.h>

#include <string>
#include <vector>

namespace wmb {

struct Fh2ml {
    const FH2ML_Services_v4* S;
    FH2ML_Mod* mod;       // our handle for every service
    std::wstring azimuth; // the folder with azimuth.exe
    std::wstring data;    // FH2ML\data\wheresmybuddy: settings, friends.txt, sign-in, logs
};

// From Initialize: keeps the services, brings the player's files over from the
// old <Azimuth>\wheresmybuddy folder (once), publishes the WheresMyBuddy.Friends
// interface and adds our Mods-page rows. False if `context` is unusable.
bool connect_fh2ml(const FH2ML_ModContext_v4* context);

// FH2ML's services and our handle, once Initialize has run; null before that,
// and in the offline tests, which run without FH2ML.
const Fh2ml* fh2ml();

// What the WheresMyBuddy.Friends interface hands out (wheresmybuddy.h).
void publish_drivatar_names(const std::vector<std::string>& names);
void publish_signed_in_gamertag(const std::string& gamertag);

} // namespace wmb
