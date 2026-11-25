#include "ui.h"
#include "app.h"
#include "audio.h"
#include "lyrics.h"
#include "playlist.h"
#include "cover.h"
#include "tray.h"
#include "draw.h"
#include "library.h"
#include "folderpick.h"

static const COLORREF BG = RGB(23, 24, 28);
static const COLORREF PANEL = RGB(34, 36, 42);
static const COLORREF ACCENT = RGB(132, 214, 201);
static const COLORREF TEXT = RGB(242, 245, 247);
static const COLORREF DIM = RGB(148, 156, 165);
static const COLORREF HAIR = RGB(45, 48, 55);

static const int M_DISC_CX = 54, M_DISC_CY = 52, M_DISC_R = 36;
static const int M_CLOSE_X = 468, M_CLOSE_Y = 9, M_CLOSE_R = 20;
static const int M_FOLDER_X = 374, M_FOLDER_Y = 12, M_FOLDER_W = 84, M_FOLDER_H = 32;
static const int M_LIST_X = 304, M_LIST_Y = 12, M_LIST_W = 64, M_LIST_H = 32;
static const int M_ROW_Y = 198, M_ROW_H = 48;
static const int M_PREV_X = 26, M_PLAY_X = 78, M_NEXT_X = 130;
static const int M_LOOP_X = 186, M_LOOP_W = 68;
static const int M_LYR_X = 262, M_LYR_W = 72;
static const int M_VOL_ICON_CX = 376, M_VOL_X = 390, M_VOL_W = 100, M_VOL_Y = 222;
static const int M_SEEK_X = 20, M_SEEK_W = 480, M_SEEK_Y = 168;

static const int BAR_DISC_CX = 44, BAR_DISC_CY = 52, BAR_DISC_R = 34;
static const int BAR_CLOSE_CX = 432, BAR_CLOSE_CY = 52, BAR_CLOSE_R = 16;
static const int BAR_PILL_X = 86, BAR_PILL_Y = 12, BAR_PILL_W = 314, BAR_PILL_H = 80;
static const int BAR_CY = 52;
static const int BAR_BTN_R = 17;
static const int BAR_PREV_CX = 128, BAR_PLAY_CX = 176, BAR_NEXT_CX = 224;
static const int BAR_VOL_CX = 254, BAR_VOL_X = 278, BAR_VOL_W = 100;

enum { MH_NONE = 0, MH_PREV, MH_PLAY, MH_NEXT, MH_LOOP, MH_LYRICS, MH_VOL, MH_SEEK, MH_FOLDER, MH_CLOSE, MH_LIST };
enum { BC_NONE = 0, BC_PREV = 1, BC_PLAY = 2, BC_NEXT = 3, BC_VOL = 4 };

static void DrawVolumeMain(HDC hdc)
{
    DrawGlyph(hdc, M_VOL_ICON_CX * UI_SCALE, M_VOL_Y * UI_SCALE, G_SPEAKER, 6 * UI_SCALE, RGB(190, 198, 205));
    int x = M_VOL_X, y = M_VOL_Y, w = M_VOL_W;
    int filled = w * g.volume / 100;
    RECT track = R2(x, y - 2, x + w, y + 2);
    HBRUSH b1 = CreateSolidBrush(RGB(60, 64, 72));
    FillRect(hdc, &track, b1);
    DeleteObject(b1);
    if (filled > 0)
    {
        RECT fill = R2(x, y - 2, x + filled, y + 2);
        HBRUSH b2 = CreateSolidBrush(ACCENT);
        FillRect(hdc, &fill, b2);
        DeleteObject(b2);
    }
    int kx = (x + filled) * UI_SCALE, ky = y * UI_SCALE;
    HBRUSH b3 = CreateSolidBrush(TEXT);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, b3);
    HPEN op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    int kr = 4 * UI_SCALE;
    Ellipse(hdc, kx - kr, ky - kr, kx + kr, ky + kr);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(b3);
}

static void DrawBarVolume(HDC hdc, int fade)
{
    DrawGlyph(hdc, BAR_VOL_CX * UI_SCALE, BAR_CY * UI_SCALE, G_SPEAKER, 7 * UI_SCALE,
        Mix(RGB(52, 56, 62), RGB(226, 232, 237), fade));
    int tx = BAR_VOL_X * UI_SCALE, ty = BAR_CY * UI_SCALE;
    int tw = BAR_VOL_W * UI_SCALE, th = 3 * UI_SCALE;
    int filled = tw * g.volume / 100;
    RECT tr = { tx, ty - th / 2, tx + tw, ty - th / 2 + th };
    HBRUSH bb = CreateSolidBrush(Mix(RGB(48, 52, 58), RGB(76, 82, 90), fade));
    FillRect(hdc, &tr, bb);
    DeleteObject(bb);
    if (filled > 0)
    {
        RECT fr = { tx, tr.top, tx + filled, tr.bottom };
        HBRUSH fb = CreateSolidBrush(Mix(RGB(58, 66, 68), ACCENT, fade));
        FillRect(hdc, &fr, fb);
        DeleteObject(fb);
    }
    BlendCircle(hdc, tx + filled, ty, 4 * UI_SCALE, RGB(255, 255, 255), fade);
}

static void PaintBarControls(HDC hdc)
{
    int a = (int)(g.barHover * 255.f);
    if (a < 4) return;

    BlendRoundRect(hdc, BAR_PILL_X * UI_SCALE, BAR_PILL_Y * UI_SCALE,
        BAR_PILL_W * UI_SCALE, BAR_PILL_H * UI_SCALE, RGB(10, 11, 14), a * 222 / 255, 44);

    const int cxs[3] = { BAR_PREV_CX, BAR_PLAY_CX, BAR_NEXT_CX };
    const int ids[3] = { BC_PREV, BC_PLAY, BC_NEXT };
    for (int i = 0; i < 3; ++i)
    {
        bool hot = (g.barHot == ids[i] || g.barPress == ids[i]);
        BlendCircle(hdc, cxs[i] * UI_SCALE, BAR_CY * UI_SCALE, BAR_BTN_R * UI_SCALE,
            RGB(255, 255, 255), (hot ? 44 : 13) * a / 255);
        int kind = G_PLAY;
        if (ids[i] == BC_PREV) kind = G_PREV;
        else if (ids[i] == BC_NEXT) kind = G_NEXT;
        else kind = g.paused ? G_PLAY : G_PAUSE;
        COLORREF gc = hot ? RGB(250, 252, 253) : Mix(RGB(52, 56, 62), RGB(230, 236, 241), a);
        DrawGlyph(hdc, cxs[i] * UI_SCALE, BAR_CY * UI_SCALE, kind, 7 * UI_SCALE, gc);
    }

    DrawBarVolume(hdc, a);
}

static void PaintBar(HDC hdc)
{
    RECT rc; GetClientRect(g.hwndBar, &rc);
    HBRUSH bg = CreateSolidBrush(RGB(26, 27, 31));
    RECT full = R2(0, 0, rc.right, rc.bottom);
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
    SetBkMode(hdc, TRANSPARENT);

    if (!Cover_DrawDisc(hdc, BAR_DISC_CX * UI_SCALE, BAR_DISC_CY * UI_SCALE,
        BAR_DISC_R * UI_SCALE, g.discAngle))
        DrawDisc(hdc, BAR_DISC_CX * UI_SCALE, BAR_DISC_CY * UI_SCALE,
            BAR_DISC_R * UI_SCALE, g.discAngle, g.playing && !g.paused);

    int textX = 86, textW = BAR_W - 86 - 58;
    int idx = Lyrics_FindLine(g.posMs);
    std::wstring line1, line2;
    EnterCriticalSection(&g.lyricCs);
    if (idx >= 0 && idx < (int)g.lyrics.size())
    {
        line1 = g.lyrics[idx].text;
        if (idx + 1 < (int)g.lyrics.size()) line2 = g.lyrics[idx + 1].text;
    }
    LeaveCriticalSection(&g.lyricCs);
    if (line1.empty() && line2.empty())
    {
        line1 = g.title.empty() ? L"LyricsBar" : g.title;
        line2 = g.artist.empty() ? L"正在准备播放…" : g.artist;
    }
    RECT r1 = R2(textX, 18, textX + textW, 54);
    RECT r2 = R2(textX, 54, textX + textW, 80);
    HFONT f1 = MakeFontS(line2.empty() ? 21 : 19, false);
    HFONT f2 = MakeFontS(12, false, L"Microsoft YaHei UI Light");

    int dim = (int)(g.barHover * 255);
    SetTextColor(hdc, Mix(TEXT, RGB(30, 31, 35), dim)); SelectObject(hdc, f1);
    DrawTextW(hdc, line1.c_str(), -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(hdc, Mix(DIM, RGB(30, 31, 35), dim)); SelectObject(hdc, f2);
    DrawTextW(hdc, line2.c_str(), -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(f1); DeleteObject(f2);

    DrawCloseCircle(hdc, BAR_CLOSE_CX * UI_SCALE, BAR_CLOSE_CY * UI_SCALE,
        BAR_CLOSE_R * UI_SCALE, g.hoverCloseBar);

    if (g.barHover > 0.02f) PaintBarControls(hdc);
}

static void PaintMain(HDC hdc)
{
    RECT rc; GetClientRect(g.hwndMain, &rc);
    HBRUSH bg = CreateSolidBrush(BG);
    RECT full = R2(0, 0, rc.right, rc.bottom);
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
    SetBkMode(hdc, TRANSPARENT);

    if (!Cover_DrawDisc(hdc, M_DISC_CX * UI_SCALE, M_DISC_CY * UI_SCALE,
        M_DISC_R * UI_SCALE, g.discAngle))
        DrawDisc(hdc, M_DISC_CX * UI_SCALE, M_DISC_CY * UI_SCALE,
            M_DISC_R * UI_SCALE, g.discAngle, g.playing && !g.paused);

    HFONT ft = MakeFontS(18, true);
    HFONT fa = MakeFontS(12, false, L"Microsoft YaHei UI Light");
    RECT rt = R2(102, 16, 292, 44);
    RECT ra = R2(102, 44, 292, 64);
    SetTextColor(hdc, TEXT); SelectObject(hdc, ft);
    DrawTextW(hdc, g.title.c_str(), -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(hdc, DIM); SelectObject(hdc, fa);
    DrawTextW(hdc, g.artist.c_str(), -1, &ra, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(ft); DeleteObject(fa);

    std::wstring status = Playlist_RootName(g.activeRoot) + L" · " +
        std::to_wstring(g.playlist.size()) + L" 首 · 循环：" + Audio_LoopModeName();
    HFONT fs = MakeFontS(10, false, L"Microsoft YaHei UI Light");
    SetTextColor(hdc, RGB(112, 120, 130)); SelectObject(hdc, fs);
    RECT rs = R2(102, 64, 292, 84);
    DrawTextW(hdc, status.c_str(), -1, &rs, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(fs);

    DrawTextButton(hdc, M_LIST_X, M_LIST_Y, M_LIST_W, M_LIST_H,
        g.mainHot == MH_LIST, L"曲目库");
    DrawTextButton(hdc, M_FOLDER_X, M_FOLDER_Y, M_FOLDER_W, M_FOLDER_H,
        g.mainHot == MH_FOLDER, L"文件夹");
    DrawCloseCircle(hdc, (M_CLOSE_X + M_CLOSE_R) * UI_SCALE, (M_CLOSE_Y + M_CLOSE_R) * UI_SCALE,
        M_CLOSE_R * UI_SCALE, g.hoverCloseMain);

    HPEN hair = CreatePen(PS_SOLID, 2, HAIR);
    HPEN oph = (HPEN)SelectObject(hdc, hair);
    MoveToEx(hdc, 24 * UI_SCALE, 96 * UI_SCALE, nullptr);
    LineTo(hdc, 496 * UI_SCALE, 96 * UI_SCALE);
    SelectObject(hdc, oph);
    DeleteObject(hair);

    int idx = Lyrics_FindLine(g.posMs);
    std::wstring l1, l2;
    EnterCriticalSection(&g.lyricCs);
    if (idx >= 0 && idx < (int)g.lyrics.size())
    {
        l1 = g.lyrics[idx].text;
        if (idx + 1 < (int)g.lyrics.size()) l2 = g.lyrics[idx + 1].text;
    }
    LeaveCriticalSection(&g.lyricCs);
    if (l1.empty()) l1 = g.lyrics.empty() ? L"正在获取歌词…" : L"♪";
    HFONT f1 = MakeFontS(17, false);
    HFONT f2 = MakeFontS(12, false, L"Microsoft YaHei UI Light");
    RECT r1 = R2(20, 104, 500, 140);
    RECT r2 = R2(20, 140, 500, 162);
    SetTextColor(hdc, TEXT); SelectObject(hdc, f1);
    DrawTextW(hdc, l1.c_str(), -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(hdc, DIM); SelectObject(hdc, f2);
    DrawTextW(hdc, l2.c_str(), -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(f1); DeleteObject(f2);

    RECT track = R2(M_SEEK_X, M_SEEK_Y, M_SEEK_X + M_SEEK_W, M_SEEK_Y + 4);
    HBRUSH bt = CreateSolidBrush(RGB(58, 62, 70));
    FillRect(hdc, &track, bt); DeleteObject(bt);
    int done = (int)(M_SEEK_W * (g.durationMs ? std::min(g.posMs, g.durationMs) * 1.0 / g.durationMs : 0));
    if (done > 0)
    {
        RECT rp = R2(M_SEEK_X, M_SEEK_Y, M_SEEK_X + done, M_SEEK_Y + 4);
        HBRUSH bp = CreateSolidBrush(ACCENT);
        FillRect(hdc, &rp, bp); DeleteObject(bp);
    }
    HFONT fm = MakeFontS(10, false, L"Consolas");
    SetTextColor(hdc, RGB(128, 136, 146)); SelectObject(hdc, fm);
    RECT tc = R2(M_SEEK_X, M_SEEK_Y + 8, M_SEEK_X + 70, M_SEEK_Y + 26);
    DrawTextW(hdc, FormatTime(g.posMs).c_str(), -1, &tc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT tt = R2(M_SEEK_X + M_SEEK_W - 70, M_SEEK_Y + 8, M_SEEK_X + M_SEEK_W, M_SEEK_Y + 26);
    DrawTextW(hdc, FormatTime(g.durationMs).c_str(), -1, &tt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(fm);

    DrawIconButton(hdc, M_PREV_X, M_ROW_Y + 2, 44, 44, g.mainHot == MH_PREV, G_PREV);
    DrawIconButton(hdc, M_PLAY_X, M_ROW_Y + 2, 44, 44, g.mainHot == MH_PLAY, g.paused ? G_PLAY : G_PAUSE);
    DrawIconButton(hdc, M_NEXT_X, M_ROW_Y + 2, 44, 44, g.mainHot == MH_NEXT, G_NEXT);
    DrawTextButton(hdc, M_LOOP_X, M_ROW_Y + 8, M_LOOP_W, 32, g.mainHot == MH_LOOP, Audio_LoopModeName());
    DrawTextButton(hdc, M_LYR_X, M_ROW_Y + 8, M_LYR_W, 32, g.mainHot == MH_LYRICS, L"歌词条");
    DrawVolumeMain(hdc);
}

static int MainControlAt(int x, int y)
{
    if (x >= M_CLOSE_X && x <= M_CLOSE_X + M_CLOSE_R * 2 &&
        y >= M_CLOSE_Y && y <= M_CLOSE_Y + M_CLOSE_R * 2) return MH_CLOSE;
    if (x >= M_LIST_X && x <= M_LIST_X + M_LIST_W &&
        y >= M_LIST_Y && y <= M_LIST_Y + M_LIST_H) return MH_LIST;
    if (x >= M_FOLDER_X && x <= M_FOLDER_X + M_FOLDER_W &&
        y >= M_FOLDER_Y && y <= M_FOLDER_Y + M_FOLDER_H) return MH_FOLDER;
    if (y >= M_ROW_Y && y <= M_ROW_Y + M_ROW_H)
    {
        if (x >= M_PREV_X && x <= M_PREV_X + 44) return MH_PREV;
        if (x >= M_PLAY_X && x <= M_PLAY_X + 44) return MH_PLAY;
        if (x >= M_NEXT_X && x <= M_NEXT_X + 44) return MH_NEXT;
        if (x >= M_LOOP_X && x <= M_LOOP_X + M_LOOP_W) return MH_LOOP;
        if (x >= M_LYR_X && x <= M_LYR_X + M_LYR_W) return MH_LYRICS;
        if (x >= M_VOL_X - 24 && x <= M_VOL_X + M_VOL_W + 6) return MH_VOL;
    }
    if (y >= M_SEEK_Y - 4 && y <= M_SEEK_Y + 14) return MH_SEEK;
    return MH_NONE;
}

static int BarControlAt(int x, int y)
{
    if (g.barHover < 0.35f) return BC_NONE;
    int dy = y - BAR_CY;
    int rr = (BAR_BTN_R + 7) * (BAR_BTN_R + 7);
    if ((x - BAR_PREV_CX) * (x - BAR_PREV_CX) + dy * dy <= rr) return BC_PREV;
    if ((x - BAR_PLAY_CX) * (x - BAR_PLAY_CX) + dy * dy <= rr) return BC_PLAY;
    if ((x - BAR_NEXT_CX) * (x - BAR_NEXT_CX) + dy * dy <= rr) return BC_NEXT;
    if (x >= BAR_VOL_CX - 12 && x <= BAR_VOL_X + BAR_VOL_W + 4 && std::abs(dy) <= 16) return BC_VOL;
    return BC_NONE;
}

static void BarDoControl(int id)
{
    if (id == BC_PREV) Audio_PlayIndex(g.track - 1);
    else if (id == BC_PLAY) App_TogglePlay();
    else if (id == BC_NEXT) App_RequestAdvance(true);
}

static void UI_Quit()
{
    Tray_Remove();
    PostQuitMessage(0);
}

static void ShowMainWindow()
{
    ShowWindow(g.hwndMain, SW_SHOW);
    SetWindowPos(g.hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(g.hwndMain);
}

void UI_UpdateTrayTip()
{
    if (!g.trayAdded) return;
    std::wstring tip = L"LyricsBar";
    if (!g.title.empty()) tip += L" · " + g.title;
    if (!g.artist.empty()) tip += L" - " + g.artist;
    if (tip.size() > 120) tip.resize(120);
    Tray_UpdateTip(tip.c_str());
}

static void HideMainToTray()
{
    if (!g.trayAdded)
    {
        UI_Quit();
        return;
    }
    ShowWindow(g.hwndMain, SW_HIDE);
    int shown = GetPrivateProfileIntW(L"Window", L"TrayHint", 0, IniPath().c_str());
    if (!shown)
    {
        Tray_Balloon(L"LyricsBar 仍在后台播放", L"单击托盘图标可以重新打开主界面，右键可退出。");
        WritePrivateProfileStringW(L"Window", L"TrayHint", L"1", IniPath().c_str());
    }
}

static void ShowTrayMenu(HWND owner)
{
    POINT pt; GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"打开主界面");
    AppendMenuW(m, MF_STRING, 7, L"曲目库");
    AppendMenuW(m, MF_STRING, 2, IsWindowVisible(g.hwndBar) ? L"隐藏歌词条" : L"显示歌词条");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 3, g.paused ? L"继续播放" : L"暂停");
    AppendMenuW(m, MF_STRING, 4, L"上一首");
    AppendMenuW(m, MF_STRING, 5, L"下一首");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 6, L"退出");
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
    PostMessageW(owner, WM_NULL, 0, 0);
    if (cmd == 1) ShowMainWindow();
    else if (cmd == 7) Library_Show();
    else if (cmd == 2) ShowWindow(g.hwndBar, IsWindowVisible(g.hwndBar) ? SW_HIDE : SW_SHOW);
    else if (cmd == 3) App_TogglePlay();
    else if (cmd == 4) Audio_PlayIndex(g.track - 1);
    else if (cmd == 5) App_RequestAdvance(true);
    else if (cmd == 6) UI_Quit();
}

static bool s_barFading = false;
static bool s_barTracking = false;

static void StartBarFade(HWND h)
{
    if (!s_barFading) { SetTimer(h, 2, 16, nullptr); s_barFading = true; }
}

static LRESULT CALLBACK BarProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        SetWindowRgn(h, CreateRoundRectRgn(0, 0, BAR_W, BAR_H, 40, 40), TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        PaintBuffer buf = BeginBuffer(h, BAR_W, BAR_H);
        PaintBar(buf.dc);
        EndBuffer(h, buf, hdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_MOUSELEAVE:
        s_barTracking = false;
        g.barHoverOn = false;
        g.barHot = BC_NONE;
        StartBarFade(h);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (wp == 2)
        {
            float target = g.barHoverOn ? 1.f : 0.f;
            if (g.barHover < target)
            {
                g.barHover += 0.18f;
                if (g.barHover > target) g.barHover = target;
            }
            else if (g.barHover > target)
            {
                g.barHover -= 0.26f;
                if (g.barHover < target) g.barHover = target;
            }
            InvalidateRect(h, nullptr, FALSE);
            if (g.barHover == target)
            {
                KillTimer(h, 2);
                s_barFading = false;
                if (target == 0.f) g.barHot = BC_NONE;
            }
        }
        return 0;
    case WM_MOUSEMOVE:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        if (!s_barTracking)
        {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            s_barTracking = true;
        }
        if (!g.barHoverOn)
        {
            g.barHoverOn = true;
            StartBarFade(h);
        }
        if (g.draggingBar)
        {
            POINT pt; GetCursorPos(&pt);
            int dx = pt.x - g.dragCursor.x, dy = pt.y - g.dragCursor.y;
            if (std::abs(dx) > 3 || std::abs(dy) > 3) g.barMoved = true;
            if (g.barMoved)
                SetWindowPos(h, nullptr, g.dragWindow.x + dx, g.dragWindow.y + dy,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (g.draggingVolume)
        {
            int v = (x - BAR_VOL_X) * 100 / BAR_VOL_W;
            Audio_SetVolume(v);
            UI_Refresh();
            return 0;
        }
        int hot = BarControlAt(x, y);
        bool close = ((x - BAR_CLOSE_CX) * (x - BAR_CLOSE_CX) +
            (y - BAR_CLOSE_CY) * (y - BAR_CLOSE_CY)) <= BAR_CLOSE_R * BAR_CLOSE_R;
        if (hot != g.barHot || close != g.hoverCloseBar)
        {
            g.barHot = hot;
            g.hoverCloseBar = close;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        if ((x - BAR_CLOSE_CX) * (x - BAR_CLOSE_CX) + (y - BAR_CLOSE_CY) * (y - BAR_CLOSE_CY) <= BAR_CLOSE_R * BAR_CLOSE_R)
        {
            ShowWindow(h, SW_HIDE);
            return 0;
        }
        int ctl = BarControlAt(x, y);
        if (ctl == BC_VOL)
        {
            g.draggingVolume = true;
            SetCapture(h);
            Audio_SetVolume((x - BAR_VOL_X) * 100 / BAR_VOL_W);
            UI_Refresh();
            return 0;
        }
        if (ctl != BC_NONE)
        {
            g.barPress = ctl;
            SetCapture(h);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        g.draggingBar = true;
        g.barMoved = false;
        GetCursorPos(&g.dragCursor);
        RECT wr; GetWindowRect(h, &wr);
        g.dragWindow.x = wr.left;
        g.dragWindow.y = wr.top;
        SetCapture(h);
        return 0;
    }
    case WM_LBUTTONUP:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        if (g.barPress != BC_NONE)
        {
            int id = g.barPress;
            g.barPress = BC_NONE;
            if (GetCapture() == h) ReleaseCapture();
            if (BarControlAt(x, y) == id) BarDoControl(id);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (g.draggingVolume)
        {
            g.draggingVolume = false;
            if (GetCapture() == h) ReleaseCapture();
            WritePrivateProfileStringW(L"Player", L"Volume", std::to_wstring(g.volume).c_str(), IniPath().c_str());
            return 0;
        }
        if (g.draggingBar)
        {
            g.draggingBar = false;
            if (GetCapture() == h) ReleaseCapture();
            if (!g.barMoved && x < BAR_DISC_CX + BAR_DISC_R && y > 12 && y < 92) App_TogglePlay();
            return 0;
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        g.draggingBar = false;
        g.draggingVolume = false;
        g.barPress = BC_NONE;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDBLCLK:
        ShowMainWindow();
        return 0;
    case WM_RBUTTONUP:
    {
        POINT pt; GetCursorPos(&pt);
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, 1, L"显示主界面");
        AppendMenuW(m, MF_STRING, 2, g.paused ? L"继续播放" : L"暂停");
        AppendMenuW(m, MF_STRING, 3, L"上一首");
        AppendMenuW(m, MF_STRING, 4, L"下一首");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 5, L"隐藏歌词条");
        AppendMenuW(m, MF_STRING, 6, L"退出");
        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
        DestroyMenu(m);
        PostMessageW(h, WM_NULL, 0, 0);
        if (cmd == 1) ShowMainWindow();
        else if (cmd == 2) App_TogglePlay();
        else if (cmd == 3) Audio_PlayIndex(g.track - 1);
        else if (cmd == 4) App_RequestAdvance(true);
        else if (cmd == 5) ShowWindow(h, SW_HIDE);
        else if (cmd == 6) UI_Quit();
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (GetKeyState(VK_SHIFT) & 0x8000)
        {
            g.alpha = std::max(80, std::min(255, g.alpha + (delta > 0 ? 15 : -15)));
            SetLayeredWindowAttributes(h, RGB(255, 0, 255), (BYTE)g.alpha, LWA_COLORKEY | LWA_ALPHA);
        }
        else Audio_SetVolume(g.volume + (delta > 0 ? 5 : -5));
        UI_Refresh();
        return 0;
    }
    case WM_APP_LYRIC_READY:
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static LRESULT CALLBACK MainProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (g.wmTaskbarCreated && msg == g.wmTaskbarCreated)
    {
        g.trayAdded = false;
        Tray_Add(h, L"LyricsBar");
        UI_UpdateTrayTip();
        return 0;
    }

    switch (msg)
    {
    case WM_CREATE:
        SetWindowRgn(h, CreateRoundRectRgn(0, 0, MAIN_W, MAIN_H, 40, 40), TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        PaintBuffer buf = BeginBuffer(h, MAIN_W, MAIN_H);
        PaintMain(buf.dc);
        EndBuffer(h, buf, hdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCHITTEST:
    {
        POINT pt = { LOWORD(lp), HIWORD(lp) };
        ScreenToClient(h, &pt);
        if (pt.y < 56 && pt.x > MAIN_W - 216) return HTCLIENT;
        if (pt.y < 96 && pt.x > 96) return HTCAPTION;
        return DefWindowProcW(h, msg, wp, lp);
    }
    case WM_MOUSEMOVE:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        int hot = MainControlAt(x, y);
        if (hot != g.mainHot)
        {
            g.mainHot = hot;
            g.hoverCloseMain = (hot == MH_CLOSE);
            g.hoverFolder = (hot == MH_FOLDER);
            InvalidateRect(h, nullptr, FALSE);
        }
        if (g.draggingVolume)
        {
            int v = (x - M_VOL_X) * 100 / M_VOL_W;
            Audio_SetVolume(v);
            UI_Refresh();
        }
        if (g.draggingSeek && g.durationMs > 0)
        {
            int px = std::max(0, std::min(M_SEEK_W, x - M_SEEK_X));
            Audio_Seek((ULONGLONG)(px * 1.0 / M_SEEK_W * g.durationMs));
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        int hot = MainControlAt(x, y);
        switch (hot)
        {
        case MH_CLOSE: HideMainToTray(); return 0;
        case MH_LIST:
            Library_Toggle();
            return 0;
        case MH_FOLDER:
            LogLine(L"Folder button clicked");
            {
                std::wstring picked = PickFolder(h);
                if (picked.empty()) LogLine(L"Folder picker: cancelled");
                else
                {
                    LogLine((L"Folder picked: " + picked).c_str());
                    Playlist_AddRoot(picked);
                    Library_RefreshData();
                    UI_Refresh();
                }
            }
            return 0;
        case MH_PREV: Audio_PlayIndex(g.track - 1); return 0;
        case MH_PLAY: App_TogglePlay(); return 0;
        case MH_NEXT: App_RequestAdvance(true); return 0;
        case MH_LOOP:
            g.loopMode = (LoopMode)((g.loopMode + 1) % 4);
            WritePrivateProfileStringW(L"Player", L"Loop", std::to_wstring((int)g.loopMode).c_str(), IniPath().c_str());
            UI_Refresh();
            return 0;
        case MH_LYRICS:
            ShowWindow(g.hwndBar, IsWindowVisible(g.hwndBar) ? SW_HIDE : SW_SHOW);
            return 0;
        case MH_VOL:
            g.draggingVolume = true;
            SetCapture(h);
            Audio_SetVolume((x - M_VOL_X) * 100 / M_VOL_W);
            UI_Refresh();
            return 0;
        case MH_SEEK:
            g.draggingSeek = true;
            SetCapture(h);
            if (g.durationMs > 0)
            {
                int px = std::max(0, std::min(M_SEEK_W, x - M_SEEK_X));
                Audio_Seek((ULONGLONG)(px * 1.0 / M_SEEK_W * g.durationMs));
            }
            return 0;
        }
        return 0;
    }
    case WM_LBUTTONUP:
        g.draggingVolume = false;
        g.draggingSeek = false;
        if (GetCapture() == h) ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        g.draggingVolume = false;
        g.draggingSeek = false;
        return 0;
    case WM_RBUTTONUP:
    {
        POINT pt; GetCursorPos(&pt);
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, 1, L"打开曲目库");
        AppendMenuW(m, MF_STRING, 2, L"收进托盘");
        AppendMenuW(m, MF_STRING, 3, IsWindowVisible(g.hwndBar) ? L"隐藏歌词条" : L"显示歌词条");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 4, L"切换播放文件夹…");
        AppendMenuW(m, MF_STRING, 5, L"恢复默认播放源");
        AppendMenuW(m, MF_STRING, 6, L"切换循环模式");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 7, L"退出");
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
        DestroyMenu(m);
        if (cmd == 1) Library_Show();
        else if (cmd == 2) HideMainToTray();
        else if (cmd == 3) ShowWindow(g.hwndBar, IsWindowVisible(g.hwndBar) ? SW_HIDE : SW_SHOW);
        else if (cmd == 4)
        {
            std::wstring picked = PickFolder(h);
            if (!picked.empty())
            {
                Playlist_AddRoot(picked);
                Library_RefreshData();
                UI_Refresh();
            }
        }
        else if (cmd == 5) { Playlist_ClearFolders(); Library_RefreshData(); App_Rescan(); }
        else if (cmd == 6) { g.loopMode = (LoopMode)((g.loopMode + 1) % 4); UI_Refresh(); }
        else if (cmd == 7) UI_Quit();
        return 0;
    }
    case WM_KEYDOWN:
    {
        if (wp == VK_ESCAPE) { HideMainToTray(); return 0; }
        if (wp == VK_UP) { Audio_SetVolume(g.volume + 5); UI_Refresh(); return 0; }
        if (wp == VK_DOWN) { Audio_SetVolume(g.volume - 5); UI_Refresh(); return 0; }
        if (wp == VK_SPACE) { App_TogglePlay(); return 0; }
        return DefWindowProcW(h, msg, wp, lp);
    }
    case WM_HOTKEY:
        if (wp == 1) { UI_Quit(); return 0; }
        return 0;
    case WM_APP_TRAY:
    {
        UINT ev = LOWORD(lp);
        if (ev == WM_LBUTTONUP)
        {
            if (IsWindowVisible(g.hwndMain) && GetForegroundWindow() == g.hwndMain)
                ShowWindow(g.hwndMain, SW_HIDE);
            else ShowMainWindow();
        }
        else if (ev == WM_LBUTTONDBLCLK) ShowMainWindow();
        else if (ev == WM_RBUTTONUP) ShowTrayMenu(h);
        return 0;
    }
    case WM_APP_NEXT:
        App_RequestAdvance(LOWORD(wp) != 0);
        return 0;
    case WM_APP_SHOW:
        ShowMainWindow();
        return 0;
    case WM_APP_RESCAN:
        App_Rescan();
        return 0;
    case WM_TIMER:
        App_RefreshUI();
        return 0;
    case WM_MOVE:
    {
        RECT r; GetWindowRect(h, &r);
        WritePrivateProfileStringW(L"Window", L"mainX", std::to_wstring(r.left).c_str(), IniPath().c_str());
        WritePrivateProfileStringW(L"Window", L"mainY", std::to_wstring(r.top).c_str(), IniPath().c_str());
        return 0;
    }
    case WM_CLOSE:
        HideMainToTray();
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void UI_Init(HINSTANCE hInst)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    HICON appIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0,
        LR_DEFAULTSIZE | LR_SHARED);
    wc.hIcon = appIcon ? appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = appIcon;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = MainProc; wc.lpszClassName = CLS_MAIN;
    RegisterClassExW(&wc);
    wc.lpfnWndProc = BarProc; wc.lpszClassName = CLS_BAR;
    RegisterClassExW(&wc);
}

void UI_CreateWindows()
{
    int mx = GetPrivateProfileIntW(L"Window", L"mainX", 120, IniPath().c_str());
    int my = GetPrivateProfileIntW(L"Window", L"mainY", 120, IniPath().c_str());
    int bx = GetPrivateProfileIntW(L"Window", L"barX", -1, IniPath().c_str());
    int by = GetPrivateProfileIntW(L"Window", L"barY", -1, IniPath().c_str());
    g.alpha = GetPrivateProfileIntW(L"Window", L"alpha", 244, IniPath().c_str());
    g.volume = GetPrivateProfileIntW(L"Player", L"Volume", 80, IniPath().c_str());
    g.loopMode = (LoopMode)GetPrivateProfileIntW(L"Player", L"Loop", 0, IniPath().c_str());

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (mx < 0 || my < 0 || mx > sw - 80 || my > sh - 80) { mx = (sw - MAIN_W) / 2; my = (sh - MAIN_H) / 2 - 80; }
    if (mx + MAIN_W > sw) mx = sw - MAIN_W;
    if (my + MAIN_H > sh) my = sh - MAIN_H;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (bx < 0 || by < 0 || bx > sw - 100 || by > sh - 60)
    {
        bx = (sw - BAR_W) / 2;
        by = sh - 190;
    }
    if (bx + BAR_W > sw) bx = sw - BAR_W;
    if (by + BAR_H > sh) by = sh - BAR_H;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    g.wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    g.hwndMain = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW, CLS_MAIN, APP_NAME,
        WS_POPUP | WS_VISIBLE, mx, my, MAIN_W, MAIN_H, nullptr, nullptr, g.hInst, nullptr);
    SetLayeredWindowAttributes(g.hwndMain, 0, 255, LWA_ALPHA);
    RegisterHotKey(g.hwndMain, 1, MOD_CONTROL | MOD_ALT, 'Q');

    g.hwndBar = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLS_BAR, APP_NAME,
        WS_POPUP | WS_VISIBLE, bx, by, BAR_W, BAR_H, nullptr, nullptr, g.hInst, nullptr);
    SetLayeredWindowAttributes(g.hwndBar, RGB(255, 0, 255), (BYTE)g.alpha, LWA_COLORKEY | LWA_ALPHA);

    Tray_Add(g.hwndMain, L"LyricsBar");
}

void UI_Refresh()
{
    if (g.hwndMain && IsWindowVisible(g.hwndMain)) InvalidateRect(g.hwndMain, nullptr, FALSE);
    if (g.hwndBar && IsWindowVisible(g.hwndBar)) InvalidateRect(g.hwndBar, nullptr, FALSE);
    Library_Tick();
}

void UI_RunMessageLoop()
{
    SetTimer(g.hwndMain, 1, 180, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    KillTimer(g.hwndMain, 1);
    UnregisterHotKey(g.hwndMain, 1);
    RECT r;
    if (GetWindowRect(g.hwndBar, &r))
    {
        WritePrivateProfileStringW(L"Window", L"barX", std::to_wstring(r.left).c_str(), IniPath().c_str());
        WritePrivateProfileStringW(L"Window", L"barY", std::to_wstring(r.top).c_str(), IniPath().c_str());
    }
    WritePrivateProfileStringW(L"Window", L"alpha", std::to_wstring(g.alpha).c_str(), IniPath().c_str());
    WritePrivateProfileStringW(L"Player", L"Volume", std::to_wstring(g.volume).c_str(), IniPath().c_str());
    WritePrivateProfileStringW(L"Player", L"Loop", std::to_wstring((int)g.loopMode).c_str(), IniPath().c_str());
    Tray_Remove();
}

void UI_Shutdown() { Library_Destroy(); }
