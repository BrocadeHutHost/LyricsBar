#include "common.h"

AppState g;

std::wstring Widen(const std::string& s, UINT cp)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(cp, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    if (n > 0) MultiByteToWideChar(cp, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

std::string Narrow(const std::wstring& s, UINT cp)
{
    if (s.empty()) return "";
    int n = WideCharToMultiByte(cp, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    if (n > 0) WideCharToMultiByte(cp, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring Lower(std::wstring s)
{
    for (auto& c : s) c = towlower(c);
    return s;
}

std::wstring ExeDir()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t i = p.find_last_of(L"\\");
    return i == std::wstring::npos ? L"." : p.substr(0, i);
}

std::wstring ParentDir(const std::wstring& p)
{
    size_t i = p.find_last_of(L"\\");
    return i == std::wstring::npos ? L"." : p.substr(0, i);
}

std::wstring MusicRoot() { return ParentDir(ParentDir(ExeDir())); }

std::wstring LyricsDir()
{
    std::wstring d = ParentDir(ExeDir()) + L"\\Lyrics";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

std::wstring CoversDir()
{
    std::wstring d = ParentDir(ExeDir()) + L"\\Covers";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

std::wstring IniPath() { return ExeDir() + L"\\" + INI_NAME; }

const wchar_t* FileNamePtr(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return path.c_str() + (p == std::wstring::npos ? 0 : p + 1);
}

std::wstring BaseNameNoExt(const std::wstring& path)
{
    std::wstring s(FileNamePtr(path));
    size_t dot = s.find_last_of(L'.');
    if (dot != std::wstring::npos) s.resize(dot);
    return s;
}

std::wstring DirOf(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? L"." : path.substr(0, p);
}

bool FileExists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void ParseSongName(const std::wstring& fileName, std::wstring& title, std::wstring& artist)
{
    std::wstring s = fileName;
    size_t i = 0;
    while (i < s.size() && (iswdigit(s[i]) || s[i] == L' ')) i++;
    if (i > 0 && i < s.size() && (s[i] == L'.' || s[i] == L' ' || s[i] == L'_' || s[i] == L'-'))
    {
        size_t j = i;
        while (j < s.size() && (s[j] == L'.' || s[j] == L' ' || s[j] == L'_' || s[j] == L'-')) j++;
        s = s.substr(j);
    }
    size_t pos = s.find(L" - ");
    if (pos == std::wstring::npos) pos = s.find(L" -");
    if (pos == std::wstring::npos) pos = s.find(L"- ");
    if (pos != std::wstring::npos && pos > 0 && pos + 1 < s.size())
    {
        title = s.substr(0, pos);
        artist = s.substr(pos + (s[pos + 1] == L' ' ? 3 : (s[pos] == L' ' ? 2 : 1)));
        if (artist.size() < 2) { title = s; artist.clear(); }
    }
    else { title = s; artist.clear(); }
    auto trim = [](std::wstring& x) {
        size_t a = x.find_first_not_of(L" \t"), b = x.find_last_not_of(L" \t");
        x = (a == std::wstring::npos) ? L"" : x.substr(a, b - a + 1);
    };
    trim(title); trim(artist);
}

std::wstring SanitizeFileName(std::wstring s)
{
    const wchar_t* bad = L"\\/:*?\"<>|";
    for (auto& c : s) if (wcschr(bad, c)) c = L'_';
    if (s.size() > 120) s.resize(120);
    return s;
}

std::wstring FormatTime(ULONGLONG ms)
{
    int sec = (int)(ms / 1000);
    wchar_t buf[32];
    swprintf(buf, 32, L"%02d:%02d", sec / 60, sec % 60);
    return buf;
}

void LogLine(const wchar_t* line)
{
    std::wstring f = ExeDir() + L"\\LyricsBar.log";
    HANDLE h = CreateFileW(f.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t head[64];
    swprintf(head, 64, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    std::string out = Narrow(std::wstring(head) + line + L"\r\n");
    DWORD w = 0;
    WriteFile(h, out.c_str(), (DWORD)out.size(), &w, nullptr);
    CloseHandle(h);
}

void TrimWorkingSet()
{
    EmptyWorkingSet(GetCurrentProcess());
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    static DWORD lastHeapMin = 0;
    DWORD now = GetTickCount();
    if (now - lastHeapMin > 20000)
    {
        lastHeapMin = now;
        _heapmin();
    }
}

RECT R2(int l, int t, int r, int b)
{
    RECT rc = { l * UI_SCALE, t * UI_SCALE, r * UI_SCALE, b * UI_SCALE };
    return rc;
}

HFONT MakeFont(int height, bool bold, const wchar_t* face)
{
    return CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

HFONT MakeFontS(int height, bool bold, const wchar_t* face) { return MakeFont(height * UI_SCALE, bold, face); }

PaintBuffer BeginBuffer(HWND hwnd, int w, int h)
{
    PaintBuffer b; b.w = w; b.h = h;
    b.screen = GetDC(hwnd);
    b.dc = CreateCompatibleDC(b.screen);
    b.bmp = CreateCompatibleBitmap(b.screen, w * UI_SCALE, h * UI_SCALE);
    b.old = (HBITMAP)SelectObject(b.dc, b.bmp);
    return b;
}

void EndBuffer(HWND hwnd, const PaintBuffer& b, HDC dest)
{
    SetStretchBltMode(dest, HALFTONE);
    SetBrushOrgEx(dest, 0, 0, nullptr);
    StretchBlt(dest, 0, 0, b.w, b.h, b.dc, 0, 0, b.w * UI_SCALE, b.h * UI_SCALE, SRCCOPY);
    SelectObject(b.dc, b.old);
    DeleteObject(b.bmp);
    DeleteObject(b.dc);
    ReleaseDC(hwnd, b.screen);
}

void DrawDisc(HDC hdc, int cx, int cy, int radius, double angle, bool playing)
{
    int r = radius;

    HBRUSH base = CreateSolidBrush(RGB(21, 22, 26));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, base);
    HPEN op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    HPEN gp = CreatePen(PS_SOLID, std::max(2, r / 22), RGB(38, 40, 47));
    SelectObject(hdc, gp);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (int k = 80; k <= 96; k += 8)
    {
        int rr = r * k / 100;
        Ellipse(hdc, cx - rr, cy - rr, cx + rr, cy + rr);
    }
    DeleteObject(gp);

    int lr = (int)(r * 0.36);
    HBRUSH label = CreateSolidBrush(RGB(132, 214, 201));
    SelectObject(hdc, label);
    Ellipse(hdc, cx - lr, cy - lr, cx + lr, cy + lr);
    DeleteObject(label);

    static const COLORREF dotCol[3] = { RGB(46, 96, 92), RGB(236, 250, 247), RGB(88, 160, 152) };
    for (int i = 0; i < 3; ++i)
    {
        double a = angle + i * 2.0944;
        int dx = cx + (int)(cos(a) * lr * 0.55);
        int dy = cy + (int)(sin(a) * lr * 0.55);
        int dr = std::max(2, lr / 5);
        HBRUSH db = CreateSolidBrush(dotCol[i]);
        SelectObject(hdc, db);
        Ellipse(hdc, dx - dr, dy - dr, dx + dr, dy + dr);
        DeleteObject(db);
    }

    int hr = std::max(2, r / 16);
    HBRUSH hub = CreateSolidBrush(RGB(24, 25, 29));
    SelectObject(hdc, hub);
    Ellipse(hdc, cx - hr, cy - hr, cx + hr, cy + hr);
    DeleteObject(hub);

    if (playing)
    {
        HPEN ring = CreatePen(PS_SOLID, std::max(2, r / 20), RGB(198, 236, 229));
        SelectObject(hdc, ring);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        int rr = r - std::max(3, r / 20);
        Arc(hdc, cx - rr, cy - rr, cx + rr, cy + rr, cx + rr, cy - rr / 2, cx + rr / 2, cy - rr);
        DeleteObject(ring);
    }

    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(base);
}

void DrawRoundPanel(HDC hdc, int x, int y, int w, int h, int radius, COLORREF fill)
{
    HRGN rgn = CreateRoundRectRgn(x, y, x + w, y + h, radius, radius);
    HBRUSH b = CreateSolidBrush(fill);
    FillRgn(hdc, rgn, b);
    DeleteObject(b);
    DeleteObject(rgn);
}
