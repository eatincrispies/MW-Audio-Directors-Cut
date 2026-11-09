#include "MusicFlow.hpp"
#include "SFXCTL_Pathfinder.hpp"
#include "../Memory.hpp"

namespace {

    #define FLOW_VTABLE                      0x008B1F4C
    #define FLOW_UPDATE_SLOT                 (FLOW_VTABLE + 0x04)
    #define FLOW_CHANGE_TO_SLOT              (FLOW_VTABLE + 0x0C)
    #define FLOW_UPDATE                      0x0071F4A0
    #define FLOW_CHANGE_TO                   0x00704C00
    #define SOUND_AI                         0x00993CC8

    #define FLOW_STATE                       0x004
    #define SOUND_AI_PURSUIT                 0x130
    #define PURSUIT_STATUS                   0x114
    #define PURSUIT_BUSTED_TIMER             0x0DC

    #define MS_PER_SECOND                    1000
    #define BUST_STARTED_MS                  30
    #define BUST_CLOSING_IN_MS               1500
    #define BUST_CLEAR_UPDATES               20

    typedef void (__thiscall* FlowUpdateFunction)(void* flow);
    typedef void (__thiscall* FlowChangeToFunction)(void* flow, int state);
    typedef int  (__thiscall* PursuitStatusFunction)(void* pursuit);

    int gBustClear = 0;

    void* GetPursuit() {
        void* soundAI = Memory::Read<void*>((const void*)SOUND_AI, 0);
        return Memory::Read<void*>(soundAI, SOUND_AI_PURSUIT);
    }

    int FlowState(void* flow) {
        return Memory::Read<int>(flow, FLOW_STATE);
    }

    void ChangeTo(void* flow, int state) {
        ((FlowChangeToFunction)FLOW_CHANGE_TO)(flow, state);
    }

    bool IsPursuitMusic(int state) {
        return state == MusicFlow::STATE_CHASE || state == MusicFlow::STATE_CLOSING_IN || state == MusicFlow::STATE_PULLING_AWAY;
    }

    bool IsTrackMusic(int state) {
        return state == MusicFlow::STATE_CHASE || state == MusicFlow::STATE_PULLING_AWAY;
    }

    bool InCooldown() {
        void* pursuit = GetPursuit();
        DWORD method = Memory::Method(pursuit, PURSUIT_STATUS);
        return method != 0 && ((PursuitStatusFunction)method)(pursuit) == MusicFlow::PURSUIT_COOLDOWN;
    }

    int BustTimerMs() {
        return (int)(Memory::Read<float>(GetPursuit(), PURSUIT_BUSTED_TIMER) * MS_PER_SECOND);
    }

    void __fastcall FlowChangeToHook(void* flow, void*, int state) {
        if (state != FlowState(flow) && SFXCTL_Pathfinder::GamePaused()) return;
        if (IsPursuitMusic(state) && InCooldown()) state = MusicFlow::STATE_COOLDOWN;
        bool switchesPart = IsTrackMusic(state) || (state == MusicFlow::STATE_CLOSING_IN && BustTimerMs() < BUST_CLOSING_IN_MS);
        if (switchesPart && IsTrackMusic(FlowState(flow)) && SFXCTL_Pathfinder::TrackActive()) return;
        ChangeTo(flow, state);
    }

    void __fastcall FlowUpdateHook(void* flow, void*) {
        ((FlowUpdateFunction)FLOW_UPDATE)(flow);
        int state = FlowState(flow);
        if (!IsPursuitMusic(state) || SFXCTL_Pathfinder::GamePaused()) return;
        if (InCooldown()) {
            ChangeTo(flow, MusicFlow::STATE_COOLDOWN);
            return;
        }
        if (!SFXCTL_Pathfinder::TrackActive()) {
            gBustClear = 0;
            return;
        }
        if (IsTrackMusic(state)) {
            gBustClear = 0;
            if (BustTimerMs() >= BUST_CLOSING_IN_MS) ChangeTo(flow, MusicFlow::STATE_CLOSING_IN);
            return;
        }
        gBustClear = (BustTimerMs() > BUST_STARTED_MS) ? 0 : gBustClear + 1;
        if (gBustClear < BUST_CLEAR_UPDATES) return;
        gBustClear = 0;
        ChangeTo(flow, MusicFlow::STATE_CHASE);
    }

}

namespace MusicFlow {

    bool Init() {
        int saved = Memory::Patches();
        bool patched = Memory::PatchPointer(FLOW_CHANGE_TO_SLOT, FLOW_CHANGE_TO, (const void*)&FlowChangeToHook)
                    && Memory::PatchPointer(FLOW_UPDATE_SLOT, FLOW_UPDATE, (const void*)&FlowUpdateHook);
        if (!patched) Memory::Unpatch(saved);
        return patched;
    }

}
