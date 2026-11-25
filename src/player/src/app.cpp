#include "app.h"
#include "audio.h"
#include "lyrics.h"
#include "playlist.h"
#include "ui.h"
#include "cover.h"

static DWORD s_lastCheckTick = 0;
static DWORD s_lastResumeSave = 0;

static void SaveResume()
{
    if (g.currentFile.empty()) return;
    WritePrivateProfileStringW(L"Player", L"LastFile", g.currentFile.c_str(), IniPath().c_str());
    WritePrivateProfileStringW(L"Player", L"LastPos", std::to_wstring(g.posMs).c_str(), IniPath().c_str());
}

void App_OnTrackChanged()
{
    Lyrics_LoadForCurrent();
    Cover_LoadForCurrent();
    UI_UpdateTrayTip();
    UI_Refresh();
    SaveResume();
    LogLine((L"Track: " + g.title + L" - " + g.artist).c_str());
}

void App_RequestAdvance(bool manual)
{
    Audio_Advance(manual);
}

void App_RefreshUI()
{
    Audio_GetPosition();
    if (g.playing && !g.paused) g.discAngle += 0.06;

    Lyrics_Poll();

    if (Audio_IsEos())
    {
        Audio_AckEos();
        App_RequestAdvance(false);
    }

    DWORD now = GetTickCount();
    if (now - s_lastCheckTick >= 2000)
    {
        s_lastCheckTick = now;

        if (g.playing && !g.paused && g.durationMs > 0)
        {
            if (g.posMs <= g.lastPosMs + 200)
            {
                if (++g.stuckCount >= 3)
                {
                    ULONGLONG saved = g.posMs;
                    std::wstring file = g.currentFile;
                    bool wasPaused = g.paused;
                    LogLine(L"Self-check: stalled, reopening track");
                    if (Audio_Open(file))
                    {
                        Audio_Seek(saved);
                        g.paused = wasPaused;
                        g.playing = true;
                    }
                    g.stuckCount = 0;
                }
            }
            else g.stuckCount = 0;
            g.lastPosMs = g.posMs;
        }

        if (!g.currentFile.empty() && !FileExists(g.currentFile) && !g.playlist.empty())
        {
            LogLine(L"Self-check: current file missing, advancing");
            App_RequestAdvance(true);
        }

        if (g.playlist.empty())
        {
            Playlist_Scan();
            if (!g.playlist.empty()) Audio_PlayIndex(0);
        }

        if (!g.fetchingLyric && g.lyrics.empty() && !g.title.empty() && g.playing)
            Lyrics_LoadForCurrent();

        Cover_Poll();

        if (g.playing && now - s_lastResumeSave > 8000)
        {
            s_lastResumeSave = now;
            SaveResume();
        }

        TrimWorkingSet();
    }
    UI_Refresh();
}

void App_TogglePlay() { Audio_Toggle(); }

void App_Rescan()
{
    std::wstring current = g.currentFile;
    Playlist_Scan();
    auto it = std::find(g.playlist.begin(), g.playlist.end(), current);
    if (it != g.playlist.end())
    {
        g.track = (int)(it - g.playlist.begin());
        UI_Refresh();
        return;
    }
    if (g.playlist.empty())
    {
        Audio_Stop();
        g.track = -1;
        g.currentFile.clear();
        UI_Refresh();
        return;
    }
    Audio_PlayIndex(0);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmd, int)
{
    g.hInst = hInst;

    HWND existing = FindWindowW(CLS_MAIN, APP_NAME);
    if (existing)
    {
        PostMessageW(existing, WM_APP_SHOW, 0, 0);
        return 0;
    }

    InitializeCriticalSection(&g.lyricCs);

    Audio_Init();
    UI_Init(hInst);
    UI_CreateWindows();
    Playlist_Init();

    std::wstring arg = cmd ? cmd : L"";
    if (arg.size() >= 2 && arg.front() == L'"' && arg.back() == L'"') arg = arg.substr(1, arg.size() - 2);
    if (!arg.empty() && FileExists(arg))
    {
        g.playlist.push_back(arg);
        Audio_PlayIndex((int)g.playlist.size() - 1);
    }
    else if (!g.playlist.empty())
    {

        wchar_t lastFile[2048] = {};
        GetPrivateProfileStringW(L"Player", L"LastFile", L"", lastFile, 2048, IniPath().c_str());
        ULONGLONG resume = (ULONGLONG)GetPrivateProfileIntW(L"Player", L"LastPos", 0, IniPath().c_str());
        auto it = std::find(g.playlist.begin(), g.playlist.end(), std::wstring(lastFile));
        int idx = (it == g.playlist.end()) ? 0 : (int)(it - g.playlist.begin());
        Audio_PlayIndex(idx);
        if (resume > 3000 && g.durationMs > resume + 5000)
        {
            Audio_Seek(resume);
            LogLine((L"Resume position: " + std::to_wstring(resume) + L" ms").c_str());
        }
        if (it != g.playlist.end())
            LogLine((L"Resume track: " + g.title).c_str());
    }

    s_lastCheckTick = GetTickCount();
    UI_RunMessageLoop();
    SaveResume();

    Audio_Shutdown();
    Lyrics_Shutdown();
    Cover_Shutdown();
    UI_Shutdown();
    DeleteCriticalSection(&g.lyricCs);
    return 0;
}
