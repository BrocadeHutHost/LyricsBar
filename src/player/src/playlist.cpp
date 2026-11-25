#include "playlist.h"
#include "app.h"
#include "audio.h"
#include "ui.h"

static bool IsExcluded(const std::wstring& dir)
{
    std::wstring low = Lower(dir);
    const wchar_t* bad[] = { L"musicplayer", L"整理后", L"b站下载", L"lyrics", L"playlists", L"tools", L"tmp", L"\\src", L"\\inc" };
    for (auto b : bad) if (low.find(b) != std::wstring::npos) return true;
    return false;
}

static bool IsAudio(const std::wstring& name)
{
    static const wchar_t* exts[] = { L".m4a", L".mp3", L".flac", L".wav", L".aac", L".ogg", L".wma" };
    std::wstring low = Lower(name);
    for (auto e : exts)
        if (low.size() >= wcslen(e) && low.compare(low.size() - wcslen(e), wcslen(e), e) == 0) return true;
    return false;
}

static void ScanDir(const std::wstring& dir, int depth, std::vector<std::wstring>& out)
{
    if (IsExcluded(dir)) return;
    WIN32_FIND_DATAW fd;
    std::wstring pattern = dir + L"\\*";
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            if (depth < 2) ScanDir(dir + L"\\" + fd.cFileName, depth + 1, out);
        }
        else if (IsAudio(fd.cFileName))
        {
            out.push_back(dir + L"\\" + fd.cFileName);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static std::wstring TrimRoot(std::wstring p)
{
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    if (p.size() == 2 && p[1] == L':') p += L'\\';
    return p;
}

static bool UnderRoot(const std::wstring& file, const std::wstring& root)
{
    std::wstring r = TrimRoot(root);
    if (r.empty() || file.size() <= r.size()) return false;
    if (_wcsnicmp(file.c_str(), r.c_str(), r.size()) != 0) return false;
    if (r.back() == L'\\') return true;
    return file[r.size()] == L'\\' || file[r.size()] == L'/';
}

void Playlist_BuildView()
{
    if (g.activeRoot < 0 || g.activeRoot >= (int)g.roots.size())
    {
        g.activeRoot = -1;
        g.playlist = g.allTracks;
        return;
    }
    std::vector<std::wstring> view;
    const std::wstring& root = g.roots[g.activeRoot];
    for (auto& f : g.allTracks)
        if (UnderRoot(f, root)) view.push_back(f);
    g.playlist.swap(view);
}

void Playlist_Init()
{
    wchar_t buf[4096] = {};
    GetPrivateProfileStringW(L"Folders", L"Roots", MusicRoot().c_str(), buf, 4096, IniPath().c_str());
    g.roots.clear();
    std::wstring raw(buf);
    size_t pos = 0;
    while (pos < raw.size())
    {
        size_t next = raw.find(L';', pos);
        std::wstring part = raw.substr(pos, next == std::wstring::npos ? std::wstring::npos : next - pos);
        if (!part.empty() && FileExists(part)) g.roots.push_back(TrimRoot(part));
        if (next == std::wstring::npos) break;
        pos = next + 1;
    }
    if (g.roots.empty()) g.roots.push_back(MusicRoot());

    g.activeRoot = GetPrivateProfileIntW(L"Folders", L"Active", -1, IniPath().c_str());
    if (g.activeRoot >= (int)g.roots.size()) g.activeRoot = -1;
    Playlist_Scan();
}

void Playlist_Save()
{
    std::wstring raw;
    for (auto& r : g.roots)
    {
        if (!raw.empty()) raw += L";";
        raw += r;
    }
    WritePrivateProfileStringW(L"Folders", L"Roots", raw.c_str(), IniPath().c_str());
    WritePrivateProfileStringW(L"Folders", L"Active", std::to_wstring(g.activeRoot).c_str(), IniPath().c_str());
}

void Playlist_Scan()
{
    std::vector<std::wstring> found;
    for (auto& root : g.roots) ScanDir(root, 0, found);
    std::sort(found.begin(), found.end(), [](const std::wstring& a, const std::wstring& b) { return Lower(a) < Lower(b); });
    found.erase(std::unique(found.begin(), found.end()), found.end());
    g.allTracks.swap(found);
    Playlist_BuildView();
    LogLine((L"Scan: " + std::to_wstring(g.allTracks.size()) + L" tracks in " +
        std::to_wstring(g.roots.size()) + L" roots, view " + std::to_wstring(g.playlist.size()) +
        L" (root " + std::to_wstring(g.activeRoot) + L")").c_str());
}

bool Playlist_SwitchTo(int idx, bool autoplay)
{
    if (idx >= (int)g.roots.size() || idx < -1) idx = -1;
    if (idx < 0 && g.roots.empty()) return false;
    g.activeRoot = idx;
    Playlist_Save();

    std::wstring current = g.currentFile;
    Playlist_BuildView();
    bool keep = false;
    if (idx < 0 && !current.empty())
    {
        auto it = std::find(g.playlist.begin(), g.playlist.end(), current);
        if (it != g.playlist.end())
        {
            g.track = (int)(it - g.playlist.begin());
            keep = true;
        }
    }

    LogLine((L"Switch source: " + std::to_wstring(idx) + L", " +
        std::to_wstring(g.playlist.size()) + L" tracks, track " +
        std::to_wstring(g.track) + (keep ? L" (keep)" : L"")).c_str());
    if (keep) return true;

    g.track = -1;
    if (g.playlist.empty())
    {
        Audio_Stop();
        g.currentFile.clear();
        g.title = L"（空文件夹）";
        g.artist.clear();
        g.durationMs = g.posMs = 0;
        UI_Refresh();
        return false;
    }
    if (autoplay) Audio_PlayIndex(0);
    else { g.track = 0; UI_Refresh(); }
    return true;
}

bool Playlist_AddRoot(const std::wstring& path)
{
    std::wstring p = TrimRoot(path);
    if (p.empty() || !FileExists(p)) { LogLine(L"Add root: invalid path"); return false; }
    auto it = std::find_if(g.roots.begin(), g.roots.end(),
        [&](const std::wstring& r) { return _wcsicmp(r.c_str(), p.c_str()) == 0; });
    if (it != g.roots.end())
    {
        LogLine(L"Add root: already exists, switch to it");
        return Playlist_SwitchTo((int)(it - g.roots.begin()), true);
    }
    g.roots.push_back(p);
    Playlist_Save();
    Playlist_Scan();
    return Playlist_SwitchTo((int)g.roots.size() - 1, true);
}

bool Playlist_RemoveRoot(int idx)
{
    if (idx < 0 || idx >= (int)g.roots.size() || g.roots.size() <= 1) return false;
    std::wstring gone = g.roots[idx];
    g.roots.erase(g.roots.begin() + idx);
    if (g.activeRoot == idx) g.activeRoot = -1;
    else if (g.activeRoot > idx) g.activeRoot--;
    Playlist_Save();
    Playlist_Scan();
    LogLine((L"Root removed: " + gone).c_str());
    return Playlist_SwitchTo(g.activeRoot, true);
}

void Playlist_ClearFolders()
{
    g.roots.clear();
    g.roots.push_back(MusicRoot());
    g.activeRoot = -1;
    Playlist_Save();
    Playlist_Scan();
    if (!g.playlist.empty()) Audio_PlayIndex(0);
}

size_t Playlist_Count() { return g.playlist.size(); }

std::wstring Playlist_RootName(int idx)
{
    if (idx < 0 || idx >= (int)g.roots.size()) return L"全部曲目";
    std::wstring name = FileNamePtr(g.roots[idx]);
    if (name.empty()) name = g.roots[idx];
    return name;
}

int Playlist_RootCount(int idx)
{
    if (idx < 0 || idx >= (int)g.roots.size()) return (int)g.allTracks.size();
    int n = 0;
    for (auto& f : g.allTracks) if (UnderRoot(f, g.roots[idx])) n++;
    return n;
}
