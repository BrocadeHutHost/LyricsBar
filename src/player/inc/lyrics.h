#pragma once
#include "common.h"

int Lyrics_FindLine(ULONGLONG t);
void Lyrics_LoadForCurrent();
void Lyrics_Poll();
void Lyrics_Shutdown();

std::wstring Lyric_EnsureFetcher();
void Lyric_ReleaseFetcher();
bool Lyric_SpawnFetcher(const std::wstring& args);
