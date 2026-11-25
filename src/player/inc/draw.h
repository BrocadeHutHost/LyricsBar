#pragma once
#include "common.h"

enum GlyphKind { G_PREV = 0, G_NEXT, G_PLAY, G_PAUSE, G_SPEAKER, G_FOLDER, G_UP, G_PLUS };

COLORREF Mix(COLORREF a, COLORREF b, int t);
void BlendShapeRgn(HDC hdc, HRGN rg, COLORREF c, int alpha);
void BlendRoundRect(HDC hdc, int x, int y, int w, int h, COLORREF c, int alpha, int radius);
void BlendCircle(HDC hdc, int cx, int cy, int r, COLORREF c, int alpha);
void DrawGlyph(HDC hdc, int cx, int cy, int kind, int s, COLORREF col);
void DrawCloseCircle(HDC hdc, int cx, int cy, int r, bool hover);
void DrawIconButton(HDC hdc, int x, int y, int w, int h, bool hot, int kind);
void DrawTextButton(HDC hdc, int x, int y, int w, int h, bool hot, const wchar_t* label);
