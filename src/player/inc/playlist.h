#pragma once
#include "common.h"

void Playlist_Init();
void Playlist_Save();
void Playlist_Scan();
void Playlist_BuildView();
bool Playlist_SwitchTo(int idx, bool autoplay = true);
bool Playlist_AddRoot(const std::wstring& path);
bool Playlist_RemoveRoot(int idx);
void Playlist_ClearFolders();
size_t Playlist_Count();
std::wstring Playlist_RootName(int idx);
int Playlist_RootCount(int idx);
