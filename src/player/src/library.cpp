#include "library.h"
#include "draw.h"
#include "playlist.h"
#include "audio.h"
#include "app.h"
#include "ui.h"
#include "folderpick.h"

static const COLORREF LBG = RGB(23, 24, 28);
static const COLORREF LPANEL = RGB(28, 30, 35);
static const COLORREF LACCENT = RGB(132, 214, 201);
static const COLORREF LTEXT = RGB(242, 245, 247);
static const COLORREF LDIM = RGB(146, 154, 164);
static const COLORREF LHAIR = RGB(45, 48, 55);

static const int L_CLOSE_CX = 590, L_CLOSE_CY = 30, L_CLOSE_R = 17;
static const int L_SIDE_X = 14, L_SIDE_W = 156, L_SIDE_Y = 62, L_SIDE_B = LIB_H - 14;
static const int L_SIDE_ITEM_X = 20, L_SIDE_ITEM_W = 144, L_SIDE_ITEM_H = 42;
static const int L_SIDE_TOP = 96;
static const int L_ADD_Y = 524, L_ADD_H = 34;
static const int L_ROW_X = 186, L_ROW_W = 420;
static const int L_ROW_Y = 102, L_ROW_H = 50;
static const int L_ROWS = 9;
static const int L_SCROLL_X = 610, L_SCROLL_W = 4;

static int s_scroll = 0;
static int s_sideScroll = 0;
static int s_hot = -1;
static int s_hotSide = -1;
static bool s_hotClose = false, s_hotAdd = false;
static bool s_dragThumb = false;
static int s_thumbGrab = 0;
static bool s_tracking = false;
static int s_lastTrack = -2;
static bool s_lastPaused = false;

enum { HIT_NONE = 0, HIT_CLOSE, HIT_ADD, HIT_THUMB, HIT_TRACK, HIT_SIDE = 100, HIT_ROW = 10000 };

static int SideItems() { return 1 + (int)g.roots.size(); }
static int SideVisible() { return (L_ADD_Y - 8 - L_SIDE_TOP) / L_SIDE_ITEM_H; }

static void ClampScroll()
{
    int maxScroll = std::max(0, (int)g.playlist.size() - L_ROWS);
    if (s_scroll > maxScroll) s_scroll = maxScroll;
    if (s_scroll < 0) s_scroll = 0;
}

static void ClampSide()
{
    int maxScroll = std::max(0, SideItems() - SideVisible());
    if (s_sideScroll > maxScroll) s_sideScroll = maxScroll;
    if (s_sideScroll < 0) s_sideScroll = 0;
}

static void FollowTrack()
{
    if (g.track < 0) return;
    if (g.track < s_scroll) s_scroll = g.track;
    if (g.track >= s_scroll + L_ROWS) s_scroll = g.track - L_ROWS + 1;
    ClampScroll();
}

static void DrawSideItem(HDC hdc, int x, int y, int w, int h, const wchar_t* name, int count,
    bool active, bool hot)
{
    if (active)
    {
        BlendRoundRect(hdc, x * UI_SCALE, y * UI_SCALE, w * UI_SCALE, h * UI_SCALE,
            LACCENT, 30, 11 * UI_SCALE);
        BlendRoundRect(hdc, x * UI_SCALE, (y + h / 2 - 10) * UI_SCALE, 3 * UI_SCALE, 20 * UI_SCALE,
            LACCENT, 235, 2 * UI_SCALE);
    }
    else if (hot)
    {
        BlendRoundRect(hdc, x * UI_SCALE, y * UI_SCALE, w * UI_SCALE, h * UI_SCALE,
            RGB(255, 255, 255), 14, 11 * UI_SCALE);
    }

    HFONT f = MakeFontS(12, active);
    SetTextColor(hdc, active ? LTEXT : (hot ? RGB(226, 232, 238) : RGB(180, 188, 197)));
    SelectObject(hdc, f);
    RECT rn = R2(x + 14, y, x + w - 44, y + h);
    DrawTextW(hdc, name, -1, &rn, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(f);

    HFONT fc = MakeFontS(10, false, L"Microsoft YaHei UI Light");
    SetTextColor(hdc, active ? RGB(198, 234, 228) : RGB(120, 128, 138));
    SelectObject(hdc, fc);
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", count);
    RECT rc = R2(x + w - 42, y, x + w - 10, y + h);
    DrawTextW(hdc, buf, -1, &rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(fc);
}

static void PaintLibrary(HDC hdc)
{
    RECT full = R2(0, 0, LIB_W, LIB_H);
    HBRUSH bg = CreateSolidBrush(LBG);
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
    SetBkMode(hdc, TRANSPARENT);

    HFONT ft = MakeFontS(18, true);
    SetTextColor(hdc, LTEXT);
    SelectObject(hdc, ft);
    RECT rt = R2(24, 12, 340, 44);
    DrawTextW(hdc, L"曲目库", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(ft);

    HFONT fs = MakeFontS(10, false, L"Microsoft YaHei UI Light");
    SetTextColor(hdc, RGB(118, 126, 136));
    SelectObject(hdc, fs);
    RECT rs = R2(24, 44, 460, 62);
    std::wstring sub = std::to_wstring(g.allTracks.size()) + L" 首曲目 · " +
        std::to_wstring(g.roots.size()) + L" 个播放源 · 单击即切换";
    DrawTextW(hdc, sub.c_str(), -1, &rs, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(fs);

    DrawCloseCircle(hdc, L_CLOSE_CX * UI_SCALE, L_CLOSE_CY * UI_SCALE,
        L_CLOSE_R * UI_SCALE, s_hotClose);

    BlendRoundRect(hdc, L_SIDE_X * UI_SCALE, L_SIDE_Y * UI_SCALE,
        L_SIDE_W * UI_SCALE, (L_SIDE_B - L_SIDE_Y) * UI_SCALE, LPANEL, 255, 16 * UI_SCALE);
    HFONT fh = MakeFontS(9, false, L"Microsoft YaHei UI Light");
    SetTextColor(hdc, RGB(116, 124, 134));
    SelectObject(hdc, fh);
    RECT rh = R2(L_SIDE_X + 14, L_SIDE_Y + 10, L_SIDE_X + L_SIDE_W - 10, L_SIDE_Y + 28);
    DrawTextW(hdc, L"播放源", -1, &rh, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(fh);

    int visSide = SideVisible();
    for (int i = 0; i < visSide; ++i)
    {
        int item = s_sideScroll + i;
        if (item >= SideItems()) break;
        int y = L_SIDE_TOP + i * L_SIDE_ITEM_H;
        int rootIdx = item - 1;
        std::wstring nameBuf = item == 0 ? L"全部曲目" : Playlist_RootName(rootIdx);
        bool active = (item == 0 && g.activeRoot == -1) || (item > 0 && g.activeRoot == rootIdx);
        DrawSideItem(hdc, L_SIDE_ITEM_X, y, L_SIDE_ITEM_W, L_SIDE_ITEM_H - 4,
            nameBuf.c_str(), Playlist_RootCount(item == 0 ? -1 : rootIdx), active, s_hotSide == item);
    }

    {
        bool hot = s_hotAdd;
        BlendRoundRect(hdc, L_SIDE_ITEM_X * UI_SCALE, L_ADD_Y * UI_SCALE,
            L_SIDE_ITEM_W * UI_SCALE, L_ADD_H * UI_SCALE, RGB(255, 255, 255), hot ? 28 : 12, 11 * UI_SCALE);
        DrawGlyph(hdc, (L_SIDE_ITEM_X + 18) * UI_SCALE, (L_ADD_Y + L_ADD_H / 2) * UI_SCALE,
            G_PLUS, 5 * UI_SCALE, hot ? LTEXT : RGB(176, 184, 193));
        HFONT fa = MakeFontS(10, false);
        SetTextColor(hdc, hot ? LTEXT : RGB(196, 204, 213));
        SelectObject(hdc, fa);
        RECT ra = R2(L_SIDE_ITEM_X + 32, L_ADD_Y, L_SIDE_ITEM_X + L_SIDE_ITEM_W - 8, L_ADD_Y + L_ADD_H);
        DrawTextW(hdc, L"添加文件夹", -1, &ra, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(fa);
    }

    std::wstring view = Playlist_RootName(g.activeRoot);
    HFONT fv = MakeFontS(16, true);
    SetTextColor(hdc, LTEXT);
    SelectObject(hdc, fv);
    RECT rv = R2(L_ROW_X, 62, L_ROW_X + 300, 92);
    DrawTextW(hdc, view.c_str(), -1, &rv, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DeleteObject(fv);

    HFONT fcnt = MakeFontS(10, false, L"Microsoft YaHei UI Light");
    SetTextColor(hdc, LDIM);
    SelectObject(hdc, fcnt);
    wchar_t cnt[32];
    swprintf(cnt, 32, L"%d 首", (int)g.playlist.size());
    RECT rcnt = R2(L_ROW_X + 300, 62, L_ROW_X + L_ROW_W, 92);
    DrawTextW(hdc, cnt, -1, &rcnt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(fcnt);

    HPEN hair = CreatePen(PS_SOLID, 2, LHAIR);
    HPEN oph = (HPEN)SelectObject(hdc, hair);
    MoveToEx(hdc, L_ROW_X * UI_SCALE, 96 * UI_SCALE, nullptr);
    LineTo(hdc, (L_ROW_X + L_ROW_W) * UI_SCALE, 96 * UI_SCALE);
    SelectObject(hdc, oph);
    DeleteObject(hair);

    HFONT fidx = MakeFontS(10, false, L"Consolas");
    HFONT fti = MakeFontS(14, false);
    HFONT fsub = MakeFontS(11, false, L"Microsoft YaHei UI Light");
    for (int i = 0; i < L_ROWS; ++i)
    {
        int idx = s_scroll + i;
        if (idx >= (int)g.playlist.size()) break;
        int y = L_ROW_Y + i * L_ROW_H;
        bool cur = (idx == g.track);
        if (cur)
        {
            BlendRoundRect(hdc, L_ROW_X * UI_SCALE, y * UI_SCALE, L_ROW_W * UI_SCALE,
                (L_ROW_H - 3) * UI_SCALE, LACCENT, 26, 10 * UI_SCALE);
            BlendRoundRect(hdc, L_ROW_X * UI_SCALE, (y + 9) * UI_SCALE, 3 * UI_SCALE,
                (L_ROW_H - 21) * UI_SCALE, LACCENT, 240, 2 * UI_SCALE);
        }
        else if (idx == s_hot)
        {
            BlendRoundRect(hdc, L_ROW_X * UI_SCALE, y * UI_SCALE, L_ROW_W * UI_SCALE,
                (L_ROW_H - 3) * UI_SCALE, RGB(255, 255, 255), 12, 10 * UI_SCALE);
        }

        std::wstring title, artist;
        ParseSongName(BaseNameNoExt(g.playlist[idx]), title, artist);
        if (artist.empty()) artist = L"未知艺术家";

        SetTextColor(hdc, cur ? LACCENT : RGB(122, 130, 140));
        SelectObject(hdc, fidx);
        wchar_t num[16];
        swprintf(num, 16, L"%d", idx + 1);
        RECT ri = R2(L_ROW_X + 6, y + 4, L_ROW_X + 36, y + 28);
        DrawTextW(hdc, num, -1, &ri, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(hdc, cur ? LACCENT : LTEXT);
        SelectObject(hdc, fti);
        RECT rtitle = R2(L_ROW_X + 46, y + 2, L_ROW_X + L_ROW_W - 34, y + 28);
        DrawTextW(hdc, title.c_str(), -1, &rtitle,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SetTextColor(hdc, cur ? RGB(170, 222, 214) : RGB(134, 142, 152));
        SelectObject(hdc, fsub);
        RECT rartist = R2(L_ROW_X + 46, y + 26, L_ROW_X + L_ROW_W - 34, y + 48);
        DrawTextW(hdc, artist.c_str(), -1, &rartist,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (cur)
            DrawGlyph(hdc, (L_ROW_X + L_ROW_W - 22) * UI_SCALE, (y + L_ROW_H / 2 - 1) * UI_SCALE,
                g.paused ? G_PLAY : G_PAUSE, 4 * UI_SCALE, LACCENT);
    }
    DeleteObject(fidx); DeleteObject(fti); DeleteObject(fsub);

    if (g.playlist.empty())
    {
        HFONT fe = MakeFontS(11, false);
        SetTextColor(hdc, RGB(120, 128, 138));
        SelectObject(hdc, fe);
        RECT re = R2(L_ROW_X, L_ROW_Y, L_ROW_X + L_ROW_W, L_ROW_Y + 160);
        DrawTextW(hdc, L"这个播放源里没有音频，点左下角“添加文件夹”换个目录", -1, &re,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DeleteObject(fe);
    }

    if ((int)g.playlist.size() > L_ROWS)
    {
        int trackH = L_ROWS * L_ROW_H - 6;
        int thumbH = std::max(28, trackH * L_ROWS / (int)g.playlist.size());
        int maxScroll = (int)g.playlist.size() - L_ROWS;
        int off = (trackH - thumbH) * s_scroll / std::max(1, maxScroll);
        BlendRoundRect(hdc, L_SCROLL_X * UI_SCALE, (L_ROW_Y + off) * UI_SCALE,
            L_SCROLL_W * UI_SCALE, thumbH * UI_SCALE, RGB(255, 255, 255), 52, 4 * UI_SCALE);
    }

}

static int HitTest(int x, int y)
{
    if ((x - L_CLOSE_CX) * (x - L_CLOSE_CX) + (y - L_CLOSE_CY) * (y - L_CLOSE_CY) <= L_CLOSE_R * L_CLOSE_R)
        return HIT_CLOSE;
    if (x >= L_SIDE_ITEM_X && x <= L_SIDE_ITEM_X + L_SIDE_ITEM_W && y >= L_ADD_Y && y <= L_ADD_Y + L_ADD_H)
        return HIT_ADD;
    if (x >= L_SIDE_ITEM_X && x <= L_SIDE_ITEM_X + L_SIDE_ITEM_W && y >= L_SIDE_TOP && y < L_ADD_Y - 8)
    {
        int item = s_sideScroll + (y - L_SIDE_TOP) / L_SIDE_ITEM_H;
        if (item < SideItems()) return HIT_SIDE + item;
    }
    if (x >= L_ROW_X && x <= L_ROW_X + L_ROW_W && y >= L_ROW_Y && y < L_ROW_Y + L_ROWS * L_ROW_H)
    {
        int idx = s_scroll + (y - L_ROW_Y) / L_ROW_H;
        if (idx < (int)g.playlist.size()) return HIT_ROW + idx;
    }
    if ((int)g.playlist.size() > L_ROWS && x >= L_SCROLL_X - 4 && x <= L_SCROLL_X + L_SCROLL_W + 4 &&
        y >= L_ROW_Y && y <= L_ROW_Y + L_ROWS * L_ROW_H)
    {
        int trackH = L_ROWS * L_ROW_H - 6;
        int thumbH = std::max(28, trackH * L_ROWS / (int)g.playlist.size());
        int maxScroll = (int)g.playlist.size() - L_ROWS;
        int off = (trackH - thumbH) * s_scroll / std::max(1, maxScroll);
        if (y >= L_ROW_Y + off && y <= L_ROW_Y + off + thumbH) return HIT_THUMB;
        return HIT_TRACK;
    }
    return HIT_NONE;
}

static void CloseLibrary(HWND h) { DestroyWindow(h); }

static LRESULT CALLBACK LibProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        SetWindowRgn(h, CreateRoundRectRgn(0, 0, LIB_W, LIB_H, 40, 40), TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        PaintBuffer buf = BeginBuffer(h, LIB_W, LIB_H);
        PaintLibrary(buf.dc);
        EndBuffer(h, buf, hdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(h, &pt);
        if (pt.y < 52 && pt.x < 560) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (!s_tracking)
        {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            s_tracking = true;
        }
        if (s_dragThumb)
        {
            int trackH = L_ROWS * L_ROW_H - 6;
            int thumbH = std::max(28, trackH * L_ROWS / std::max(1, (int)g.playlist.size()));
            int span = std::max(1, trackH - thumbH);
            int maxScroll = std::max(0, (int)g.playlist.size() - L_ROWS);
            s_scroll = (y - L_ROW_Y - s_thumbGrab) * maxScroll / span;
            ClampScroll();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        int hit = HitTest(x, y);
        int hot = (hit >= HIT_ROW && hit < HIT_SIDE) ? hit - HIT_ROW : -1;
        int hotSide = (hit >= HIT_SIDE && hit - HIT_SIDE < SideItems()) ? hit - HIT_SIDE : -1;
        bool close = (hit == HIT_CLOSE);
        bool add = (hit == HIT_ADD);
        if (hot != s_hot || hotSide != s_hotSide || close != s_hotClose || add != s_hotAdd)
        {
            s_hot = hot; s_hotSide = hotSide; s_hotClose = close; s_hotAdd = add;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        s_tracking = false;
        s_hot = -1; s_hotSide = -1; s_hotClose = false; s_hotAdd = false;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = HitTest(x, y);
        if (hit == HIT_CLOSE) { CloseLibrary(h); return 0; }
        if (hit == HIT_ADD)
        {
            std::wstring path = PickFolder(h);
            if (!path.empty())
            {
                Playlist_AddRoot(path);
                s_scroll = 0;
                s_sideScroll = std::max(0, SideItems() - SideVisible());
                ClampScroll();
                FollowTrack();
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        }
        if (hit >= HIT_SIDE && hit - HIT_SIDE < SideItems())
        {
            int item = hit - HIT_SIDE;
            Playlist_SwitchTo(item == 0 ? -1 : item - 1, true);
            s_scroll = 0;
            FollowTrack();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (hit >= HIT_ROW)
        {
            int idx = hit - HIT_ROW;
            LogLine((L"List click: row " + std::to_wstring(idx) + L"/" + std::to_wstring(g.playlist.size()) +
                L" scroll " + std::to_wstring(s_scroll)).c_str());
            if (idx == g.track) App_TogglePlay();
            else Audio_PlayIndex(idx);
            s_lastTrack = g.track;
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (hit == HIT_THUMB || hit == HIT_TRACK)
        {
            int trackH = L_ROWS * L_ROW_H - 6;
            int thumbH = std::max(28, trackH * L_ROWS / std::max(1, (int)g.playlist.size()));
            int maxScroll = std::max(0, (int)g.playlist.size() - L_ROWS);
            if (hit == HIT_TRACK)
            {
                int span = std::max(1, trackH - thumbH);
                s_scroll = (y - L_ROW_Y - thumbH / 2) * maxScroll / span;
                ClampScroll();
            }
            else
            {
                s_thumbGrab = y - L_ROW_Y - (trackH - thumbH) * s_scroll / std::max(1, maxScroll);
            }
            s_dragThumb = true;
            SetCapture(h);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }

        ReleaseCapture();
        SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }
    case WM_LBUTTONUP:
        if (s_dragThumb)
        {
            s_dragThumb = false;
            if (GetCapture() == h) ReleaseCapture();
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        s_dragThumb = false;
        return 0;
    case WM_MOUSEWHEEL:
    {
        POINT pt; GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (pt.x < L_SIDE_X + L_SIDE_W)
        {
            s_sideScroll += (delta > 0 ? -1 : 1);
            ClampSide();
        }
        else
        {
            s_scroll += (delta > 0 ? -3 : 3);
            ClampScroll();
            s_hot = -1;
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_RBUTTONUP:
    {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = HitTest(x, y);
        POINT pt; GetCursorPos(&pt);
        HMENU m = CreatePopupMenu();
        if (hit >= HIT_SIDE && hit - HIT_SIDE < SideItems())
        {
            int item = hit - HIT_SIDE;
            if (item > 0)
            {
                AppendMenuW(m, MF_STRING, 1, L"切换到这个播放源");
                AppendMenuW(m, MF_STRING, 2, g.roots.size() > 1 ? L"移除此播放源" : L"移除此播放源（至少保留一个）");
                if (g.roots.size() <= 1) EnableMenuItem(m, 2, MF_GRAYED);
            }
            else AppendMenuW(m, MF_STRING, 1, L"显示全部曲目");
        }
        else if (hit >= HIT_ROW)
        {
            AppendMenuW(m, MF_STRING, 3, L"播放这首");
            AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenuW(m, MF_STRING, 4, L"重新扫描曲目");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 5, L"添加文件夹…");
        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
        DestroyMenu(m);
        PostMessageW(h, WM_NULL, 0, 0);
        if (cmd == 1)
        {
            int item = hit - HIT_SIDE;
            Playlist_SwitchTo(item == 0 ? -1 : item - 1, true);
            s_scroll = 0;
            FollowTrack();
            InvalidateRect(h, nullptr, FALSE);
        }
        else if (cmd == 2)
        {
            int item = hit - HIT_SIDE;
            Playlist_RemoveRoot(item - 1);
            ClampSide();
            FollowTrack();
            InvalidateRect(h, nullptr, FALSE);
        }
        else if (cmd == 3)
        {
            Audio_PlayIndex(hit - HIT_ROW);
            FollowTrack();
            InvalidateRect(h, nullptr, FALSE);
        }
        else if (cmd == 4)
        {
            std::wstring cur = g.currentFile;
            Playlist_Scan();
            auto it = std::find(g.playlist.begin(), g.playlist.end(), cur);
            g.track = (it == g.playlist.end()) ? -1 : (int)(it - g.playlist.begin());
            ClampScroll();
            InvalidateRect(h, nullptr, FALSE);
        }
        else if (cmd == 5)
        {
            std::wstring path = PickFolder(h);
            if (!path.empty())
            {
                Playlist_AddRoot(path);
                s_scroll = 0;
                s_sideScroll = std::max(0, SideItems() - SideVisible());
                ClampScroll();
                FollowTrack();
                InvalidateRect(h, nullptr, FALSE);
            }
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { CloseLibrary(h); return 0; }
        if (wp == VK_UP) { s_scroll--; ClampScroll(); InvalidateRect(h, nullptr, FALSE); return 0; }
        if (wp == VK_DOWN) { s_scroll++; ClampScroll(); InvalidateRect(h, nullptr, FALSE); return 0; }
        return 0;
    case WM_DESTROY:
        g.hwndLib = nullptr;
        return 0;
    case WM_CLOSE:
        CloseLibrary(h);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static bool s_classReady = false;

static void EnsureWindow()
{
    if (g.hwndLib) return;
    if (!s_classReady)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = LibProc;
        wc.lpszClassName = CLS_LIB;
        if (!RegisterClassExW(&wc)) return;
        s_classReady = true;
    }

    RECT mr = {};
    int x = 200, y = 100;
    if (g.hwndMain && GetWindowRect(g.hwndMain, &mr))
    {
        x = mr.right + 14;
        y = mr.top;
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        if (x + LIB_W > sw) x = mr.left - 14 - LIB_W;
        if (x < 0) x = (sw - LIB_W) / 2;
        if (y + LIB_H > sh) y = sh - LIB_H - 40;
        if (y < 0) y = 0;
    }
    g.hwndLib = CreateWindowExW(WS_EX_TOOLWINDOW, CLS_LIB, L"曲目库",
        WS_POPUP | WS_VISIBLE, x, y, LIB_W, LIB_H, g.hwndMain, nullptr, g.hInst, nullptr);
    s_scroll = 0;
    FollowTrack();
    LogLine((L"Library created: track " + std::to_wstring(g.track) + L" scroll " + std::to_wstring(s_scroll)).c_str());
}

void Library_Show()
{
    EnsureWindow();
    if (!g.hwndLib) return;
    ClampScroll();
    SetWindowPos(g.hwndLib, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(g.hwndLib, SW_SHOW);
    SetForegroundWindow(g.hwndLib);
    InvalidateRect(g.hwndLib, nullptr, FALSE);
}

void Library_Hide()
{
    if (g.hwndLib) ShowWindow(g.hwndLib, SW_HIDE);
}

void Library_Toggle()
{
    if (g.hwndLib && IsWindowVisible(g.hwndLib)) Library_Hide();
    else Library_Show();
}

void Library_RefreshData()
{
    ClampScroll();
    ClampSide();
    if (g.hwndLib && IsWindowVisible(g.hwndLib)) InvalidateRect(g.hwndLib, nullptr, FALSE);
}

void Library_Tick()
{
    if (!g.hwndLib || !IsWindowVisible(g.hwndLib)) return;
    if (g.track != s_lastTrack || g.paused != s_lastPaused)
    {
        s_lastTrack = g.track;
        s_lastPaused = g.paused;
        FollowTrack();
        InvalidateRect(g.hwndLib, nullptr, FALSE);
    }
}

bool Library_Visible() { return g.hwndLib && IsWindowVisible(g.hwndLib); }

void Library_Destroy()
{
    if (g.hwndLib) { DestroyWindow(g.hwndLib); g.hwndLib = nullptr; }
}
