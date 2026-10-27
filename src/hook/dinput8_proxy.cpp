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
volatile LONG g_calls_477890 = 0;
volatile LONG g_calls_477f30 = 0;
volatile LONG g_calls_430fb0 = 0;

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

// `.skel` runtime object (0x64) and its five vectors.
struct SkelVec {
    void* vptr;
    void* first;
    void* last;
    void* end;
};

struct McString {
    char* buf;
    std::uint32_t cap;
    std::uint32_t len;
};

struct Skeleton {
    void* vptr;          // 0x00
    std::uint32_t id;    // 0x04 = 0x56789ABC
    char* name;          // 0x08 mcString
    std::uint32_t name_cap;
    std::uint32_t name_len;
    SkelVec matrices;    // 0x14, elem 0x40
    SkelVec children;    // 0x24, elem 0x10
    SkelVec parents;     // 0x34, elem 4
    SkelVec names;       // 0x44, elem 0x0C
    SkelVec flags;       // 0x54, elem 4
};

using ResizeMatFn = void(__thiscall*)(void* first_ptr, std::uint32_t count,
                                      const void* fill);
using ResizeChildFn = void(__thiscall*)(void* first_ptr, std::uint32_t count,
                                        const SkelVec* fill);
using ResizeNameFn = void(__thiscall*)(void* first_ptr, std::uint32_t count,
                                       const McString* fill);

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
        std::snprintf(
            line, sizeof(line),
            "counters: mesh=%ld skin=%ld anim=%ld skel=%ld vec16=%ld vec32=%ld "
            "fnt=%ld",
            static_cast<long>(g_calls_477f30),
            static_cast<long>(g_calls_47d880),
            static_cast<long>(g_calls_4839e0),
            static_cast<long>(g_calls_477890),
            static_cast<long>(g_calls_47d460),
            static_cast<long>(g_calls_47cc90),
            static_cast<long>(g_calls_430fb0));
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

// Replacement for FUN_00477890: the `.skel` reader. The generic loader has
// already consumed the 13-byte file header; this reads the five sections:
// matrices, child lists, parents, names and flags.
bool __thiscall hook_477890(Skeleton* self, Stream* stream, void* context) {
    (void)context;
    InterlockedIncrement(&g_calls_477890);
    if (self == nullptr || stream == nullptr || stream->vtable == nullptr) {
        return false;
    }
    auto read = reinterpret_cast<StreamReadFn>(stream->vtable[2]);

    std::uint32_t count = 0;
    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    const float zero_matrix[16] = {};
    reinterpret_cast<ResizeMatFn>(exe_va(0x425140))(
        &self->matrices.first, count, zero_matrix);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (!hook_4663a0(static_cast<char*>(self->matrices.first) +
                             static_cast<std::size_t>(i) * 0x40,
                         stream)) {
            return false;
        }
    }

    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    SkelVec zero_child{};
    zero_child.vptr = reinterpret_cast<void*>(exe_va(0x6256cc));
    reinterpret_cast<ResizeChildFn>(exe_va(0x47b7d0))(
        &self->children.first, count, &zero_child);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto* child = reinterpret_cast<SkelVec*>(
            static_cast<char*>(self->children.first) +
            static_cast<std::size_t>(i) * 0x10);
        if (!hook_47cc90(reinterpret_cast<Vec<std::uint32_t>*>(child),
                         stream)) {
            return false;
        }
    }

    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    const std::uint32_t zero_u32 = 0;
    reinterpret_cast<ResizeU32Fn>(exe_va(0x47a7c0))(
        &self->parents.first, count, &zero_u32);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (read(stream, static_cast<std::uint32_t*>(self->parents.first) + i,
                 4, 1) != 1) {
            return false;
        }
    }

    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    McString zero_name{};
    reinterpret_cast<ResizeNameFn>(exe_va(0x40a270))(
        &self->names.first, count, &zero_name);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto* name = reinterpret_cast<McString*>(
            static_cast<char*>(self->names.first) +
            static_cast<std::size_t>(i) * 0x0c);
        std::uint32_t length = 0;
        if (read(stream, &length, 4, 1) != 1) {
            continue;
        }
        reinterpret_cast<McStringReserveFn>(exe_va(0x4058b0))(name,
                                                              length + 1);
        if (read(stream, name->buf, 1, length) ==
            static_cast<int>(length)) {
            name->len = static_cast<std::uint32_t>(
                reinterpret_cast<StrlenFn>(exe_va(0x4056c0))(name->buf));
        }
    }

    if (read(stream, &count, 4, 1) != 1) {
        return false;
    }
    reinterpret_cast<ResizeU32Fn>(exe_va(0x47a7c0))(
        &self->flags.first, count, &zero_u32);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (read(stream, static_cast<std::uint32_t*>(self->flags.first) + i,
                 4, 1) != 1) {
            return false;
        }
    }
    return true;
}

// `.mesh`/`.skin` whole-file block reader. The vtable slot 0x626abc points at
// 0x477f30; the hook branches on the object magic and forwards `.skin` to the
// original function (the vtable patch leaves the code entry untouched).
using BlockCtorFn = void(__thiscall*)(void* block);
using BlockDtorFn = void(__thiscall*)(void* block, std::uint32_t flags);
using BlockReadFn = bool(__thiscall*)(void* block, Stream* file, void* name);
using InsertBlocksFn = void(__thiscall*)(void* vec, void* where,
                                        std::uint32_t extra,
                                        const void* value);
using MoveBlocksFn = void*(__cdecl*)(void* first, void* last, void* dest,
                                     void* tag);
using ReadBlocksFn = bool(__thiscall*)(void* self, Stream* file, void* name);

bool __thiscall hook_477f30(void* self, Stream* file, void* name) {
    InterlockedIncrement(&g_calls_477f30);
    if (self == nullptr || file == nullptr || file->vtable == nullptr) {
        return false;
    }
    const std::uint32_t magic = *reinterpret_cast<std::uint32_t*>(
        static_cast<char*>(self) + 4);
    if (magic != 0x12345678) {
        // `.skin` (0x02468ACE) and any other variant: use the original.
        return reinterpret_cast<ReadBlocksFn>(exe_va(0x477f30))(self, file,
                                                               name);
    }

    auto read = reinterpret_cast<StreamReadFn>(file->vtable[2]);
    std::uint32_t count = 0;
    if (read(file, &count, 4, 1) != 1) {
        return false;
    }
    auto* vec = reinterpret_cast<std::uint8_t*>(
        *reinterpret_cast<void**>(static_cast<char*>(self) + 0x14));
    if (vec == nullptr) {
        return false;
    }
    vec += 4;  // {begin, end, capacity}

    alignas(16) std::uint8_t temp[0xB8];
    reinterpret_cast<BlockCtorFn>(exe_va(0x471bd0))(temp);

    auto* begin = *reinterpret_cast<std::uint8_t**>(vec);
    auto* end = *reinterpret_cast<std::uint8_t* const*>(vec + 4);
    const std::size_t current = static_cast<std::size_t>(end - begin) / 0xB8;
    if (count < current) {
        auto* new_end = begin + static_cast<std::size_t>(count) * 0xB8;
        std::uint8_t scratch = 0;
        new_end = static_cast<std::uint8_t*>(
            reinterpret_cast<MoveBlocksFn>(exe_va(0x47e560))(
                end, end, new_end, &scratch));
        for (auto* p = new_end; p != end; p += 0xB8) {
            reinterpret_cast<BlockDtorFn>(exe_va(0x462700))(p, 0);
        }
        *reinterpret_cast<std::uint8_t**>(vec + 4) = new_end;
    } else if (count > current) {
        reinterpret_cast<InsertBlocksFn>(exe_va(0x47dd70))(
            vec, end, count - static_cast<std::uint32_t>(current), temp);
    }
    reinterpret_cast<BlockDtorFn>(exe_va(0x462700))(temp, 0);

    begin = *reinterpret_cast<std::uint8_t**>(vec);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (!reinterpret_cast<BlockReadFn>(exe_va(0x472240))(
                begin + static_cast<std::size_t>(i) * 0xB8, file, name)) {
            return false;
        }
    }
    return true;
}

bool install_mesh_vtable_hook() {
    auto* slot = reinterpret_cast<void**>(exe_va(0x626abc));
    if (*slot != reinterpret_cast<void*>(exe_va(0x477f30))) {
        log_line("hook: mesh vtable slot mismatch");
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE,
                        &old_protect)) {
        log_line("hook: mesh vtable VirtualProtect failed");
        return false;
    }
    *slot = reinterpret_cast<void*>(&hook_477f30);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    VirtualProtect(slot, sizeof(void*), old_protect, &old_protect);
    log_line("hook: FUN_00477f30 patched (vtable 0x626abc)");
    return true;
}

// `.fnt` loader replacement (HYB-7). 0x430fb0 is
// `void __thiscall LoadFile(mgFontDx8* this, const mcString* path)`, RET 4.
// The loader only builds the glyph map; the pixel atlas is fetched lazily by
// 0x4316e0 from the path stored in each glyph. The replacement reads the file
// itself and inserts every code/metric/path with the engine helpers 0x432d70
// (build insertion record) and 0x432fd0 (hash insert), then sets the
// glyph-grid capacity (+0x2c) that the retail loader computes.
//
// File layout (see docs/formats/fnt.md): 7 header dwords ending at 0x1b, the
// glyph count at 0x1c, then `count` u16 codes starting at 0x20 -- the 0x20
// dword is the hash, so the runtime code table is [hash_lo, hash_hi,
// code[0]..code[count-3]] and pairs with metrics[0..count-1]. That is exactly
// the port's `codes[i] -> metrics[i+2]` mapping. The u32 at
// 0x20 + count*2 is the metric count and the metrics follow it.
struct FntGlyphValue {
    std::uint16_t rect[4];  // x0, y0, x1, y1 (inclusive)
    std::uint32_t f08;
    std::uint32_t f0c;
    std::uint32_t f10;
    std::uint32_t f14;
    McString path;
    std::uint32_t tex_index;
};
static_assert(sizeof(FntGlyphValue) == 0x28, "fnt glyph value layout");

struct FntInsertRecord {
    std::uint16_t key;
    std::uint16_t pad[3];
    FntGlyphValue value;
};
static_assert(sizeof(FntInsertRecord) == 0x30, "fnt insert record layout");

using GlyphBuildRecordFn =
    FntInsertRecord*(__thiscall*)(FntInsertRecord*, const std::uint16_t*,
                                  const FntGlyphValue*);
using GlyphInsertFn = void*(__thiscall*)(void* map, const FntInsertRecord*);
using McStringDestroyFn = void(__thiscall*)(McString*);

void __thiscall hook_430fb0(void* self, const McString* path) {
    InterlockedIncrement(&g_calls_430fb0);
    if (self == nullptr || path == nullptr || path->buf == nullptr) {
        return;
    }
    HANDLE file = CreateFileA(path->buf, GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        log_line("hook_430fb0: open failed");
        return;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0x206 ||
        size.QuadPart > (64 << 20)) {
        CloseHandle(file);
        log_line("hook_430fb0: bad size");
        return;
    }
    auto* data = static_cast<std::uint8_t*>(
        HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(size.QuadPart)));
    DWORD got = 0;
    const bool read_ok =
        data != nullptr &&
        ReadFile(file, data, static_cast<DWORD>(size.QuadPart), &got, nullptr) !=
            0 &&
        got == static_cast<DWORD>(size.QuadPart);
    CloseHandle(file);
    if (!read_ok) {
        if (data != nullptr) {
            HeapFree(GetProcessHeap(), 0, data);
        }
        log_line("hook_430fb0: read failed");
        return;
    }
    auto u32 = [&](std::size_t off) {
        std::uint32_t value = 0;
        std::memcpy(&value, data + off, sizeof(value));
        return value;
    };
    const std::uint32_t version = u32(0x00);
    const std::uint32_t font_size = u32(0x0c);
    const std::uint32_t cell_height = u32(0x10);
    const std::uint32_t pitch_x = u32(0x14);
    const std::uint32_t pitch_y = u32(0x18);
    const std::uint32_t code_count = u32(0x1c);
    const std::size_t codes_off = 0x20;
    const std::size_t metrics_off = codes_off + code_count * 2 + 4;
    const std::uint32_t metric_count =
        metrics_off <= static_cast<std::size_t>(size.QuadPart)
            ? u32(metrics_off - 4)
            : 0;
    if (version != 1 || code_count == 0 || code_count > 10000 ||
        metric_count != code_count ||
        metrics_off + static_cast<std::size_t>(metric_count) * 8 >
            static_cast<std::size_t>(size.QuadPart)) {
        HeapFree(GetProcessHeap(), 0, data);
        log_line("hook_430fb0: header rejected");
        return;
    }

    auto* base = static_cast<char*>(self);
    *reinterpret_cast<std::uint32_t*>(base + 0x10) = font_size;
    // Retail keeps the larger pitch X and only fills pitch Y when it is zero.
    auto* pitch_x_field = reinterpret_cast<std::uint32_t*>(base + 0x18);
    auto* pitch_y_field = reinterpret_cast<std::uint32_t*>(base + 0x1c);
    if (pitch_x > *pitch_x_field) {
        *pitch_x_field = pitch_x;
    }
    if (*pitch_y_field == 0) {
        *pitch_y_field = pitch_y;
    }
    *reinterpret_cast<std::uint32_t*>(base + 0x20) = cell_height;
    const std::uint32_t tex_w = *reinterpret_cast<std::uint32_t*>(base + 0x30);
    const std::uint32_t tex_h = *reinterpret_cast<std::uint32_t*>(base + 0x34);
    if (*pitch_x_field != 0 && *pitch_y_field != 0) {
        *reinterpret_cast<std::uint32_t*>(base + 0x2c) =
            (tex_h / *pitch_y_field) * (tex_w / *pitch_x_field);
    }

    void* map = base + 0x40;
    const std::uint8_t* metrics = data + metrics_off;
    for (std::uint32_t i = 0; i < code_count; ++i) {
        FntGlyphValue value = {};
        std::memcpy(value.rect, metrics + static_cast<std::size_t>(i) * 8, 8);
        value.path.buf = path->buf;
        value.path.cap = path->len + 1;
        value.path.len = path->len;
        FntInsertRecord rec = {};
        std::memcpy(&rec.key, data + codes_off + static_cast<std::size_t>(i) * 2,
                    sizeof(rec.key));
        reinterpret_cast<GlyphBuildRecordFn>(exe_va(0x432d70))(
            &rec, &rec.key, &value);
        reinterpret_cast<GlyphInsertFn>(exe_va(0x432fd0))(map, &rec);
        // The insert deep-copies the value; release the record's path copy
        // exactly as the retail loader does.
        reinterpret_cast<McStringDestroyFn>(exe_va(0x404440))(
            &rec.value.path);
    }
    HeapFree(GetProcessHeap(), 0, data);

    char line[192];
    std::snprintf(line, sizeof(line),
                  "hook_430fb0: %s glyphs=%lu size=%lu cell=%lu pitch=%lux%lu",
                  path->buf, static_cast<unsigned long>(code_count),
                  static_cast<unsigned long>(font_size),
                  static_cast<unsigned long>(cell_height),
                  static_cast<unsigned long>(pitch_x),
                  static_cast<unsigned long>(pitch_y));
    log_line(line);
}

// ---------------------------------------------------------------------------
// Virtual DirectInput mouse (opt-in with DINO_VIRTUAL_MOUSE=1).
//
// Dino Island reads mouse buttons through DirectInput, not WM_LBUTTONDOWN.
// The harness posts WM_MOUSEMOVE to place the game's cursor, and writes
// `1`/`0` to dino_input.txt. This hook overrides the mouse device's
// GetDeviceState button byte, so no physical cursor movement is needed.
// ---------------------------------------------------------------------------
const GUID kGUID_SysMouse = {0x6F1D2B60,
                             0xD5A0,
                             0x11CF,
                             {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

using CreateDeviceFn =
    HRESULT(STDMETHODCALLTYPE*)(void*, const GUID*, void**, IUnknown*);
using GetDeviceStateFn =
    HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, void*);
using GetDeviceDataFn =
    HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, void*, DWORD*, DWORD);
using SetCooperativeLevelFn =
    HRESULT(STDMETHODCALLTYPE*)(void*, HWND, DWORD);

CreateDeviceFn g_real_create_device = nullptr;
GetDeviceStateFn g_real_get_device_state = nullptr;
GetDeviceDataFn g_real_get_device_data = nullptr;
SetCooperativeLevelFn g_real_set_cooperative_level = nullptr;
void* g_mouse_device = nullptr;
void* g_mouse_devices[8] = {};
volatile LONG g_mouse_device_count = 0;
volatile LONG g_mouse_state_calls = 0;
volatile LONG g_mouse_data_calls = 0;
volatile LONG g_mouse_button_overrides = 0;
bool g_data_button_down = false;
bool g_state_button_down = false;

bool is_mouse_device(void* self) {
    const LONG count = g_mouse_device_count;
    for (LONG i = 0; i < count && i < 8; ++i) {
        if (g_mouse_devices[i] == self) {
            return true;
        }
    }
    return false;
}

bool virtual_mouse_enabled() {
    static int cached = -1;
    if (cached < 0) {
        char value[8] = {};
        cached = GetEnvironmentVariableA("DINO_VIRTUAL_MOUSE", value,
                                         sizeof(value)) > 0
                     ? 1
                     : 0;
    }
    return cached == 1;
}

// Optional click-to-capture gate for normal windowed play. While inactive the
// game receives zero mouse axes/buttons; the first click inside the focused
// game window activates it. Alt-Tab (focus loss) deactivates it again.
bool click_capture_enabled() {
    static int cached = -1;
    if (cached < 0) {
        char value[8] = {};
        cached = GetEnvironmentVariableA("DINO_MOUSE_CLICK_CAPTURE", value,
                                         sizeof(value)) > 0
                     ? 1
                     : 0;
    }
    return cached == 1;
}

// Opt-in activation shim for the background harness. Retail disables the
// mouse/keyboard poll flags (miInputDx8+8/+0x30) when the window loses
// activation; its poll task still runs every frame. With DINO_VIRTUAL_MOUSE=1
// (or DINO_MOUSE_CLICK_CAPTURE=1) plus DINO_FORCE_INPUT_ACTIVE=1, start a
// thread that keeps the input update alive while the window stays in the
// background. Normal play does not define the variables.
bool force_input_active_enabled() {
    static int cached = -1;
    if (cached < 0) {
        char value[8] = {};
        cached = GetEnvironmentVariableA("DINO_FORCE_INPUT_ACTIVE", value,
                                         sizeof(value)) > 0
                     ? 1
                     : 0;
    }
    return cached == 1;
}

DWORD WINAPI force_active_thread(LPVOID) {
    DWORD last_sample = 0;
    int last_slot = -1;
    LONG slot_changes = 0;
    LONG view_updates = 0;
    for (;;) {
        auto* input = *reinterpret_cast<char**>(exe_va(0x675104));
        if (input != nullptr) {
            const int slot = *reinterpret_cast<int*>(input + 0x294);
            if (slot != last_slot) {
                last_slot = slot;
                InterlockedIncrement(&slot_changes);
            }
            input[8] = 1;   // miInputDx8 mouse poll enable
            input[0x30] = 1;  // miInputDx8 keyboard poll enable
            const DWORD now = GetTickCount();
            if (now - last_sample >= 1000) {
                last_sample = now;
                auto* app = *reinterpret_cast<char**>(exe_va(0x674528));
                const bool app_ok =
                    app != nullptr && !IsBadReadPtr(app, 0x100);
                char line[192];
                std::snprintf(
                    line, sizeof(line),
                    "virtual mouse: force sample slot_changes=%ld "
                    "state_calls=%ld app54=%d view_updates=%ld",
                    static_cast<long>(slot_changes),
                    static_cast<long>(g_mouse_state_calls),
                    app_ok ? static_cast<unsigned char>(app[0x54]) : -1,
                    static_cast<long>(view_updates));
                log_line(line);
            }
        }
        auto* app = *reinterpret_cast<char**>(exe_va(0x674528));
        if (app != nullptr && !IsBadReadPtr(app, 0x100)) {
            // On deactivation the engine stops calling the view update
            // (FUN_00524610). The detector is app+0x18 bit 0x00010000: it is
            // initialized to 1 (ctor) and observed to go 0 in the runtime
            // active/stuck memory diff, but it is not statically attributed
            // yet. app+0x54 is the WndProc's active byte (0x4235b0).
            //
            // RACE: this assumes the game thread skips the view update while
            // the bit is clear; if it ever runs concurrently the view/D3D8
            // state is touched by two threads. Env-gated
            // (DINO_FORCE_INPUT_ACTIVE + DINO_VIRTUAL_MOUSE) for the harness
            // only; do not enable it for normal play.
            const bool inactive =
                (*reinterpret_cast<std::uint32_t*>(app + 0x18) &
                 0x00010000u) == 0;
            if (inactive) {
                auto* view = *reinterpret_cast<char**>(exe_va(0x675a88));
                static DWORD last_update = 0;
                const DWORD now = GetTickCount();
                if (view != nullptr && !IsBadReadPtr(view, 0x64) &&
                    now - last_update >= 15) {
                    last_update = now;
                    using ViewUpdateFn = void(__fastcall*)(void*);
                    reinterpret_cast<ViewUpdateFn>(exe_va(0x524610))(view);
                    InterlockedIncrement(&view_updates);
                }
            }
            if (app[0x54] == 0) {
                app[0x54] = 1;
            }
        }
        Sleep(5);
    }
    return 0;
}

void start_force_active_thread() {
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    HANDLE thread =
        CreateThread(nullptr, 0, force_active_thread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
        log_line("virtual mouse: force-active thread started");
    } else {
        log_line("virtual mouse: force-active thread FAILED");
    }
}

bool g_click_capture_active = false;

bool cursor_inside_game_client() {
    // Deliberately not tied to the foreground window: the user may click the
    // game while it is in the background. WindowFromPoint tells us what would
    // actually receive the click.
    POINT point = {};
    if (!GetCursorPos(&point)) {
        return false;
    }
    HWND hit = WindowFromPoint(point);
    if (hit == nullptr) {
        return false;
    }
    HWND root = GetAncestor(hit, GA_ROOT);
    if (root == nullptr) {
        root = hit;
    }
    DWORD process_id = 0;
    GetWindowThreadProcessId(root, &process_id);
    if (process_id != GetCurrentProcessId()) {
        return false;
    }
    RECT client = {};
    if (!GetClientRect(root, &client)) {
        return false;
    }
    if (!ScreenToClient(root, &point)) {
        return false;
    }
    return point.x >= client.left && point.x < client.right &&
           point.y >= client.top && point.y < client.bottom;
}

bool gate_mouse_input(void* data, DWORD cbData) {
    if (!click_capture_enabled() || virtual_mouse_enabled()) {
        return true;
    }
    const SHORT state = GetAsyncKeyState(VK_LBUTTON);
    const bool pressed = (state & 0x8001) != 0;
    const bool inside = cursor_inside_game_client();
    if (!g_click_capture_active) {
        if (pressed && inside) {
            g_click_capture_active = true;
            log_line("click capture: active");
            // Let the activating click through so it can also operate the UI.
            return true;
        }
        // Suppress all mouse input until the game is clicked.
        if (data != nullptr && cbData > 0) {
            std::memset(data, 0, cbData);
        }
        return false;
    }
    if (pressed && !inside) {
        g_click_capture_active = false;
        log_line("click capture: released (click outside)");
        if (data != nullptr && cbData > 0) {
            std::memset(data, 0, cbData);
        }
        return false;
    }
    return true;
}

bool read_virtual_input(int& target_x, int& target_y, bool& down,
                        bool& has_move) {
    // The shim can poll at 64 Hz; re-opening the file on every poll makes the
    // x32dbg CreateFileA breakpoint dominate the debuggee. 50 ms still catches
    // the harness's 100 ms click hold.
    static DWORD last_read = 0;
    static int cached_x = 0;
    static int cached_y = 0;
    static bool cached_down = false;
    static bool cached_has_move = false;
    static bool cached_valid = false;
    const DWORD now = GetTickCount();
    if (cached_valid && now - last_read < 50) {
        target_x = cached_x;
        target_y = cached_y;
        down = cached_down;
        has_move = cached_has_move;
        return true;
    }
    last_read = now;
    HANDLE file = CreateFileA("dino_input.txt", GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        cached_valid = false;
        return false;
    }
    char buffer[64] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
    CloseHandle(file);
    if (!ok || read == 0) {
        return false;
    }
    buffer[read] = '\0';
    int a = 0;
    int b = 0;
    int c = 0;
    const int count = std::sscanf(buffer, "%d %d %d", &a, &b, &c);
    if (count >= 3) {
        target_x = a;
        target_y = b;
        down = c != 0;
        has_move = true;
        cached_x = target_x;
        cached_y = target_y;
        cached_down = down;
        cached_has_move = has_move;
        cached_valid = true;
        return true;
    }
    if (count >= 1) {
        down = a != 0;
        has_move = false;
        cached_x = target_x;
        cached_y = target_y;
        cached_down = down;
        cached_has_move = has_move;
        cached_valid = true;
        return true;
    }
    cached_valid = false;
    return false;
}

// The DirectInput mouse object is miInputDx8 at DAT_00675104. It keeps the
// current cursor in a three-slot ring; FUN_004458f0 adds lX/lY to the current
// slot. Read it back so a scripted target can be reached in one or two polls.
void apply_virtual_cursor(std::uint8_t* data, DWORD cbData, int target_x,
                          int target_y) {
    if (cbData < 16) {
        return;
    }
    auto* input = *reinterpret_cast<char**>(exe_va(0x675104));
    if (input == nullptr) {
        return;
    }
    const int slot = *reinterpret_cast<int*>(input + 0x294);
    if (slot < 0 || slot > 2) {
        return;
    }
    float scale = *reinterpret_cast<float*>(input + 0x2c);
    if (scale <= 0.01f) {
        scale = 1.0f;
    }
    const int current_x =
        *reinterpret_cast<int*>(input + slot * 0x10 + 0xc);
    const int current_y =
        *reinterpret_cast<int*>(input + (2 - slot) * 0x10);
    const int dx = static_cast<int>((target_x - current_x) / scale);
    const int dy = static_cast<int>((target_y - current_y) / scale);
    *reinterpret_cast<int*>(data) = dx;
    *reinterpret_cast<int*>(data + 4) = dy;
}

bool patch_vtable_entry(void** vtable, int index, void* replacement,
                        void** original) {
    if (vtable == nullptr || index < 0) {
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*),
                        PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }
    *original = vtable[index];
    vtable[index] = replacement;
    DWORD ignored = 0;
    VirtualProtect(&vtable[index], sizeof(void*), old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &vtable[index], sizeof(void*));
    return true;
}

HRESULT STDMETHODCALLTYPE hook_get_device_state(void* self, DWORD cbData,
                                                void* data) {
    const HRESULT hr = g_real_get_device_state != nullptr
                           ? g_real_get_device_state(self, cbData, data)
                           : E_FAIL;
    if (FAILED(hr) || !is_mouse_device(self) || data == nullptr) {
        return hr;
    }
    if (!gate_mouse_input(data, cbData)) {
        return hr;
    }
    const LONG calls = InterlockedIncrement(&g_mouse_state_calls);
    if (calls == 1) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        const auto caller = reinterpret_cast<std::uintptr_t>(
            __builtin_return_address(0));
        const auto base =
            reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
        char line[192];
        std::snprintf(line, sizeof(line),
                      "virtual mouse: GetDeviceState self=%p cb=%lu "
                      "x=%ld y=%ld z=%ld b0=%02x caller=+0x%lx",
                      self, static_cast<unsigned long>(cbData),
                      static_cast<long>(*reinterpret_cast<const std::int32_t*>(bytes)),
                      static_cast<long>(*reinterpret_cast<const std::int32_t*>(bytes + 4)),
                      static_cast<long>(*reinterpret_cast<const std::int32_t*>(bytes + 8)),
                      bytes[12], static_cast<unsigned long>(caller - base));
        log_line(line);
    }
    int target_x = 0;
    int target_y = 0;
    bool down = false;
    bool has_move = false;
    if (read_virtual_input(target_x, target_y, down, has_move)) {
        if (has_move) {
            apply_virtual_cursor(static_cast<std::uint8_t*>(data), cbData,
                                 target_x, target_y);
        }
        if (down != g_state_button_down) {
            char line[96];
            std::snprintf(line, sizeof(line), "virtual mouse: state button=%s",
                          down ? "down" : "up");
            log_line(line);
            g_state_button_down = down;
        }
        if (down && cbData >= 16) {
            // DIMOUSESTATE / DIMOUSESTATE2: lX, lY, lZ, then rgbButtons.
            static_cast<std::uint8_t*>(data)[12] = 0x80;
            const LONG overrides =
                InterlockedIncrement(&g_mouse_button_overrides);
            if (overrides == 1 || (overrides % 100) == 0) {
                char line[96];
                std::snprintf(line, sizeof(line),
                              "virtual mouse: left down (%ld)",
                              static_cast<long>(overrides));
                log_line(line);
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE hook_get_device_data(void* self, DWORD cbObjectData,
                                               void* rgdod, DWORD* pdwInOut,
                                               DWORD dwFlags) {
    const DWORD capacity = pdwInOut != nullptr ? *pdwInOut : 0;
    const HRESULT hr = g_real_get_device_data != nullptr
                           ? g_real_get_device_data(self, cbObjectData, rgdod,
                                                    pdwInOut, dwFlags)
                           : E_FAIL;
    if (FAILED(hr) || rgdod == nullptr || pdwInOut == nullptr) {
        return hr;
    }
    const LONG calls = InterlockedIncrement(&g_mouse_data_calls);
    if (calls == 1) {
        char line[128];
        std::snprintf(line, sizeof(line),
                      "virtual mouse: GetDeviceData self=%p cb=%lu mouse=%d",
                      self, static_cast<unsigned long>(cbObjectData),
                      is_mouse_device(self) ? 1 : 0);
        log_line(line);
    }
    if (!is_mouse_device(self)) {
        return hr;
    }
    if (!gate_mouse_input(nullptr, 0)) {
        *pdwInOut = 0;
        return hr;
    }
    int target_x = 0;
    int target_y = 0;
    bool down = false;
    bool has_move = false;
    if (!read_virtual_input(target_x, target_y, down, has_move)) {
        return hr;
    }
    if (down != g_data_button_down && *pdwInOut < capacity) {
        // DIDEVICEOBJECTDATA: offset, data, timestamp, sequence.
        struct Event {
            DWORD offset;
            DWORD data;
            DWORD timestamp;
            DWORD sequence;
        };
        auto* events = static_cast<Event*>(rgdod);
        Event& event = events[*pdwInOut];
        event.offset = 12;  // DIMOFS_BUTTON0
        event.data = down ? 0x80 : 0;
        event.timestamp = GetTickCount();
        event.sequence = 0;
        ++(*pdwInOut);
        g_data_button_down = down;
        const LONG injected = InterlockedIncrement(&g_mouse_button_overrides);
        if (injected == 1 || (injected % 100) == 0) {
            char line[96];
            std::snprintf(line, sizeof(line),
                          "virtual mouse: button event=%s (%ld)",
                          down ? "down" : "up", static_cast<long>(injected));
            log_line(line);
        }
    }
    return hr;
}

// Retail asks for DISCL_FOREGROUND; the test harness keeps the game in the
// background, so the device would stop reporting. Force the background
// cooperative level and keep the device non-exclusive.
HRESULT STDMETHODCALLTYPE hook_set_cooperative_level(void* self, HWND hwnd,
                                                     DWORD flags) {
    constexpr DWORD kForeground = 0x00000004;
    constexpr DWORD kBackground = 0x00000008;
    constexpr DWORD kNonexclusive = 0x00000002;
    constexpr DWORD kNoWinKey = 0x00000010;
    const DWORD forced =
        (flags | kBackground | kNonexclusive) & ~(kForeground | kNoWinKey);
    const HRESULT hr =
        g_real_set_cooperative_level(self, hwnd, forced);
    char line[160];
    std::snprintf(line, sizeof(line),
                  "virtual mouse: SetCooperativeLevel flags=0x%lx -> 0x%lx "
                  "hr=0x%lx",
                  static_cast<unsigned long>(flags),
                  static_cast<unsigned long>(forced),
                  static_cast<unsigned long>(hr));
    log_line(line);
    return hr;
}

HRESULT STDMETHODCALLTYPE hook_create_device(void* self, const GUID* rguid,
                                             void** out, IUnknown* outer) {
    const HRESULT hr = g_real_create_device(self, rguid, out, outer);
    if (SUCCEEDED(hr) && out != nullptr && *out != nullptr &&
        rguid != nullptr &&
        std::memcmp(rguid, &kGUID_SysMouse, sizeof(GUID)) == 0) {
        g_mouse_device = *out;
        const LONG device_index =
            InterlockedIncrement(&g_mouse_device_count) - 1;
        if (device_index < 8) {
            g_mouse_devices[device_index] = *out;
        }
        char capture_line[128];
        std::snprintf(capture_line, sizeof(capture_line),
                      "virtual mouse: GUID_SysMouse device captured (%ld)",
                      static_cast<long>(device_index + 1));
        log_line(capture_line);
        void** device_vtable = *reinterpret_cast<void***>(*out);
        if (g_real_get_device_state == nullptr) {
            if (patch_vtable_entry(device_vtable, 9,
                                   reinterpret_cast<void*>(
                                       &hook_get_device_state),
                                   reinterpret_cast<void**>(
                                       &g_real_get_device_state))) {
                log_line("virtual mouse: GetDeviceState patched");
            } else {
                log_line("virtual mouse: GetDeviceState patch FAILED");
            }
        }
        if (g_real_get_device_data == nullptr) {
            if (patch_vtable_entry(device_vtable, 10,
                                   reinterpret_cast<void*>(
                                       &hook_get_device_data),
                                   reinterpret_cast<void**>(
                                       &g_real_get_device_data))) {
                log_line("virtual mouse: GetDeviceData patched");
            } else {
                log_line("virtual mouse: GetDeviceData patch FAILED");
            }
        }
        if (g_real_set_cooperative_level == nullptr) {
            if (patch_vtable_entry(
                    device_vtable, 13,
                    reinterpret_cast<void*>(&hook_set_cooperative_level),
                    reinterpret_cast<void**>(&g_real_set_cooperative_level))) {
                log_line("virtual mouse: SetCooperativeLevel patched");
            } else {
                log_line("virtual mouse: SetCooperativeLevel patch FAILED");
            }
        }
    }
    return hr;
}

void install_virtual_mouse(void* dinput8) {
    if (dinput8 == nullptr ||
        (!virtual_mouse_enabled() && !click_capture_enabled())) {
        return;
    }
    if (force_input_active_enabled()) {
        start_force_active_thread();
    }
    void** vtable = *reinterpret_cast<void***>(dinput8);
    if (vtable == nullptr ||
        vtable[3] == reinterpret_cast<void*>(&hook_create_device)) {
        return;
    }
    if (patch_vtable_entry(vtable, 3,
                           reinterpret_cast<void*>(&hook_create_device),
                           reinterpret_cast<void**>(&g_real_create_device))) {
        log_line("virtual mouse: IDirectInput8::CreateDevice patched");
    } else {
        log_line("virtual mouse: CreateDevice patch FAILED");
    }
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

// 12-byte prologue (sub esp,N / 4 pushes / mov esi,ecx): jmp rel32 + 7 NOP.
bool install_detour12(const char* name, std::uintptr_t rva, void* hook,
                      const std::uint8_t (&expected)[12]) {
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleA(nullptr));
    if (base == nullptr) {
        log_line("hook: GetModuleHandle(NULL) failed");
        return false;
    }
    auto* target = base + rva;
    if (std::memcmp(target, expected, 12) != 0) {
        char line[128];
        std::snprintf(line, sizeof(line), "hook: %s prologue mismatch", name);
        log_line(line);
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE, &old_protect)) {
        log_line("hook: VirtualProtect failed");
        return false;
    }
    const auto from = reinterpret_cast<std::intptr_t>(target) + 5;
    const auto to = reinterpret_cast<std::intptr_t>(hook);
    const auto relative = to - from;
    if (relative < INT32_MIN || relative > INT32_MAX) {
        VirtualProtect(target, 12, old_protect, &old_protect);
        log_line("hook: relative jump out of range");
        return false;
    }
    target[0] = 0xE9;
    const auto rel32 = static_cast<std::int32_t>(relative);
    std::memcpy(target + 1, &rel32, sizeof(rel32));
    for (int i = 5; i < 12; ++i) {
        target[i] = 0x90;
    }
    FlushInstructionCache(GetCurrentProcess(), target, 12);
    VirtualProtect(target, 12, old_protect, &old_protect);
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
    // 0x477890: sub esp,0x5c; push ebx/ebp/esi.
    static constexpr std::uint8_t kPrologueSkel[6] = {0x83, 0xEC, 0x5C,
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
    // The .skel reader was verified on the free-game park (21 skeletons);
    // install by default.
    const bool g = install_detour6("FUN_00477890", 0x477890 - 0x400000,
                                   reinterpret_cast<void*>(&hook_477890),
                                   kPrologueSkel);
    // The .mesh whole-file reader was verified on the free-game park
    // (121 files); install by default.
    const bool h = install_mesh_vtable_hook();
    // The .fnt loader was verified on the free-game HUD: six font files, 247
    // glyphs each, HUD text renders; install by default.
    static constexpr std::uint8_t kPrologueFnt[12] = {
        0x81, 0xEC, 0x40, 0x02, 0x00, 0x00, 0x53,
        0x55, 0x56, 0x57, 0x8B, 0xF1};
    const bool i = install_detour12("FUN_00430fb0", 0x430FB0 - 0x400000,
                                    reinterpret_cast<void*>(&hook_430fb0),
                                    kPrologueFnt);
    return a && b && c && d && e && f && g && h && i;
}

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    if (g_real_create == nullptr) {
        return E_FAIL;
    }
    const HRESULT result = g_real_create(hinst, version, riid, out, outer);
    if (SUCCEEDED(result) && out != nullptr && *out != nullptr) {
        install_virtual_mouse(*out);
    }
    return result;
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
        char line[320];
        std::snprintf(
            line, sizeof(line),
            "hook_4663a0: total calls=%ld, hook_4809a0: total calls=%ld, "
            "hook_47d460: total calls=%ld, hook_47cc90: total calls=%ld, "
            "hook_47d880: total calls=%ld, hook_4839e0: total calls=%ld, "
            "hook_477890: total calls=%ld, hook_477f30: total calls=%ld, "
            "hook_430fb0: total calls=%ld",
            static_cast<long>(g_calls_4663a0),
            static_cast<long>(g_calls_4809a0),
            static_cast<long>(g_calls_47d460),
            static_cast<long>(g_calls_47cc90),
            static_cast<long>(g_calls_47d880),
            static_cast<long>(g_calls_4839e0),
            static_cast<long>(g_calls_477890),
            static_cast<long>(g_calls_477f30),
            static_cast<long>(g_calls_430fb0));
        log_line(line);
        char mouse_line[128];
        std::snprintf(mouse_line, sizeof(mouse_line),
                      "virtual mouse: state calls=%ld data calls=%ld overrides=%ld",
                      static_cast<long>(g_mouse_state_calls),
                      static_cast<long>(g_mouse_data_calls),
                      static_cast<long>(g_mouse_button_overrides));
        log_line(mouse_line);
        if (g_real_dinput8 != nullptr) {
            FreeLibrary(g_real_dinput8);
            g_real_dinput8 = nullptr;
        }
    }
    return TRUE;
}
