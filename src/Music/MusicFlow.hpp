#pragma once
#include "SFXCTL_Pathfinder.hpp"

namespace MusicFlow {

    bool Init();
    void Restore();

}

namespace {

    constexpr int    kFlowUpdateSlot = 1;
    constexpr int    kFlowChangeToSlot = 3;
    constexpr size_t kFlowStateOffset = 4;
    constexpr int    kFlowChase = 0;
    constexpr int    kFlowClosingIn = 1;
    constexpr int    kFlowPullingAway = 2;
    constexpr int    kFlowCoolDown = 3;
    constexpr int    kPursuitCoolDown = 2;
    constexpr size_t kPursuitBustedTimer = 0xDC;
    constexpr float  kBustStarted = 0.03f;
    constexpr float  kBustClosingIn = 1.5f;
    constexpr int    kBustClearUpdates = 20;

    using FlowUpdateFn    = void(__thiscall*)(void* flow);
    using FlowChangeToFn  = void(__thiscall*)(void* flow, int state);
    using PursuitStatusFn = int(__thiscall*)(void* pursuit);

    FlowUpdateFn   gFlowUpdate = nullptr;
    FlowChangeToFn gFlowChangeTo = nullptr;
    bool           gFlowPatched = false;
    int            gBustClear = 0;

    uintptr_t FlowSlot(int slot) {
        return gSettings.FlowVTable + sizeof(uint32_t) * static_cast<uintptr_t>(slot);
    }

    int FlowState(void* flow) {
        return *reinterpret_cast<const int*>(static_cast<const uint8_t*>(flow) + kFlowStateOffset);
    }

    bool IsPursuitMusic(int state) {
        return state == kFlowChase || state == kFlowClosingIn || state == kFlowPullingAway;
    }

    bool IsTrackMusic(int state) {
        return state == kFlowChase || state == kFlowPullingAway;
    }

    bool InCoolDown() {
        uintptr_t ai = 0;
        uintptr_t pursuit = 0;
        uintptr_t vtable = 0;
        uintptr_t status = 0;
        if (!Read(gSettings.SoundAI, &ai, sizeof(ai)) || ai == 0) return false;
        if (!Read(ai + gSettings.SoundAIPursuit, &pursuit, sizeof(pursuit)) || pursuit == 0) return false;
        if (!Read(pursuit, &vtable, sizeof(vtable)) || vtable == 0) return false;
        if (!Read(vtable + gSettings.PursuitStatus, &status, sizeof(status)) || status == 0) return false;
        return reinterpret_cast<PursuitStatusFn>(status)(reinterpret_cast<void*>(pursuit)) == kPursuitCoolDown;
    }

    float BustTimer() {
        uintptr_t ai = 0;
        uintptr_t pursuit = 0;
        float timer = 0.0f;
        if (!Read(gSettings.SoundAI, &ai, sizeof(ai)) || ai == 0) return 0.0f;
        if (!Read(ai + gSettings.SoundAIPursuit, &pursuit, sizeof(pursuit)) || pursuit == 0) return 0.0f;
        return Read(pursuit + kPursuitBustedTimer, &timer, sizeof(timer)) ? timer : 0.0f;
    }

    void __fastcall FlowChangeToHook(void* flow, void*, int state) {
        if (state != FlowState(flow) && GamePaused()) {
            LOG("paused: music state %d stays, %d waits until the game unpauses", FlowState(flow), state);
            return;
        }
        if (IsPursuitMusic(state) && InCoolDown()) {
            if (FlowState(flow) != kFlowCoolDown) LOG("music state %d asked for %d during COOLDOWN: cooldown music instead", FlowState(flow), state);
            state = kFlowCoolDown;
        }
        const bool switchesPart = IsTrackMusic(state) || (state == kFlowClosingIn && BustTimer() < kBustClosingIn);
        if (switchesPart && IsTrackMusic(FlowState(flow)) && TrackActive()) {
            static int lastKept = -1;
            if (state != lastKept) LOG("full track: music state %d asked for %d, the track plays on", FlowState(flow), state);
            lastKept = state;
            return;
        }
        gFlowChangeTo(flow, state);
    }

    void __fastcall FlowUpdateHook(void* flow, void*) {
        gFlowUpdate(flow);
        const int state = FlowState(flow);
#if defined(_DEBUG)
        static int lastState = -2;
        if (state != lastState) {
            LOG("MusicFlow state %d -> %d", lastState, state);
            lastState = state;
        }
#endif
        if (!IsPursuitMusic(state) || GamePaused()) return;
        if (InCoolDown()) {
            LOG("music state %d during COOLDOWN: cooldown music", state);
            gFlowChangeTo(flow, kFlowCoolDown);
            return;
        }
        if (!TrackActive()) {
            gBustClear = 0;
            return;
        }
        if (IsTrackMusic(state)) {
            gBustClear = 0;
            if (BustTimer() < kBustClosingIn) return;
            LOG("full track: the busted meter is %.0f%% full, the closing-in music starts", BustTimer() * 20.0f);
            gFlowChangeTo(flow, kFlowClosingIn);
            return;
        }
        gBustClear = BustTimer() > kBustStarted ? 0 : gBustClear + 1;
        if (gBustClear < kBustClearUpdates) return;
        LOG("full track: the busted meter emptied, the music goes back to the track");
        gBustClear = 0;
        gFlowChangeTo(flow, kFlowChase);
    }

    bool InstallFlowHooks() {
        uint32_t update = 0;
        uint32_t changeTo = 0;
        if (!Read(FlowSlot(kFlowUpdateSlot), &update, sizeof(update)) || update != gSettings.FlowUpdate) return false;
        if (!Read(FlowSlot(kFlowChangeToSlot), &changeTo, sizeof(changeTo)) || changeTo != gSettings.FlowChangeTo) return false;
        gFlowUpdate = reinterpret_cast<FlowUpdateFn>(update);
        gFlowChangeTo = reinterpret_cast<FlowChangeToFn>(changeTo);
        const uint32_t updateHook = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FlowUpdateHook));
        const uint32_t changeToHook = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FlowChangeToHook));
        if (!Write(FlowSlot(kFlowChangeToSlot), &changeToHook, sizeof(changeToHook))) return false;
        if (!Write(FlowSlot(kFlowUpdateSlot), &updateHook, sizeof(updateHook))) {
            Write(FlowSlot(kFlowChangeToSlot), &changeTo, sizeof(changeTo));
            return false;
        }
        return true;
    }

}

namespace MusicFlow {

    bool Init() {
        gFlowPatched = InstallFlowHooks();
        LOG(gFlowPatched ? "MusicFlow hooked: COOLDOWN always plays the cooldown music, the full track keeps playing between closing-in moments, and the music state waits while the game is paused" : "the MusicFlow vtable doesn't match, the cooldown music, full track and pausing are left as the game plays them");
        return gFlowPatched;
    }

    void Restore() {
        if (gFlowPatched) {
            Write(FlowSlot(kFlowUpdateSlot), &gSettings.FlowUpdate, sizeof(gSettings.FlowUpdate));
            Write(FlowSlot(kFlowChangeToSlot), &gSettings.FlowChangeTo, sizeof(gSettings.FlowChangeTo));
        }
        gFlowPatched = false;
    }

}
