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
volatile LONG g_calls_47d880 = 0;
volatile LONG g_calls_4839e0 = 0;

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

// `.skin` per-bone record (0x6C) and its outer vector.
struct SkinBone {
    char* name;          // mcString: buf/cap/len
    std::uint32_t cap;
    std::uint32_t len;
    void* u32_vptr;      // 0x6256cc
    std::uint32_t* idx_first;
    std::uint32_t* idx_last;
    std::uint32_t* idx_end;
    void* f32_vptr;      // 0x625eac
    float* w_first;
    float* w_last;
    float* w_end;
    float matrix[16];
};

struct SkinBoneVec {
    void* vptr;          // 0x625e84
    SkinBone* first;
    SkinBone* last;
    SkinBone* end;
};

// `.anim` bone record (0x2C).
struct AnimBone {
    char* name;
    std::uint32_t cap;
    std::uint32_t len;
    void* times_vptr;    // 0x625f34
    std::uint64_t* t_first;
    std::uint64_t* t_last;
    std::uint64_t* t_end;
    void* mats_vptr;     // 0x625f30
    float* m_first;
    float* m_last;
    float* m_end;
};

using McStringReserveFn = bool(__thiscall*)(void* str, std::uint32_t size_incl_nul);
using StrlenFn = int(__cdecl*)(const char* text);
using ResizeU32Fn = void(__thiscall*)(void* first_ptr, std::uint32_t count,
                                     const std::uint32_t* value);
using ResizeF32Fn = void(__thiscall*)(void* first_ptr, std::uint32_t count,
                                     const float* value);
using InsertBoneFn = void(__thiscall*)(void* first_ptr, void* where,
                                      std::uint32_t count,
                                      const SkinBone* value);
using EraseBoneFn = void*(__thiscall*)(void* first_ptr, void* new_first,
                                      void* old_last);
using InsertU64Fn = void(__thiscall*)(void* first_ptr, void* where,
                                     std::uint32_t count,
                                     const std::uint64_t* value);
using EraseU64Fn = void*(__thiscall*)(void* first_ptr, void* new_last,
                                     void* old_last);
using InsertMatFn = void(__thiscall*)(void* first_ptr, void* where,
                                     std::uint32_t count, const void* value);

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
    if ((calls % 10000) == 0) {
        char line[192];
        std::snprintf(line, sizeof(line),
                      "counters: skin=%ld anim=%ld vec16=%ld vec32=%ld",
                      static_cast<long>(g_calls_47d880),
                      static_cast<long>(g_calls_4839e0),
                      static_cast<long>(g_calls_47d460),
                      static_cast<long>(g_calls_47cc90));
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

// Replacement for FUN_0047d880: read the `.skin` per-bone table (count,
// name, u32 indices, f32 weights, 64-byte inverse-bind matrix) into the
// 0x6C-record vector at [ECX+4].
bool __thiscall hook_47d880(SkinBoneVec* vec, Stream* stream) {
    InterlockedIncrement(&g_calls_47d880);
    // No per-call logging: this reader can run during early asset loading,
    // where CreateFileA under the loader lock can deadlock. The total is
    // written at DLL detach.
    if (vec == nullptr || stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);
    std::uint32_t count = 0;
    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    const std::size_t current =
        vec->first != nullptr ? static_cast<std::size_t>(vec->last - vec->first) : 0;
    if (count < current) {
        reinterpret_cast<EraseBoneFn>(exe_va(0x47c210))(
            &vec->first, vec->first + count, vec->last);
    } else if (count > current) {
        SkinBone zero{};
        zero.u32_vptr = reinterpret_cast<void*>(exe_va(0x6256cc));
        zero.f32_vptr = reinterpret_cast<void*>(exe_va(0x625eac));
        reinterpret_cast<InsertBoneFn>(exe_va(0x47f8c0))(
            &vec->first, vec->last,
            count - static_cast<std::uint32_t>(current), &zero);
    }

    for (std::uint32_t i = 0; i < count; ++i) {
        SkinBone* bone = &vec->first[i];
        std::uint32_t name_len = 0;
        if (read(stream, &name_len, 4, 1) == 1) {
            reinterpret_cast<McStringReserveFn>(exe_va(0x4058b0))(bone,
                                                                  name_len + 1);
            if (read(stream, bone->name, 1, name_len) ==
                static_cast<int>(name_len)) {
                bone->len = static_cast<std::uint32_t>(
                    reinterpret_cast<StrlenFn>(exe_va(0x4056c0))(bone->name));
            }
        }
        std::uint32_t index_count = 0;
        if (read(stream, &index_count, 4, 1) == 1) {
            const std::uint32_t zero = 0;
            reinterpret_cast<ResizeU32Fn>(exe_va(0x47a7c0))(
                &bone->idx_first, index_count, &zero);
            for (std::uint32_t k = 0; k < index_count; ++k) {
                if (read(stream, &bone->idx_first[k], 4, 1) != 1) {
                    break;
                }
            }
        }
        std::uint32_t weight_count = 0;
        if (read(stream, &weight_count, 4, 1) == 1) {
            const float zero = 0.0f;
            reinterpret_cast<ResizeF32Fn>(exe_va(0x47daa0))(
                &bone->w_first, weight_count, &zero);
            for (std::uint32_t k = 0; k < weight_count; ++k) {
                if (read(stream, &bone->w_first[k], 4, 1) != 1) {
                    break;
                }
            }
        }
        if (!hook_4663a0(bone->matrix, stream)) {
            return false;
        }
    }
    return true;
}

// Replacement for FUN_004839e0: read one `.anim` bone (name, u64 key times,
// matrix count, 64-byte matrices) into the 0x2C record at ECX.
bool __thiscall hook_4839e0(AnimBone* bone, Stream* stream) {
    InterlockedIncrement(&g_calls_4839e0);
    // No per-call logging (early asset loading; see hook_47d880).
    if (bone == nullptr || stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);
    std::uint32_t name_len = 0;
    if (read(stream, &name_len, 4, 1) == 1) {
        reinterpret_cast<McStringReserveFn>(exe_va(0x4058b0))(bone,
                                                              name_len + 1);
        if (read(stream, bone->name, 1, name_len) ==
            static_cast<int>(name_len)) {
            bone->len = static_cast<std::uint32_t>(
                reinterpret_cast<StrlenFn>(exe_va(0x4056c0))(bone->name));
        }
    }

    std::uint32_t key_count = 0;
    if (read(stream, &key_count, 4, 1) == 1) {
        const std::size_t current =
            bone->t_first != nullptr
                ? static_cast<std::size_t>(bone->t_last - bone->t_first)
                : 0;
        if (key_count < current) {
            reinterpret_cast<EraseU64Fn>(exe_va(0x484d30))(
                &bone->t_first, bone->t_first + key_count, bone->t_last);
        } else if (key_count > current) {
            const std::uint64_t zero = 0;
            reinterpret_cast<InsertU64Fn>(exe_va(0x486a60))(
                &bone->t_first, bone->t_last,
                key_count - static_cast<std::uint32_t>(current), &zero);
        }
        for (std::uint32_t k = 0; k < key_count; ++k) {
            if (read(stream, &bone->t_first[k], 8, 1) != 1) {
                break;
            }
        }
    }

    std::uint32_t matrix_count = 0;
    if (read(stream, &matrix_count, 4, 1) != 1) {
        return false;
    }
    const std::size_t current =
        bone->m_first != nullptr
            ? static_cast<std::size_t>(bone->m_last - bone->m_first)
            : 0;
    if (matrix_count < current) {
        bone->m_last = bone->m_first + matrix_count;
    } else if (matrix_count > current) {
        const float zero[16] = {};
        reinterpret_cast<InsertMatFn>(exe_va(0x4855f0))(
            &bone->m_first, bone->m_last,
            matrix_count - static_cast<std::uint32_t>(current), zero);
    }
    for (std::uint32_t k = 0; k < matrix_count; ++k) {
        if (!hook_4663a0(&bone->m_first[k * 16], stream)) {
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

// 6-byte prologue (sub esp,N / pushes): jmp rel32 + 1 NOP.
bool install_detour6(const char* name, std::uintptr_t rva, void* hook,
                     const std::uint8_t (&expected)[6]) {
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleA(nullptr));
    if (base == nullptr) {
        log_line("hook: GetModuleHandle(NULL) failed");
        return false;
    }
    auto* target = base + rva;
    if (std::memcmp(target, expected, 6) != 0) {
        char line[128];
        std::snprintf(line, sizeof(line), "hook: %s prologue mismatch", name);
        log_line(line);
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(target, 6, PAGE_EXECUTE_READWRITE, &old_protect)) {
        log_line("hook: VirtualProtect failed");
        return false;
    }
    const auto from = reinterpret_cast<std::intptr_t>(target) + 5;
    const auto to = reinterpret_cast<std::intptr_t>(hook);
    const auto relative = to - from;
    if (relative < INT32_MIN || relative > INT32_MAX) {
        VirtualProtect(target, 6, old_protect, &old_protect);
        log_line("hook: relative jump out of range");
        return false;
    }
    target[0] = 0xE9;
    const auto rel32 = static_cast<std::int32_t>(relative);
    std::memcpy(target + 1, &rel32, sizeof(rel32));
    target[5] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), target, 6);
    VirtualProtect(target, 6, old_protect, &old_protect);
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
    // 0x47d880: sub esp,0x94. 0x4839e0: sub esp,0x48; push ebx/ebp/esi.
    static constexpr std::uint8_t kPrologueSkin[6] = {0x81, 0xEC, 0x94,
                                                      0x00, 0x00, 0x00};
    static constexpr std::uint8_t kPrologueAnim[6] = {0x83, 0xEC, 0x48,
                                                      0x53, 0x55, 0x56};
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
    // The .skin/.anim readers were verified on the free-game park
    // (2221 skin bones, 8911+ animation keys); install by default.
    const bool e = install_detour6("FUN_0047d880", 0x47D880 - 0x400000,
                                   reinterpret_cast<void*>(&hook_47d880),
                                   kPrologueSkin);
    const bool f = install_detour6("FUN_004839e0", 0x4839E0 - 0x400000,
                                   reinterpret_cast<void*>(&hook_4839e0),
                                   kPrologueAnim);
    return a && b && c && d && e && f;
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
        char line[256];
        std::snprintf(
            line, sizeof(line),
            "hook_4663a0: total calls=%ld, hook_4809a0: total calls=%ld, "
            "hook_47d460: total calls=%ld, hook_47cc90: total calls=%ld, "
            "hook_47d880: total calls=%ld, hook_4839e0: total calls=%ld",
            static_cast<long>(g_calls_4663a0),
            static_cast<long>(g_calls_4809a0),
            static_cast<long>(g_calls_47d460),
            static_cast<long>(g_calls_47cc90),
            static_cast<long>(g_calls_47d880),
            static_cast<long>(g_calls_4839e0));
        log_line(line);
        if (g_real_dinput8 != nullptr) {
            FreeLibrary(g_real_dinput8);
            g_real_dinput8 = nullptr;
        }
    }
    return TRUE;
}
