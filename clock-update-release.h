#pragma once
#include <winhttp.h>
#include <array>
#include <chrono>
#include <memory>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "version.lib")

constexpr wchar_t GithubProjectUrl[] = L"https://github.com/junhwan123/SkeaClock";
constexpr wchar_t GithubLatestReleaseUrl[] = L"https://github.com/junhwan123/SkeaClock/releases/latest";
constexpr long long UpdateCheckInterval = 72LL * 60 * 60;

long long UpdateCheckTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

using UpdateVersion = std::array<unsigned, 3>;

bool ParseUpdateVersion(const std::wstring& tag, UpdateVersion& version) {
    size_t position = !tag.empty() && (tag[0] == L'v' || tag[0] == L'V') ? 1 : 0;
    version = {};
    for (size_t part = 0; part < version.size(); ++part) {
        const size_t start = position;
        unsigned value = 0;
        while (position < tag.size() && tag[position] >= L'0' && tag[position] <= L'9') {
            value = value * 10 + (tag[position++] - L'0');
            if (value > 65535) return false;
        }
        if (position == start) return false;
        version[part] = value;
        if (part + 1 < version.size() && (position == tag.size() || tag[position++] != L'.')) return false;
    }
    return position == tag.size();
}

bool ReadProductVersion(UpdateVersion& version) {
    wchar_t executable[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    const DWORD size = GetFileVersionInfoSizeW(executable, nullptr);
    if (!size) return false;
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(executable, 0, size, data.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &infoSize) ||
        infoSize < sizeof(*info) || info->dwSignature != 0xFEEF04BD) return false;
    version = { HIWORD(info->dwProductVersionMS), LOWORD(info->dwProductVersionMS),
        HIWORD(info->dwProductVersionLS) };
    return true;
}

std::wstring RequestLatestUpdateTag(const std::wstring& path) {
    using HttpHandle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;
    HttpHandle session(WinHttpOpen(L"SkeaClock", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0), WinHttpCloseHandle);
    if (!session || !WinHttpSetTimeouts(session.get(), 5000, 5000, 5000, 5000)) return {};
    HttpHandle connection(WinHttpConnect(session.get(), L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0),
        WinHttpCloseHandle);
    if (!connection) return {};
    HttpHandle request(WinHttpOpenRequest(connection.get(), L"HEAD", L"/junhwan123/SkeaClock/releases/latest",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE), WinHttpCloseHandle);
    if (!request) return {};
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled))) return {};

    // Persist the attempt before waiting for a response, including failed requests.
    const std::wstring checkedAt = std::to_wstring(UpdateCheckTime());
    if (!WritePrivateProfileStringW(L"updates", L"lastCheck", checkedAt.c_str(), path.c_str())) return {};
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.get(), nullptr)) return {};
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status < 300 || status >= 400)
        return {};
    wchar_t location[2048] = {};
    size = sizeof(location);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
        location, &size, WINHTTP_NO_HEADER_INDEX)) return {};
    const std::wstring url = location;
    const std::wstring prefix = std::wstring(GithubProjectUrl) + L"/releases/tag/";
    if (url.compare(0, prefix.size(), prefix) != 0) return {};
    return url.substr(prefix.size());
}
