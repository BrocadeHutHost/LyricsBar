#pragma once
#include "common.h"

bool Tray_Add(HWND owner, const wchar_t* tip);
void Tray_UpdateTip(const wchar_t* tip);
void Tray_Balloon(const wchar_t* title, const wchar_t* text);
void Tray_Remove();
