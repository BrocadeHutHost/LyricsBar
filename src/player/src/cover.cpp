#include "cover.h"
#include "app.h"
#include "lyrics.h"
#include "ui.h"
#include <wincodec.h>
#pragma comment(lib, "msimg32.lib")

static const CLSID kClsidWICImagingFactory =
{ 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0xa } };
static const GUID kGuidPixelBGRA =
{ 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };

static const int COVER_PX = 160;

static HBITMAP s_coverBmp = nullptr;
static HDC s_coverDc = nullptr;
static void* s_coverBits = nullptr;
static HGDIOBJ s_coverOld = nullptr;

static HBITMAP s_rotBmp = nullptr;
static HDC s_rotDc = nullptr;
static void* s_rotBits = nullptr;
static HGDIOBJ s_rotOld = nullptr;
static double s_rotAngle = -999.0;

static void FreeRotateBuffer()
{
    if (s_rotDc)
    {
        if (s_rotOld) SelectObject(s_rotDc, s_rotOld);
        DeleteDC(s_rotDc);
    }
    if (s_rotBmp) DeleteObject(s_rotBmp);
    s_rotDc = nullptr; s_rotBmp = nullptr; s_rotBits = nullptr; s_rotOld = nullptr;
    s_rotAngle = -999.0;
}

void Cover_Free()
{
    if (s_coverDc)
    {
        if (s_coverOld) SelectObject(s_coverDc, s_coverOld);
        DeleteDC(s_coverDc);
    }
    if (s_coverBmp) DeleteObject(s_coverBmp);
    s_coverDc = nullptr; s_coverBmp = nullptr; s_coverBits = nullptr; s_coverOld = nullptr;
    g.coverReady = false;
    FreeRotateBuffer();
}

static bool DecodeCover(const std::wstring& file, int size)
{
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(kClsidWICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory), (void**)&factory);
    if (FAILED(hr) || !factory) { LogLine(L"Cover: WIC unavailable"); return false; }

    bool ok = false;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    IWICBitmapScaler* scaler = nullptr;

    if (SUCCEEDED(factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &dec)) && dec &&
        SUCCEEDED(dec->GetFrame(0, &frame)) && frame &&
        SUCCEEDED(factory->CreateFormatConverter(&conv)) && conv &&
        SUCCEEDED(conv->Initialize(frame, kGuidPixelBGRA, WICBitmapDitherTypeNone,
            nullptr, 0.0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(factory->CreateBitmapScaler(&scaler)) && scaler &&
        SUCCEEDED(scaler->Initialize(conv, size, size, WICBitmapInterpolationModeFant)))
    {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = size;
        bi.bmiHeader.biHeight = -size;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp && bits &&
            SUCCEEDED(scaler->CopyPixels(nullptr, size * 4, size * size * 4, (BYTE*)bits)))
        {
            HDC dc = CreateCompatibleDC(nullptr);
            if (dc)
            {
                s_coverOld = SelectObject(dc, bmp);
                s_coverBmp = bmp; s_coverDc = dc; s_coverBits = bits;
                ok = true;
            }
            else DeleteObject(bmp);
        }
        else if (bmp) DeleteObject(bmp);
    }

    if (scaler) scaler->Release();
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    factory->Release();
    return ok;
}

static void RotateCover(double angle)
{
    if (!s_coverBits) return;
    const int D = COVER_PX;
    if (!s_rotDc)
    {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = D;
        bi.bmiHeader.biHeight = -D;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!bmp || !dc) { if (bmp) DeleteObject(bmp); if (dc) DeleteDC(dc); return; }
        s_rotOld = SelectObject(dc, bmp);
        s_rotBmp = bmp; s_rotDc = dc; s_rotBits = bits;
        s_rotAngle = -999.0;
    }
    if (fabs(angle - s_rotAngle) < 0.01) return;
    s_rotAngle = angle;

    const int S = COVER_PX;
    const BYTE* src = (const BYTE*)s_coverBits;
    BYTE* dst = (BYTE*)s_rotBits;
    const double ca = cos(angle), sa = sin(angle);
    const double half = D * 0.5;

    for (int y = 0; y < D; ++y)
    {
        BYTE* row = dst + (size_t)y * D * 4;
        double ny = (y + 0.5 - half) / half;
        for (int x = 0; x < D; ++x)
        {
            double nx = (x + 0.5 - half) / half;

            double u = nx * ca + ny * sa;
            double v = -nx * sa + ny * ca;
            double sx = (u * 0.5 + 0.5) * (S - 1);
            double sy = (v * 0.5 + 0.5) * (S - 1);
            if (sx < 0) sx = 0; else if (sx > S - 1) sx = S - 1;
            if (sy < 0) sy = 0; else if (sy > S - 1) sy = S - 1;

            int x0 = (int)sx, y0 = (int)sy;
            int x1 = x0 + 1 < S ? x0 + 1 : x0;
            int y1 = y0 + 1 < S ? y0 + 1 : y0;
            double fx = sx - x0, fy = sy - y0;

            const BYTE* p00 = src + ((size_t)y0 * S + x0) * 4;
            const BYTE* p10 = src + ((size_t)y0 * S + x1) * 4;
            const BYTE* p01 = src + ((size_t)y1 * S + x0) * 4;
            const BYTE* p11 = src + ((size_t)y1 * S + x1) * 4;
            BYTE* o = row + x * 4;
            for (int c = 0; c < 3; ++c)
            {
                double top = p00[c] + (p10[c] - p00[c]) * fx;
                double bot = p01[c] + (p11[c] - p01[c]) * fx;
                o[c] = (BYTE)(top + (bot - top) * fy + 0.5);
            }

            double ddx = x + 0.5 - half, ddy = y + 0.5 - half;
            double dist = sqrt(ddx * ddx + ddy * ddy);
            double feather = (half - dist) / 1.5;
            if (feather <= 0) o[3] = 0;
            else if (feather >= 1) o[3] = 255;
            else o[3] = (BYTE)(feather * 255);
        }
    }
}

bool Cover_Ready() { return g.coverReady; }

bool Cover_DrawDisc(HDC hdc, int cx, int cy, int radius, double angle)
{
    if (!g.coverReady || !s_coverDc) return false;

    int r = radius;

    HBRUSH vinyl = CreateSolidBrush(RGB(19, 20, 24));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, vinyl);
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    int groove = std::max(2, r / 22);
    HPEN gp = CreatePen(PS_SOLID, groove, RGB(40, 42, 49));
    SelectObject(hdc, gp);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (int k = 82; k <= 97; k += 5)
    {
        int rr = r * k / 100;
        Ellipse(hdc, cx - rr, cy - rr, cx + rr, cy + rr);
    }
    DeleteObject(gp);

    int cvr = (int)(r * 0.70);
    int D = cvr * 2;
    RotateCover(angle);
    if (s_rotDc)
    {
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, nullptr);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(hdc, cx - cvr, cy - cvr, D, D, s_rotDc, 0, 0, COVER_PX, COVER_PX, bf);
    }

    HPEN ring = CreatePen(PS_SOLID, std::max(2, r / 20), RGB(126, 196, 187));
    SelectObject(hdc, ring);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - cvr, cy - cvr, cx + cvr, cy + cvr);
    DeleteObject(ring);

    int hr = std::max(3, r / 14);
    HBRUSH hub = CreateSolidBrush(RGB(24, 25, 29));
    SelectObject(hdc, hub);
    Ellipse(hdc, cx - hr, cy - hr, cx + hr, cy + hr);
    DeleteObject(hub);
    HBRUSH hl = CreateSolidBrush(RGB(150, 158, 165));
    SelectObject(hdc, hl);
    int hd = std::max(1, hr / 3);
    Ellipse(hdc, cx - hd, cy - hd, cx + hd, cy + hd);
    DeleteObject(hl);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(vinyl);
    return true;
}

void Cover_LoadForCurrent()
{
    Cover_Free();
    g.coverPath.clear();
    g.fetchingCover = false;
    if (g.title.empty()) return;

    std::wstring safe = SanitizeFileName(g.title) +
        (g.artist.empty() ? L"" : L"_" + SanitizeFileName(g.artist));
    std::wstring path = CoversDir() + L"\\" + safe + L".jpg";
    g.coverPath = path;

    if (FileExists(path))
    {
        if (DecodeCover(path, COVER_PX)) { g.coverReady = true; return; }
        DeleteFileW(path.c_str());
    }

    std::wstring query = g.title + (g.artist.empty() ? L"" : L" " + g.artist);
    std::wstring args = L"--cover \"" + query + L"\" \"" + path + L"\"";
    if (Lyric_SpawnFetcher(args))
    {
        g.fetchingCover = true;
        g.coverTick = GetTickCount();
    }
}

void Cover_Poll()
{
    if (!g.fetchingCover) return;
    if (!g.coverPath.empty() && FileExists(g.coverPath))
    {
        if (DecodeCover(g.coverPath, COVER_PX))
        {
            g.coverReady = true;
            g.fetchingCover = false;
            UI_Refresh();
            LogLine(L"Cover loaded");
        }
        else if (GetTickCount() - g.coverTick > 4000)
        {
            DeleteFileW(g.coverPath.c_str());
            g.fetchingCover = false;
            LogLine(L"Cover decode failed");
        }
    }
    else if (GetTickCount() - g.coverTick > 12000)
    {
        g.fetchingCover = false;
        LogLine(L"Cover fetch timeout");
    }
}

void Cover_Shutdown()
{
    Cover_Free();
    g.fetchingCover = false;
    g.coverPath.clear();
}
