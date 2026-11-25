#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <cstdio>
#include <cctype>
#include <cstring>
#pragma comment(lib, "winhttp.lib")

static std::string Narrow(const std::wstring& s)
{
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

static std::wstring Widen(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

static std::string UrlEncode(const std::string& in)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : in)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

static std::string JsonString(const std::string& json, const char* key)
{
    std::string pat = std::string("\"") + key + "\":\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    std::string out;
    for (; p < json.size(); ++p)
    {
        char c = json[p];
        if (c == '"') break;
        if (c == '\\' && p + 1 < json.size())
        {
            char n = json[++p];
            switch (n)
            {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case '/': out += '/'; break;
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            default: out += n; break;
            }
            continue;
        }
        out += c;
    }
    return out;
}

static std::string HttpGet(const wchar_t* host, const std::string& path, DWORD maxBytes)
{
    std::string result;
    HINTERNET s = WinHttpOpen(L"LyricsFetch/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return "";
    WinHttpSetTimeouts(s, 4000, 4000, 6000, 9000);
    HINTERNET c = WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (c)
    {
        std::wstring wp = Widen(path);
        HINTERNET r = WinHttpOpenRequest(c, L"GET", wp.c_str(), nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (r)
        {
            WinHttpAddRequestHeaders(r, L"Referer: https://y.qq.com/\r\n", -1, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(r, nullptr, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(r, nullptr))
            {
                DWORD total = 0;
                for (;;)
                {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(r, &avail) || avail == 0) break;
                    if (avail > 65536) avail = 65536;
                    std::string ch(avail, '\0'); DWORD got = 0;
                    if (!WinHttpReadData(r, &ch[0], avail, &got) || got == 0) break;
                    ch.resize(got); total += got;
                    if (total > maxBytes) break;
                    result += ch;
                }
            }
            WinHttpCloseHandle(r);
        }
        WinHttpCloseHandle(c);
    }
    WinHttpCloseHandle(s);
    return result;
}

static std::string Search(const std::wstring& query)
{
    std::string enc = UrlEncode(Narrow(query));
    return HttpGet(L"c.y.qq.com",
        "/soso/fcgi-bin/client_search_cp?w=" + enc + "&p=1&n=5&format=json&new_json=1", 300000);
}

static bool WriteFileBytes(const std::wstring& path, const std::string& data)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &wr, nullptr);
    CloseHandle(h);
    return ok && wr == data.size();
}

static int FetchLyric(const std::wstring& query, const std::wstring& outFile)
{
    std::string js = Search(query);
    size_t pos = js.find("\"language\":");
    std::string mid = JsonString(pos == std::string::npos ? js : js.substr(pos), "mid");
    if (mid.empty()) return 2;
    std::string lj = HttpGet(L"c.y.qq.com",
        "/lyric/fcgi-bin/fcg_query_lyric_new.fcg?songmid=" + mid +
        "&g_tk=5381&format=json&inCharset=utf-8&outCharset=utf-8&nobase64=1", 300000);
    std::string lrc = JsonString(lj, "lyric");
    if (lrc.empty()) return 3;
    return WriteFileBytes(outFile, lrc) ? 0 : 4;
}

static int FetchCover(const std::wstring& query, const std::wstring& outFile)
{
    std::string js = Search(query);
    size_t alb = js.find("\"album\":");
    if (alb == std::string::npos) return 2;
    std::string albumMid = JsonString(js.substr(alb), "mid");
    if (albumMid.empty()) return 3;

    const char* sizes[] = { "T002R500x500M000", "T002R300x300M000" };
    for (auto sz : sizes)
    {
        std::string img = HttpGet(L"y.gtimg.cn",
            std::string("/music/photo_new/") + sz + albumMid + ".jpg", 900000);
        if (img.size() < 1200) continue;
        std::wstring tmp = outFile + L".part";
        if (!WriteFileBytes(tmp, img)) return 4;
        if (MoveFileExW(tmp.c_str(), outFile.c_str(), MOVEFILE_REPLACE_EXISTING)) return 0;
        DeleteFileW(tmp.c_str());
        return 5;
    }
    return 6;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc >= 4 && wcscmp(argv[1], L"--cover") == 0)
        return FetchCover(argv[2], argv[3]);
    if (argc < 3) return 1;
    return FetchLyric(argv[1], argv[2]);
}
