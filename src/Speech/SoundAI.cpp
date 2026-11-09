#include "SoundAI.hpp"
#include "../Config.hpp"
#include "../Memory.hpp"
#include <stdlib.h>
#include <string.h>

namespace {

    #define WORLD_TIMER                      0x00925AE8
    #define TICKS_PER_SECOND                 4000

    #define ON_COLLISION_REACTION            0x0071C383
    #define ON_COLLISION_DONE                0x0071C910
    #define PREVALIDATE_POST_VALIDATE        0x0070CD1A
    #define GETNEXTEVENT_POST_VALIDATE       0x00711AAD
    #define SERVICE_DECISION                 0x00714112
    #define SERVICE_PLAY                     0x0071411A
    #define SERVICE_REQUEUE                  0x00714272
    #define HELI_UPDATE_SITE                 0x007213DB
    #define SPEECH_UPDATE_SITE               0x004D089E
    #define PERP_BUSTED_SITE                 0x00700FB6
    #define NIS_CLEAR_SITE                   0x006F73B7
    #define NIS_REPLY_SITE                   0x006F73CB

    #define MANAGER_POST_VALIDATE            0x0070C600
    #define MANAGER_UPDATE                   0x00709A70
    #define MANAGER_CLEAR_PLAYBACK           0x0070E570
    #define MANAGER_COP_SPEECH               0x00992230
    #define DISPATCH_ARREST_REPLY            0x00717110
    #define EAX_IS_PAUSED                    0x004AE8B0
    #define SND_STRM_IS_PLAYING              0x004B21B0
    #define SND_STRM_GET_STATUS              0x004B2450
    #define SND_STRM_GET_REQUEST_STATUS      0x004B2480
    #define SND_STRM_PURGE                   0x004B2580
    #define SOUND_AI                         0x00993CC8
    #define FIND_CLOSEST_COP                 0x00708390
    #define STRING_TO_KEY                    0x004549B0
    #define EVENT_ID_FROM_KEY                0x004BA9B0
    #define SPEECH_EVENT_TABLE               0x009933F0

    #define SOUND_AI_DISPATCH                0x0D8
    #define SOUND_AI_HELI                    0x0E0
    #define SOUND_AI_PLAYER_SPEED            0x108
    #define SOUND_AI_PURSUIT                 0x130
    #define SOUND_AI_PURSUIT_STATE           0x1D4
    #define COLLISION_IMPULSE_A              0x05C
    #define COLLISION_IMPULSE_B              0x06C
    #define COLLISION_INFO_ON_STACK          0x0BC
    #define EVENT_ID                         0x008
    #define COP_DESTROYED                    0x034
    #define GAME_SPEECH_FLAGS                0x028
    #define GAME_SPEECH_CHANNEL              0x02C
    #define GAME_SPEECH_CURRENT_EVENT        0x088
    #define GAME_SPEECH_BUSY                 0x008
    #define CHANNEL_STREAM                   0x004
    #define PERP_FLAGS                       0x054
    #define PERP_BUSTED_FLAG                 0x080

    #define COP_INTERRUPT_RAM                0x010
    #define COP_INTERRUPT_RAM_HIGH           0x014
    #define COP_GET_HANDLE                   0x028
    #define COP_UPDATE                       0x054
    #define COP_ARREST                       0x0E0
    #define COP_CALL_FOR_EV                  0x0FC
    #define COP_UNIT_DISABLED                0x100
    #define COP_REAR_ENDED                   0x178
    #define COP_HEAD_ON                      0x17C
    #define COP_SIDE_SWIPED                  0x180
    #define COP_T_BONED                      0x184
    #define PURSUIT_COPS_DESTROYED           0x098

    #define EVENT_CALL_FOR_EV                112
    #define EVENT_INTERRUPT_RAM              186
    #define EVENT_INTERRUPT_RAM_HIGH         198
    #define ARREST_EVENT_NAME                "Arrest_Arrest"
    #define NO_EVENT                         (-1)

    #define CHECK_EXPIRY                     0x001
    #define CHECK_MAX_PLAYBACK               0x002
    #define CHECK_INTERVAL                   0x008
    #define CHECK_DEP_FOLLOW                 0x020
    #define CHECK_ON_SCREEN_ONLY             0x040
    #define CHECK_PLAYER_SPEED               0x200

    #define RAMMED_KPH                       20
    #define RAMMED_HIGH_KPH                  35
    #define INTERRUPT_RAM_KPH                35
    #define INTERRUPT_RAM_PERCENT            75
    #define INTERRUPT_RAM_HIGH_KPH           55
    #define CUT_IN_PERCENT                   75
    #define SCREAM_GAP_TICKS                 (3 * TICKS_PER_SECOND)
    #define CUT_IN_TICKS                     (3 * TICKS_PER_SECOND)
    #define CALL_FOR_EV_TICKS                (15 * TICKS_PER_SECOND)
    #define STUCK_PLAYING_TICKS              (TICKS_PER_SECOND / 4)
    #define STUCK_WAITING_TICKS              TICKS_PER_SECOND
    #define PILEUP_WRECKS                    3
    #define PILEUP_TICKS                     (20 * TICKS_PER_SECOND)
    #define WRECK_SETTLE_TICKS               (TICKS_PER_SECOND / 2)
    #define HELI_WRECK_MATCH_TICKS           (2 * TICKS_PER_SECOND)
    #define UNIT_DOWN_GAP_TICKS              (10 * TICKS_PER_SECOND)
    #define EMS_GAP_TICKS                    (30 * TICKS_PER_SECOND)
    #define EMS_REPEAT_TICKS                 (120 * TICKS_PER_SECOND)
    #define ARREST_WAIT_MS                   8000
    #define ARREST_HOLD_MS                   6000
    #define SPEECH_RUNNING_MS                500

    #define KPH_PER_METRE_PER_SECOND         3.6f
    #define KPH_PER_MPH                      1.609344f

    const BYTE kReactionCode[]               = { 0xD9, 0x44, 0x24, 0x1C, 0xD8, 0x1D, 0xEC, 0x10, 0x89, 0x00 };
    const BYTE kDoneCode[]                   = { 0x5F, 0x5D, 0x8B, 0x8C, 0x24, 0xA4, 0x00, 0x00, 0x00 };
    const BYTE kPreValidateCallCode[]        = { 0xE8, 0xE1, 0xF8, 0xFF, 0xFF };
    const BYTE kGetNextEventCallCode[]       = { 0xE8, 0x4E, 0xAB, 0xFF, 0xFF };
    const BYTE kDecisionCode[]               = { 0x84, 0xDB, 0x0F, 0x84, 0x58, 0x01, 0x00, 0x00 };
    const BYTE kPlayCode[]                   = { 0x8B, 0x5C, 0x24, 0x18 };
    const BYTE kRequeueCode[]                = { 0x68, 0xD0, 0x33, 0x99, 0x00 };
    const BYTE kHeliUpdateCode[]             = { 0x8B, 0x8D, 0xE0, 0x00, 0x00, 0x00, 0x8B, 0x01, 0xFF, 0x50, 0x54 };
    const BYTE kSpeechUpdateCode[]           = { 0xE8, 0xCD, 0x91, 0x23, 0x00 };
    const BYTE kPerpBustedCode[]             = { 0x81, 0x4E, 0x54, 0x80, 0x00, 0x00, 0x00 };
    const BYTE kNisClearCode[]               = { 0xE8, 0xB4, 0x71, 0x01, 0x00 };
    const BYTE kNisReplyCode[]               = { 0xE8, 0x40, 0xFD, 0x01, 0x00 };

    typedef void          (__thiscall* VoidMethod)(void* object);
    typedef int           (__thiscall* IntQuery)(void* object);
    typedef bool          (__thiscall* BoolQuery)(void* object);
    typedef void*         (__thiscall* PointerQuery)(void* object);
    typedef void*         (__thiscall* FindCopMethod)(void* soundAI, bool enforceLOS, bool includeHeli);
    typedef int           (__thiscall* EventIDMethod)(void* table, unsigned int key);
    typedef int           (__thiscall* StreamStatusMethod)(void* stream, SndStrmWrapper::StreamStatus* status);
    typedef int           (__thiscall* RequestStatusMethod)(void* stream, int request, SndStrmWrapper::RequestStatus* status);
    typedef int           (__cdecl* PostValidateFunction)(void* event, unsigned int mask);
    typedef void          (__cdecl* UpdateFunction)(float timestep);
    typedef void          (__cdecl* VoidFunction)(void);
    typedef bool          (__cdecl* PausedFunction)(void);
    typedef unsigned int  (__cdecl* KeyFunction)(const char* name);

    DWORD gDone                              = ON_COLLISION_DONE;
    DWORD gServicePlay                       = SERVICE_PLAY;
    DWORD gServiceRequeue                    = SERVICE_REQUEUE;
    bool  gCutInPatched                      = false;
    int   gCutInID                           = 0;
    int   gCutInUntil                        = 0;
    int   gScreamUntil                       = 0;
    bool  gCallingForEV                      = false;
    bool  gCallingForHeli                    = false;
    int   gCallForEVUntil                    = 0;
    int   gEMSUntil                          = 0;
    int   gPileupUntil                       = 0;
    int   gHeliDownUntil                     = 0;
    int   gUnitDownUntil                     = 0;
    void* gGasStationEvent                   = NULL;
    int   gGasStationKeptAt                  = 0;
    int   gGasStationUntil                   = 0;
    void* gDownedHeli                        = NULL;
    void* gWatchedPursuit                    = NULL;
    int   gCopsDestroyed                     = 0;
    int   gWreckTimes[PILEUP_WRECKS];
    int   gWreckCount                        = 0;
    int   gNewWrecks                         = 0;
    int   gNewWrecksAt                       = 0;
    int   gHeliWrecksPending                 = 0;
    int   gHeliWreckAt                       = 0;
    int   gArrestID                          = NO_EVENT;
    bool  gArrestCalled                      = false;
    bool  gArrestStarted                     = false;
    DWORD gArrestCalledAt                    = 0;
    bool  gHolding                           = false;
    bool  gReplyHeld                         = false;
    DWORD gHoldSince                         = 0;
    DWORD gSpeechRanAt                       = 0;
    bool  gStalling                          = false;
    int   gStallSince                        = 0;
    int   gStallPlayed                       = 0;
    int   gStallRequest                      = 0;

    int   Now();
    int   TicksSince(int start);
    bool  TimeBefore(int until);
    void* GetSoundAI();
    void* FindGroundCop(void* soundAI);
    int   CurrentEventID();

    DWORD GetRammedSlot(int impact) {
        switch (impact) {
            case EAXCop::IMPACT_REAR_ENDED:  return COP_REAR_ENDED;
            case EAXCop::IMPACT_T_BONED:     return COP_T_BONED;
            case EAXCop::IMPACT_HEAD_ON:     return COP_HEAD_ON;
            case EAXCop::IMPACT_SIDE_SWIPED: return COP_SIDE_SWIPED;
        }
        return 0;
    }

    SoundAI::Reaction PickReaction(float kph, float playerKph, bool hasLine) {
        const Config::Settings& s = Config::gSettings;
        if (kph >= INTERRUPT_RAM_HIGH_KPH && playerKph >= s.InterruptRamHighMinPlayerSpeed) return SoundAI::REACT_INTERRUPT_RAM_HIGH;
        if (kph >= INTERRUPT_RAM_KPH && playerKph >= s.InterruptRamMinPlayerSpeed && rand() % 100 < INTERRUPT_RAM_PERCENT) return SoundAI::REACT_INTERRUPT_RAM;
        if (kph >= RAMMED_HIGH_KPH) return hasLine ? SoundAI::REACT_RAMMED_HIGH : SoundAI::REACT_NONE;
        if (kph >= RAMMED_KPH) return hasLine ? SoundAI::REACT_RAMMED_NORMAL : SoundAI::REACT_NONE;
        return SoundAI::REACT_NONE;
    }

    void CutIn(int eventID) {
        gCutInID = eventID;
        gCutInUntil = Now() + CUT_IN_TICKS;
    }

    bool __cdecl EventCutsIn(const void* event) {
        if (gCutInID == 0 || !TimeBefore(gCutInUntil)) return false;
        return Memory::Read<int>(event, EVENT_ID) == gCutInID;
    }

    void __cdecl React(void* cop, const void* info, int impact) {
        float kph = (Memory::Read<float>(info, COLLISION_IMPULSE_A) + Memory::Read<float>(info, COLLISION_IMPULSE_B)) * KPH_PER_METRE_PER_SECOND;
        float mph = Memory::Read<float>(GetSoundAI(), SOUND_AI_PLAYER_SPEED);
        float playerKph = ((mph < 0) ? -mph : mph) * KPH_PER_MPH;
        DWORD rammedSlot = GetRammedSlot(impact);
        SoundAI::Reaction reaction = PickReaction(kph, playerKph, rammedSlot != 0);
        bool scream = (reaction == SoundAI::REACT_INTERRUPT_RAM || reaction == SoundAI::REACT_INTERRUPT_RAM_HIGH);
        if (scream && TimeBefore(gScreamUntil)) {
            reaction = (rammedSlot != 0) ? SoundAI::REACT_RAMMED_HIGH : SoundAI::REACT_NONE;
            scream = false;
        }
        if (scream) {
            gScreamUntil = Now() + SCREAM_GAP_TICKS;
            if (gCutInPatched && rand() % 100 < CUT_IN_PERCENT)
                CutIn((reaction == SoundAI::REACT_INTERRUPT_RAM_HIGH) ? EVENT_INTERRUPT_RAM_HIGH : EVENT_INTERRUPT_RAM);
        }

        switch (reaction) {
            case SoundAI::REACT_RAMMED_NORMAL:      Memory::CallVirtualWith(cop, rammedSlot, EAXCop::INTENSITY_NORMAL); break;
            case SoundAI::REACT_RAMMED_HIGH:        Memory::CallVirtualWith(cop, rammedSlot, EAXCop::INTENSITY_HIGH); break;
            case SoundAI::REACT_INTERRUPT_RAM:      Memory::CallVirtual(cop, COP_INTERRUPT_RAM); break;
            case SoundAI::REACT_INTERRUPT_RAM_HIGH: Memory::CallVirtual(cop, COP_INTERRUPT_RAM_HIGH); break;
            default:                                break;
        }
    }

    bool CallForEV(void* cop, EAXCop::EVType type) {
        if (cop == NULL || TimeBefore(gEMSUntil)) return false;
        gCallingForEV = true;
        gCallingForHeli = (type == EAXCop::EV_HELI_DOWN);
        gCallForEVUntil = Now() + CALL_FOR_EV_TICKS;
        gEMSUntil = Now() + EMS_GAP_TICKS;
        Memory::CallVirtualWith(cop, COP_CALL_FOR_EV, type);
        return true;
    }

    bool KeptGasStation(const void* event) {
        return event == gGasStationEvent && TicksSince(gGasStationKeptAt) < CALL_FOR_EV_TICKS;
    }

    bool ArrestPending() {
        return gArrestCalled && gArrestID != NO_EVENT && GetTickCount() - gArrestCalledAt < ARREST_WAIT_MS;
    }

    bool ArrestSpeaking() {
        if (!ArrestPending()) return false;
        bool current = (CurrentEventID() == gArrestID);
        if (current) gArrestStarted = true;
        return current || !gArrestStarted;
    }

    int __cdecl PostValidate(void* event, unsigned int mask) {
        int id = Memory::Read<int>(event, EVENT_ID);
        bool ours = false;
        if (id == gArrestID && ArrestPending()) {
            mask &= CHECK_MAX_PLAYBACK;
        } else if (id == EVENT_INTERRUPT_RAM || id == EVENT_INTERRUPT_RAM_HIGH) {
            mask &= ~(CHECK_PLAYER_SPEED | CHECK_INTERVAL);
            if (EventCutsIn(event)) mask &= CHECK_EXPIRY | CHECK_MAX_PLAYBACK | CHECK_ON_SCREEN_ONLY;
        } else if (id == EVENT_CALL_FOR_EV) {
            ours = gCallingForEV && TimeBefore(gCallForEVUntil);
            if (ours) {
                mask &= ~(CHECK_EXPIRY | CHECK_DEP_FOLLOW);
                if (gCallingForHeli) mask &= ~CHECK_INTERVAL;
            } else if (!KeptGasStation(event) && (TimeBefore(gGasStationUntil) || TimeBefore(gEMSUntil))) {
                return SoundAI::POST_VALIDATE_DITCH;
            }
        }
        int result = ((PostValidateFunction)MANAGER_POST_VALIDATE)(event, mask);
        if (id == EVENT_CALL_FOR_EV && !ours && result == SoundAI::POST_VALIDATE_KEEP && !KeptGasStation(event)) {
            gGasStationEvent = event;
            gGasStationKeptAt = Now();
            gGasStationUntil = Now() + EMS_REPEAT_TICKS;
            gEMSUntil = Now() + EMS_GAP_TICKS;
        }
        return result;
    }

    void __cdecl UpdateHeli(void* soundAI) {
        void* heli = Memory::Read<void*>(soundAI, SOUND_AI_HELI);
        bool wasDown = Memory::Read<bool>(heli, COP_DESTROYED);
        Memory::CallVirtual(heli, COP_UPDATE);
        DWORD method = Memory::Method(heli, COP_GET_HANDLE);
        void* handle = (method != 0) ? ((PointerQuery)method)(heli) : NULL;
        if (!Memory::Read<bool>(heli, COP_DESTROYED)) {
            if (handle == gDownedHeli) gDownedHeli = NULL;
            return;
        }
        if (wasDown || handle == gDownedHeli) return;

        gDownedHeli = handle;
        ++gHeliWrecksPending;
        gHeliWreckAt = Now();
        if (!Config::gSettings.CallForEVHeliDown || TimeBefore(gHeliDownUntil)) return;
        if (CallForEV(FindGroundCop(soundAI), EAXCop::EV_HELI_DOWN)) gHeliDownUntil = Now() + EMS_REPEAT_TICKS;
    }

    int RememberWrecks(int wrecks) {
        for (int i = 0; i < wrecks; ++i) {
            if (gWreckCount == PILEUP_WRECKS) {
                memmove(gWreckTimes, gWreckTimes + 1, sizeof(int) * (PILEUP_WRECKS - 1));
                --gWreckCount;
            }
            gWreckTimes[gWreckCount++] = Now();
        }
        int recent = 0;
        for (int i = 0; i < gWreckCount; ++i)
            if (TicksSince(gWreckTimes[i]) <= PILEUP_TICKS) ++recent;
        return recent;
    }

    void WatchUnitsDown() {
        void* soundAI = GetSoundAI();
        void* pursuit = (soundAI != NULL && Memory::Read<int>(soundAI, SOUND_AI_PURSUIT_STATE) == 0) ? Memory::Read<void*>(soundAI, SOUND_AI_PURSUIT) : NULL;
        DWORD method = Memory::Method(pursuit, PURSUIT_COPS_DESTROYED);
        int destroyed = (method != 0) ? ((IntQuery)method)(pursuit) : 0;
        if (gHeliWrecksPending > 0 && TicksSince(gHeliWreckAt) > HELI_WRECK_MATCH_TICKS) gHeliWrecksPending = 0;
        if (pursuit != gWatchedPursuit || destroyed < gCopsDestroyed) {
            gWatchedPursuit = pursuit;
            gCopsDestroyed = destroyed;
            gWreckCount = 0;
            gNewWrecks = 0;
            gHeliWrecksPending = 0;
            return;
        }
        if (destroyed > gCopsDestroyed) {
            if (gNewWrecks == 0) gNewWrecksAt = Now();
            gNewWrecks += destroyed - gCopsDestroyed;
            gCopsDestroyed = destroyed;
        }
        if (gNewWrecks == 0 || TicksSince(gNewWrecksAt) < WRECK_SETTLE_TICKS) return;

        int wrecks = gNewWrecks;
        gNewWrecks = 0;
        while (wrecks > 0 && gHeliWrecksPending > 0) {
            --wrecks;
            --gHeliWrecksPending;
        }
        if (wrecks == 0) return;

        int recent = RememberWrecks(wrecks);
        void* cop = FindGroundCop(soundAI);
        if (cop == NULL) return;
        if (recent >= PILEUP_WRECKS && Config::gSettings.CallForEVUnitsDown && !TimeBefore(gPileupUntil) && CallForEV(cop, EAXCop::EV_MULTIPLE_UNITS_DOWN)) {
            gWreckCount = 0;
            gPileupUntil = Now() + EMS_REPEAT_TICKS;
        } else if (!TimeBefore(gUnitDownUntil)) {
            gUnitDownUntil = Now() + UNIT_DOWN_GAP_TICKS;
            Memory::CallVirtualWith(cop, COP_UNIT_DISABLED, EAXCop::ABOUT_OTHER_UNIT);
        }
    }

    void __cdecl ArrestAtBust(void* soundAI) {
        void* cop = FindGroundCop(soundAI);
        if (cop == NULL) return;
        if (gArrestID == NO_EVENT) gArrestID = ((EventIDMethod)EVENT_ID_FROM_KEY)((void*)SPEECH_EVENT_TABLE, ((KeyFunction)STRING_TO_KEY)(ARREST_EVENT_NAME));
        if (gArrestID == NO_EVENT) return;

        gArrestCalled = true;
        gArrestStarted = false;
        gArrestCalledAt = GetTickCount();
        if (gCutInPatched) CutIn(gArrestID);
        Memory::CallVirtual(cop, COP_ARREST);
    }

    void __cdecl ClearOrHold(void) {
        if (gHolding) return;
        if (GetTickCount() - gSpeechRanAt < SPEECH_RUNNING_MS && ArrestSpeaking()) {
            gHolding = true;
            gReplyHeld = false;
            gHoldSince = GetTickCount();
            return;
        }
        ((VoidFunction)MANAGER_CLEAR_PLAYBACK)();
    }

    void __cdecl ReplyOrHold(void* dispatch) {
        if (gHolding) {
            gReplyHeld = true;
            return;
        }
        ((VoidMethod)DISPATCH_ARREST_REPLY)(dispatch);
    }

    void WatchArrest() {
        gSpeechRanAt = GetTickCount();
        bool speaking = ArrestSpeaking();
        if (!gHolding) {
            if (gArrestCalled && !ArrestPending()) gArrestCalled = false;
            return;
        }
        if (speaking && GetTickCount() - gHoldSince < ARREST_HOLD_MS) return;

        gHolding = false;
        gArrestCalled = false;
        ((VoidFunction)MANAGER_CLEAR_PLAYBACK)();
        if (!gReplyHeld) return;
        gReplyHeld = false;
        void* dispatch = Memory::Read<void*>(GetSoundAI(), SOUND_AI_DISPATCH);
        if (dispatch != NULL) ((VoidMethod)DISPATCH_ARREST_REPLY)(dispatch);
    }

    void PollRadioTimeout() {
        void* speech = *(void* volatile*)MANAGER_COP_SPEECH;
        BYTE* channel = Memory::Read<BYTE*>(speech, GAME_SPEECH_CHANNEL);
        void* stream = (channel != NULL) ? channel + CHANNEL_STREAM : NULL;
        bool busy = (Memory::Read<unsigned int>(speech, GAME_SPEECH_FLAGS) & GAME_SPEECH_BUSY) != 0;
        if (stream == NULL || !busy || ((PausedFunction)EAX_IS_PAUSED)() || !((BoolQuery)SND_STRM_IS_PLAYING)(stream)) {
            gStalling = false;
            return;
        }

        SndStrmWrapper::StreamStatus status = { 0, 0, 0 };
        SndStrmWrapper::RequestStatus request = { 0, 0, 0, 0 };
        if (((StreamStatusMethod)SND_STRM_GET_STATUS)(stream, &status) < 0 || status.requests <= 0
            || ((RequestStatusMethod)SND_STRM_GET_REQUEST_STATUS)(stream, status.head, &request) < 0) {
            gStalling = false;
            return;
        }

        if (!gStalling || request.played != gStallPlayed || status.head != gStallRequest) {
            gStalling = true;
            gStallPlayed = request.played;
            gStallRequest = status.head;
            gStallSince = Now();
            return;
        }
        int limit = (request.state == SndStrmWrapper::REQUEST_PLAYING) ? STUCK_PLAYING_TICKS : STUCK_WAITING_TICKS;
        if (TicksSince(gStallSince) < limit) return;

        ((IntQuery)SND_STRM_PURGE)(stream);
        gStalling = false;
    }

    void __cdecl SpeechUpdate(float timestep) {
        ((UpdateFunction)MANAGER_UPDATE)(timestep);
        PollRadioTimeout();
        WatchUnitsDown();
        WatchArrest();
    }

    __declspec(naked) void OnCollisionReaction() {
        __asm {
            mov  eax, esp
            push edi
            push dword ptr [eax + COLLISION_INFO_ON_STACK]
            push ebp
            call React
            add  esp, 12
            jmp  dword ptr [gDone]
        }
    }

    __declspec(naked) void ServiceDecision() {
        __asm {
            test bl, bl
            jnz  play
            push ebp
            call EventCutsIn
            add  esp, 4
            test al, al
            jnz  play
            jmp  dword ptr [gServiceRequeue]
        play:
            jmp  dword ptr [gServicePlay]
        }
    }

    __declspec(naked) void HeliUpdateCall() {
        __asm {
            push ebp
            call UpdateHeli
            add  esp, 4
            ret
        }
    }

    __declspec(naked) void PerpBustedCall() {
        __asm {
            or   dword ptr [esi + PERP_FLAGS], PERP_BUSTED_FLAG
            push esi
            call ArrestAtBust
            add  esp, 4
            ret
        }
    }

    __declspec(naked) void NisReplyCall() {
        __asm {
            push ecx
            call ReplyOrHold
            add  esp, 4
            ret
        }
    }

}

namespace SoundAI {

    bool Init() {
        srand(GetTickCount());

        if (!Memory::Matches(ON_COLLISION_DONE, kDoneCode, sizeof(kDoneCode))
            || !Memory::PatchBranch(ON_COLLISION_REACTION, kReactionCode, sizeof(kReactionCode), OPCODE_JUMP, (const void*)&OnCollisionReaction))
            return false;

        int saved = Memory::Patches();
        bool rules = Memory::PatchBranch(PREVALIDATE_POST_VALIDATE, kPreValidateCallCode, sizeof(kPreValidateCallCode), OPCODE_CALL, (const void*)&PostValidate)
                  && Memory::PatchBranch(GETNEXTEVENT_POST_VALIDATE, kGetNextEventCallCode, sizeof(kGetNextEventCallCode), OPCODE_CALL, (const void*)&PostValidate);
        if (!rules) Memory::Unpatch(saved);

        gCutInPatched = rules
                     && Memory::Matches(SERVICE_PLAY, kPlayCode, sizeof(kPlayCode))
                     && Memory::Matches(SERVICE_REQUEUE, kRequeueCode, sizeof(kRequeueCode))
                     && Memory::PatchBranch(SERVICE_DECISION, kDecisionCode, sizeof(kDecisionCode), OPCODE_JUMP, (const void*)&ServiceDecision);

        Memory::PatchBranch(HELI_UPDATE_SITE, kHeliUpdateCode, sizeof(kHeliUpdateCode), OPCODE_CALL, (const void*)&HeliUpdateCall);
        bool speech = Memory::PatchBranch(SPEECH_UPDATE_SITE, kSpeechUpdateCode, sizeof(kSpeechUpdateCode), OPCODE_CALL, (const void*)&SpeechUpdate);

        saved = Memory::Patches();
        bool arrest = speech
                   && Memory::PatchBranch(NIS_CLEAR_SITE, kNisClearCode, sizeof(kNisClearCode), OPCODE_CALL, (const void*)&ClearOrHold)
                   && Memory::PatchBranch(NIS_REPLY_SITE, kNisReplyCode, sizeof(kNisReplyCode), OPCODE_CALL, (const void*)&NisReplyCall)
                   && Memory::PatchBranch(PERP_BUSTED_SITE, kPerpBustedCode, sizeof(kPerpBustedCode), OPCODE_CALL, (const void*)&PerpBustedCall);
        if (!arrest) Memory::Unpatch(saved);
        return true;
    }

}

namespace {

    int Now() {
        return *(volatile int*)WORLD_TIMER;
    }

    int TicksSince(int start) {
        return (int)((unsigned int)Now() - (unsigned int)start);
    }

    bool TimeBefore(int until) {
        return TicksSince(until) < 0;
    }

    void* GetSoundAI() {
        return *(void* volatile*)SOUND_AI;
    }

    void* FindGroundCop(void* soundAI) {
        FindCopMethod find = (FindCopMethod)FIND_CLOSEST_COP;
        void* cop = find(soundAI, true, false);
        return (cop != NULL) ? cop : find(soundAI, false, false);
    }

    int CurrentEventID() {
        void* speech = *(void* volatile*)MANAGER_COP_SPEECH;
        void* event = Memory::Read<void*>(speech, GAME_SPEECH_CURRENT_EVENT);
        return (event != NULL) ? Memory::Read<int>(event, EVENT_ID) : NO_EVENT;
    }

}
