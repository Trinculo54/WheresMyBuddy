// Browser sign-in: the standard "Sign in with Microsoft" page, OAuth 2.0
// authorization code flow with PKCE and a loopback redirect.
//
//   1. Listen on localhost (loopback only, random port).
//   2. Open login.microsoftonline.com/consumers/.../authorize in the
//      default browser with redirect_uri=http://localhost:<port>.
//   3. After you approve, the browser comes back to that port with
//      ?code=...; we answer with a "you're signed in" page.
//   4. xbox.cpp swaps the code (plus the PKCE verifier) for tokens.
//
// The Azure app needs http://localhost as a "Mobile and desktop
// applications" redirect URI; any port is accepted for loopback.

#include <winsock2.h>
#include <ws2tcpip.h>

#include "common.hpp"
#include "signin_browser.hpp"

#include <bcrypt.h>
#include <shellapi.h>

#include <atomic>
#include <thread>

namespace wmb {

namespace {

constexpr const wchar_t* kDialogTitle = L"WheresMyBuddy: sign in to Xbox";

std::string b64url(const unsigned char* d, std::size_t n) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < n; i += 3) {
        const unsigned v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        out += t[v >> 18];
        out += t[(v >> 12) & 63];
        out += t[(v >> 6) & 63];
        out += t[v & 63];
    }
    if (n - i == 1) {
        const unsigned v = d[i] << 16;
        out += t[v >> 18];
        out += t[(v >> 12) & 63];
    } else if (n - i == 2) {
        const unsigned v = (d[i] << 16) | (d[i + 1] << 8);
        out += t[v >> 18];
        out += t[(v >> 12) & 63];
        out += t[(v >> 6) & 63];
    }
    return out;
}

bool random_b64url(std::size_t bytes, std::string& out) {
    std::string buf(bytes, '\0');
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(buf.data()), static_cast<ULONG>(bytes),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return false;
    out = b64url(reinterpret_cast<const unsigned char*>(buf.data()), bytes);
    return true;
}

bool sha256_b64url(const std::string& in, std::string& out) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h  = nullptr;
    unsigned char digest[32];
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
              BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0 &&
              BCryptHashData(h, reinterpret_cast<PUCHAR>(const_cast<char*>(in.data())),
                             static_cast<ULONG>(in.size()), 0) == 0 &&
              BCryptFinishHash(h, digest, sizeof(digest), 0) == 0;
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (ok) out = b64url(digest, sizeof(digest));
    return ok;
}

std::string url_decode(std::string_view s) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
            out += static_cast<char>(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string query_param(std::string_view q, std::string_view key) {
    while (!q.empty()) {
        const auto amp       = q.find('&');
        std::string_view kv  = q.substr(0, amp);
        const auto eq        = kv.find('=');
        if (kv.substr(0, eq) == key) return eq == std::string_view::npos ? "" : url_decode(kv.substr(eq + 1));
        if (amp == std::string_view::npos) break;
        q.remove_prefix(amp + 1);
    }
    return {};
}

std::string html_escape(std::string_view s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '&': o += "&amp;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}

void send_all(SOCKET c, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int n = send(c, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return;
        sent += static_cast<std::size_t>(n);
    }
}

void send_page(SOCKET c, int status, const std::string& heading, const std::string& text) {
    const std::string body =
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>WheresMyBuddy</title>"
        "<style>body{font-family:'Segoe UI',system-ui,sans-serif;background:#f3f3f3;color:#1b1b1b;"
        "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
        "main{background:#fff;padding:40px 48px;border-radius:8px;box-shadow:0 2px 12px rgba(0,0,0,.12);"
        "max-width:440px}h1{font-size:22px;font-weight:600;margin:0 0 12px}p{margin:0;line-height:1.5}"
        "@media(prefers-color-scheme:dark){body{background:#1f1f1f;color:#eee}main{background:#2b2b2b}}"
        "</style></head><body><main><h1>" +
        html_escape(heading) + "</h1><p>" + html_escape(text) + "</p></main></body></html>";
    send_all(c, std::format("HTTP/1.1 {} {}\r\nContent-Type: text/html; charset=utf-8\r\n"
                            "Content-Length: {}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
                            status, status == 200 ? "OK" : "Not Found", body.size()) +
                    body);
}

// Reads one request line ("GET /?code=... HTTP/1.1") from a fresh connection.
std::string read_request_target(SOCKET c) {
    DWORD timeout = 5000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    std::string req;
    char buf[2048];
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 16384) {
        const int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, static_cast<std::size_t>(n));
    }
    if (req.rfind("GET ", 0) != 0) return {};
    const auto end = req.find(' ', 4);
    return end == std::string::npos ? std::string{} : req.substr(4, end - 4);
}

SOCKET listen_on(int family, const sockaddr* addr, int len) {
    SOCKET s = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return s;
    if (bind(s, addr, len) != 0 || listen(s, 4) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

// A small always-on-top note so the browser tab isn't lost behind the game.
// It closes itself when sign-in finishes.
void show_note() {
    std::thread([] {
        MessageBoxW(nullptr,
                    L"WheresMyBuddy opened the Microsoft sign-in page in your browser.\n\n"
                    L"Sign in with the account you use on Xbox and allow access. "
                    L"This box closes by itself when you're done.\n\n"
                    L"(Set enabled=0 under [xbox] in FH2ML\\data\\wheresmybuddy\\WheresMyBuddy.ini to stop this.)",
                    kDialogTitle, MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }).detach();
}

void close_note() {
    for (int i = 0; i < 10; ++i) {
        if (HWND w = FindWindowW(L"#32770", kDialogTitle)) {
            PostMessageW(w, WM_CLOSE, 0, 0);
            return;
        }
        Sleep(100);
    }
}

} // namespace

void (*g_open_url)(const std::string& url) = nullptr;

BrowserResult browser_sign_in(const Settings& s, const char* scope, BrowserCode& out) {
    std::string state, challenge;
    if (!random_b64url(48, out.verifier) || !random_b64url(16, state) ||
        !sha256_b64url(out.verifier, challenge)) {
        log("browser sign-in: couldn't generate the PKCE values");
        return BrowserResult::Unavailable;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        log("browser sign-in: Winsock didn't start");
        return BrowserResult::Unavailable;
    }

    // Loopback only, so no firewall prompt. Browsers may resolve
    // "localhost" to either 127.0.0.1 or ::1, so listen on both.
    sockaddr_in v4{};
    v4.sin_family      = AF_INET;
    v4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    SOCKET s4          = listen_on(AF_INET, reinterpret_cast<sockaddr*>(&v4), sizeof(v4));
    if (s4 == INVALID_SOCKET) {
        log("browser sign-in: couldn't listen on localhost ({})", WSAGetLastError());
        WSACleanup();
        return BrowserResult::Unavailable;
    }
    int len = sizeof(v4);
    getsockname(s4, reinterpret_cast<sockaddr*>(&v4), &len);
    const unsigned short port = ntohs(v4.sin_port);
    sockaddr_in6 v6{};
    v6.sin6_family = AF_INET6;
    v6.sin6_addr   = in6addr_loopback;
    v6.sin6_port   = htons(port);
    SOCKET s6      = listen_on(AF_INET6, reinterpret_cast<sockaddr*>(&v6), sizeof(v6));

    out.redirect_uri = std::format("http://localhost:{}", port);
    const std::string url =
        "https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize"
        "?client_id=" + url_encode(s.client_id) +
        "&response_type=code&response_mode=query"
        "&redirect_uri=" + url_encode(out.redirect_uri) +
        "&scope=" + url_encode(scope) +
        "&state=" + state +
        "&code_challenge=" + challenge + "&code_challenge_method=S256"
        "&prompt=select_account";

    log("opening Microsoft sign-in in the browser (waiting on {})", out.redirect_uri);
    if (g_open_url) {
        g_open_url(url);
    } else {
        ShellExecuteW(nullptr, L"open", to_wide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        show_note();
    }

    BrowserResult result = BrowserResult::Failed;
    const ULONGLONG deadline = GetTickCount64() + 10 * 60 * 1000;
    bool done = false;
    while (!done && GetTickCount64() < deadline) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(s4, &rd);
        if (s6 != INVALID_SOCKET) FD_SET(s6, &rd);
        timeval tv{1, 0};
        if (select(0, &rd, nullptr, nullptr, &tv) <= 0) continue;
        for (SOCKET ls : {s4, s6}) {
            if (done || ls == INVALID_SOCKET || !FD_ISSET(ls, &rd)) continue;
            SOCKET c = accept(ls, nullptr, nullptr);
            if (c == INVALID_SOCKET) continue;
            const std::string target = read_request_target(c);
            const auto qpos          = target.find('?');
            const std::string path   = target.substr(0, qpos);
            const std::string query  = qpos == std::string::npos ? "" : target.substr(qpos + 1);
            if (path != "/" || query.empty()) {
                send_page(c, 404, "Not found", "Nothing here.");
            } else if (query_param(query, "state") != state) {
                send_page(c, 200, "Something went wrong",
                          "This sign-in link is out of date. Restart the game to try again.");
            } else if (const auto err = query_param(query, "error"); !err.empty()) {
                const auto why = query_param(query, "error_description");
                log("browser sign-in was cancelled or refused: {} {}", err, why.substr(0, 200));
                send_page(c, 200, "Not signed in",
                          err == "access_denied"
                              ? "You didn't allow access, so WheresMyBuddy will keep the normal "
                                "Drivatar names. You'll be asked again next time you start the game."
                              : "Microsoft didn't complete the sign-in (" + err +
                                    "). You can close this tab; you'll be asked again next time.");
                done = true;
            } else if (auto code = query_param(query, "code"); !code.empty()) {
                out.code = std::move(code);
                send_page(c, 200, "You're signed in",
                          "WheresMyBuddy can now see your Xbox friends. You can close this tab and go "
                          "back to Forza Horizon 2.");
                result = BrowserResult::Ok;
                done   = true;
            } else {
                send_page(c, 404, "Not found", "Nothing here.");
            }
            shutdown(c, SD_SEND);
            closesocket(c);
        }
    }
    if (!done) log("browser sign-in timed out; you'll be asked again next launch");
    closesocket(s4);
    if (s6 != INVALID_SOCKET) closesocket(s6);
    WSACleanup();
    close_note();
    return result;
}

} // namespace wmb
