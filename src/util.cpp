#include "common.hpp"

#include <winhttp.h>

#include <chrono>
#include <cstdio>
#include <mutex>

namespace wmb {

namespace {
std::mutex g_log_mu;
FILE* g_log = nullptr;
} // namespace

void log_open(const std::filesystem::path& file) {
    std::scoped_lock lk{g_log_mu};
    if (g_log) std::fclose(g_log);
    g_log = _wfopen(file.c_str(), L"w");
}

void log_write(std::string_view line) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::scoped_lock lk{g_log_mu};
    if (!g_log) return;
    std::fprintf(g_log, "%02u:%02u:%02u.%03u %.*s\n", t.wHour, t.wMinute, t.wSecond,
                 t.wMilliseconds, static_cast<int>(line.size()), line.data());
    std::fflush(g_log);
}

std::string to_utf8(std::wstring_view w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                        nullptr);
    return s;
}

std::wstring to_wide(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string url_encode(std::string_view s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

HttpResult http_request(const wchar_t* method, const std::wstring& url,
                        const std::vector<std::pair<std::wstring, std::wstring>>& headers,
                        const std::string& body) {
    HttpResult r;
    URL_COMPONENTS uc{};
    uc.dwStructSize      = sizeof(uc);
    uc.dwHostNameLength  = static_cast<DWORD>(-1);
    uc.dwUrlPathLength   = static_cast<DWORD>(-1);
    uc.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        r.error = "bad url";
        return r;
    }
    const std::wstring host{uc.lpszHostName, uc.dwHostNameLength};
    std::wstring path{uc.lpszUrlPath, uc.dwUrlPathLength};
    path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET session = WinHttpOpen(L"WheresMyBuddy/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        r.error = std::format("WinHttpOpen failed ({})", GetLastError());
        return r;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    HINTERNET req  = conn ? WinHttpOpenRequest(conn, method, path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                          : nullptr;
    if (!req) {
        r.error = std::format("connect to {} failed ({})", to_utf8(host), GetLastError());
    } else {
        std::wstring hdrs;
        for (const auto& [k, v] : headers) hdrs += k + L": " + v + L"\r\n";
        const BOOL sent = WinHttpSendRequest(
            req, hdrs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdrs.c_str(),
            hdrs.empty() ? 0 : static_cast<DWORD>(-1L),
            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
        if (!sent || !WinHttpReceiveResponse(req, nullptr)) {
            r.error = std::format("request to {} failed ({})", to_utf8(host), GetLastError());
        } else {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
            r.status = static_cast<int>(status);
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail) || !avail) break;
                std::string chunk(avail, '\0');
                DWORD got = 0;
                if (!WinHttpReadData(req, chunk.data(), avail, &got) || !got) break;
                r.body.append(chunk.data(), got);
            }
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return r;
}

} // namespace wmb
