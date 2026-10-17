// Xbox Live: Microsoft account sign-in -> Xbox user token -> XSTS -> friends.
//
//  1. Microsoft identity platform, "consumers" tenant, public client:
//       first run : device code flow (you approve at microsoft.com/link)
//       after that: refresh token, kept in account.dat encrypted with DPAPI
//                   (only your Windows account can decrypt it)
//     scope "XboxLive.signin offline_access"
//  2. user.auth.xboxlive.com   RpsTicket "d=<access token>" -> user token + uhs
//  3. xsts.auth.xboxlive.com   RelyingParty http://xboxlive.com -> XSTS token
//  4. peoplehub.xboxlive.com   /users/me/people/social -> people you follow

#include "common.hpp"
#include "signin_browser.hpp"

#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <wincrypt.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <thread>

namespace wmb {

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace {

constexpr const wchar_t* kLogin =
    L"https://login.microsoftonline.com/consumers/oauth2/v2.0/";
constexpr const char* kScope = "XboxLive.signin offline_access";

const std::vector<std::pair<std::wstring, std::wstring>> kForm = {
    {L"Content-Type", L"application/x-www-form-urlencoded"}};

json parse(const HttpResult& r) {
    try {
        return json::parse(r.body);
    } catch (...) {
        return json::object();
    }
}

std::string jstr(const json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

// ---- refresh token, DPAPI-protected --------------------------------------------

std::string load_refresh_token(const Paths& p) {
    std::ifstream in{p.data / "account.dat", std::ios::binary};
    std::string blob((std::istreambuf_iterator<char>(in)), {});
    if (blob.empty()) return {};
    DATA_BLOB src{static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE*>(blob.data())}, dst{};
    if (!CryptUnprotectData(&src, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dst))
        return {};
    std::string out(reinterpret_cast<char*>(dst.pbData), dst.cbData);
    LocalFree(dst.pbData);
    return out;
}

void save_refresh_token(const Paths& p, const std::string& token) {
    DATA_BLOB src{static_cast<DWORD>(token.size()),
                  reinterpret_cast<BYTE*>(const_cast<char*>(token.data()))},
        dst{};
    if (!CryptProtectData(&src, L"WheresMyBuddy", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &dst)) {
        log("couldn't encrypt the sign-in token (error {}); you'll be asked to sign in again",
            GetLastError());
        return;
    }
    std::ofstream{p.data / "account.dat", std::ios::binary | std::ios::trunc}.write(
        reinterpret_cast<char*>(dst.pbData), dst.cbData);
    LocalFree(dst.pbData);
}

// ---- Microsoft sign-in -----------------------------------------------------------

struct MsaTokens {
    std::string access, refresh;
};

bool refresh_sign_in(const Settings& s, const std::string& refresh, MsaTokens& out) {
    const std::string body = "client_id=" + url_encode(s.client_id) +
                             "&grant_type=refresh_token&refresh_token=" + url_encode(refresh) +
                             "&scope=" + url_encode(kScope);
    const auto r = http_request(L"POST", std::wstring{kLogin} + L"token", kForm, body);
    const auto j = parse(r);
    out.access   = jstr(j, "access_token");
    out.refresh  = jstr(j, "refresh_token");
    if (r.status == 200 && !out.access.empty()) return true;
    log("saved sign-in no longer works ({} {}{}); signing in again", r.status, jstr(j, "error"),
        r.error.empty() ? "" : " " + r.error);
    return false;
}

bool copy_to_clipboard(const std::string& text) {
    const std::wstring w = to_wide(text);
    if (!OpenClipboard(nullptr)) return false;
    bool ok = false;
    EmptyClipboard();
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t))) {
        std::memcpy(GlobalLock(mem), w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(mem);
        ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
        if (!ok) GlobalFree(mem);
    }
    CloseClipboard();
    return ok;
}

void show_sign_in(const std::string& code, const std::string& url) {
    // Open the plain page: the prefilled "?otc=" link fails with a
    // "first party application" consent error, typing the code works.
    // The code goes on the clipboard so it's just Ctrl+V. The dialog runs
    // on its own thread so it never blocks the game.
    const bool copied = copy_to_clipboard(code);
    ShellExecuteW(nullptr, L"open", to_wide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    std::thread([code, url, copied] {
        const std::wstring msg =
            L"WheresMyBuddy wants to use your Xbox friends as Forza Horizon 2 Drivatars.\n\n"
            L"A browser window has opened at\n    " +
            to_wide(url) + L"\nEnter this code there" +
            (copied ? L" (it's already copied, just press Ctrl+V)" : L"") + L":\n\n    " + to_wide(code) +
            L"\n\nSign in with the Microsoft account you use on Xbox. You only need to do this once.\n"
            L"(Set enabled=0 under [xbox] in wheresmybuddy\\WheresMyBuddy.ini to stop this.)";
        MessageBoxW(nullptr, msg.c_str(), L"WheresMyBuddy: sign in to Xbox",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }).detach();
}

bool device_code_sign_in(const Paths& p, const Settings& s, MsaTokens& out) {
    const std::string body = "client_id=" + url_encode(s.client_id) + "&scope=" + url_encode(kScope);
    const auto r = http_request(L"POST", std::wstring{kLogin} + L"devicecode", kForm, body);
    const auto j = parse(r);
    const std::string device = jstr(j, "device_code");
    const std::string code   = jstr(j, "user_code");
    std::string url          = jstr(j, "verification_uri");
    if (r.status != 200 || device.empty() || code.empty()) {
        log("couldn't start Microsoft sign-in ({} {} {}{})", r.status, jstr(j, "error"),
            jstr(j, "error_description").substr(0, 200), r.error.empty() ? "" : " " + r.error);
        return false;
    }
    if (url.empty()) url = "https://www.microsoft.com/link";
    int interval   = j.value("interval", 5);
    const int life = j.value("expires_in", 900);

    std::ofstream{p.data / "SIGN-IN.txt", std::ios::trunc}
        << "To let WheresMyBuddy read your Xbox friends, go to " << url
        << " and enter the code " << code << "\r\n(This file is deleted once you've signed in.)\r\n";
    log("sign in at {} with code {}", url, code);
    show_sign_in(code, url);

    const std::string poll = "client_id=" + url_encode(s.client_id) +
                             "&grant_type=urn:ietf:params:oauth:grant-type:device_code"
                             "&device_code=" +
                             url_encode(device);
    for (int waited = 0; waited < life; waited += interval) {
        Sleep(static_cast<DWORD>(interval) * 1000);
        const auto t  = http_request(L"POST", std::wstring{kLogin} + L"token", kForm, poll);
        const auto tj = parse(t);
        if (t.status == 200) {
            out.access  = jstr(tj, "access_token");
            out.refresh = jstr(tj, "refresh_token");
            std::error_code ec;
            fs::remove(p.data / "SIGN-IN.txt", ec);
            log("signed in to Microsoft");
            return !out.access.empty();
        }
        const std::string err = jstr(tj, "error");
        if (err == "authorization_pending") continue;
        if (err == "slow_down") {
            interval += 5;
            continue;
        }
        log("sign-in didn't finish: {} {}", err, jstr(tj, "error_description").substr(0, 200));
        return false;
    }
    log("sign-in code expired; you'll get a new one next launch");
    return false;
}

bool browser_code_sign_in(const Settings& s, const BrowserCode& bc, MsaTokens& out) {
    const std::string body = "client_id=" + url_encode(s.client_id) +
                             "&grant_type=authorization_code&code=" + url_encode(bc.code) +
                             "&redirect_uri=" + url_encode(bc.redirect_uri) +
                             "&code_verifier=" + url_encode(bc.verifier) + "&scope=" + url_encode(kScope);
    const auto r = http_request(L"POST", std::wstring{kLogin} + L"token", kForm, body);
    const auto j = parse(r);
    out.access   = jstr(j, "access_token");
    out.refresh  = jstr(j, "refresh_token");
    if (r.status == 200 && !out.access.empty()) {
        log("signed in to Microsoft");
        return true;
    }
    log("sign-in didn't finish ({} {} {}{})", r.status, jstr(j, "error"),
        jstr(j, "error_description").substr(0, 200), r.error.empty() ? "" : " " + r.error);
    return false;
}

// Browser page first; the microsoft.com/link code if the browser route
// can't start (or browser_sign_in=0).
bool interactive_sign_in(const Paths& p, const Settings& s, MsaTokens& out) {
    if (s.browser_sign_in) {
        BrowserCode bc;
        switch (browser_sign_in(s, kScope, bc)) {
            case BrowserResult::Ok: return browser_code_sign_in(s, bc, out);
            case BrowserResult::Failed: return false;
            case BrowserResult::Unavailable: log("falling back to a sign-in code"); break;
        }
    }
    return device_code_sign_in(p, s, out);
}

// ---- Xbox Live tokens --------------------------------------------------------------

struct Xsts {
    std::string token, uhs;
    std::string gamertag, xuid; // who actually signed in
};

std::string xerr_text(const json& j) {
    const auto x = j.value("XErr", 0LL);
    switch (x) {
        case 2148916233LL: return "this Microsoft account has no Xbox profile yet";
        case 2148916235LL: return "Xbox Live isn't available in your country";
        case 2148916236LL:
        case 2148916237LL: return "the account needs adult verification on xbox.com";
        case 2148916238LL: return "it's a child account; an adult has to add it to a Family";
        default: return x ? std::format("XErr {}", x) : std::string{};
    }
}

bool xbox_tokens(const std::string& access, Xsts& out) {
    const std::vector<std::pair<std::wstring, std::wstring>> hj = {
        {L"Content-Type", L"application/json"}, {L"Accept", L"application/json"},
        {L"x-xbl-contract-version", L"1"}};

    const json ub{{"RelyingParty", "http://auth.xboxlive.com"},
                  {"TokenType", "JWT"},
                  {"Properties",
                   {{"AuthMethod", "RPS"}, {"SiteName", "user.auth.xboxlive.com"}, {"RpsTicket", "d=" + access}}}};
    const auto u  = http_request(L"POST", L"https://user.auth.xboxlive.com/user/authenticate", hj, ub.dump());
    const auto uj = parse(u);
    const std::string user_token = jstr(uj, "Token");
    if (u.status != 200 || user_token.empty()) {
        log("Xbox Live user sign-in failed ({}{})", u.status, u.error.empty() ? "" : " " + u.error);
        return false;
    }

    const json xb{{"RelyingParty", "http://xboxlive.com"},
                  {"TokenType", "JWT"},
                  {"Properties", {{"SandboxId", "RETAIL"}, {"UserTokens", json::array({user_token})}}}};
    const auto x  = http_request(L"POST", L"https://xsts.auth.xboxlive.com/xsts/authorize", hj, xb.dump());
    const auto xj = parse(x);
    out.token     = jstr(xj, "Token");
    try {
        const auto& xui = xj.at("DisplayClaims").at("xui").at(0);
        out.uhs         = jstr(xui, "uhs");
        out.gamertag    = jstr(xui, "gtg");
        out.xuid        = jstr(xui, "xid");
    } catch (...) {}
    if (x.status != 200 || out.token.empty() || out.uhs.empty()) {
        const auto why = xerr_text(xj);
        log("Xbox Live authorization failed ({}{}{})", x.status, why.empty() ? "" : ": " + why,
            x.error.empty() ? "" : " " + x.error);
        return false;
    }
    log("signed in to Xbox Live as {}", out.gamertag.empty() ? std::string{"(gamertag not returned)"}
                                                             : out.gamertag);
    return true;
}

struct Person {
    std::string tag;
    bool favorite;
};

std::wstring xbl_auth(const Xsts& x) { return to_wide("XBL3.0 x=" + x.uhs + ";" + x.token); }

std::string snippet(const std::string& body) {
    std::string s = body.substr(0, 600);
    for (char& c : s)
        if (c == '\r' || c == '\n') c = ' ';
    return s;
}

// The people hub: one call, gamertags included.
bool people_hub(const Xsts& x, std::vector<Person>& people) {
    const auto r = http_request(
        L"GET", L"https://peoplehub.xboxlive.com/users/me/people/social/decoration/detail",
        {{L"Authorization", xbl_auth(x)},
         {L"x-xbl-contract-version", L"5"},
         {L"Accept", L"application/json"},
         {L"Accept-Language", L"en-US"}});
    if (r.status != 200) {
        log("people hub: {}{} {}", r.status, r.error.empty() ? "" : " " + r.error, snippet(r.body));
        return false;
    }
    try {
        const json body = json::parse(r.body);
        for (const auto& person : body.at("people")) {
            std::string tag = jstr(person, "gamertag");
            // New-style gamertags are "Name#1234"; the classic one is the
            // same name without the suffix, which is what fits a Drivatar.
            if (tag.empty() || to_wide(tag).size() > kMaxGamertag) tag = jstr(person, "modernGamertag");
            if (tag.empty()) continue;
            people.push_back({tag, person.value("isFavorite", false)});
        }
    } catch (const std::exception& e) {
        log("people hub reply wasn't understood ({}): {}", e.what(), snippet(r.body));
        return false;
    }
    if (people.empty()) log("people hub returned nobody: {}", snippet(r.body));
    return true;
}

// Fallback: the social service lists who you follow (xuids only), then the
// profile service turns those into gamertags.
bool social_and_profile(const Xsts& x, std::vector<Person>& people) {
    if (x.xuid.empty()) {
        log("social: no xuid in the Xbox token, skipping");
        return false;
    }
    const auto r = http_request(
        L"GET", to_wide("https://social.xboxlive.com/users/xuid(" + x.xuid + ")/people?maxItems=1000"),
        {{L"Authorization", xbl_auth(x)}, {L"x-xbl-contract-version", L"1"}, {L"Accept", L"application/json"}});
    if (r.status != 200) {
        log("social: {}{} {}", r.status, r.error.empty() ? "" : " " + r.error, snippet(r.body));
        return false;
    }
    std::vector<std::pair<std::string, bool>> ids;
    try {
        const json body = json::parse(r.body);
        for (const auto& person : body.at("people")) {
            std::string id = jstr(person, "xuid");
            if (!id.empty()) ids.emplace_back(id, person.value("isFavorite", false));
        }
    } catch (const std::exception& e) {
        log("social reply wasn't understood ({}): {}", e.what(), snippet(r.body));
        return false;
    }
    log("social: you follow {} people", ids.size());
    if (ids.empty()) {
        log("social returned nobody: {}", snippet(r.body));
        return true;
    }

    for (std::size_t at = 0; at < ids.size(); at += 100) {
        json req{{"settings", json::array({"Gamertag", "ModernGamertag"})}, {"userIds", json::array()}};
        for (std::size_t i = at; i < ids.size() && i < at + 100; ++i) req["userIds"].push_back(ids[i].first);
        const auto pr = http_request(L"POST", L"https://profile.xboxlive.com/users/batch/profile/settings",
                                     {{L"Authorization", xbl_auth(x)},
                                      {L"x-xbl-contract-version", L"2"},
                                      {L"Content-Type", L"application/json"},
                                      {L"Accept", L"application/json"}},
                                     req.dump());
        if (pr.status != 200) {
            log("profile: {}{} {}", pr.status, pr.error.empty() ? "" : " " + pr.error, snippet(pr.body));
            return !people.empty();
        }
        try {
            const json body = json::parse(pr.body);
            for (const auto& u : body.at("profileUsers")) {
                std::string tag, modern;
                for (const auto& st : u.at("settings")) {
                    if (jstr(st, "id") == "Gamertag") tag = jstr(st, "value");
                    if (jstr(st, "id") == "ModernGamertag") modern = jstr(st, "value");
                }
                if (tag.empty() || to_wide(tag).size() > kMaxGamertag) tag = modern;
                if (tag.empty()) continue;
                const std::string id = jstr(u, "id");
                bool fav = false;
                for (const auto& [xid, f] : ids)
                    if (xid == id) fav = f;
                people.push_back({tag, fav});
            }
        } catch (const std::exception& e) {
            log("profile reply wasn't understood ({}): {}", e.what(), snippet(pr.body));
            return !people.empty();
        }
    }
    return true;
}

} // namespace

// ---- friends ---------------------------------------------------------------------

bool fetch_xbox_friends(const Paths& p, const Settings& s, std::vector<std::string>& out) {
    out.clear();
    if (s.client_id.empty()) {
        log("Xbox sign-in skipped: no client_id set under [xbox] in WheresMyBuddy.ini");
        return false;
    }

    MsaTokens msa;
    const std::string saved = load_refresh_token(p);
    if (saved.empty() || !refresh_sign_in(s, saved, msa)) {
        if (!interactive_sign_in(p, s, msa)) return false;
    }
    if (!msa.refresh.empty()) save_refresh_token(p, msa.refresh);

    Xsts xsts;
    if (!xbox_tokens(msa.access, xsts)) return false;

    std::vector<Person> people;
    const bool hub_ok = people_hub(xsts, people);
    if (people.empty()) {
        log("trying the social + profile services instead");
        if (!social_and_profile(xsts, people) && !hub_ok) return false;
    }
    if (s.favorites_first)
        std::stable_partition(people.begin(), people.end(), [](const Person& x) { return x.favorite; });
    for (const auto& x : people) out.push_back(x.tag);
    log("got {} people from Xbox Live ({} favourites)", out.size(),
        std::count_if(people.begin(), people.end(), [](const Person& x) { return x.favorite; }));
    return true;
}

} // namespace wmb
