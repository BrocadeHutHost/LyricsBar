#pragma once
#include "common.h"

void App_OnTrackChanged();
void App_RequestAdvance(bool manual);
void App_RefreshUI();
void App_TogglePlay();
void App_Rescan();
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmd, int);
