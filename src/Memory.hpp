#pragma once
#include <windows.h>

#ifdef _WIN64
#error AudioDirectorsCut is a 32-bit ASI
#endif

#define OPCODE_CALL                      0xE8
#define OPCODE_JUMP                      0xE9

namespace Memory {

    template <typename T>
    T Read(const void* base, DWORD offset) {
        T value = T();
        __try {
            if (base != NULL) value = *(const T*)((const BYTE*)base + offset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        return value;
    }

    bool  Matches(DWORD address, const BYTE* bytes, size_t length);
    bool  PatchBranch(DWORD site, const BYTE* expected, size_t length, BYTE opcode, const void* target);
    bool  PatchPointer(DWORD slot, DWORD expected, const void* target);
    int   Patches();
    void  Unpatch(int count);
    DWORD Method(void* object, DWORD slot);
    void  CallVirtual(void* object, DWORD slot);
    void  CallVirtualWith(void* object, DWORD slot, int value);

}
