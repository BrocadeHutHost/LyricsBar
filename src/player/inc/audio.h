#pragma once
#include "common.h"

void Audio_Init();
void Audio_Shutdown();
bool Audio_Open(const std::wstring& file);
void Audio_Stop(bool resetPosition = true);
void Audio_Toggle();
void Audio_Seek(ULONGLONG ms);
void Audio_SetVolume(int vol);
ULONGLONG Audio_GetPosition();
bool Audio_IsEos();
void Audio_AckEos();
void Audio_PlayIndex(int idx);
void Audio_Advance(bool manual);
const wchar_t* Audio_LoopModeName();
