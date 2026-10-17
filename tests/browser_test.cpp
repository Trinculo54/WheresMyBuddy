// Offline test of the loopback browser sign-in: pretends to be the browser.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "common.hpp"
#include "signin_browser.hpp"
#include <cstdio>
#include <thread>

using namespace wmb;
static std::string g_url;

static std::string param(const std::string& url, const std::string& key) {
    auto p = url.find("&" + key + "=");
    if (p == std::string::npos) p = url.find("?" + key + "=");
    if (p == std::string::npos) return {};
    p += key.size() + 2;
    return url.substr(p, url.find('&', p) - p);
}

static std::string get(unsigned short port, const std::string& target, bool v6) {
    SOCKET s = socket(v6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int rc;
    if (v6) {
        sockaddr_in6 a{}; a.sin6_family = AF_INET6; a.sin6_addr = in6addr_loopback; a.sin6_port = htons(port);
        rc = connect(s, (sockaddr*)&a, sizeof a);
    } else {
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons(port);
        rc = connect(s, (sockaddr*)&a, sizeof a);
    }
    if (rc != 0) { closesocket(s); return "CONNECT FAILED"; }
    std::string req = "GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(s, req.data(), (int)req.size(), 0);
    std::string resp; char buf[4096]; int n;
    while ((n = recv(s, buf, sizeof buf, 0)) > 0) resp.append(buf, n);
    closesocket(s);
    return resp;
}

int main(int argc, char** argv) {
    log_open("browser_test.log");
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    const bool deny = argc > 1;
    g_open_url = [](const std::string& u) { g_url = u; };
    Settings s; s.client_id = "12864041-3b5d-477c-b77c-b058dbb68ba0";
    BrowserCode bc;
    std::thread browser([&] {
        while (g_url.empty()) Sleep(50);
        const std::string redirect = param(g_url, "redirect_uri"); // http%3A%2F%2Flocalhost%3A1234
        const unsigned short port = (unsigned short)std::stoi(redirect.substr(redirect.rfind("%3A") + 3));
        const std::string state = param(g_url, "state");
        std::printf("url: %s\n", g_url.c_str());
        auto r1 = get(port, "/favicon.ico", false);
        std::printf("favicon -> %s\n", r1.substr(0, r1.find("\r\n")).c_str());
        auto r2 = get(port, "/?code=M.bad&state=wrong", true);
        if (r2 == "CONNECT FAILED") r2 = get(port, "/?code=M.bad&state=wrong", false); // no IPv6 here
        std::printf("bad state (v6) -> %s\n", r2.find("out of date") != std::string::npos ? "rejected ok" : r2.substr(0, 60).c_str());
        auto r3 = deny ? get(port, "/?error=access_denied&error_description=nope&state=" + state, false)
                       : get(port, "/?code=M.C507_BAY.2.U.abc%2Bdef-_x&state=" + state, false);
        auto t = r3.find("<h1>");
        std::printf("final -> %s / %s\n", r3.substr(0, r3.find("\r\n")).c_str(), r3.substr(t, r3.find("</h1>") - t).c_str());
    });
    auto res = browser_sign_in(s, "XboxLive.signin offline_access", bc);
    browser.join();
    std::printf("result=%d code=[%s] redirect=[%s]\nverifier=%s\nchallenge=%s\n", (int)res, bc.code.c_str(),
                bc.redirect_uri.c_str(), bc.verifier.c_str(), param(g_url, "code_challenge").c_str());
    return 0;
}
