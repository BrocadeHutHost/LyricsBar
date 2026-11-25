#include "lyrics.h"
#include "app.h"

static std::wstring s_fetcherPath;
static bool s_fetcherTried = false;

std::wstring Lyric_EnsureFetcher()
{
    if (s_fetcherTried) return s_fetcherPath;
    s_fetcherTried = true;

    std::wstring path = LyricsDir() + L"\\_fetch.exe";

    DeleteFileW(path.c_str());
    if (FileExists(path)) { s_fetcherPath = path; return path; }

    HRSRC hr = FindResourceW(g.hInst, MAKEINTRESOURCE(101), MAKEINTRESOURCE(10));
    if (!hr) return L"";
    HGLOBAL hg = LoadResource(g.hInst, hr);
    DWORD size = SizeofResource(g.hInst, hr);
    void* data = hg ? LockResource(hg) : nullptr;
    if (!data || !size) return L"";
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return L"";
    DWORD wr = 0;
    WriteFile(f, data, size, &wr, nullptr);
    CloseHandle(f);
    if (wr != size) { DeleteFileW(path.c_str()); return L""; }
    s_fetcherPath = path;
    return path;
}

void Lyric_ReleaseFetcher()
{

    if (!s_fetcherPath.empty()) DeleteFileW(s_fetcherPath.c_str());
    s_fetcherPath.clear();
    s_fetcherTried = false;
}

bool Lyric_SpawnFetcher(const std::wstring& args)
{
    std::wstring exe = Lyric_EnsureFetcher();
    if (exe.empty()) { LogLine(L"Fetcher resource missing"); return false; }
    std::wstring line = L"\"" + exe + L"\" " + args;
    std::vector<wchar_t> buf(line.begin(), line.end());
    buf.push_back(0);
    STARTUPINFOW si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(exe.c_str(), &buf[0], nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi))
    {
        LogLine(L"Failed to start fetcher");
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static void ParseLrcText(const std::string& data)
{
    std::vector<LyricLine> lines;
    std::wstring text = Widen(data);
    if (!text.empty() && text[0] == 0xFEFF) text.erase(0, 1);
    int offsetMs = 0;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t nl = text.find(L'\n', start);
        std::wstring line = text.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        start = (nl == std::wstring::npos) ? text.size() + 1 : nl + 1;
        if (line.empty()) continue;
        if (line.compare(0, 7, L"[offset") == 0)
        {
            size_t colon = line.find(L':');
            if (colon != std::wstring::npos) offsetMs = _wtoi(line.c_str() + colon + 1);
            continue;
        }
        if (line[0] != L'[') continue;
        size_t close = line.find(L']');
        if (close == std::wstring::npos || close < 6) continue;
        int mm = 0, ss = 0, cs = 0;
        if (swscanf(line.substr(1, close - 1).c_str(), L"%d:%d.%d", &mm, &ss, &cs) < 2) continue;
        std::wstring content = line.substr(close + 1);
        if (content.empty()) continue;
        LyricLine ll;
        ll.t = (ULONGLONG)(mm * 60000 + ss * 1000 + cs * 10) + offsetMs;
        ll.text = content;
        if (!lines.empty() && lines.back().t == ll.t && lines.back().text != content)
            lines.back().text += L" / " + content;
        else lines.push_back(ll);
    }
    std::stable_sort(lines.begin(), lines.end(), [](const LyricLine& a, const LyricLine& b) { return a.t < b.t; });
    EnterCriticalSection(&g.lyricCs);
    g.lyrics.swap(lines);
    LeaveCriticalSection(&g.lyricCs);
}

static bool ParseLrcFile(const std::wstring& file)
{
    HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, nullptr);
    if (size == 0 || size > 400000) { CloseHandle(h); return false; }
    std::string data(size, '\0'); DWORD got = 0;
    ReadFile(h, &data[0], size, &got, nullptr);
    CloseHandle(h);
    data.resize(got);
    ParseLrcText(data);
    return !g.lyrics.empty();
}

int Lyrics_FindLine(ULONGLONG t)
{
    if (g.lyrics.empty()) return -1;
    int lo = 0, hi = (int)g.lyrics.size() - 1, ans = -1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (g.lyrics[mid].t <= t) { ans = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return ans;
}

void Lyrics_LoadForCurrent()
{
    EnterCriticalSection(&g.lyricCs);
    g.lyrics.clear();
    LeaveCriticalSection(&g.lyricCs);
    g.lastLyricsIndex = -2;
    g.pendingLyricFile.clear();

    std::wstring base = BaseNameNoExt(g.currentFile);
    std::wstring safe = SanitizeFileName(g.title) +
        (g.artist.empty() ? L"" : L"_" + SanitizeFileName(g.artist));
    std::wstring candidates[3] = {
        DirOf(g.currentFile) + L"\\" + base + L".lrc",
        LyricsDir() + L"\\" + base + L".lrc",
        LyricsDir() + L"\\" + safe + L".lrc"
    };
    for (auto& f : candidates)
    {
        if (FileExists(f) && ParseLrcFile(f)) return;
    }
    if (g.fetchingLyric || g.title.empty()) return;

    std::wstring query = g.title + (g.artist.empty() ? L"" : L" " + g.artist);
    g.pendingLyricFile = candidates[2];
    std::wstring args = L"\"" + query + L"\" \"" + g.pendingLyricFile + L"\"";
    if (Lyric_SpawnFetcher(args))
    {
        g.fetchingLyric = true;
        g.fetchTick = GetTickCount();
    }
    else
    {
        g.pendingLyricFile.clear();
    }
}

void Lyrics_Poll()
{
    if (!g.fetchingLyric) return;
    if (!g.pendingLyricFile.empty() && FileExists(g.pendingLyricFile))
    {
        ParseLrcFile(g.pendingLyricFile);
        g.fetchingLyric = false;
        g.pendingLyricFile.clear();
        App_RefreshUI();
    }
    else if (GetTickCount() - g.fetchTick > 15000)
    {
        g.fetchingLyric = false;
        g.pendingLyricFile.clear();
        LogLine(L"Lyric fetch timeout");
    }
}

void Lyrics_Shutdown()
{
    g.fetchingLyric = false;
    g.pendingLyricFile.clear();
    Lyric_ReleaseFetcher();
    EnterCriticalSection(&g.lyricCs);
    g.lyrics.clear();
    LeaveCriticalSection(&g.lyricCs);
}
