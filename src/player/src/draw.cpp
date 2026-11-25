#include "draw.h"
#pragma comment(lib, "msimg32.lib")

COLORREF Mix(COLORREF a, COLORREF b, int t)
{
    if (t < 0) t = 0;
    if (t > 255) t = 255;
    int r = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / 255;
    int g = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / 255;
    int bl = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / 255;
    return RGB(r, g, bl);
}

static HDC s_solidDc = nullptr;
static DWORD* s_solidBits = nullptr;

static HDC SolidDC()
{
    if (s_solidDc) return s_solidDc;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = 8;
    bi.bmiHeader.biHeight = -8;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); return nullptr; }
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) { DeleteObject(bmp); return nullptr; }
    SelectObject(dc, bmp);
    s_solidDc = dc;
    s_solidBits = (DWORD*)bits;
    return dc;
}

void BlendShapeRgn(HDC hdc, HRGN rg, COLORREF c, int alpha)
{
    if (alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    HDC src = SolidDC();
    if (!src) return;
    DWORD v = ((DWORD)GetRValue(c) << 16) | ((DWORD)GetGValue(c) << 8) | GetBValue(c);
    for (int i = 0; i < 64; ++i) s_solidBits[i] = v;
    RECT r; GetRgnBox(rg, &r);
    int saved = SaveDC(hdc);
    SelectClipRgn(hdc, rg);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
    AlphaBlend(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top, src, 0, 0, 8, 8, bf);
    RestoreDC(hdc, saved);
}

void BlendRoundRect(HDC hdc, int x, int y, int w, int h, COLORREF c, int alpha, int radius)
{
    if (w <= 0 || h <= 0) return;
    HRGN rg = CreateRoundRectRgn(x, y, x + w, y + h, radius, radius);
    BlendShapeRgn(hdc, rg, c, alpha);
    DeleteObject(rg);
}

void BlendCircle(HDC hdc, int cx, int cy, int r, COLORREF c, int alpha)
{
    if (r <= 0) return;
    HRGN rg = CreateEllipticRgn(cx - r, cy - r, cx + r, cy + r);
    BlendShapeRgn(hdc, rg, c, alpha);
    DeleteObject(rg);
}

void DrawGlyph(HDC hdc, int cx, int cy, int kind, int s, COLORREF col)
{
    HBRUSH br = CreateSolidBrush(col);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    HPEN op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    switch (kind)
    {
    case G_PLAY:
    {
        POINT p[3] = { { cx - s + 2, cy - s }, { cx - s + 2, cy + s }, { cx + s, cy } };
        Polygon(hdc, p, 3);
        break;
    }
    case G_PAUSE:
    {
        int w = s * 2 / 5;
        RECT a = { cx - s + 1, cy - s, cx - s + 1 + w, cy + s };
        RECT b = { cx + s - 1 - w, cy - s, cx + s - 1, cy + s };
        FillRect(hdc, &a, br);
        FillRect(hdc, &b, br);
        break;
    }
    case G_PREV:
    case G_NEXT:
    {
        RECT bar;
        if (kind == G_PREV) { bar.left = cx - s; bar.right = cx - s + s / 3 + 1; }
        else { bar.left = cx + s - s / 3 - 1; bar.right = cx + s; }
        bar.top = cy - s; bar.bottom = cy + s;
        FillRect(hdc, &bar, br);
        POINT p[3];
        if (kind == G_PREV)
        {
            p[0].x = cx + s; p[0].y = cy - s;
            p[1].x = cx + s; p[1].y = cy + s;
            p[2].x = cx - s + s / 3; p[2].y = cy;
        }
        else
        {
            p[0].x = cx - s; p[0].y = cy - s;
            p[1].x = cx - s; p[1].y = cy + s;
            p[2].x = cx + s - s / 3; p[2].y = cy;
        }
        Polygon(hdc, p, 3);
        break;
    }
    case G_SPEAKER:
    {
        POINT p[6] = {
            { cx - s, cy - s / 3 }, { cx - s / 2, cy - s / 3 }, { cx + s / 5, cy - s },
            { cx + s / 5, cy + s }, { cx - s / 2, cy + s / 3 }, { cx - s, cy + s / 3 }
        };
        Polygon(hdc, p, 6);
        HPEN pen = CreatePen(PS_SOLID, 2, col);
        HPEN old = (HPEN)SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        POINT wave[7];
        for (int i = 0; i < 7; ++i)
        {
            double ang = (-50 + i * 16.5) * 3.14159265 / 180.0;
            wave[i].x = cx + s / 3 + (int)(cos(ang) * s * 0.95);
            wave[i].y = cy + (int)(sin(ang) * s * 0.95);
        }
        Polyline(hdc, wave, 7);
        SelectObject(hdc, old);
        DeleteObject(pen);
        break;
    }
    case G_FOLDER:
    {
        int w = s * 3 / 2;
        RECT tab = { cx - w, cy - s, cx - w / 4, cy - s / 2 };
        RECT body = { cx - w, cy - s / 2, cx + w, cy + s };
        FillRect(hdc, &tab, br);
        FillRect(hdc, &body, br);
        break;
    }
    case G_UP:
    {
        POINT p[3] = { { cx, cy - s }, { cx - s, cy + s / 2 }, { cx + s, cy + s / 2 } };
        Polygon(hdc, p, 3);
        RECT stem = { cx - s / 3, cy + s / 2, cx + s / 3, cy + s };
        FillRect(hdc, &stem, br);
        break;
    }
    case G_PLUS:
    {
        RECT a = { cx - s, cy - std::max(2, s / 4), cx + s, cy + std::max(2, s / 4) };
        RECT b = { cx - std::max(2, s / 4), cy - s, cx + std::max(2, s / 4), cy + s };
        FillRect(hdc, &a, br);
        FillRect(hdc, &b, br);
        break;
    }
    }
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
}

static void DrawX(HDC hdc, int cx, int cy, int half, int thick, COLORREF col)
{
    HPEN p = CreatePen(PS_SOLID, thick, col);
    HPEN op = (HPEN)SelectObject(hdc, p);
    MoveToEx(hdc, cx - half, cy - half, nullptr);
    LineTo(hdc, cx + half, cy + half);
    MoveToEx(hdc, cx - half, cy + half, nullptr);
    LineTo(hdc, cx + half, cy - half);
    SelectObject(hdc, op);
    DeleteObject(p);
}

void DrawCloseCircle(HDC hdc, int cx, int cy, int r, bool hover)
{
    BlendCircle(hdc, cx, cy, r, hover ? RGB(198, 74, 74) : RGB(255, 255, 255), hover ? 228 : 22);
    DrawX(hdc, cx, cy, r / 2, std::max(2, r / 9), hover ? RGB(255, 255, 255) : RGB(208, 214, 220));
}

void DrawIconButton(HDC hdc, int x, int y, int w, int h, bool hot, int kind)
{
    BlendRoundRect(hdc, x * UI_SCALE, y * UI_SCALE, w * UI_SCALE, h * UI_SCALE,
        RGB(255, 255, 255), hot ? 30 : 12, 22 * UI_SCALE);
    DrawGlyph(hdc, (x + w / 2) * UI_SCALE, (y + h / 2) * UI_SCALE, kind, 7 * UI_SCALE,
        hot ? RGB(250, 252, 253) : RGB(206, 213, 219));
}

void DrawTextButton(HDC hdc, int x, int y, int w, int h, bool hot, const wchar_t* label)
{
    BlendRoundRect(hdc, x * UI_SCALE, y * UI_SCALE, w * UI_SCALE, h * UI_SCALE,
        RGB(255, 255, 255), hot ? 30 : 12, 12 * UI_SCALE);
    RECT r = R2(x, y, x + w, y + h);
    HFONT f = MakeFontS(11, false);
    SetTextColor(hdc, hot ? RGB(250, 252, 253) : RGB(214, 220, 226));
    SelectObject(hdc, f);
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(f);
}
