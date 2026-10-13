/*
 * GTA2.0 test host: DirectInput 8 proxy for the ORIGINAL game (gta2.exe 9.6 imports DINPUT8.dll).
 *
 * Placed next to gta2.exe for a capture run only. It loads the system dinput8.dll, forwards every
 * call to it and, for the system keyboard:
 *   - logs what the game does (cooperative level, data format, Acquire / GetDeviceState /
 *     GetDeviceData results) to gta20-dinput.log, so a run shows what input the game sees;
 *   - merges scripted key presses from gta20-input.txt into the state and buffered data the game
 *     reads. This does not depend on window focus, the input desktop or SendInput.
 *
 * Script format (one event per line, times in ms since this DLL was loaded):
 *     <ms> down <scancode-hex>
 *     <ms> up   <scancode-hex>
 * Scan codes are DirectInput DIK_* values (set 1; extended keys such as arrows use 0xC8 etc.).
 *
 * Our own code, written against the public DirectInput 8 API (dinput.h). Build (32-bit):
 *     zig cc -target x86-windows-gnu -shared -O2 -o dinput8.dll dinput8_proxy.c dinput8_proxy.def
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#include <windows.h>
#include <dinput.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* GUID_SysKeyboard, defined here so no import library is needed. */
static const GUID kSysKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

#define MAX_EVENTS 512
#define LOG_CALLS 40 /* detailed log lines per call type, then only counters */

typedef struct {
    DWORD ms;
    BYTE down;
    BYTE dik;
} ScriptEvent;

static HMODULE g_real;
static FILE *g_log;
static DWORD g_t0;
static ScriptEvent g_events[MAX_EVENTS];
static int g_event_count;
static int g_next_buffered;         /* next scripted event not yet delivered by GetDeviceData */
static DWORD g_sequence = 0x10000000;
static CRITICAL_SECTION g_lock;
static long g_counts[8];            /* acquire, state, state_err, data, data_err, data_items, injected, coop */

static DWORD now_ms(void) { return GetTickCount() - g_t0; }

static void logf_(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(g_log, "%7lu ", (unsigned long)now_ms());
    vfprintf(g_log, fmt, ap);
    fputc('\n', g_log);
    fflush(g_log);
    va_end(ap);
}

static void load_script(void) {
    FILE *f = fopen("gta20-input.txt", "r");
    if (!f) { logf_("no gta20-input.txt: pass-through only"); return; }
    char line[128];
    while (fgets(line, sizeof line, f) && g_event_count < MAX_EVENTS) {
        unsigned long ms; char what[8]; unsigned int dik;
        if (sscanf(line, "%lu %7s %x", &ms, what, &dik) == 3 && dik < 256) {
            g_events[g_event_count].ms = (DWORD)ms;
            g_events[g_event_count].down = (BYTE)(strcmp(what, "down") == 0);
            g_events[g_event_count].dik = (BYTE)dik;
            g_event_count++;
        }
    }
    fclose(f);
    logf_("script: %d events", g_event_count);
}

/* ------------------------------------------------------------------ wrappers */
typedef struct Wrap {
    void **vtbl;   /* must be first: COM pointer */
    void *real;    /* the real interface (forward target) */
    LONG ref;
    int keyboard;
} Wrap;

/* Generic forwarder: replace `this` (first stdcall stack argument) with the real interface and
 * jump to the same vtable slot of the real object. Used for every method we do not override. */
#define FWD(n, off) \
    __attribute__((naked)) static void fwd_##n(void) { \
        __asm__ volatile( \
            "movl 4(%esp), %eax\n\t" \
            "movl 4(%eax), %eax\n\t" \
            "movl %eax, 4(%esp)\n\t" \
            "movl (%eax), %eax\n\t" \
            "jmp *" #off "(%eax)\n\t"); \
    }
FWD(0, 0) FWD(1, 4) FWD(2, 8) FWD(3, 12) FWD(4, 16) FWD(5, 20) FWD(6, 24) FWD(7, 28) FWD(8, 32)
FWD(9, 36) FWD(10, 40) FWD(11, 44) FWD(12, 48) FWD(13, 52) FWD(14, 56) FWD(15, 60) FWD(16, 64)
FWD(17, 68) FWD(18, 72) FWD(19, 76) FWD(20, 80) FWD(21, 84) FWD(22, 88) FWD(23, 92) FWD(24, 96)
FWD(25, 100) FWD(26, 104) FWD(27, 108) FWD(28, 112) FWD(29, 116) FWD(30, 120) FWD(31, 124)

static void *g_fwd[32] = {
    fwd_0, fwd_1, fwd_2, fwd_3, fwd_4, fwd_5, fwd_6, fwd_7, fwd_8, fwd_9, fwd_10, fwd_11, fwd_12,
    fwd_13, fwd_14, fwd_15, fwd_16, fwd_17, fwd_18, fwd_19, fwd_20, fwd_21, fwd_22, fwd_23,
    fwd_24, fwd_25, fwd_26, fwd_27, fwd_28, fwd_29, fwd_30, fwd_31};

#define REAL_DEV(w) ((IDirectInputDevice8A *)(w)->real)

static ULONG WINAPI wrap_AddRef(Wrap *w) { return (ULONG)InterlockedIncrement(&w->ref); }
static ULONG WINAPI wrap_Release(Wrap *w) {
    LONG r = InterlockedDecrement(&w->ref);
    IUnknown *real = (IUnknown *)w->real;
    real->lpVtbl->Release(real);
    return (ULONG)(r < 0 ? 0 : r); /* the small wrapper itself is intentionally never freed */
}

/* --- keyboard device overrides --- */
static HRESULT WINAPI dev_SetCooperativeLevel(Wrap *w, HWND hwnd, DWORD flags) {
    HRESULT hr = REAL_DEV(w)->lpVtbl->SetCooperativeLevel(REAL_DEV(w), hwnd, flags);
    if (w->keyboard) logf_("keyboard SetCooperativeLevel hwnd=%p flags=0x%lx -> 0x%lx", (void *)hwnd, flags, hr);
    return hr;
}

static HRESULT WINAPI dev_SetDataFormat(Wrap *w, LPCDIDATAFORMAT fmt) {
    HRESULT hr = REAL_DEV(w)->lpVtbl->SetDataFormat(REAL_DEV(w), fmt);
    if (w->keyboard) logf_("keyboard SetDataFormat dwDataSize=%lu -> 0x%lx", fmt ? fmt->dwDataSize : 0, hr);
    return hr;
}

static HRESULT WINAPI dev_SetProperty(Wrap *w, REFGUID prop, LPCDIPROPHEADER hdr) {
    HRESULT hr = REAL_DEV(w)->lpVtbl->SetProperty(REAL_DEV(w), prop, hdr);
    if (w->keyboard) {
        DWORD value = (hdr && hdr->dwSize >= sizeof(DIPROPDWORD)) ? ((const DIPROPDWORD *)hdr)->dwData : 0;
        logf_("keyboard SetProperty %p value=%lu -> 0x%lx", (const void *)prop, value, hr);
    }
    return hr;
}

static HRESULT WINAPI dev_Acquire(Wrap *w) {
    HRESULT hr = REAL_DEV(w)->lpVtbl->Acquire(REAL_DEV(w));
    if (w->keyboard) {
        long n = InterlockedIncrement(&g_counts[0]);
        if (n <= LOG_CALLS || (hr != DI_OK && hr != S_FALSE && n % 100 == 0))
            logf_("keyboard Acquire #%ld -> 0x%lx", n, hr);
        /* With a script the game must keep reading even if the real device cannot be acquired. */
        if (g_event_count > 0 && FAILED(hr)) return DI_OK;
    }
    return hr;
}

static void scripted_state(BYTE *state, DWORD size) {
    DWORD t = now_ms();
    for (int i = 0; i < g_event_count; i++) {
        if (g_events[i].ms > t) break;
        if (g_events[i].dik < size) state[g_events[i].dik] = g_events[i].down ? 0x80 : 0x00;
    }
}

static HRESULT WINAPI dev_GetDeviceState(Wrap *w, DWORD size, LPVOID data) {
    HRESULT hr = REAL_DEV(w)->lpVtbl->GetDeviceState(REAL_DEV(w), size, data);
    if (!w->keyboard) return hr;
    long n = InterlockedIncrement(&g_counts[1]);
    if (FAILED(hr)) {
        long e = InterlockedIncrement(&g_counts[2]);
        if (e <= LOG_CALLS || e % 500 == 0) logf_("keyboard GetDeviceState #%ld -> 0x%lx (errors %ld)", n, hr, e);
        if (g_event_count == 0) return hr;
        memset(data, 0, size);
    } else if (n <= 3 || n % 1000 == 0) {
        int down = 0;
        for (DWORD i = 0; i < size; i++) down += (((BYTE *)data)[i] & 0x80) != 0;
        logf_("keyboard GetDeviceState #%ld ok, real keys down=%d", n, down);
    }
    if (g_event_count > 0) {
        BYTE before[256] = {0};
        DWORD m = size < 256 ? size : 256;
        memcpy(before, data, m);
        scripted_state((BYTE *)data, size);
        if (memcmp(before, data, m) != 0) {
            long k = InterlockedIncrement(&g_counts[6]);
            if (k <= LOG_CALLS * 4) logf_("keyboard GetDeviceState #%ld: scripted keys applied", n);
        }
    }
    return DI_OK;
}

static HRESULT WINAPI dev_GetDeviceData(Wrap *w, DWORD cb, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inout, DWORD flags) {
    DWORD capacity = inout ? *inout : 0;
    HRESULT hr = REAL_DEV(w)->lpVtbl->GetDeviceData(REAL_DEV(w), cb, rgdod, inout, flags);
    if (!w->keyboard) return hr;
    long n = InterlockedIncrement(&g_counts[3]);
    DWORD got = (SUCCEEDED(hr) && inout) ? *inout : 0;
    if (FAILED(hr)) {
        long e = InterlockedIncrement(&g_counts[4]);
        if (e <= LOG_CALLS || e % 500 == 0) logf_("keyboard GetDeviceData #%ld flags=0x%lx -> 0x%lx (errors %ld)", n, flags, hr, e);
        if (g_event_count == 0) return hr;
        got = 0;
    } else if (got > 0) {
        long d = InterlockedExchangeAdd(&g_counts[5], (long)got) + (long)got;
        if (d <= LOG_CALLS) {
            for (DWORD i = 0; i < got && rgdod; i++) {
                DIDEVICEOBJECTDATA *o = (DIDEVICEOBJECTDATA *)((BYTE *)rgdod + i * cb);
                logf_("keyboard GetDeviceData #%ld real item ofs=0x%lx data=0x%lx", n, o->dwOfs, o->dwData);
            }
        }
    } else if (n <= 3 || n % 2000 == 0) {
        logf_("keyboard GetDeviceData #%ld ok, no real items (flags=0x%lx)", n, flags);
    }
    if (g_event_count > 0 && rgdod && inout && cb >= 16) {
        EnterCriticalSection(&g_lock);
        DWORD t = now_ms();
        int idx = g_next_buffered;
        while (idx < g_event_count && g_events[idx].ms <= t && got < capacity) {
            DIDEVICEOBJECTDATA *o = (DIDEVICEOBJECTDATA *)((BYTE *)rgdod + got * cb);
            memset(o, 0, cb);
            o->dwOfs = g_events[idx].dik;
            o->dwData = g_events[idx].down ? 0x80 : 0x00;
            o->dwTimeStamp = GetTickCount();
            o->dwSequence = g_sequence++;
            logf_("keyboard GetDeviceData #%ld: injected dik=0x%02x %s", n, g_events[idx].dik,
                  g_events[idx].down ? "down" : "up");
            got++;
            idx++;
        }
        if (!(flags & DIGDD_PEEK)) g_next_buffered = idx;
        LeaveCriticalSection(&g_lock);
        *inout = got;
        return DI_OK;
    }
    return hr;
}

static void *make_device_vtbl(void) {
    void **v = (void **)HeapAlloc(GetProcessHeap(), 0, 32 * sizeof(void *));
    memcpy(v, g_fwd, sizeof g_fwd);
    v[1] = (void *)wrap_AddRef;
    v[2] = (void *)wrap_Release;
    v[6] = (void *)dev_SetProperty;      /* IDirectInputDevice: GetCapabilities(3) EnumObjects(4) GetProperty(5) */
    v[7] = (void *)dev_Acquire;
    v[9] = (void *)dev_GetDeviceState;
    v[10] = (void *)dev_GetDeviceData;
    v[11] = (void *)dev_SetDataFormat;
    v[13] = (void *)dev_SetCooperativeLevel;
    return v;
}

/* --- IDirectInput8A overrides --- */
static void **g_dev_vtbl;

static HRESULT WINAPI di_CreateDevice(Wrap *w, REFGUID guid, LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN outer) {
    IDirectInput8A *real = (IDirectInput8A *)w->real;
    HRESULT hr = real->lpVtbl->CreateDevice(real, guid, out, outer);
    int keyboard = guid && IsEqualGUID(guid, &kSysKeyboard);
    logf_("CreateDevice %s -> 0x%lx", keyboard ? "GUID_SysKeyboard" : "(other)", hr);
    if (SUCCEEDED(hr) && out && *out) {
        Wrap *d = (Wrap *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Wrap));
        d->vtbl = g_dev_vtbl;
        d->real = *out;
        d->ref = 1;
        d->keyboard = keyboard;
        *out = (LPDIRECTINPUTDEVICE8A)d;
    }
    return hr;
}

typedef HRESULT(WINAPI *DI8Create)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD version, REFIID iid, LPVOID *out,
                                                        LPUNKNOWN outer) {
    if (!g_real) {
        char path[MAX_PATH];
        UINT len = GetSystemDirectoryA(path, MAX_PATH); /* SysWOW64 for this 32-bit process */
        if (len == 0 || len > MAX_PATH - 16) return E_FAIL;
        strcat(path, "\\dinput8.dll");
        g_real = LoadLibraryA(path);
        logf_("loaded %s -> %p", path, (void *)g_real);
        if (!g_real) return E_FAIL;
    }
    DI8Create create = (DI8Create)(void *)GetProcAddress(g_real, "DirectInput8Create");
    if (!create) return E_FAIL;
    HRESULT hr = create(inst, version, iid, out, outer);
    logf_("DirectInput8Create version=0x%lx -> 0x%lx", version, hr);
    if (SUCCEEDED(hr) && out && *out) {
        void **v = (void **)HeapAlloc(GetProcessHeap(), 0, 11 * sizeof(void *));
        memcpy(v, g_fwd, 11 * sizeof(void *));
        v[1] = (void *)wrap_AddRef;
        v[2] = (void *)wrap_Release;
        v[3] = (void *)di_CreateDevice;
        Wrap *w = (Wrap *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Wrap));
        w->vtbl = v;
        w->real = *out;
        w->ref = 1;
        *out = w;
    }
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)inst;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_t0 = GetTickCount();
        InitializeCriticalSection(&g_lock);
        g_log = fopen("gta20-dinput.log", "w");
        logf_("gta20 dinput8 proxy loaded");
        load_script();
        g_dev_vtbl = (void **)make_device_vtbl();
    } else if (reason == DLL_PROCESS_DETACH && g_log) {
        logf_("counters: acquire=%ld state=%ld state_err=%ld data=%ld data_err=%ld real_items=%ld injected_state=%ld",
              g_counts[0], g_counts[1], g_counts[2], g_counts[3], g_counts[4], g_counts[5], g_counts[6]);
        fclose(g_log);
        g_log = NULL;
    }
    return TRUE;
}
