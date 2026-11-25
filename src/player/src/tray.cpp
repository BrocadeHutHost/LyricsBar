#include "tray.h"
#include <shellapi.h>

typedef BOOL(WINAPI* PFN_SHELLNOTIFY)(DWORD, PNOTIFYICONDATAW);

static HICON s_icon = nullptr;
static NOTIFYICONDATAW s_nid = {};

static HMODULE LoadShell() { return LoadLibraryW(L"shell32.dll"); }

bool Tray_Add(HWND owner, const wchar_t* tip)
{
    if (g.trayAdded) return true;
    if (!s_icon) s_icon = (HICON)LoadImageW(g.hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    HMODULE sh = LoadShell();
    if (!sh) return false;
    PFN_SHELLNOTIFY fn = (PFN_SHELLNOTIFY)GetProcAddress(sh, "Shell_NotifyIconW");
    if (!fn) { FreeLibrary(sh); return false; }

    memset(&s_nid, 0, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = owner;
    s_nid.uID = 1;
    s_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    s_nid.uCallbackMessage = WM_APP_TRAY;
    s_nid.hIcon = s_icon ? s_icon : LoadIconW(nullptr, IDI_APPLICATION);
    if (tip) wcsncpy(s_nid.szTip, tip, 127);

    BOOL ok = fn(NIM_ADD, &s_nid);
    FreeLibrary(sh);
    g.trayAdded = ok != FALSE;
    LogLine(g.trayAdded ? L"Tray icon added" : L"Tray icon add failed");
    return g.trayAdded;
}

void Tray_UpdateTip(const wchar_t* tip)
{
    if (!g.trayAdded || !tip) return;
    HMODULE sh = LoadShell();
    if (!sh) return;
    PFN_SHELLNOTIFY fn = (PFN_SHELLNOTIFY)GetProcAddress(sh, "Shell_NotifyIconW");
    if (fn)
    {
        wcsncpy(s_nid.szTip, tip, 127);
        s_nid.uFlags = NIF_TIP;
        fn(NIM_MODIFY, &s_nid);
    }
    FreeLibrary(sh);
}

void Tray_Balloon(const wchar_t* title, const wchar_t* text)
{
    if (!g.trayAdded) return;
    HMODULE sh = LoadShell();
    if (!sh) return;
    PFN_SHELLNOTIFY fn = (PFN_SHELLNOTIFY)GetProcAddress(sh, "Shell_NotifyIconW");
    if (fn)
    {
        wcsncpy(s_nid.szInfoTitle, title ? title : L"", 63);
        wcsncpy(s_nid.szInfo, text ? text : L"", 255);
        s_nid.dwInfoFlags = NIIF_NOSOUND;
        s_nid.uFlags = NIF_INFO;
        fn(NIM_MODIFY, &s_nid);
    }
    FreeLibrary(sh);
}

void Tray_Remove()
{
    if (!g.trayAdded) return;
    HMODULE sh = LoadShell();
    if (sh)
    {
        PFN_SHELLNOTIFY fn = (PFN_SHELLNOTIFY)GetProcAddress(sh, "Shell_NotifyIconW");
        if (fn) { s_nid.uFlags = 0; fn(NIM_DELETE, &s_nid); }
        FreeLibrary(sh);
    }
    g.trayAdded = false;
    if (s_icon) { DestroyIcon(s_icon); s_icon = nullptr; }
}
