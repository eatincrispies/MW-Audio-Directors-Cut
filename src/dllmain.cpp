#include <windows.h>
#include "Config.hpp"
#include "Memory.hpp"
#include "Music/SFXCTL_Pathfinder.hpp"
#include "Music/MusicFlow.hpp"
#include "Music/SFXObj_PFEATrax.hpp"
#include "Speech/SoundAI.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        Config::Load(module);
        SFXCTL_Pathfinder::Init();
        MusicFlow::Init();
        SFXObj_PFEATrax::Init();
        SoundAI::Init();
        break;

    case DLL_PROCESS_DETACH:
        if (reserved == NULL) Memory::Unpatch(0);
        break;
    }
    return TRUE;
}
