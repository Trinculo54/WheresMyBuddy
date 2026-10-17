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

#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <wincrypt.h>

#include <algorithm>
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

void show_sign_in(const std::string& code, const std::string& url) {
    // Prefill the code on microsoft.com/link; the dialog is a fallback and
    // runs on its own thread so it never blocks the game.
    const std::wstring link = to_wide(url + "?otc=" + code);
    ShellExecuteW(nullptr, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    std::thread([code, url] {
        const std::wstring msg =
            L"WheresMyBuddy wants to use your Xbox friends as Forza Horizon 2 Drivatars.\n\n"
            L"A browser window has opened. If it didn't, go to\n    " +
            to_wide(url) + L"\nand enter the code\n\n    " + to_wide(code) +
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

// ---- Xbox Live tokens --------------------------------------------------------------

struct Xsts {
    std::string token, uhs;
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
        out.uhs = xj.at("DisplayClaims").at("xui").at(0).at("uhs").get<std::string>();
    } catch (...) {}
    if (x.status != 200 || out.token.empty() || out.uhs.empty()) {
        const auto why = xerr_text(xj);
        log("Xbox Live authorization failed ({}{}{})", x.status, why.empty() ? "" : ": " + why,
            x.error.empty() ? "" : " " + x.error);
        return false;
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
        if (!device_code_sign_in(p, s, msa)) return false;
    }
    if (!msa.refresh.empty()) save_refresh_token(p, msa.refresh);

    Xsts xsts;
    if (!xbox_tokens(msa.access, xsts)) return false;

    const auto r = http_request(
        L"GET", L"https://peoplehub.xboxlive.com/users/me/people/social/decoration/detail",
        {{L"Authorization", to_wide("XBL3.0 x=" + xsts.uhs + ";" + xsts.token)},
         {L"x-xbl-contract-version", L"5"},
         {L"Accept", L"application/json"},
         {L"Accept-Language", L"en-US"}});
    if (r.status != 200) {
        log("couldn't read your friends list ({}{})", r.status, r.error.empty() ? "" : " " + r.error);
        return false;
    }

    struct Person {
        std::string tag;
        bool favorite;
    };
    std::vector<Person> people;
    try {
        for (const auto& person : json::parse(r.body).at("people")) {
            std::string tag = jstr(person, "gamertag");
            // New-style gamertags are "Name#1234"; the classic one is the
            // same name without the suffix, which is what fits a Drivatar.
            if (tag.empty() || to_wide(tag).size() > kMaxGamertag) tag = jstr(person, "modernGamertag");
            if (tag.empty()) continue;
            people.push_back({tag, person.value("isFavorite", false)});
        }
    } catch (const std::exception& e) {
        log("friends list response wasn't understood: {}", e.what());
        return false;
    }
    if (s.favorites_first)
        std::stable_partition(people.begin(), people.end(), [](const Person& x) { return x.favorite; });
    for (const auto& x : people) out.push_back(x.tag);
    log("got {} people from Xbox Live ({} favourites)", out.size(),
        std::count_if(people.begin(), people.end(), [](const Person& x) { return x.favorite; }));
    return true;
}

} // namespace wmb
