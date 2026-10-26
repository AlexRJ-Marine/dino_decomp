// Dino Island hybrid hook - HYB-1.
//
// This DLL is named DINPUT8.dll and placed in run/. The game imports exactly
// one DInput8 function (DirectInput8Create), which we forward to the real
// system DLL. DllMain installs an inline detour on the retail function
// 0x4663a0 (read 16 floats / 64 bytes from the memory stream), replacing it
// with the C++ implementation below.
//
// The DLL writes dino_hook.log in the process working directory so a headless
// run can prove the replacement was called.

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

HMODULE g_real_dinput8 = nullptr;
DirectInput8CreateFn g_real_create = nullptr;
volatile LONG g_calls_4663a0 = 0;
volatile LONG g_calls_4809a0 = 0;
volatile LONG g_calls_47d460 = 0;
volatile LONG g_calls_47cc90 = 0;

// Engine vector: 16-byte polymorphic wrapper, data triple at +4/+8/+0xc.
template <typename T>
struct Vec {
    void* vptr;
    T* first;
    T* last;
    T* end;
};

using InsertFn = void(__thiscall*)(void* pfirst, void* where, std::uint32_t count,
                                   const void* value);

bool resize_to(Vec<std::uint16_t>& vec, std::uint32_t count, InsertFn insert) {
    const std::size_t current =
        static_cast<std::size_t>(vec.last - vec.first);
    if (count < current) {
        vec.last = vec.first + count;
    } else if (count > current) {
        const std::uint32_t zero = 0;
        insert(&vec.first, vec.last, count - static_cast<std::uint32_t>(current),
               &zero);
    }
    return true;
}

bool resize_to(Vec<std::uint32_t>& vec, std::uint32_t count, InsertFn insert) {
    const std::size_t current =
        static_cast<std::size_t>(vec.last - vec.first);
    if (count < current) {
        vec.last = vec.first + count;
    } else if (count > current) {
        const std::uint32_t zero = 0;
        insert(&vec.first, vec.last, count - static_cast<std::uint32_t>(current),
               &zero);
    }
    return true;
}

std::uintptr_t exe_va(std::uint32_t va) {
    return reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr)) +
           (va - 0x400000);
}

void log_line(const char* text) {
    HANDLE file = CreateFileA("dino_hook.log", FILE_APPEND_DATA, FILE_SHARE_READ,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    const DWORD length = static_cast<DWORD>(std::strlen(text));
    WriteFile(file, text, length, &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

// The retail memory stream exposes read(buffer, elem_size, count) at vtable +8.
struct Stream {
    void** vtable;
};
using StreamReadFn = int(__thiscall*)(Stream*, void*, int, int);

// Replacement for FUN_004663a0. The original prologue is
//   83 EC 0C 53 55  (sub esp,0xc; push ebx; push ebp)
// so a 5-byte relative jump replaces the whole implementation.
bool __thiscall hook_4663a0(void* buffer, Stream* stream) {
    const LONG calls = InterlockedIncrement(&g_calls_4663a0);
    if (calls == 1 || (calls % 1000) == 0) {
        char line[96];
        std::snprintf(line, sizeof(line), "hook_4663a0: calls=%ld", static_cast<long>(calls));
        log_line(line);
    }
    if (stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);
    return read(stream, buffer, 4, 0x10) == 0x10;
}

// Replacement for FUN_004809a0: read 12 bytes (3 floats) for a normal.
bool __thiscall hook_4809a0(void* buffer, Stream* stream) {
    const LONG calls = InterlockedIncrement(&g_calls_4809a0);
    if (calls == 1 || (calls % 1000) == 0) {
        char line[96];
        std::snprintf(line, sizeof(line), "hook_4809a0: calls=%ld", static_cast<long>(calls));
        log_line(line);
    }
    if (stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);
    return read(stream, buffer, 4, 3) == 3;
}

// Replacement for FUN_0047d460: read a u32 count then `count` u16 elements
// into the vector at [ECX+4..0xc] (mesh record +0x14 subobject, member +0x78).
bool __thiscall hook_47d460(Vec<std::uint16_t>* vec, Stream* stream) {
    const LONG calls = InterlockedIncrement(&g_calls_47d460);
    if (calls == 1 || (calls % 1000) == 0) {
        char line[96];
        std::snprintf(line, sizeof(line), "hook_47d460: calls=%ld",
                      static_cast<long>(calls));
        log_line(line);
    }
    if (vec == nullptr || stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);
    std::uint32_t count = 0;
    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    resize_to(*vec, count, reinterpret_cast<InsertFn>(exe_va(0x47d500)));
    for (std::uint32_t i = 0; i < count; ++i) {
        if (read(stream, &vec->first[i], 2, 1) != 1) {
            return false;
        }
    }
    return true;
}

// Replacement for FUN_0047cc90: read a u32 count then `count` u32 child
// indices into one bone's inner vector (the `.skel` child lists).
bool __thiscall hook_47cc90(Vec<std::uint32_t>* vec, Stream* stream) {
    const LONG calls = InterlockedIncrement(&g_calls_47cc90);
    if (calls == 1 || (calls % 1000) == 0) {
        char line[96];
        std::snprintf(line, sizeof(line), "hook_47cc90: calls=%ld",
                      static_cast<long>(calls));
        log_line(line);
    }
    if (vec == nullptr || stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);
    std::uint32_t count = 0;
    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    resize_to(*vec, count, reinterpret_cast<InsertFn>(exe_va(0x47cac0)));
    for (std::uint32_t i = 0; i < count; ++i) {
        if (read(stream, &vec->first[i], 4, 1) != 1) {
            return false;
        }
    }
    return true;
}

bool install_detour(const char* name, std::uintptr_t rva, void* hook,
                    const std::uint8_t (&expected)[5]) {
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleA(nullptr));
    if (base == nullptr) {
        log_line("hook: GetModuleHandle(NULL) failed");
        return false;
    }
    auto* target = base + rva;
    if (std::memcmp(target, expected, 5) != 0) {
        char line[128];
        std::snprintf(line, sizeof(line), "hook: %s prologue mismatch", name);
        log_line(line);
        return false;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old_protect)) {
        log_line("hook: VirtualProtect failed");
        return false;
    }
    const auto from = reinterpret_cast<std::intptr_t>(target) + 5;
    const auto to = reinterpret_cast<std::intptr_t>(hook);
    const auto relative = to - from;
    if (relative < INT32_MIN || relative > INT32_MAX) {
        VirtualProtect(target, 5, old_protect, &old_protect);
        log_line("hook: relative jump out of range");
        return false;
    }
    target[0] = 0xE9;
    const auto rel32 = static_cast<std::int32_t>(relative);
    std::memcpy(target + 1, &rel32, sizeof(rel32));
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    VirtualProtect(target, 5, old_protect, &old_protect);
    char line[128];
    std::snprintf(line, sizeof(line), "hook: %s patched", name);
    log_line(line);
    return true;
}

// The vector readers start with an 8-byte prologue
// (push ecx; push ebx; push esi; push edi; mov edi,[esp+0x14]), so the
// 5-byte jump splits an instruction. Patch 8 bytes: jmp rel32 + 3 NOP.
bool install_detour8(const char* name, std::uintptr_t rva, void* hook,
                     const std::uint8_t (&expected)[8]) {
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleA(nullptr));
    if (base == nullptr) {
        log_line("hook: GetModuleHandle(NULL) failed");
        return false;
    }
    auto* target = base + rva;
    if (std::memcmp(target, expected, 8) != 0) {
        char line[128];
        std::snprintf(line, sizeof(line), "hook: %s prologue mismatch", name);
        log_line(line);
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(target, 8, PAGE_EXECUTE_READWRITE, &old_protect)) {
        log_line("hook: VirtualProtect failed");
        return false;
    }
    const auto from = reinterpret_cast<std::intptr_t>(target) + 5;
    const auto to = reinterpret_cast<std::intptr_t>(hook);
    const auto relative = to - from;
    if (relative < INT32_MIN || relative > INT32_MAX) {
        VirtualProtect(target, 8, old_protect, &old_protect);
        log_line("hook: relative jump out of range");
        return false;
    }
    target[0] = 0xE9;
    const auto rel32 = static_cast<std::int32_t>(relative);
    std::memcpy(target + 1, &rel32, sizeof(rel32));
    target[5] = 0x90;
    target[6] = 0x90;
    target[7] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), target, 8);
    VirtualProtect(target, 8, old_protect, &old_protect);
    char line[128];
    std::snprintf(line, sizeof(line), "hook: %s patched", name);
    log_line(line);
    return true;
}

bool install_hooks() {
    // 0x4663a0: 64-byte matrix, prologue 8B C1 6A 10 8B.
    static constexpr std::uint8_t kPrologue_4663a0[5] = {0x8B, 0xC1, 0x6A, 0x10, 0x8B};
    // 0x4809a0: 12-byte normal, prologue 8B C1 6A 03 8B.
    static constexpr std::uint8_t kPrologue_4809a0[5] = {0x8B, 0xC1, 0x6A, 0x03, 0x8B};
    // 0x47d460 / 0x47cc90: 51 53 56 57 8B 7C 24 14.
    static constexpr std::uint8_t kPrologueVector[8] = {0x51, 0x53, 0x56,
                                                        0x57, 0x8B, 0x7C,
                                                        0x24, 0x14};
    const bool a = install_detour("FUN_004663a0", 0x4663A0 - 0x400000,
                                  reinterpret_cast<void*>(&hook_4663a0),
                                  kPrologue_4663a0);
    const bool b = install_detour("FUN_004809a0", 0x4809A0 - 0x400000,
                                  reinterpret_cast<void*>(&hook_4809a0),
                                  kPrologue_4809a0);
    // The vector readers were verified during a Cpg04 campaign load
    // (2000+ calls each, park renders); install them by default.
    const bool c = install_detour8("FUN_0047d460", 0x47D460 - 0x400000,
                                   reinterpret_cast<void*>(&hook_47d460),
                                   kPrologueVector);
    const bool d = install_detour8("FUN_0047cc90", 0x47CC90 - 0x400000,
                                   reinterpret_cast<void*>(&hook_47cc90),
                                   kPrologueVector);
    return a && b && c && d;
}

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    if (g_real_create == nullptr) {
        return E_FAIL;
    }
    return g_real_create(hinst, version, riid, out, outer);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        log_line("DINPUT8 proxy attached");

        char path[MAX_PATH] = {};
        const UINT length = GetSystemDirectoryA(path, MAX_PATH);
        if (length > 0 && length + 13 < MAX_PATH) {
            std::strcat(path, "\\dinput8.dll");
            g_real_dinput8 = LoadLibraryA(path);
        }
        if (g_real_dinput8 != nullptr) {
            g_real_create = reinterpret_cast<DirectInput8CreateFn>(
                GetProcAddress(g_real_dinput8, "DirectInput8Create"));
        }
        log_line(g_real_create != nullptr ? "DINPUT8 real DLL loaded"
                                          : "DINPUT8 real DLL load FAILED");
        install_hooks();
    } else if (reason == DLL_PROCESS_DETACH) {
        char line[192];
        std::snprintf(line, sizeof(line),
                      "hook_4663a0: total calls=%ld, hook_4809a0: total calls=%ld, "
                      "hook_47d460: total calls=%ld, hook_47cc90: total calls=%ld",
                      static_cast<long>(g_calls_4663a0),
                      static_cast<long>(g_calls_4809a0),
                      static_cast<long>(g_calls_47d460),
                      static_cast<long>(g_calls_47cc90));
        log_line(line);
        if (g_real_dinput8 != nullptr) {
            FreeLibrary(g_real_dinput8);
            g_real_dinput8 = nullptr;
        }
    }
    return TRUE;
}
