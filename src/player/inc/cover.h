#pragma once
#include "common.h"

void Cover_LoadForCurrent();
void Cover_Poll();
void Cover_Shutdown();
bool Cover_Ready();

bool Cover_DrawDisc(HDC hdc, int cx, int cy, int radius, double angle);
