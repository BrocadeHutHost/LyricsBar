#include "folderpick.h"
#include "draw.h"

static const int PK_W = 600, PK_H = 520;
static const wchar_t* CLS_PICK = L"LyricsBarPick";

static const COLORREF PBG = RGB(23, 24, 28);
static const COLORREF PACCENT = RGB(132, 214, 201);
static const COLORREF PTEXT = RGB(242, 245, 247);
static const COLORREF PDIM = RGB(152, 160, 170);

static const int PK_LIST_Y = 148, PK_ROW_Y = 152, PK_ROW_H = 36, PK_ROWS = 8;

struct PickState
{
    std::wstring cwd;
    std::vector<std::wstring> names;
    std::vector<std::wstring> paths;
    int scroll = 0;
    int hotRow = -1;
    int hotBtn = 0;
    int hotDrive = -1;
    std::wstring result;
    bool done = false;
    bool tracking = false;
};

static PickState s_pk;
static HWND s_pkHwnd = nullptr;
static bool s_pkOpen = false;
static bool s_pkClass = false;

enum { PB_CLOSE = 1, PB_UP, PB_DRIVES, PB_CANCEL, PB_OK };

static void LoadDir(const std::wstring& dir)
{
    s_pk.cwd = dir;
    s_pk.names.clear();
    s_pk.paths.clear();
    s_pk.scroll = 0;
    s_pk.hotRow = -1;

    if (dir.empty())
    {
        wchar_t buf[512] = {};
        if (GetLogicalDriveStringsW(511, buf))
        {
            for (wchar_t* p = buf; *p; p += wcslen(p) + 1)
            {
                UINT t = GetDriveTypeW(p);
                if (t == DRIVE_FIXED || t == DRIVE_REMOVABLE || t == DRIVE_REMOTE || t == DRIVE_RAMDISK)
                {
                    std::wstring drive(p);
                    s_pk.names.push_back(drive);
                    s_pk.paths.push_back(drive);
                }
            }
        }
        return;
    }

    std::wstring pattern = dir;
    if (!pattern.empty() && pattern.back() != L'\\') pattern += L'\\';
    pattern += L'*';
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
            s_pk.names.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(s_pk.names.begin(), s_pk.names.end(),
        [](const std::wstring& a, const std::wstring& b) { return Lower(a) < Lower(b); });
    std::wstring prefix = pattern.substr(0, pattern.size() - 1);
    for (auto& n : s_pk.names) s_pk.paths.push_back(prefix + n);
}

static void GoUp()
{
    if (s_pk.cwd.empty()) return;
    std::wstring c = s_pk.cwd;
    while (c.size() > 3 && (c.back() == L'\\' || c.back() == L'/')) c.pop_back();
    if (c.size() <= 3) { LoadDir(L""); return; }
    size_t p = c.find_last_of(L'\\');
    if (p == 2) LoadDir(c.substr(0, 3));
    else if (p != std::wstring::npos) LoadDir(c.substr(0, p));
    else LoadDir(L"");
}

static int RowAt(int x, int y)
{
    if (x < 24 || x > 576 || y < PK_ROW_Y || y >= PK_ROW_Y + PK_ROWS * PK_ROW_H) return -1;
    int idx = s_pk.scroll + (y - PK_ROW_Y) / PK_ROW_H;
    return idx < (int)s_pk.names.size() ? idx : -1;
}

static void Finish(bool ok)
{
    if (ok && !s_pk.cwd.empty()) s_pk.result = s_pk.cwd;
    else s_pk.result.clear();
    s_pk.done = true;
    if (s_pkHwnd) PostMessageW(s_pkHwnd, WM_CLOSE, 0, 0);
}

static void DrawPill(HDC hdc, int x, int y, int w, int h, bool hot, bool primary, const wchar_t* label, int kind = -1)
{
    if (primary)
        BlendRoundRect(hdc, x * UI_SCALE, y * UI_SCALE, w * UI_SCALE, h * UI_SCALE,
            PACCENT, hot ? 255 : 232, 11 * UI_SCALE);
    else
        BlendRoundRect(hdc, x * UI_SCALE, y * UI_SCALE, w * UI_SCALE, h * UI_SCALE,
            RGB(255, 255, 255), hot ? 34 : 18, 11 * UI_SCALE);
    HFONT f = MakeFontS(13, primary);
    SetTextColor(hdc, primary ? RGB(18, 20, 23) : (hot ? RGB(248, 250, 252) : RGB(216, 222, 228)));
    SelectObject(hdc, f);
    RECT r = R2(x, y, x + w, y + h);
    if (kind >= 0)
    {
        DrawGlyph(hdc, (x + 20) * UI_SCALE, (y + h / 2) * UI_SCALE, kind, 6 * UI_SCALE,
            primary ? RGB(18, 20, 23) : RGB(216, 222, 228));
        RECT tr = r; tr.left += 14 * UI_SCALE;
        DrawTextW(hdc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(f);
}

static void PaintPick(HDC hdc)
{
    RECT full = R2(0, 0, PK_W, PK_H);
    HBRUSH bg = CreateSolidBrush(PBG);
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
    SetBkMode(hdc, TRANSPARENT);

    HFONT ft = MakeFontS(18, true);
    SetTextColor(hdc, PTEXT);
    SelectObject(hdc, ft);
    RECT rt = R2(24, 14, 480, 50);
    DrawTextW(hdc, L"选择音乐文件夹", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(ft);
    DrawCloseCircle(hdc, 570 * UI_SCALE, 32 * UI_SCALE, 18 * UI_SCALE, s_pk.hotBtn == PB_CLOSE);

    BlendRoundRect(hdc, 20 * UI_SCALE, 60 * UI_SCALE, 560 * UI_SCALE, 36 * UI_SCALE,
        RGB(255, 255, 255), 12, 10 * UI_SCALE);
    HFONT fp = MakeFontS(12, false, L"Microsoft YaHei UI Light");
    SetTextColor(hdc, PDIM);
    SelectObject(hdc, fp);
    RECT rp = R2(32, 60, 568, 96);
    std::wstring path = s_pk.cwd.empty() ? L"此电脑" : s_pk.cwd;
    DrawTextW(hdc, path.c_str(), -1, &rp, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    DeleteObject(fp);

    DrawPill(hdc, 20, 104, 92, 34, s_pk.hotBtn == PB_UP, false, L"上一级", G_UP);
    DrawPill(hdc, 118, 104, 68, 34, s_pk.hotBtn == PB_DRIVES, false, L"磁盘");

    wchar_t buf[512] = {};
    int dx = 194;
    if (GetLogicalDriveStringsW(511, buf))
    {
        for (wchar_t* p = buf; *p; p += wcslen(p) + 1)
        {
            UINT t = GetDriveTypeW(p);
            if (!(t == DRIVE_FIXED || t == DRIVE_REMOVABLE || t == DRIVE_REMOTE || t == DRIVE_RAMDISK)) continue;
            wchar_t label[8] = { p[0], L':', 0 };
            if (dx + 50 > 580) break;
            DrawPill(hdc, dx, 104, 50, 34, s_pk.hotDrive == dx, false, label);
            dx += 56;
        }
    }

    BlendRoundRect(hdc, 20 * UI_SCALE, PK_LIST_Y * UI_SCALE, 560 * UI_SCALE, 314 * UI_SCALE,
        RGB(28, 30, 36), 235, 16 * UI_SCALE);
    HFONT fi = MakeFontS(14, false);
    SelectObject(hdc, fi);
    for (int i = 0; i < PK_ROWS; ++i)
    {
        int idx = s_pk.scroll + i;
        if (idx >= (int)s_pk.names.size()) break;
        int y = PK_ROW_Y + i * PK_ROW_H;
        bool hot = (idx == s_pk.hotRow);
        if (hot)
            BlendRoundRect(hdc, 24 * UI_SCALE, (y - 2) * UI_SCALE, 552 * UI_SCALE, 34 * UI_SCALE,
                RGB(255, 255, 255), 18, 9 * UI_SCALE);
        DrawGlyph(hdc, 44 * UI_SCALE, (y + 16) * UI_SCALE, G_FOLDER, 7 * UI_SCALE,
            s_pk.cwd.empty() ? RGB(198, 206, 214) : RGB(132, 214, 201));
        SetTextColor(hdc, hot ? PTEXT : RGB(216, 222, 228));
        RECT rr = R2(64, y, 560, y + PK_ROW_H);
        DrawTextW(hdc, s_pk.names[idx].c_str(), -1, &rr,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    DeleteObject(fi);

    if ((int)s_pk.names.size() > PK_ROWS)
    {
        int trackH = PK_ROWS * PK_ROW_H;
        int thumbH = std::max(28, trackH * PK_ROWS / (int)s_pk.names.size());
        int maxScroll = (int)s_pk.names.size() - PK_ROWS;
        int off = (trackH - thumbH) * s_pk.scroll / std::max(1, maxScroll);
        BlendRoundRect(hdc, 574 * UI_SCALE, (PK_ROW_Y + off) * UI_SCALE,
            4 * UI_SCALE, thumbH * UI_SCALE, RGB(255, 255, 255), 70, 4 * UI_SCALE);
    }

    DrawPill(hdc, 322, 472, 98, 38, s_pk.hotBtn == PB_CANCEL, false, L"取消");
    DrawPill(hdc, 430, 472, 150, 38, s_pk.hotBtn == PB_OK, true, L"选择此文件夹");
}

static LRESULT CALLBACK PickProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        SetWindowRgn(h, CreateRoundRectRgn(0, 0, PK_W, PK_H, 40, 40), TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        PaintBuffer buf = BeginBuffer(h, PK_W, PK_H);
        PaintPick(buf.dc);
        EndBuffer(h, buf, hdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(h, &pt);
        if (pt.y < 56 && pt.x < 540) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (!s_pk.tracking)
        {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            s_pk.tracking = true;
        }
        int btn = 0;
        if ((x - 570) * (x - 570) + (y - 32) * (y - 32) <= 18 * 18) btn = PB_CLOSE;
        else if (x >= 20 && x <= 112 && y >= 104 && y <= 138) btn = PB_UP;
        else if (x >= 118 && x <= 186 && y >= 104 && y <= 138) btn = PB_DRIVES;
        else if (x >= 322 && x <= 420 && y >= 472 && y <= 510) btn = PB_CANCEL;
        else if (x >= 430 && x <= 580 && y >= 472 && y <= 510) btn = PB_OK;

        int drive = -1;
        wchar_t buf[512] = {};
        if (GetLogicalDriveStringsW(511, buf))
        {
            int dx = 194;
            for (wchar_t* p = buf; *p; p += wcslen(p) + 1)
            {
                UINT t = GetDriveTypeW(p);
                if (!(t == DRIVE_FIXED || t == DRIVE_REMOVABLE || t == DRIVE_REMOTE || t == DRIVE_RAMDISK)) continue;
                if (dx + 50 > 580) break;
                if (x >= dx && x <= dx + 50 && y >= 104 && y <= 138) { drive = dx; break; }
                dx += 56;
            }
        }
        int row = RowAt(x, y);
        if (btn != s_pk.hotBtn || row != s_pk.hotRow || drive != s_pk.hotDrive)
        {
            s_pk.hotBtn = btn; s_pk.hotRow = row; s_pk.hotDrive = drive;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        s_pk.tracking = false;
        s_pk.hotBtn = 0; s_pk.hotRow = -1; s_pk.hotDrive = -1;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if ((x - 570) * (x - 570) + (y - 32) * (y - 32) <= 18 * 18) { Finish(false); return 0; }
        if (x >= 20 && x <= 112 && y >= 104 && y <= 138) { GoUp(); InvalidateRect(h, nullptr, FALSE); return 0; }
        if (x >= 118 && x <= 186 && y >= 104 && y <= 138) { LoadDir(L""); InvalidateRect(h, nullptr, FALSE); return 0; }
        if (x >= 322 && x <= 420 && y >= 472 && y <= 510) { Finish(false); return 0; }
        if (x >= 430 && x <= 580 && y >= 472 && y <= 510) { Finish(true); return 0; }

        wchar_t buf[512] = {};
        if (GetLogicalDriveStringsW(511, buf))
        {
            int dx = 194;
            for (wchar_t* p = buf; *p; p += wcslen(p) + 1)
            {
                UINT t = GetDriveTypeW(p);
                if (!(t == DRIVE_FIXED || t == DRIVE_REMOVABLE || t == DRIVE_REMOTE || t == DRIVE_RAMDISK)) continue;
                if (dx + 50 > 580) break;
                if (x >= dx && x <= dx + 50 && y >= 104 && y <= 138) { LoadDir(p); InvalidateRect(h, nullptr, FALSE); return 0; }
                dx += 56;
            }
        }

        int row = RowAt(x, y);
        if (row >= 0 && row < (int)s_pk.paths.size())
        {
            LoadDir(s_pk.paths[row]);
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int maxScroll = std::max(0, (int)s_pk.names.size() - PK_ROWS);
        s_pk.scroll += (delta > 0 ? -3 : 3);
        if (s_pk.scroll < 0) s_pk.scroll = 0;
        if (s_pk.scroll > maxScroll) s_pk.scroll = maxScroll;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { Finish(false); return 0; }
        if (wp == VK_RETURN) { Finish(true); return 0; }
        if (wp == VK_BACK) { GoUp(); InvalidateRect(h, nullptr, FALSE); return 0; }
        return 0;
    case WM_CLOSE:
        if (!s_pk.done) s_pk.done = true;
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

std::wstring PickFolder(HWND owner)
{
    if (s_pkOpen) return L"";
    if (!s_pkClass)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpfnWndProc = PickProc;
        wc.lpszClassName = CLS_PICK;
        if (!RegisterClassExW(&wc)) return L"";
        s_pkClass = true;
    }

    s_pk.result.clear();
    s_pk.done = false;
    s_pk.hotBtn = 0;
    s_pk.hotRow = -1;
    s_pk.hotDrive = -1;
    LoadDir(FileExists(MusicRoot()) ? MusicRoot() : L"");

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - PK_W) / 2, y = (sh - PK_H) / 2;
    RECT orc;
    if (owner && GetWindowRect(owner, &orc))
    {
        x = orc.left + ((orc.right - orc.left) - PK_W) / 2;
        y = orc.top + ((orc.bottom - orc.top) - PK_H) / 2;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + PK_W > sw) x = sw - PK_W;
    if (y + PK_H > sh) y = sh - PK_H;

    s_pkHwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, CLS_PICK, L"选择音乐文件夹",
        WS_POPUP | WS_VISIBLE, x, y, PK_W, PK_H, owner, nullptr, g.hInst, nullptr);
    if (!s_pkHwnd) return L"";

    s_pkOpen = true;
    if (owner) EnableWindow(owner, FALSE);
    SetForegroundWindow(s_pkHwnd);

    MSG msg;
    while (!s_pk.done && GetMessageW(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_QUIT) { PostQuitMessage((int)msg.wParam); break; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (IsWindow(s_pkHwnd)) DestroyWindow(s_pkHwnd);
    s_pkHwnd = nullptr;
    s_pkOpen = false;
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    return s_pk.result;
}
