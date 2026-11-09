#include "Memory.hpp"
#include <string.h>

namespace {

    #define CODE_END                         0x00890000
    #define IMAGE_END                        0x009C7000
    #define OPCODE_NOP                       0x90
    #define BRANCH_SIZE                      5
    #define MOST_BYTES                       16
    #define MOST_PATCHES                     24

    typedef void (__thiscall* VoidMethod)(void* object);
    typedef void (__thiscall* IntMethod)(void* object, int value);

    struct SavedPatch {
        DWORD  address;
        size_t length;
        BYTE   bytes[MOST_BYTES];
    };

    SavedPatch gSaved[MOST_PATCHES];
    int        gSavedCount = 0;

    void Write(DWORD address, const void* bytes, size_t length);
    void Save(DWORD address, const void* bytes, size_t length);

}

namespace Memory {

    bool Matches(DWORD address, const BYTE* bytes, size_t length) {
        __try {
            return memcmp((const void*)address, bytes, length) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool PatchBranch(DWORD site, const BYTE* expected, size_t length, BYTE opcode, const void* target) {
        if (length < BRANCH_SIZE || length > MOST_BYTES || gSavedCount >= MOST_PATCHES || !Matches(site, expected, length)) return false;
        BYTE code[MOST_BYTES];
        memset(code, OPCODE_NOP, length);
        code[0] = opcode;
        *(int*)(code + 1) = (int)((DWORD)target - (site + BRANCH_SIZE));
        Save(site, expected, length);
        Write(site, code, length);
        return true;
    }

    bool PatchPointer(DWORD slot, DWORD expected, const void* target) {
        if (gSavedCount >= MOST_PATCHES || !Matches(slot, (const BYTE*)&expected, sizeof(expected))) return false;
        DWORD value = (DWORD)target;
        Save(slot, &expected, sizeof(expected));
        Write(slot, &value, sizeof(value));
        return true;
    }

    int Patches() {
        return gSavedCount;
    }

    void Unpatch(int count) {
        while (gSavedCount > count) {
            SavedPatch& saved = gSaved[--gSavedCount];
            Write(saved.address, saved.bytes, saved.length);
        }
    }

    DWORD Method(void* object, DWORD slot) {
        DWORD table = Read<DWORD>(object, 0);
        return (table >= CODE_END && table < IMAGE_END) ? Read<DWORD>((const void*)table, slot) : 0;
    }

    void CallVirtual(void* object, DWORD slot) {
        DWORD method = Method(object, slot);
        if (method != 0) ((VoidMethod)method)(object);
    }

    void CallVirtualWith(void* object, DWORD slot, int value) {
        DWORD method = Method(object, slot);
        if (method != 0) ((IntMethod)method)(object, value);
    }

}

namespace {

    void Write(DWORD address, const void* bytes, size_t length) {
        DWORD oldProtect = 0;
        VirtualProtect((void*)address, length, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)address, bytes, length);
        VirtualProtect((void*)address, length, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), (void*)address, length);
    }

    void Save(DWORD address, const void* bytes, size_t length) {
        SavedPatch& saved = gSaved[gSavedCount++];
        saved.address = address;
        saved.length = length;
        memcpy(saved.bytes, bytes, length);
    }

}
