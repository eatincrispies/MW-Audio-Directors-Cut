#include <windows.h>
#include "Music/SFXCTL_Pathfinder.hpp"
#include "Music/MusicFlow.hpp"
#include "Music/SFXObj_PFEATrax.hpp"
#include "Speech/SoundAI.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        SFXCTL_Pathfinder::Init(module);
        MusicFlow::Init();
        SFXObj_PFEATrax::Init();
        SoundAI::Init(module);
        break;

    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) {
            SoundAI::Restore();
            SFXObj_PFEATrax::Restore();
            MusicFlow::Restore();
            SFXCTL_Pathfinder::Restore();
        }
        break;
    }
    return TRUE;
}
