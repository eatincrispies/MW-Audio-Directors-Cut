#include "SoundAI.hpp"
#include <stdlib.h>
#include <string.h>

namespace {

    #define CODE_END                         0x00890000
    #define IMAGE_END                        0x009C7000
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
    #define MANAGER_COP_SPEECH               0x00992230
    #define EAX_IS_PAUSED                    0x004AE8B0
    #define SND_STRM_IS_PLAYING              0x004B21B0
    #define SND_STRM_GET_STATUS              0x004B2450
    #define SND_STRM_GET_REQUEST_STATUS      0x004B2480
    #define SND_STRM_PURGE                   0x004B2580
    #define SOUND_AI                         0x00993CC8
    #define FIND_CLOSEST_COP                 0x00708390
    #define MANAGER_CLEAR_PLAYBACK           0x0070E570
    #define DISPATCH_ARREST_REPLY            0x00717110
    #define STRING_TO_KEY                    0x004549B0
    #define EVENT_ID_FROM_KEY                0x004BA9B0
    #define SPEECH_EVENT_TABLE               0x009933F0
    #define ARREST_EVENT_NAME                "Arrest_Arrest"

    #define SOUND_AI_DISPATCH                0x0D8
    #define SOUND_AI_HELI                    0x0E0
    #define SOUND_AI_PLAYER_SPEED            0x108
    #define SOUND_AI_PURSUIT                 0x130
    #define SOUND_AI_PURSUIT_STATE           0x1D4
    #define COLLISION_IMPULSE_A              0x05C
    #define COLLISION_IMPULSE_B              0x06C
    #define EVENT_ID                         0x008
    #define COP_DESTROYED                    0x034
    #define GAME_SPEECH_FLAGS                0x028
    #define GAME_SPEECH_CHANNEL              0x02C
    #define GAME_SPEECH_CURRENT_EVENT        0x088
    #define CHANNEL_STREAM                   0x004

    #define COP_INTERRUPT_RAM                0x010
    #define COP_INTERRUPT_RAM_HIGH           0x014
    #define COP_GET_HANDLE                   0x028
    #define COP_UPDATE                       0x054
    #define COP_CALL_FOR_EV                  0x0FC
    #define COP_UNIT_DISABLED                0x100
    #define COP_ARREST                       0x0E0
    #define COP_REAR_ENDED                   0x178
    #define COP_HEAD_ON                      0x17C
    #define COP_SIDE_SWIPED                  0x180
    #define COP_T_BONED                      0x184
    #define PURSUIT_COPS_DESTROYED           0x098

    #define EVENT_CALL_FOR_EV                112
    #define EVENT_INTERRUPT_RAM              186
    #define EVENT_INTERRUPT_RAM_HIGH         198

    #define CHECK_EXPIRY                     0x001
    #define CHECK_MAX_PLAYBACK               0x002
    #define CHECK_INTERVAL                   0x008
    #define CHECK_DEP_FOLLOW                 0x020
    #define CHECK_ON_SCREEN_ONLY             0x040
    #define CHECK_PLAYER_SPEED               0x200
    #define GAME_SPEECH_BUSY                 0x008

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
    #define NO_EVENT                         (-1)
    #define POST_VALIDATE_KEEP               0
    #define POST_VALIDATE_DITCH              2
    #define MOST_KPH                         1000

    #define KPH_PER_METRE_PER_SECOND         3.6f
    #define KPH_PER_MPH                      1.609344f

    #define OPCODE_CALL                      0xE8
    #define OPCODE_JUMP                      0xE9
    #define OPCODE_NOP                       0x90
    #define BRANCH_SIZE                      5
    #define MOST_BYTES                       16
    #define MOST_PATCHES                     10

    const BYTE kReactionCode[]         = { 0xD9, 0x44, 0x24, 0x1C, 0xD8, 0x1D, 0xEC, 0x10, 0x89, 0x00 };
    const BYTE kDoneCode[]             = { 0x5F, 0x5D, 0x8B, 0x8C, 0x24, 0xA4, 0x00, 0x00, 0x00 };
    const BYTE kPreValidateCallCode[]  = { 0xE8, 0xE1, 0xF8, 0xFF, 0xFF };
    const BYTE kGetNextEventCallCode[] = { 0xE8, 0x4E, 0xAB, 0xFF, 0xFF };
    const BYTE kDecisionCode[]         = { 0x84, 0xDB, 0x0F, 0x84, 0x58, 0x01, 0x00, 0x00 };
    const BYTE kPlayCode[]             = { 0x8B, 0x5C, 0x24, 0x18 };
    const BYTE kRequeueCode[]          = { 0x68, 0xD0, 0x33, 0x99, 0x00 };
    const BYTE kHeliUpdateCode[]       = { 0x8B, 0x8D, 0xE0, 0x00, 0x00, 0x00, 0x8B, 0x01, 0xFF, 0x50, 0x54 };    const BYTE kSpeechUpdateCode[]     = { 0xE8, 0xCD, 0x91, 0x23, 0x00 };
    const BYTE kPerpBustedCode[]       = { 0x81, 0x4E, 0x54, 0x80, 0x00, 0x00, 0x00 };
    const BYTE kNisClearCode[]         = { 0xE8, 0xB4, 0x71, 0x01, 0x00 };
    const BYTE kNisReplyCode[]         = { 0xE8, 0x40, 0xFD, 0x01, 0x00 };

    typedef void  (__thiscall* VoidMethod)(void* object);
    typedef void  (__thiscall* IntMethod)(void* object, int value);
    typedef int   (__thiscall* IntQuery)(void* object);
    typedef bool  (__thiscall* BoolQuery)(void* object);
    typedef void* (__thiscall* PointerQuery)(void* object);
    typedef void* (__thiscall* FindCopMethod)(void* soundAI, bool enforceLOS, bool includeHeli);    typedef int   (__cdecl* PostValidateFunction)(void* event, unsigned int mask);
    typedef void  (__cdecl* UpdateFunction)(float timestep);
    typedef void  (__cdecl* VoidFunction)(void);
    typedef unsigned int (__cdecl* KeyFunction)(const char* name);
    typedef int   (__thiscall* EventIDMethod)(void* table, unsigned int key);
    typedef bool  (__cdecl* PausedFunction)(void);
    typedef int   (__thiscall* StreamStatusMethod)(void* stream, SndStrmWrapper::StreamStatus* status);
    typedef int   (__thiscall* RequestStatusMethod)(void* stream, int request, SndStrmWrapper::RequestStatus* status);

    struct SavedPatch {
        DWORD  address;
        size_t length;
        BYTE   bytes[MOST_BYTES];
    };

    SoundAI::Settings gSettings;
    SavedPatch        gSaved[MOST_PATCHES];
    int               gSavedCount        = 0;
    DWORD             gDone              = ON_COLLISION_DONE;
    DWORD             gServicePlay       = SERVICE_PLAY;
    DWORD             gServiceRequeue    = SERVICE_REQUEUE;
    bool              gCutInPatched      = false;
    int               gCutInID           = 0;
    int               gCutInUntil        = 0;
    int               gScreamUntil       = 0;
    bool              gCallingForEV      = false;
    bool              gCallingForHeli    = false;
    int               gCallForEVUntil    = 0;
    void*             gDownedHeli        = NULL;
    void*             gWatchedPursuit    = NULL;
    int               gCopsDestroyed     = 0;
    int               gWreckTimes[PILEUP_WRECKS];
    int               gWreckCount        = 0;
    int               gNewWrecks         = 0;
    int               gNewWrecksAt       = 0;
    int               gHeliWrecksPending = 0;
    int               gHeliWreckAt       = 0;
    int               gUnitDownUntil     = 0;
    int               gEMSUntil          = 0;
    int               gPileupUntil       = 0;
    int               gHeliDownUntil     = 0;
    void*             gGasStationEvent   = NULL;
    int               gGasStationKeptAt  = 0;
    int               gGasStationUntil   = 0;
    int               gArrestID          = NO_EVENT;
    bool              gArrestCalled      = false;
    bool              gArrestStarted     = false;
    DWORD             gArrestCalledAt    = 0;
    bool              gHolding           = false;
    bool              gReplyHeld         = false;
    DWORD             gHoldSince         = 0;
    DWORD             gSpeechRanAt       = 0;
    bool              gStalling          = false;
    int               gStallSince        = 0;
    int               gStallPlayed       = 0;
    int               gStallRequest      = 0;

    template <typename T> T Read(const void* base, DWORD offset);
    void  Write(DWORD address, const void* bytes, size_t length);
    bool  Matches(DWORD address, const BYTE* bytes, size_t length);
    bool  Patch(DWORD site, const BYTE* expected, size_t length, BYTE opcode, const void* target);
    void  Unpatch(int count);
    DWORD Method(void* object, DWORD slot);
    void  CallVirtual(void* object, DWORD slot);
    void  CallVirtualWith(void* object, DWORD slot, int value);
    int   TicksSince(int start);
    bool  TimeBefore(int until);
    void  LoadConfig(HMODULE module);

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
        if (kph >= INTERRUPT_RAM_HIGH_KPH && playerKph >= gSettings.InterruptRamHighMinPlayerSpeed)
            return SoundAI::REACT_INTERRUPT_RAM_HIGH;
        if (kph >= INTERRUPT_RAM_KPH && playerKph >= gSettings.InterruptRamMinPlayerSpeed && rand() % 100 < INTERRUPT_RAM_PERCENT)
            return SoundAI::REACT_INTERRUPT_RAM;
        if (kph >= RAMMED_HIGH_KPH) return hasLine ? SoundAI::REACT_RAMMED_HIGH : SoundAI::REACT_NONE;
        if (kph >= RAMMED_KPH) return hasLine ? SoundAI::REACT_RAMMED_NORMAL : SoundAI::REACT_NONE;
        return SoundAI::REACT_NONE;
    }

    bool __cdecl EventCutsIn(const void* event) {
        if (gCutInID == 0 || !TimeBefore(gCutInUntil)) return false;
        return Read<int>(event, EVENT_ID) == gCutInID;
    }

    void TriggerCutIn(SoundAI::Reaction reaction) {
        gCutInID = (reaction == SoundAI::REACT_INTERRUPT_RAM_HIGH) ? EVENT_INTERRUPT_RAM_HIGH : EVENT_INTERRUPT_RAM;
        gCutInUntil = *(volatile int*)WORLD_TIMER + CUT_IN_TICKS;
    }

    void* GetSoundAI() {
        return *(void* volatile*)SOUND_AI;
    }

    void* GetActivePursuit(void* soundAI) {
        if (soundAI == NULL || Read<int>(soundAI, SOUND_AI_PURSUIT_STATE) != 0) return NULL;
        return Read<void*>(soundAI, SOUND_AI_PURSUIT);
    }

    void* FindGroundCop(void* soundAI) {
        FindCopMethod find = (FindCopMethod)FIND_CLOSEST_COP;
        void* cop = find(soundAI, true, false);
        return (cop != NULL) ? cop : find(soundAI, false, false);
    }

    bool CallForEV(void* cop, EAXCop::EVType type) {
        if (cop == NULL || TimeBefore(gEMSUntil)) return false;
        int now = *(volatile int*)WORLD_TIMER;
        gCallingForEV = true;
        gCallingForHeli = (type == EAXCop::EV_HELI_DOWN);
        gCallForEVUntil = now + CALL_FOR_EV_TICKS;
        gEMSUntil = now + EMS_GAP_TICKS;
        CallVirtualWith(cop, COP_CALL_FOR_EV, type);
        return true;
    }

    bool KeptGasStation(const void* event) {
        return event == gGasStationEvent && TicksSince(gGasStationKeptAt) < CALL_FOR_EV_TICKS;
    }

    bool ArrestPending() {
        return gArrestCalled && gArrestID != NO_EVENT && GetTickCount() - gArrestCalledAt < ARREST_WAIT_MS;
    }

    int CurrentEventID() {
        void* speech = *(void* volatile*)MANAGER_COP_SPEECH;
        void* event = Read<void*>(speech, GAME_SPEECH_CURRENT_EVENT);
        return (event != NULL) ? Read<int>(event, EVENT_ID) : NO_EVENT;
    }

    bool ArrestSpeaking() {
        if (!ArrestPending()) return false;
        bool current = (CurrentEventID() == gArrestID);
        if (current) gArrestStarted = true;
        return current || !gArrestStarted;
    }

    void __cdecl React(void* cop, const void* info, int impact) {
        float kph = (Read<float>(info, COLLISION_IMPULSE_A) + Read<float>(info, COLLISION_IMPULSE_B)) * KPH_PER_METRE_PER_SECOND;
        float mph = Read<float>(GetSoundAI(), SOUND_AI_PLAYER_SPEED);
        float playerKph = (mph < 0 ? -mph : mph) * KPH_PER_MPH;
        DWORD rammedSlot = GetRammedSlot(impact);
        SoundAI::Reaction reaction = PickReaction(kph, playerKph, rammedSlot != 0);
        bool scream = (reaction == SoundAI::REACT_INTERRUPT_RAM || reaction == SoundAI::REACT_INTERRUPT_RAM_HIGH);
        if (scream && TimeBefore(gScreamUntil)) {
            reaction = (rammedSlot != 0) ? SoundAI::REACT_RAMMED_HIGH : SoundAI::REACT_NONE;
            scream = false;
        }
        if (scream) gScreamUntil = *(volatile int*)WORLD_TIMER + SCREAM_GAP_TICKS;

        if (scream && gCutInPatched && rand() % 100 < CUT_IN_PERCENT) TriggerCutIn(reaction);

        switch (reaction) {
            case SoundAI::REACT_RAMMED_NORMAL:      CallVirtualWith(cop, rammedSlot, EAXCop::INTENSITY_NORMAL); break;
            case SoundAI::REACT_RAMMED_HIGH:        CallVirtualWith(cop, rammedSlot, EAXCop::INTENSITY_HIGH); break;
            case SoundAI::REACT_INTERRUPT_RAM:      CallVirtual(cop, COP_INTERRUPT_RAM); break;
            case SoundAI::REACT_INTERRUPT_RAM_HIGH: CallVirtual(cop, COP_INTERRUPT_RAM_HIGH); break;
            default:                                break;
        }
    }

    int __cdecl PostValidate(void* event, unsigned int mask) {
        int id = Read<int>(event, EVENT_ID);
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
                return POST_VALIDATE_DITCH;
            }
        }
        int result = ((PostValidateFunction)MANAGER_POST_VALIDATE)(event, mask);
        if (id == EVENT_CALL_FOR_EV && !ours && result == POST_VALIDATE_KEEP && !KeptGasStation(event)) {
            int now = *(volatile int*)WORLD_TIMER;
            gGasStationEvent = event;
            gGasStationKeptAt = now;
            gGasStationUntil = now + EMS_REPEAT_TICKS;
            gEMSUntil = now + EMS_GAP_TICKS;
        }
        return result;
    }

    void __cdecl UpdateHeli(void* soundAI) {
        void* heli = Read<void*>(soundAI, SOUND_AI_HELI);
        bool wasDown = Read<bool>(heli, COP_DESTROYED);
        CallVirtual(heli, COP_UPDATE);
        DWORD method = Method(heli, COP_GET_HANDLE);
        void* handle = (method != 0) ? ((PointerQuery)method)(heli) : NULL;
        if (!Read<bool>(heli, COP_DESTROYED)) {
            if (handle == gDownedHeli) gDownedHeli = NULL;
            return;
        }
        if (wasDown || handle == gDownedHeli) return;

        gDownedHeli = handle;
        ++gHeliWrecksPending;
        gHeliWreckAt = *(volatile int*)WORLD_TIMER;
        if (!gSettings.CallForEVHeliDown || TimeBefore(gHeliDownUntil)) return;
        if (CallForEV(FindGroundCop(soundAI), EAXCop::EV_HELI_DOWN))
            gHeliDownUntil = *(volatile int*)WORLD_TIMER + EMS_REPEAT_TICKS;
    }

    int RememberWrecks(int wrecks) {
        int now = *(volatile int*)WORLD_TIMER;
        for (int i = 0; i < wrecks; ++i) {
            if (gWreckCount == PILEUP_WRECKS) {
                memmove(gWreckTimes, gWreckTimes + 1, sizeof(int) * (PILEUP_WRECKS - 1));
                --gWreckCount;
            }
            gWreckTimes[gWreckCount++] = now;
        }
        int recent = 0;
        for (int i = 0; i < gWreckCount; ++i)
            if (TicksSince(gWreckTimes[i]) <= PILEUP_TICKS) ++recent;
        return recent;
    }

    void WatchUnitsDown() {
        void* soundAI = GetSoundAI();
        void* pursuit = GetActivePursuit(soundAI);
        DWORD method = Method(pursuit, PURSUIT_COPS_DESTROYED);
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
            if (gNewWrecks == 0) gNewWrecksAt = *(volatile int*)WORLD_TIMER;
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
        if (recent >= PILEUP_WRECKS && gSettings.CallForEVUnitsDown && !TimeBefore(gPileupUntil)
            && CallForEV(cop, EAXCop::EV_MULTIPLE_UNITS_DOWN)) {
            gWreckCount = 0;
            gPileupUntil = *(volatile int*)WORLD_TIMER + EMS_REPEAT_TICKS;
        } else if (!TimeBefore(gUnitDownUntil)) {
            gUnitDownUntil = *(volatile int*)WORLD_TIMER + UNIT_DOWN_GAP_TICKS;
            CallVirtualWith(cop, COP_UNIT_DISABLED, EAXCop::ABOUT_OTHER_UNIT);
        }
    }

    void __cdecl ArrestAtBust(void* soundAI) {
        void* cop = FindGroundCop(soundAI);
        if (cop == NULL) return;
        if (gArrestID == NO_EVENT)
            gArrestID = ((EventIDMethod)EVENT_ID_FROM_KEY)((void*)SPEECH_EVENT_TABLE, ((KeyFunction)STRING_TO_KEY)(ARREST_EVENT_NAME));
        if (gArrestID == NO_EVENT) return;

        gArrestCalled = true;
        gArrestStarted = false;
        gArrestCalledAt = GetTickCount();
        if (gCutInPatched) {
            gCutInID = gArrestID;
            gCutInUntil = *(volatile int*)WORLD_TIMER + CUT_IN_TICKS;
        }
        CallVirtual(cop, COP_ARREST);
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
        void* dispatch = Read<void*>(GetSoundAI(), SOUND_AI_DISPATCH);
        if (dispatch != NULL) ((VoidMethod)DISPATCH_ARREST_REPLY)(dispatch);
    }

    void PollRadioTimeout() {
        void* speech = *(void* volatile*)MANAGER_COP_SPEECH;
        BYTE* channel = Read<BYTE*>(speech, GAME_SPEECH_CHANNEL);
        void* stream = (channel != NULL) ? channel + CHANNEL_STREAM : NULL;
        bool busy = (Read<unsigned int>(speech, GAME_SPEECH_FLAGS) & GAME_SPEECH_BUSY) != 0;
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
            gStallSince = *(volatile int*)WORLD_TIMER;
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
            push dword ptr [eax + 0xBC]
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
            or   dword ptr [esi + 0x54], 0x80
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

    Settings::Settings()
        : InterruptRamMinPlayerSpeed(48.0f),
          InterruptRamHighMinPlayerSpeed(48.0f),
          CallForEVUnitsDown(true),
          CallForEVHeliDown(true) {
    }

    bool Init(HMODULE module) {
        LoadConfig(module);
        srand(GetTickCount());

        if (!Matches(ON_COLLISION_DONE, kDoneCode, sizeof(kDoneCode))
            || !Patch(ON_COLLISION_REACTION, kReactionCode, sizeof(kReactionCode), OPCODE_JUMP, (const void*)&OnCollisionReaction))
            return false;

        bool rules = Patch(PREVALIDATE_POST_VALIDATE, kPreValidateCallCode, sizeof(kPreValidateCallCode), OPCODE_CALL, (const void*)&PostValidate)
                  && Patch(GETNEXTEVENT_POST_VALIDATE, kGetNextEventCallCode, sizeof(kGetNextEventCallCode), OPCODE_CALL, (const void*)&PostValidate);
        if (!rules) Unpatch(1);

        gCutInPatched = rules
                     && Matches(SERVICE_PLAY, kPlayCode, sizeof(kPlayCode))
                     && Matches(SERVICE_REQUEUE, kRequeueCode, sizeof(kRequeueCode))
                     && Patch(SERVICE_DECISION, kDecisionCode, sizeof(kDecisionCode), OPCODE_JUMP, (const void*)&ServiceDecision);
        Patch(HELI_UPDATE_SITE, kHeliUpdateCode, sizeof(kHeliUpdateCode), OPCODE_CALL, (const void*)&HeliUpdateCall);        bool speech = Patch(SPEECH_UPDATE_SITE, kSpeechUpdateCode, sizeof(kSpeechUpdateCode), OPCODE_CALL, (const void*)&SpeechUpdate);

        int saved = gSavedCount;
        bool arrest = speech
                   && Patch(NIS_CLEAR_SITE, kNisClearCode, sizeof(kNisClearCode), OPCODE_CALL, (const void*)&ClearOrHold)
                   && Patch(NIS_REPLY_SITE, kNisReplyCode, sizeof(kNisReplyCode), OPCODE_CALL, (const void*)&NisReplyCall)
                   && Patch(PERP_BUSTED_SITE, kPerpBustedCode, sizeof(kPerpBustedCode), OPCODE_CALL, (const void*)&PerpBustedCall);
        if (!arrest) Unpatch(saved);
        return true;
    }

    void Restore() {
        Unpatch(0);
        gCutInPatched = false;
    }

}

namespace {

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

    void Write(DWORD address, const void* bytes, size_t length) {
        DWORD oldProtect = 0;
        VirtualProtect((void*)address, length, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)address, bytes, length);
        VirtualProtect((void*)address, length, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), (void*)address, length);
    }

    bool Matches(DWORD address, const BYTE* bytes, size_t length) {
        __try {
            return memcmp((const void*)address, bytes, length) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool Patch(DWORD site, const BYTE* expected, size_t length, BYTE opcode, const void* target) {
        if (gSavedCount >= MOST_PATCHES || !Matches(site, expected, length)) return false;
        BYTE code[MOST_BYTES];
        memset(code, OPCODE_NOP, length);
        code[0] = opcode;
        *(int*)(code + 1) = (int)((DWORD)target - (site + BRANCH_SIZE));
        Write(site, code, length);

        SavedPatch& saved = gSaved[gSavedCount++];
        saved.address = site;
        saved.length = length;
        memcpy(saved.bytes, expected, length);
        return true;
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

    int TicksSince(int start) {
        return (int)((unsigned int)*(volatile int*)WORLD_TIMER - (unsigned int)start);
    }

    bool TimeBefore(int until) {
        return TicksSince(until) < 0;
    }

    bool ReadSwitch(const char* path, const char* section, const char* key, bool fallback) {
        char text[16] = "";
        GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
        if (_strnicmp(text, "true", 4) == 0) return true;
        if (_strnicmp(text, "false", 5) == 0) return false;
        return fallback;
    }

    float ReadSpeed(const char* path, const char* section, const char* key, float fallback) {
        char text[32] = "";
        GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
        char* end = NULL;
        double number = strtod(text, &end);
        return (end != text && number >= 0 && number <= MOST_KPH) ? (float)number : fallback;
    }

    void LoadConfig(HMODULE module) {
        char path[MAX_PATH] = "";
        GetModuleFileNameA(module, path, MAX_PATH - 8);
        char* dot = strrchr(path, '.');
        if (dot == NULL) return;
        strcpy(dot, ".toml");

        SoundAI::Settings& s = gSettings;
        s.InterruptRamMinPlayerSpeed      = ReadSpeed(path,  "InterruptRam",     "MinPlayerSpeed",  s.InterruptRamMinPlayerSpeed);
        s.InterruptRamHighMinPlayerSpeed  = ReadSpeed(path,  "InterruptRamHigh", "MinPlayerSpeed",  s.InterruptRamHighMinPlayerSpeed);        s.CallForEVUnitsDown              = ReadSwitch(path, "CallForEV",        "UnitsDown",       s.CallForEVUnitsDown);
        s.CallForEVHeliDown               = ReadSwitch(path, "CallForEV",        "HeliDown",        s.CallForEVHeliDown);
    }

}
