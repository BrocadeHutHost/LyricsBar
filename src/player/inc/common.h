#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <psapi.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <malloc.h>
#include <algorithm>
#include <string>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")

static const wchar_t* APP_NAME = L"LyricsBar";
static const wchar_t* INI_NAME = L"LyricsBar.ini";
static const wchar_t* CLS_MAIN = L"LyricsBarMain";
static const wchar_t* CLS_BAR = L"LyricsBarFloat";
static const wchar_t* CLS_LIB = L"LyricsBarLibrary";
static const UINT WM_APP_NEXT = WM_APP + 2;
static const UINT WM_APP_LYRIC_READY = WM_APP + 3;
static const UINT WM_APP_RESCAN = WM_APP + 4;
static const UINT WM_APP_TRAY = WM_APP + 5;
static const UINT WM_APP_SHOW = WM_APP + 6;

static const int MAIN_W = 520, MAIN_H = 280;
static const int BAR_W = 460, BAR_H = 104;
static const int LIB_W = 620, LIB_H = 580;
static const int UI_SCALE = 2;

enum LoopMode { LOOP_LIST = 0, LOOP_SINGLE = 1, LOOP_SHUFFLE = 2, LOOP_SEQUENCE = 3 };

struct LyricLine { ULONGLONG t; std::wstring text; };

struct AppState
{
    HINSTANCE hInst = nullptr;
    HWND hwndMain = nullptr;
    HWND hwndBar = nullptr;
    HWND hwndLib = nullptr;

    std::vector<std::wstring> roots;
    std::vector<std::wstring> allTracks;
    std::vector<std::wstring> playlist;
    int activeRoot = -1;
    int track = -1;
    std::wstring currentFile, title, artist;

    bool playing = false, paused = false;
    ULONGLONG posMs = 0, durationMs = 0;
    int volume = 80;
    int alpha = 230;
    LoopMode loopMode = LOOP_LIST;

    IMFSourceReader* reader = nullptr;
    HWAVEOUT waveOut = nullptr;
    WAVEFORMATEX wfx = {};
    std::vector<WAVEHDR> headers;
    HANDLE audioThread = nullptr;
    CRITICAL_SECTION audioCs = {};
    volatile bool threadStop = false, eos = false, eosNotified = false;
    ULONGLONG seekBytes = 0, bytesPerSec = 1;

    std::vector<LyricLine> lyrics;
    CRITICAL_SECTION lyricCs = {};
    volatile bool fetchingLyric = false;
    std::wstring pendingLyricFile;
    DWORD fetchTick = 0;

    double discAngle = 0.0;
    bool hoverCloseMain = false, hoverCloseBar = false, hoverFolder = false;
    int mainHot = -1;
    bool draggingVolume = false;
    bool draggingSeek = false;
    int lastLyricsIndex = -2;

    bool coverReady = false, fetchingCover = false;
    std::wstring coverPath;
    DWORD coverTick = 0;

    float barHover = 0.f;
    bool barHoverOn = false;
    bool draggingBar = false, barMoved = false;
    int barPress = -1, barHot = -1;
    POINT dragCursor = {}, dragWindow = {};

    bool trayAdded = false;
    UINT wmTaskbarCreated = 0;

    DWORD lastPosTick = 0;
    ULONGLONG lastPosMs = 0;
    int stuckCount = 0;
    DWORD lastTrimTick = 0;
    int failedOpens = 0;
};

extern AppState g;

std::wstring Widen(const std::string& s, UINT cp = CP_UTF8);
std::string Narrow(const std::wstring& s, UINT cp = CP_UTF8);
std::wstring Lower(std::wstring s);
std::wstring ExeDir();
std::wstring ParentDir(const std::wstring& p);
std::wstring MusicRoot();
std::wstring LyricsDir();
std::wstring CoversDir();
std::wstring IniPath();
std::wstring BaseNameNoExt(const std::wstring& path);
std::wstring DirOf(const std::wstring& path);
const wchar_t* FileNamePtr(const std::wstring& path);
bool FileExists(const std::wstring& path);
void ParseSongName(const std::wstring& fileName, std::wstring& title, std::wstring& artist);
std::wstring SanitizeFileName(std::wstring s);
std::wstring FormatTime(ULONGLONG ms);
void LogLine(const wchar_t* line);
void TrimWorkingSet();

struct PaintBuffer { HDC dc = nullptr; HDC screen = nullptr; HBITMAP bmp = nullptr, old = nullptr; int w = 0, h = 0; };
RECT R2(int l, int t, int r, int b);
HFONT MakeFont(int height, bool bold = false, const wchar_t* face = L"Microsoft YaHei UI");
HFONT MakeFontS(int height, bool bold = false, const wchar_t* face = L"Microsoft YaHei UI");
PaintBuffer BeginBuffer(HWND hwnd, int w, int h);
void EndBuffer(HWND hwnd, const PaintBuffer& b, HDC dest);
void DrawDisc(HDC hdc, int cx, int cy, int radius, double angle, bool playing);
void DrawRoundPanel(HDC hdc, int x, int y, int w, int h, int radius, COLORREF fill);
