#include "SFXCTL_Pathfinder.hpp"
#include "../Config.hpp"
#include "../Memory.hpp"
#include <string.h>

namespace {

    #define PICK_BRANCH                      0x0082FE00
    #define RANDOM_SEED                      0x00830E30
    #define CURRENT_PROJECT                  0x009C2964
    #define SOUND_STATES                     0x00911FE8
    #define SOUND_PAUSED_STATES              0x0003483B
    #define VANILLA_NODES                    3681

    #define MAP_NODE_COUNT                   0x12
    #define MAP_NODE_TABLE                   0x14
    #define NODE_FLAGS                       0x04
    #define NODE_PARENT                      0x08
    #define NODE_SIZE                        16
    #define BRANCH_SIZE                      4
    #define BRANCH_COUNT_SHIFT               12
    #define BRANCH_COUNT_MASK                0x1F

    #define HEAD_SAMPLE                      0x0000
    #define FIRST_SPECIAL_SAMPLE             0xFFF0
    #define FIRE_SAMPLE                      0xFFFD
    #define RANDOM_SAMPLE                    0xFFFE
    #define ROUTER_SAMPLE                    0xFFFF

    #define THEMES                           4
    #define ENDING_SWAPS                     2
    #define MOST_ENDING                      16
    #define MOST_NODES                       8192
    #define MOST_SEGMENTS                    8192
    #define MOST_WALK                        1024
    #define NO_LAYER                         (-1)

    const BYTE kPickBranchCode[]             = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
    const BYTE kRandomSeedCode[]             = { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08 };

    const int kThemeStarts[THEMES + 1]       = { 0, 427, 755, 1127, 1487 };
    const int kChaseHeads[THEMES]            = { 0, 427, 755, 1127 };
    const int kClosingInHeads[THEMES]        = { 106, 535, 828, 1193 };
    const int kPullingAwayHeads[THEMES]      = { 213, 613, 889, 1293 };
    const int kChaseEndingHeads[THEMES]      = { 415, 743, 1116, 1475 };
    const int kPullingAwayEndingHeads[THEMES] = { 421, 749, 1121, 1481 };
    const int kBustedStingerParts[]          = { 383, 705 };
    const int kEvadeStingerParts[]           = { 393, 399, 717, 727 };

    typedef void (__cdecl* RandomSeedFunction)(int seed);

    struct Branch {
        signed char low;
        signed char high;
        short       next;
    };

    DWORD       gPickBranchBack              = PICK_BRANCH + sizeof(kPickBranchCode);
    int         gHeldIntensity               = -1;
    const BYTE* gTrackMap                    = NULL;
    short       gSegmentNode[MOST_SEGMENTS];
    signed char gNodeLayer[MOST_NODES];
    int         gWalkSeen[MOST_NODES];
    int         gTrackStart[THEMES]          = { NO_NODE, NO_NODE, NO_NODE, NO_NODE };
    int         gTrackLast[THEMES]           = { NO_NODE, NO_NODE, NO_NODE, NO_NODE };
    int         gTrackTheme                  = -1;
    int         gTrackSegment                = -1;
    int         gEnding[THEMES][MOST_ENDING];
    int         gEndingCount[THEMES];
    int         gEndingSwap[THEMES][ENDING_SWAPS];

    int    __cdecl GamePickBranch(int node, int value, int flag);
    int    NodeCount(const BYTE* map);
    DWORD  NodeAt(const BYTE* map, int node);
    int    SegmentOf(const BYTE* map, int node);
    int    PartHead(const BYTE* map, int node);
    int    BranchCount(const BYTE* map, DWORD at);
    Branch BranchAt(const BYTE* map, DWORD at, int index);
    int    PickedBranch(const BYTE* map, DWORD at, int value);
    bool   HasAudio(int segment);
    bool   InParts(const int* parts, int count, int part);
    int    ThemeOfPart(int part, const int* heads);

    const BYTE* CurrentMap() {
        BYTE* project = Memory::Read<BYTE*>((const void*)CURRENT_PROJECT, 0);
        return Memory::Read<const BYTE*>(project, PATH_PROJECT_MAP);
    }

    int ThemeOf(int node) {
        for (int theme = 0; theme < THEMES; ++theme)
            if (node >= kThemeStarts[theme] && node < kThemeStarts[theme + 1]) return theme;
        return -1;
    }

    bool ThemeOn(int theme) {
        return theme < 0 || theme >= RESTORED_THEMES || Config::gSettings.UnusedTheme[theme];
    }

    int TrackTheme(int part) {
        int chase = ThemeOfPart(part, kChaseHeads);
        return (chase >= 0) ? chase : ThemeOfPart(part, kPullingAwayHeads);
    }

    int PursuitTheme(int part) {
        int track = TrackTheme(part);
        return (track >= 0) ? track : ThemeOfPart(part, kClosingInHeads);
    }

    int EndingTheme(int part) {
        int chase = ThemeOfPart(part, kChaseEndingHeads);
        return (chase >= 0) ? chase : ThemeOfPart(part, kPullingAwayEndingHeads);
    }

    int FirstGameTarget(const BYTE* map, int node) {
        DWORD at = NodeAt(map, node);
        int count = BranchCount(map, at);
        for (int i = 0; i < count; ++i) {
            Branch branch = BranchAt(map, at, i);
            if (branch.next >= 0 && branch.next < VANILLA_NODES) return branch.next;
        }
        return NO_NODE;
    }

    bool RestoredPickOn(const BYTE* map, int game) {
        int part = PartHead(map, game);
        if (InParts(kBustedStingerParts, sizeof(kBustedStingerParts) / sizeof(kBustedStingerParts[0]), part)) return Config::gSettings.UnusedBustedTheme;
        if (InParts(kEvadeStingerParts, sizeof(kEvadeStingerParts) / sizeof(kEvadeStingerParts[0]), part)) return Config::gSettings.UnusedCooldownTheme;
        return ThemeOn(ThemeOf(game));
    }

    int KeptTarget(const BYTE* map, int node, int value, int next) {
        DWORD at = NodeAt(map, node);
        int count = BranchCount(map, at);
        int picked = PickedBranch(map, at, value);
        if (picked < 0 || BranchAt(map, at, picked).next != next)
            for (picked = 0; picked < count && BranchAt(map, at, picked).next != next; ++picked) {}
        if (picked >= count) return NO_NODE;
        Branch taken = BranchAt(map, at, picked);
        for (int i = picked + 1; i < count; ++i) {
            Branch kept = BranchAt(map, at, i);
            if (kept.low == taken.low && kept.high == taken.high && kept.next >= 0 && kept.next < VANILLA_NODES) return kept.next;
        }
        return NO_NODE;
    }

    void WalkLayer(const BYTE* map, int head, int value, int stamp) {
        int node = head;
        for (int step = 0; step < MOST_WALK; ++step) {
            int picked = PickedBranch(map, NodeAt(map, node), value);
            if (picked < 0) return;
            node = BranchAt(map, NodeAt(map, node), picked).next;
            if (node < 0 || node >= NodeCount(map) || gWalkSeen[node] == stamp || TrackTheme(SFXCTL_Pathfinder::PlayingPart(map, node)) < 0) return;
            gWalkSeen[node] = stamp;
            if (HasAudio(SegmentOf(map, node)) && gNodeLayer[node] == NO_LAYER) gNodeLayer[node] = (signed char)value;
        }
    }

    int WalkEnding(const BYTE* map, int head, int theme) {
        if (head >= NodeCount(map) || SegmentOf(map, head) != HEAD_SAMPLE) return NO_NODE;
        int swap = NO_NODE;
        int node = head;
        for (int step = 0; step < MOST_ENDING * 2; ++step) {
            DWORD at = NodeAt(map, node);
            if (BranchCount(map, at) == 0) break;
            node = BranchAt(map, at, 0).next;
            if (node < 0 || node >= NodeCount(map) || SFXCTL_Pathfinder::PlayingPart(map, node) != head) break;
            int segment = SegmentOf(map, node);
            if (segment == FIRE_SAMPLE) {
                if (step > 0) break;
                swap = node;
            } else if (HasAudio(segment) && gEndingCount[theme] < MOST_ENDING) {
                gEnding[theme][gEndingCount[theme]++] = node;
            }
        }
        return swap;
    }

    void FindTracks(const BYTE* map) {
        gTrackMap = map;
        for (int theme = 0; theme < THEMES; ++theme) gTrackStart[theme] = gTrackLast[theme] = NO_NODE;
        int nodes = NodeCount(map);
        if (SFXCTL_Pathfinder::MapDword(map, 0) != PATH_MAP_MAGIC || nodes > MOST_NODES) return;
        memset(gSegmentNode, 0xFF, sizeof(gSegmentNode));
        memset(gNodeLayer, 0xFF, sizeof(gNodeLayer));
        memset(gWalkSeen, 0, sizeof(gWalkSeen));

        for (int node = 0; node < nodes; ++node) {
            int segment = SegmentOf(map, node);
            if (!HasAudio(segment) || segment >= MOST_SEGMENTS || PursuitTheme(SFXCTL_Pathfinder::PlayingPart(map, node)) < 0) continue;
            if (gSegmentNode[segment] == NO_NODE) gSegmentNode[segment] = (short)node;
        }
        for (int segment = 0; segment < MOST_SEGMENTS; ++segment) {
            if (gSegmentNode[segment] == NO_NODE) continue;
            int theme = TrackTheme(SFXCTL_Pathfinder::PlayingPart(map, gSegmentNode[segment]));
            if (theme < 0) gSegmentNode[segment] = NO_NODE;
            else if (segment > gTrackLast[theme]) gTrackLast[theme] = segment;
        }

        int stamp = 0;
        for (int theme = 0; theme < THEMES; ++theme) {
            int heads[2] = { kChaseHeads[theme], kPullingAwayHeads[theme] };
            for (int h = 0; h < 2; ++h) {
                int head = heads[h];
                if (head >= nodes || SegmentOf(map, head) != HEAD_SAMPLE) continue;
                DWORD at = NodeAt(map, head);
                for (int b = 0; b < BranchCount(map, at); ++b) {
                    Branch branch = BranchAt(map, at, b);
                    int low = (branch.low < 0) ? 0 : branch.low;
                    WalkLayer(map, head, (low + branch.high) / 2, ++stamp);
                }
            }
            int chase = kChaseHeads[theme];
            if (chase >= nodes || SegmentOf(map, chase) != HEAD_SAMPLE || BranchCount(map, NodeAt(map, chase)) == 0) continue;
            int start = SegmentOf(map, BranchAt(map, NodeAt(map, chase), 0).next);
            if (!HasAudio(start) || start >= MOST_SEGMENTS || gSegmentNode[start] == NO_NODE || start > gTrackLast[theme]) continue;
            gTrackStart[theme] = start;
        }

        for (int theme = 0; theme < THEMES; ++theme) {
            gEndingCount[theme] = 0;
            gEndingSwap[theme][0] = WalkEnding(map, kChaseEndingHeads[theme], theme);
            gEndingSwap[theme][1] = WalkEnding(map, kPullingAwayEndingHeads[theme], theme);
            if (gEndingSwap[theme][0] == NO_NODE || gEndingSwap[theme][1] == NO_NODE) gEndingCount[theme] = 0;
        }
    }

    int EndingPick(int theme, int node) {
        if (gEndingCount[theme] == 0) return NO_NODE;
        if (node == gEndingSwap[theme][0] || node == gEndingSwap[theme][1]) return gEnding[theme][0];
        for (int i = 0; i + 1 < gEndingCount[theme]; ++i)
            if (gEnding[theme][i] == node) return gEnding[theme][i + 1];
        return NO_NODE;
    }

    int NextTrackNode(int theme, int segment) {
        int at = segment;
        for (int guard = gTrackStart[theme]; guard <= gTrackLast[theme]; ++guard) {
            at = (at >= gTrackLast[theme]) ? gTrackStart[theme] : at + 1;
            int node = gSegmentNode[at];
            if (node != NO_NODE && (node < VANILLA_NODES || ThemeOn(theme))) return node;
        }
        return NO_NODE;
    }

    int GamePick(const BYTE* map, int node, int value, int flag, bool paused) {
        int next = GamePickBranch(node, value, flag);
        if (next >= VANILLA_NODES && node < VANILLA_NODES) {
            if (paused || !ThemeOn(ThemeOf(node))) {
                int kept = KeptTarget(map, node, value, next);
                if (kept != NO_NODE) next = kept;
            }
        } else if (next >= VANILLA_NODES && node >= VANILLA_NODES && SegmentOf(map, node) == RANDOM_SAMPLE) {
            int game = FirstGameTarget(map, node);
            if (game != NO_NODE && !RestoredPickOn(map, game)) next = game;
        }
        return next;
    }

    int TrackPick(const BYTE* map, int theme, int node, int value, int flag, bool paused) {
        int segment = SegmentOf(map, node);
        if (paused) {
            if (!HasAudio(segment) || gNodeLayer[node] == NO_LAYER) return GamePick(map, node, value, flag, true);
            return GamePick(map, node, gNodeLayer[node], flag, true);
        }
        int next = NO_NODE;
        if (segment == HEAD_SAMPLE) {
            int resume = (gTrackTheme == theme && gTrackSegment >= 0) ? gSegmentNode[gTrackSegment] : NO_NODE;
            next = (resume != NO_NODE && (resume < VANILLA_NODES || ThemeOn(theme))) ? resume : gSegmentNode[gTrackStart[theme]];
        } else if (HasAudio(segment) && segment >= gTrackStart[theme] && segment <= gTrackLast[theme]) {
            next = NextTrackNode(theme, segment);
        }
        if (next == NO_NODE) return GamePick(map, node, value, flag, false);
        gTrackTheme = theme;
        gTrackSegment = SegmentOf(map, next);
        return next;
    }

    int __cdecl PickBranchHook(int node, int value, int flag) {
        const BYTE* map = CurrentMap();
        if (map == NULL) return GamePickBranch(node, value, flag);
        bool paused = SFXCTL_Pathfinder::GamePaused();
        if (!paused) gHeldIntensity = value;
        else if (gHeldIntensity >= 0) value = gHeldIntensity;
        if (map != gTrackMap) FindTracks(map);

        int part = SFXCTL_Pathfinder::PlayingPart(map, node);
        int ending = EndingTheme(part);
        int next = (ending >= 0) ? EndingPick(ending, node) : NO_NODE;
        if (next != NO_NODE) return next;
        int theme = TrackTheme(part);
        if (theme >= 0 && gTrackStart[theme] != NO_NODE) return TrackPick(map, theme, node, value, flag, paused);
        return GamePick(map, node, value, flag, paused);
    }

    void SeedRandom() {
        if (!Memory::Matches(RANDOM_SEED, kRandomSeedCode, sizeof(kRandomSeedCode))) return;
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        DWORD seed = counter.LowPart ^ (DWORD)counter.HighPart ^ GetTickCount() ^ (GetCurrentProcessId() << 16);
        ((RandomSeedFunction)RANDOM_SEED)((int)seed);
    }

    __declspec(naked) int __cdecl GamePickBranch(int, int, int) {
        __asm {
            push ebp
            mov  ebp, esp
            mov  eax, dword ptr [ebp + 8]
            jmp  dword ptr [gPickBranchBack]
        }
    }

}

namespace SFXCTL_Pathfinder {

    bool Init() {
        bool patched = Memory::PatchBranch(PICK_BRANCH, kPickBranchCode, sizeof(kPickBranchCode), OPCODE_JUMP, (const void*)&PickBranchHook);
        SeedRandom();
        return patched;
    }

    WORD MapWord(const BYTE* map, DWORD at) {
        WORD value = 0;
        memcpy(&value, map + at, sizeof(value));
        return value;
    }

    DWORD MapDword(const BYTE* map, DWORD at) {
        DWORD value = 0;
        memcpy(&value, map + at, sizeof(value));
        return value;
    }

    int PlayingPart(const BYTE* map, int node) {
        if (node < 0 || node >= NodeCount(map)) return NO_NODE;
        DWORD at = NodeAt(map, node);
        WORD sample = MapWord(map, at);
        if (sample == HEAD_SAMPLE) return node;
        if (sample == ROUTER_SAMPLE) return NO_NODE;
        return MapWord(map, at + NODE_PARENT);
    }

    bool TrackActive() {
        for (int theme = 0; theme < THEMES; ++theme)
            if (gTrackStart[theme] != NO_NODE) return true;
        return false;
    }

    bool GamePaused() {
        return (Memory::Read<DWORD>((const void*)SOUND_STATES, 0) & SOUND_PAUSED_STATES) != 0;
    }

    void RestartTrack() {
        gTrackSegment = -1;
    }

}

namespace {

    int NodeCount(const BYTE* map) {
        return SFXCTL_Pathfinder::MapWord(map, MAP_NODE_COUNT);
    }

    DWORD NodeAt(const BYTE* map, int node) {
        return (DWORD)SFXCTL_Pathfinder::MapWord(map, SFXCTL_Pathfinder::MapDword(map, MAP_NODE_TABLE) + 2 * node) * 4;
    }

    int SegmentOf(const BYTE* map, int node) {
        return (node >= 0 && node < NodeCount(map)) ? SFXCTL_Pathfinder::MapWord(map, NodeAt(map, node)) : NO_NODE;
    }

    int PartHead(const BYTE* map, int node) {
        if (node < 0 || node >= NodeCount(map)) return NO_NODE;
        DWORD at = NodeAt(map, node);
        WORD sample = SFXCTL_Pathfinder::MapWord(map, at);
        if (sample == HEAD_SAMPLE) return node;
        if (sample >= FIRST_SPECIAL_SAMPLE) return NO_NODE;
        return SFXCTL_Pathfinder::MapWord(map, at + NODE_PARENT);
    }

    int BranchCount(const BYTE* map, DWORD at) {
        return (int)((SFXCTL_Pathfinder::MapDword(map, at + NODE_FLAGS) >> BRANCH_COUNT_SHIFT) & BRANCH_COUNT_MASK);
    }

    Branch BranchAt(const BYTE* map, DWORD at, int index) {
        Branch branch;
        memcpy(&branch, map + at + NODE_SIZE + BRANCH_SIZE * index, sizeof(branch));
        return branch;
    }

    int PickedBranch(const BYTE* map, DWORD at, int value) {
        int count = BranchCount(map, at);
        for (int i = 0; i < count; ++i) {
            Branch branch = BranchAt(map, at, i);
            if (value >= branch.low && value <= branch.high) return i;
        }
        return -1;
    }

    bool HasAudio(int segment) {
        return segment > 0 && segment < FIRST_SPECIAL_SAMPLE;
    }

    bool InParts(const int* parts, int count, int part) {
        for (int i = 0; i < count; ++i)
            if (parts[i] == part) return true;
        return false;
    }

    int ThemeOfPart(int part, const int* heads) {
        for (int theme = 0; theme < THEMES; ++theme)
            if (part == heads[theme]) return theme;
        return -1;
    }

}
