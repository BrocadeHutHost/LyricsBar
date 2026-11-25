#include "audio.h"
#include "app.h"
#include "lyrics.h"

static WAVEHDR* FreeHeader()
{
    for (auto& h : g.headers) if (h.dwFlags & WHDR_DONE) return &h;
    return nullptr;
}

static DWORD WINAPI AudioThread(LPVOID)
{
    while (!g.threadStop && g.reader)
    {
        if (g.paused || !g.playing) { Sleep(40); continue; }
        WAVEHDR* hdr = nullptr;
        EnterCriticalSection(&g.audioCs);
        hdr = FreeHeader();
        LeaveCriticalSection(&g.audioCs);
        if (!hdr) { Sleep(8); continue; }

        IMFSample* sample = nullptr;
        DWORD flags = 0;
        LONGLONG pts = 0;
        HRESULT hr = g.reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &pts, &sample);
        if (FAILED(hr)) { Sleep(30); continue; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) g.eos = true;

        if (sample)
        {
            IMFMediaBuffer* buf = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf)) && buf)
            {
                BYTE* data = nullptr; DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&data, nullptr, &len)))
                {
                    if (len > hdr->dwBufferLength) len = hdr->dwBufferLength;
                    memcpy(hdr->lpData, data, len);
                    buf->Unlock();
                }
                hdr->dwBufferLength = len;
                buf->Release();
                if (len > 0)
                {
                    EnterCriticalSection(&g.audioCs);
                    hdr->dwFlags &= ~WHDR_DONE;
                    waveOutWrite(g.waveOut, hdr, sizeof(WAVEHDR));
                    LeaveCriticalSection(&g.audioCs);
                }
            }
            sample->Release();
        }
        else if (flags & MF_SOURCE_READERF_ENDOFSTREAM) g.eos = true;

        if (g.eos)
        {
            bool allDone = true;
            EnterCriticalSection(&g.audioCs);
            for (auto& h : g.headers) if (!(h.dwFlags & WHDR_DONE)) allDone = false;
            LeaveCriticalSection(&g.audioCs);
            if (allDone && !g.eosNotified)
            {
                g.eosNotified = true;
                LogLine(L"EOS detected");
                if (g.hwndMain) PostMessageW(g.hwndMain, WM_APP_NEXT, 0, 0);
            }
            Sleep(20);
        }
    }
    return 0;
}

void Audio_Init()
{
    InitializeCriticalSection(&g.audioCs);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

void Audio_Shutdown()
{
    Audio_Stop();
    DeleteCriticalSection(&g.audioCs);
    MFShutdown();
    CoUninitialize();
}

void Audio_Stop(bool resetPosition)
{
    g.threadStop = true;
    if (g.audioThread) { WaitForSingleObject(g.audioThread, 1500); g.audioThread = nullptr; }
    if (g.waveOut)
    {
        waveOutReset(g.waveOut);
        for (auto& h : g.headers)
        {
            if (h.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(g.waveOut, &h, sizeof(WAVEHDR));
            free(h.lpData);
        }
        g.headers.clear();
        waveOutClose(g.waveOut);
        g.waveOut = nullptr;
    }
    if (g.reader) { g.reader->Release(); g.reader = nullptr; }
    if (resetPosition) g.posMs = 0;
    g.eos = false; g.eosNotified = false;
    g.playing = false;
}

bool Audio_Open(const std::wstring& file)
{
    Audio_Stop();

    IMFAttributes* attrs = nullptr;
    MFCreateAttributes(&attrs, 1);
    if (attrs) attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
    HRESULT hr = MFCreateSourceReaderFromURL(file.c_str(), attrs, &g.reader);
    if (attrs) attrs->Release();
    if (FAILED(hr) || !g.reader) return false;

    g.reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    g.reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    IMFMediaType* native = nullptr;
    UINT32 channels = 2, sampleRate = 44100;
    if (SUCCEEDED(g.reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &native)) && native)
    {
        native->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        native->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
        native->Release();
    }
    IMFMediaType* outType = nullptr;
    MFCreateMediaType(&outType);
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    outType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    outType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    outType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    outType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    hr = g.reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, outType);
    outType->Release();
    if (FAILED(hr)) { Audio_Stop(); return false; }

    PROPVARIANT dur; PropVariantInit(&dur);
    if (SUCCEEDED(g.reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &dur)))
    {
        g.durationMs = dur.uhVal.QuadPart / 10000;
        PropVariantClear(&dur);
    }
    else g.durationMs = 0;

    memset(&g.wfx, 0, sizeof(g.wfx));
    g.wfx.wFormatTag = WAVE_FORMAT_PCM;
    g.wfx.nChannels = (WORD)channels;
    g.wfx.nSamplesPerSec = sampleRate;
    g.wfx.wBitsPerSample = 16;
    g.wfx.nBlockAlign = (WORD)(channels * 2);
    g.wfx.nAvgBytesPerSec = sampleRate * g.wfx.nBlockAlign;
    g.bytesPerSec = g.wfx.nAvgBytesPerSec;
    g.seekBytes = 0;

    if (waveOutOpen(&g.waveOut, WAVE_MAPPER, &g.wfx, (DWORD_PTR)g.hwndMain, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR)
    { Audio_Stop(); return false; }

    DWORD bufBytes = (sampleRate / 5) * g.wfx.nBlockAlign;
    g.headers.resize(6);
    for (auto& h : g.headers)
    {
        memset(&h, 0, sizeof(h));
        h.lpData = (LPSTR)malloc(bufBytes);
        h.dwBufferLength = bufBytes;
        waveOutPrepareHeader(g.waveOut, &h, sizeof(WAVEHDR));
        h.dwFlags |= WHDR_DONE;
    }
    g.threadStop = false;
    g.playing = true;
    g.paused = false;
    g.eos = false; g.eosNotified = false;
    g.lastPosTick = GetTickCount();
    g.lastPosMs = 0;
    g.stuckCount = 0;
    Audio_SetVolume(g.volume);
    g.audioThread = CreateThread(nullptr, 0, AudioThread, nullptr, 0, nullptr);
    return true;
}

void Audio_Toggle()
{
    if (!g.waveOut)
    {
        if (!g.playlist.empty()) Audio_PlayIndex(g.track < 0 ? 0 : g.track);
        return;
    }
    if (g.paused) { waveOutRestart(g.waveOut); g.paused = false; }
    else { waveOutPause(g.waveOut); g.paused = true; }
    App_RefreshUI();
}

void Audio_Seek(ULONGLONG ms)
{
    if (!g.reader || !g.waveOut || g.durationMs == 0) return;
    ms = std::min(ms, g.durationMs);
    PROPVARIANT var; PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = (LONGLONG)(ms * 10000);
    g.reader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);
    waveOutReset(g.waveOut);
    for (auto& h : g.headers) h.dwFlags |= WHDR_DONE;
    g.seekBytes = ms * g.bytesPerSec / 1000;
    g.posMs = ms;
    g.eos = false; g.eosNotified = false;
}

void Audio_SetVolume(int vol)
{
    g.volume = std::max(0, std::min(100, vol));
    if (g.waveOut)
    {
        DWORD v = (DWORD)(g.volume * 655);
        waveOutSetVolume(g.waveOut, v | (v << 16));
    }
}

ULONGLONG Audio_GetPosition()
{
    if (g.waveOut && !g.paused)
    {
        MMTIME mm; mm.wType = TIME_BYTES;
        if (waveOutGetPosition(g.waveOut, &mm, sizeof(mm)) == MMSYSERR_NOERROR)
            g.posMs = (g.seekBytes + mm.u.cb) * 1000 / std::max((ULONGLONG)1, g.bytesPerSec);
    }
    return g.posMs;
}

bool Audio_IsEos() { return g.eosNotified; }
void Audio_AckEos() { g.eosNotified = false; }

void Audio_PlayIndex(int idx)
{
    if (g.playlist.empty()) return;
    if (idx < 0) idx = (int)g.playlist.size() - 1;
    if (idx >= (int)g.playlist.size()) idx = 0;
    g.track = idx;
    g.currentFile = g.playlist[idx];
    ParseSongName(BaseNameNoExt(g.currentFile), g.title, g.artist);
    if (!Audio_Open(g.currentFile))
    {
        g.failedOpens++;
        LogLine((L"Open failed: " + g.currentFile).c_str());
        if (g.failedOpens < (int)g.playlist.size())
        {
            PostMessageW(g.hwndMain, WM_APP_NEXT, 1, 0);
        }
        else
        {
            g.failedOpens = 0;
            Audio_Stop();
        }
        return;
    }
    g.failedOpens = 0;
    App_OnTrackChanged();
}

const wchar_t* Audio_LoopModeName()
{
    switch (g.loopMode)
    {
    case LOOP_SINGLE: return L"单曲";
    case LOOP_SHUFFLE: return L"随机";
    case LOOP_SEQUENCE: return L"顺序";
    default: return L"列表";
    }
}

void Audio_Advance(bool manual)
{
    if (g.playlist.empty()) return;
    if (!manual && g.loopMode == LOOP_SINGLE)
    {
        Audio_Seek(0);
        g.eos = false;
        g.eosNotified = false;
        g.playing = true;
        if (g.waveOut && g.paused) { waveOutRestart(g.waveOut); g.paused = false; }
        return;
    }
    int next = g.track + 1;
    if (g.loopMode == LOOP_SHUFFLE && g.playlist.size() > 1)
    {
        next = (g.track + 1 + (int)(GetTickCount() % (g.playlist.size() - 1))) % (int)g.playlist.size();
        if (next == g.track) next = (next + 1) % (int)g.playlist.size();
    }
    if (next >= (int)g.playlist.size())
    {
        if (g.loopMode == LOOP_SEQUENCE && !manual)
        {
            Audio_Stop();
            g.posMs = g.durationMs;
            App_RefreshUI();
            return;
        }
        next = 0;
    }
    Audio_PlayIndex(next);
}
