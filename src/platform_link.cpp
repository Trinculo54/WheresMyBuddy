#include "platform_link.hpp"

#include <atomic>
#include <string>

namespace {
std::atomic<const FH2ModApi*> g_api{nullptr};
std::atomic<FH2Mod*> g_mod{nullptr};
std::atomic<bool> g_gave_up{false};
HANDLE g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
} // namespace

namespace wmb {

const FH2ModApi* wait_for_platform(DWORD ms) {
    // Only the first caller waits; after that, no platform means no platform.
    if (!g_api && !g_gave_up && g_ready && WaitForSingleObject(g_ready, ms) != WAIT_OBJECT_0) g_gave_up = true;
    return g_api;
}

FH2Mod* platform_mod() { return g_mod; }

} // namespace wmb

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api || api->api_version < 1) return 1;
    // [updates] check in our ini; read here directly since the rest of the
    // settings load on our own thread.
    const std::wstring ini = std::wstring{api->azimuth_dir} + L"\\wheresmybuddy\\WheresMyBuddy.ini";
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "WheresMyBuddy";
    info.version          = WMB_VERSION;
    info.github_repo      = "Trinculo54/WheresMyBuddy";
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = GetPrivateProfileIntW(L"updates", L"check", 1, ini.c_str()) != 0;
    g_mod = api->register_mod(&info);
    g_api = api;
    if (g_ready) SetEvent(g_ready);
    return g_mod ? 0 : 1;
}
