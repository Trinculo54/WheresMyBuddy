#pragma once

#include "common.hpp"

namespace wmb {

enum class BrowserResult {
    Ok,          // got an authorization code
    Failed,      // cancelled, refused or timed out
    Unavailable, // couldn't even start (no loopback listener, etc.)
};

struct BrowserCode {
    std::string code, redirect_uri, verifier;
};

// Tests replace this to catch the sign-in URL instead of opening a browser.
extern void (*g_open_url)(const std::string& url);

BrowserResult browser_sign_in(const Settings& s, const char* scope, BrowserCode& out);

} // namespace wmb
