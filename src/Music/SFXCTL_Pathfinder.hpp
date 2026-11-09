#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(void*) == 4, "AudioDirectorsCut is a 32-bit ASI");

namespace SFXCTL_Pathfinder {

    bool Init(HMODULE module);
    void Restore();

}

namespace {

    constexpr const char* kVersion = "V1.0.0";

    constexpr uint8_t  kPickBranchPrologue[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
    constexpr uint8_t  kRandomSeedPrologue[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08 };
    constexpr uint16_t kRandomSample = 0xFFFE;
    constexpr uint8_t  kJump = 0xE9;
    constexpr size_t   kJumpSize = 5;
    constexpr uint32_t kMapMagic = 0x50464478u;
    constexpr uint16_t kHeadSample = 0;
    constexpr uint16_t kRouterSample = 0xFFFF;
    constexpr uint16_t kFirstSpecialSample = 0xFFF0;
    constexpr size_t   kProjectTracks = 0x40;
    constexpr size_t   kTrackVolume = 2;
    constexpr size_t   kMusicType = 0x148;
    constexpr int      kMostHeads = 16;
    constexpr int      kMostIntensity = 127;
    constexpr size_t   kNodeSize = 16;
    constexpr size_t   kBranchSize = 4;
    constexpr int      kBranchCountShift = 12;
    constexpr uint32_t kBranchCountMask = 0x1F;
    constexpr uintptr_t kSoundStates = 0x00911FE8u;
    constexpr uint32_t kSoundPausedStates = 0x0003483Bu;
    constexpr int      kMostSongs = 30;
    constexpr size_t   kSongText = 128;
    constexpr size_t   kDefPlayText = 8;
    constexpr int      kRestoredThemes = 3;
    constexpr int      kThemeStarts[] = { 0, 427, 755, 1127, 1487 };
    constexpr int      kBustedStingerParts[] = { 383, 705 };
    constexpr int      kEvadeStingerParts[] = { 393, 399, 717, 727 };
    constexpr int      kThemes = 4;
    constexpr int      kChaseHeads[kThemes] = { 0, 427, 755, 1127 };
    constexpr int      kClosingInHeads[kThemes] = { 106, 535, 828, 1193 };
    constexpr int      kPullingAwayHeads[kThemes] = { 213, 613, 889, 1293 };
    constexpr int      kChaseEndingHeads[kThemes] = { 415, 743, 1116, 1475 };
    constexpr int      kPullingAwayEndingHeads[kThemes] = { 421, 749, 1121, 1481 };
    constexpr int      kMostEnding = 16;
    constexpr uint16_t kFireSample = 0xFFFD;
    constexpr int      kMostNodes = 8192;
    constexpr int      kMostSegments = 8192;
    constexpr int      kMostWalk = 1024;

    struct Branch {
        int8_t  low;
        int8_t  high;
        int16_t next;
    };

    static_assert(sizeof(Branch) == kBranchSize, "a PathFinder branch is 4 bytes");

    struct Settings {
        uint32_t PickBranch     = 0x0082FE00u;
        uint32_t CurrentProject = 0x009C2964u;
        uint32_t ProjectMap     = 0x18u;
        uint32_t RandomSeed     = 0x00830E30u;
        uint32_t VanillaNodes   = 3681u;
        uint32_t IntensityFloor = 66u;
        uint32_t FlowVTable     = 0x008B1F4Cu;
        uint32_t FlowUpdate     = 0x0071F4A0u;
        uint32_t FlowChangeTo   = 0x00704C00u;
        uint32_t SoundAI        = 0x00993CC8u;
        uint32_t SoundAIPursuit = 0x130u;
        uint32_t PursuitStatus  = 0x114u;
    };

    struct SongEntry {
        char     Album[kSongText];
        char     Artist[kSongText];
        char     SongName[kSongText];
        char     DefPlay[kDefPlayText];
        uint32_t PathEvent;
    };

    struct Switches {
        bool Theme[kRestoredThemes] = { true, true, true };
        bool Cooldown = true;
        bool Busted = true;
        bool SongEndings = true;
    };

    using PickBranchFn = int(__cdecl*)(int node, int value, int flag);
    using RandomSeedFn = void(__cdecl*)(int seed);

    Settings       gSettings;
    Switches       gSwitches;
    SongEntry      gSongs[kMostSongs] = {};
    int            gSongCount = 0;
    PickBranchFn   gPickBranch = nullptr;
    uint8_t*       gTrampoline = nullptr;
    bool           gPatched = false;
    const uint8_t* gHeadsMap = nullptr;
    int            gHeads[kMostHeads] = {};
    bool           gReached[kMostHeads] = {};
    int            gHoldAt[kMostHeads] = {};
    int            gHeadCount = 0;
    int            gHeldIntensity = -1;
    bool           gWasPaused = false;
    const uint8_t* gTrackMap = nullptr;
    int16_t        gSegmentNode[kMostSegments] = {};
    int8_t         gNodeLayer[kMostNodes] = {};
    int            gWalkSeen[kMostNodes] = {};
    int            gTrackStart[kThemes] = { -1, -1, -1, -1 };
    int            gTrackLast[kThemes] = { -1, -1, -1, -1 };
    int            gTrackTheme = -1;
    int            gTrackSegment = -1;
    int            gEnding[kThemes][kMostEnding] = {};
    int            gEndingCount[kThemes] = {};
    int            gEndingSwap[kThemes][2] = {};
    uintptr_t      gEATrax = 0;
    DWORD          gPursuitStartTick = 0;

#if defined(_DEBUG)
    FILE* gLog = nullptr;
    DWORD gLogStart = 0;

    void OpenLog(HMODULE module) {
        char path[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
        char* dot = length && length < MAX_PATH ? std::strrchr(path, '.') : nullptr;
        if (dot == nullptr || dot < std::strrchr(path, '\\')) return;
        std::strcpy(dot, ".log");
        gLog = std::fopen(path, "w");
        gLogStart = GetTickCount();
    }

    void CloseLog() {
        if (gLog != nullptr) std::fclose(gLog);
        gLog = nullptr;
    }

    void Log(const char* format, ...) {
        if (gLog == nullptr) return;
        std::fprintf(gLog, "%9.2f  ", (GetTickCount() - gLogStart) / 1000.0);
        va_list args;
        va_start(args, format);
        std::vfprintf(gLog, format, args);
        va_end(args);
        std::fputc('\n', gLog);
        std::fflush(gLog);
    }
#define LOG(...) Log(__VA_ARGS__)
#else
    void OpenLog(HMODULE) {}

    void CloseLog() {}

    void Log(const char*, ...) {}
#define LOG(...) ((void)sizeof((Log(__VA_ARGS__), 0)))
#endif

    char* Trim(char* text) {
        while (*text == ' ' || *text == '\t') ++text;
        size_t length = std::strlen(text);
        while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n')) text[--length] = '\0';
        return text;
    }

    void StripComment(char* line) {
        bool quoted = false;
        for (char* at = line; *at != '\0'; ++at) {
            if (*at == '\\' && quoted && at[1] != '\0') {
                ++at;
                continue;
            }
            if (*at == '"') quoted = !quoted;
            if (*at == '#' && !quoted) {
                *at = '\0';
                return;
            }
        }
    }

    bool ReadQuoted(const char* text, char* out, size_t size) {
        if (*text != '"' || size == 0) return false;
        size_t used = 0;
        for (const char* at = text + 1; *at != '\0'; ++at) {
            char c = *at;
            if (c == '"') {
                out[used] = '\0';
                return true;
            }
            if (c == '\\' && at[1] != '\0') c = *++at;
            if (used + 1 < size) out[used++] = c;
        }
        return false;
    }

    bool ReadBool(const char* value, bool* out) {
        if (_stricmp(value, "true") == 0) *out = true;
        else if (_stricmp(value, "false") == 0) *out = false;
        else return false;
        return true;
    }

    void ApplySwitch(const char* key, const char* value) {
        bool* target = nullptr;
        if (std::strcmp(key, "Unused Theme 1") == 0) target = &gSwitches.Theme[0];
        else if (std::strcmp(key, "Unused Theme 2") == 0) target = &gSwitches.Theme[1];
        else if (std::strcmp(key, "Unused Theme 3") == 0) target = &gSwitches.Theme[2];
        else if (std::strcmp(key, "Unused Cooldown Theme") == 0) target = &gSwitches.Cooldown;
        else if (std::strcmp(key, "Unused Busted Theme") == 0) target = &gSwitches.Busted;
        else if (std::strcmp(key, "Song Ending Transitions") == 0) target = &gSwitches.SongEndings;
        if (target == nullptr) {
            LOG("\"%s\" isn't a setting, ignored", key);
            return;
        }
        if (!ReadBool(value, target)) LOG("\"%s\" = %s ignored: it must be true or false", key, value);
    }

    void ApplySongKey(SongEntry& song, const char* key, const char* value) {
        char text[kSongText] = "";
        if (std::strcmp(key, "PathEvent") == 0) {
            char* end = nullptr;
            const unsigned long number = std::strtoul(value, &end, 0);
            if (end == value || *end != '\0') LOG("PathEvent = %s ignored: not a number", value);
            else song.PathEvent = static_cast<uint32_t>(number);
            return;
        }
        if (!ReadQuoted(value, text, sizeof(text))) {
            LOG("%s = %s ignored: it must be a quoted text", key, value);
            return;
        }
        if (std::strcmp(key, "Album") == 0) std::snprintf(song.Album, sizeof(song.Album), "%s", text);
        else if (std::strcmp(key, "Artist") == 0) std::snprintf(song.Artist, sizeof(song.Artist), "%s", text);
        else if (std::strcmp(key, "SongName") == 0) std::snprintf(song.SongName, sizeof(song.SongName), "%s", text);
        else if (std::strcmp(key, "DefPlay") == 0) std::snprintf(song.DefPlay, sizeof(song.DefPlay), "%s", text);
        else LOG("\"%s\" isn't a song setting, ignored", key);
    }

    void LoadSettings(HMODULE module) {
        char path[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return;
        char* extension = std::strrchr(path, '.');
        if (extension == nullptr || extension < std::strrchr(path, '\\') || static_cast<size_t>(extension - path) + sizeof(".toml") > MAX_PATH) return;
        std::memcpy(extension, ".toml", sizeof(".toml"));

        FILE* file = nullptr;
        if (fopen_s(&file, path, "r") != 0 || file == nullptr) {
            LOG("%s not found, every unused theme is on and no songs are added", path);
            return;
        }
        char line[512];
        SongEntry* song = nullptr;
        bool root = true;
        while (std::fgets(line, sizeof(line), file)) {
            char* text = line;
            if (static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) text += 3;
            StripComment(text);
            text = Trim(text);
            if (*text == '\0') continue;
            if (text[0] == '[') {
                root = false;
                song = nullptr;
                if (std::strcmp(text, "[[Song]]") == 0) {
                    if (gSongCount < kMostSongs) {
                        song = &gSongs[gSongCount++];
                        std::memset(song, 0, sizeof(*song));
                        std::snprintf(song->DefPlay, sizeof(song->DefPlay), "%s", "IG");
                    } else {
                        LOG("more than %d songs, the rest are ignored", kMostSongs);
                    }
                }
                continue;
            }
            char key[kSongText] = "";
            char* value = nullptr;
            if (text[0] == '"') {
                if (!ReadQuoted(text, key, sizeof(key))) continue;
                char* close = std::strchr(text + 1, '"');
                char* equals = close != nullptr ? std::strchr(close + 1, '=') : nullptr;
                if (equals == nullptr) continue;
                value = Trim(equals + 1);
            } else {
                char* equals = std::strchr(text, '=');
                if (equals == nullptr) continue;
                *equals = '\0';
                std::snprintf(key, sizeof(key), "%s", Trim(text));
                value = Trim(equals + 1);
            }
            if (song != nullptr) ApplySongKey(*song, key, value);
            else if (root) ApplySwitch(key, value);
        }
        std::fclose(file);

        int kept = 0;
        for (int i = 0; i < gSongCount; ++i) {
            if (gSongs[i].PathEvent == 0 || gSongs[i].SongName[0] == '\0') {
                LOG("song %d has no PathEvent or SongName, left out", i + 1);
                continue;
            }
            if (kept != i) gSongs[kept] = gSongs[i];
            ++kept;
        }
        gSongCount = kept;
        LOG("%s: unused themes 1 %d, 2 %d, 3 %d, cooldown %d, busted %d; song ending transitions %d; %d songs", path, gSwitches.Theme[0], gSwitches.Theme[1],
            gSwitches.Theme[2], gSwitches.Cooldown, gSwitches.Busted, gSwitches.SongEndings, gSongCount);
    }

    bool Read(uintptr_t va, void* out, size_t length) {
        __try {
            std::memcpy(out, reinterpret_cast<const void*>(va), length);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool Write(uintptr_t va, const void* bytes, size_t length) {
        void* target = reinterpret_cast<void*>(va);
        DWORD previous = 0;
        if (!VirtualProtect(target, length, PAGE_EXECUTE_READWRITE, &previous)) return false;
        std::memcpy(target, bytes, length);
        DWORD unused = 0;
        VirtualProtect(target, length, previous, &unused);
        FlushInstructionCache(GetCurrentProcess(), target, length);
        return true;
    }

    uint16_t U16(const uint8_t* map, size_t at) {
        uint16_t value = 0;
        std::memcpy(&value, map + at, sizeof(value));
        return value;
    }

    uint32_t U32(const uint8_t* map, size_t at) {
        uint32_t value = 0;
        std::memcpy(&value, map + at, sizeof(value));
        return value;
    }

    int NodeCount(const uint8_t* map) {
        return U16(map, 0x12);
    }

    size_t NodeAt(const uint8_t* map, int node) {
        return static_cast<size_t>(U16(map, U32(map, 0x14) + 2 * static_cast<size_t>(node))) * 4;
    }

    const uint8_t* CurrentMap() {
        uintptr_t project = 0;
        uintptr_t map = 0;
        if (!Read(gSettings.CurrentProject, &project, sizeof(project)) || project == 0) return nullptr;
        if (!Read(project + gSettings.ProjectMap, &map, sizeof(map)) || map == 0) return nullptr;
        return reinterpret_cast<const uint8_t*>(map);
    }

    int PartHead(const uint8_t* map, int node) {
        if (node < 0 || node >= NodeCount(map)) return -1;
        const size_t at = NodeAt(map, node);
        const uint16_t sample = U16(map, at);
        if (sample == kHeadSample) return node;
        if (sample >= kFirstSpecialSample) return -1;
        return U16(map, at + 8);
    }

    int SegmentOf(const uint8_t* map, int node) {
        return node >= 0 && node < NodeCount(map) ? U16(map, NodeAt(map, node)) : -1;
    }

    int PlayingPart(const uint8_t* map, int node) {
        if (node < 0 || node >= NodeCount(map)) return -1;
        const size_t at = NodeAt(map, node);
        const uint16_t sample = U16(map, at);
        if (sample == kHeadSample) return node;
        if (sample == kRouterSample) return -1;
        return U16(map, at + 8);
    }

    int MusicVolume() {
        uintptr_t project = 0;
        uintptr_t track = 0;
        int8_t volume = -1;
        if (!Read(gSettings.CurrentProject, &project, sizeof(project)) || project == 0) return -1;
        if (!Read(project + kProjectTracks, &track, sizeof(track)) || track == 0) return -1;
        if (!Read(track + kTrackVolume, &volume, sizeof(volume))) return -1;
        return volume;
    }

    int MusicType() {
        int type = -1;
        if (gEATrax == 0 || !Read(gEATrax + kMusicType, &type, sizeof(type))) return -1;
        return type;
    }

    void AddHead(int node) {
        if (node < 0) return;
        for (int i = 0; i < gHeadCount; ++i)
            if (gHeads[i] == node) return;
        if (gHeadCount < kMostHeads) gHeads[gHeadCount++] = node;
    }

    int AlbumPart(int head) {
        for (int i = 0; i < gHeadCount; ++i)
            if (gHeads[i] == head) return i;
        return -1;
    }

    int BranchCount(const uint8_t* map, size_t at) {
        return static_cast<int>((U32(map, at + 4) >> kBranchCountShift) & kBranchCountMask);
    }

    Branch BranchAt(const uint8_t* map, size_t at, int index) {
        Branch branch = {};
        std::memcpy(&branch, map + at + kNodeSize + kBranchSize * static_cast<size_t>(index), sizeof(branch));
        return branch;
    }

    int PickedBranch(const uint8_t* map, size_t at, int value) {
        const int count = BranchCount(map, at);
        for (int i = 0; i < count; ++i) {
            const Branch branch = BranchAt(map, at, i);
            if (value >= branch.low && value <= branch.high) return i;
        }
        return -1;
    }

    void FindHoldLevels(const uint8_t* map) {
        const int floor = static_cast<int>(gSettings.IntensityFloor);
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        for (int i = 0; i < gHeadCount; ++i) gHoldAt[i] = floor;
        const int pcNodes = vanillaNodes < NodeCount(map) ? vanillaNodes : NodeCount(map);
        for (int node = 0; node < pcNodes; ++node) {
            const int part = AlbumPart(PartHead(map, node));
            if (part < 0) continue;
            const size_t at = NodeAt(map, node);
            const int count = BranchCount(map, at);
            for (int branch = 0; branch < count; ++branch) {
                if (BranchAt(map, at, branch).next < vanillaNodes) continue;
                int lowest = -1;
                int highest = -1;
                for (int value = 0; value <= kMostIntensity; ++value) {
                    if (PickedBranch(map, at, value) != branch) continue;
                    if (lowest < 0) lowest = value;
                    highest = value;
                }
                if (highest >= 0 && highest < floor && lowest < gHoldAt[part]) gHoldAt[part] = lowest;
            }
        }
    }

    void FindHeads(const uint8_t* map) {
        gHeadsMap = map;
        gHeadCount = 0;
        std::memset(gReached, 0, sizeof(gReached));
        if (U32(map, 0) != kMapMagic) return;
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        const int pcNodes = vanillaNodes < NodeCount(map) ? vanillaNodes : NodeCount(map);
        for (int node = 0; node < pcNodes; ++node) {
            const size_t at = NodeAt(map, node);
            const int count = BranchCount(map, at);
            for (int branch = 0; branch < count; ++branch)
                if (BranchAt(map, at, branch).next >= vanillaNodes) {
                    AddHead(PartHead(map, node));
                    break;
                }
        }
        FindHoldLevels(map);
#if defined(_DEBUG)
        char heads[512] = "none";
        size_t used = 0;
        for (int i = 0; i < gHeadCount && used < sizeof(heads); ++i)
            used += static_cast<size_t>(std::snprintf(heads + used, sizeof(heads) - used, i ? ", %d (holds at %d)" : "%d (holds at %d)", gHeads[i], gHoldAt[i]));
        LOG("MW_Music map @%p: %d nodes, parts with album inserts: %s", static_cast<const void*>(map), NodeCount(map), heads);
#endif
    }

    int ThemeOf(int node) {
        for (int theme = 0; theme + 1 < static_cast<int>(sizeof(kThemeStarts) / sizeof(kThemeStarts[0])); ++theme)
            if (node >= kThemeStarts[theme] && node < kThemeStarts[theme + 1]) return theme;
        return -1;
    }

    bool ThemeOn(int theme) {
        return theme < 0 || theme >= kRestoredThemes || gSwitches.Theme[theme];
    }

    bool InParts(const int* parts, size_t count, int part) {
        for (size_t i = 0; i < count; ++i)
            if (parts[i] == part) return true;
        return false;
    }

    int FirstGameTarget(const uint8_t* map, int node) {
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        const size_t at = NodeAt(map, node);
        const int count = BranchCount(map, at);
        for (int i = 0; i < count; ++i) {
            const Branch branch = BranchAt(map, at, i);
            if (branch.next >= 0 && branch.next < vanillaNodes) return branch.next;
        }
        return -1;
    }

    bool RestoredPickOn(const uint8_t* map, int game) {
        const int part = PartHead(map, game);
        if (InParts(kBustedStingerParts, sizeof(kBustedStingerParts) / sizeof(kBustedStingerParts[0]), part)) return gSwitches.Busted;
        if (InParts(kEvadeStingerParts, sizeof(kEvadeStingerParts) / sizeof(kEvadeStingerParts[0]), part)) return gSwitches.Cooldown;
        return ThemeOn(ThemeOf(game));
    }

    int Floored(const uint8_t* map, int node, int value) {
        const int floor = static_cast<int>(gSettings.IntensityFloor);
        if (floor > kMostIntensity) return value;
        if (map != gHeadsMap) FindHeads(map);
        const int head = PartHead(map, node);
        const int part = AlbumPart(head);
        if (part < 0 || !ThemeOn(ThemeOf(head))) return value;
        const int hold = gHoldAt[part];
        if (node == head) {
            LOG("part %d starts at intensity %d, as the game plays it", head, value);
            gReached[part] = false;
            return value;
        }
        if (value >= hold) {
            if (!gReached[part] && hold > 0) LOG("part %d reached intensity %d at segment %d: held at %d from here", head, value, SegmentOf(map, node), hold);
            gReached[part] = true;
            return value;
        }
        if (!gReached[part]) return value;
        LOG("node %d (segment %d, part %d): intensity %d -> %d", node, SegmentOf(map, node), head, value, hold);
        return hold;
    }

    bool GamePaused() {
        uint32_t states = 0;
        return Read(kSoundStates, &states, sizeof(states)) && (states & kSoundPausedStates) != 0;
    }

    int KeptTarget(const uint8_t* map, int node, int value, int next) {
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        const size_t at = NodeAt(map, node);
        const int count = BranchCount(map, at);
        int picked = PickedBranch(map, at, value);
        if (picked < 0 || BranchAt(map, at, picked).next != next)
            for (picked = 0; picked < count && BranchAt(map, at, picked).next != next; ++picked) {}
        if (picked >= count) return -1;
        const Branch taken = BranchAt(map, at, picked);
        for (int j = picked + 1; j < count; ++j) {
            const Branch kept = BranchAt(map, at, j);
            if (kept.low == taken.low && kept.high == taken.high && kept.next >= 0 && kept.next < vanillaNodes) return kept.next;
        }
        return -1;
    }

    int ThemeOfPart(int part, const int* heads) {
        for (int theme = 0; theme < kThemes; ++theme)
            if (part == heads[theme]) return theme;
        return -1;
    }

    int TrackTheme(int part) {
        const int chase = ThemeOfPart(part, kChaseHeads);
        return chase >= 0 ? chase : ThemeOfPart(part, kPullingAwayHeads);
    }

    int PursuitTheme(int part) {
        const int track = TrackTheme(part);
        return track >= 0 ? track : ThemeOfPart(part, kClosingInHeads);
    }

    bool HasAudio(int segment) {
        return segment > 0 && segment < kFirstSpecialSample;
    }

    void WalkLayer(const uint8_t* map, int head, int value, int stamp) {
        int node = head;
        for (int step = 0; step < kMostWalk; ++step) {
            const int picked = PickedBranch(map, NodeAt(map, node), value);
            if (picked < 0) return;
            node = BranchAt(map, NodeAt(map, node), picked).next;
            if (node < 0 || node >= NodeCount(map) || gWalkSeen[node] == stamp || TrackTheme(PlayingPart(map, node)) < 0) return;
            gWalkSeen[node] = stamp;
            if (HasAudio(SegmentOf(map, node)) && gNodeLayer[node] < 0) gNodeLayer[node] = static_cast<int8_t>(value);
        }
    }

    int WalkEnding(const uint8_t* map, int head, int theme) {
        if (head >= NodeCount(map) || SegmentOf(map, head) != kHeadSample) return -1;
        int swap = -1;
        int node = head;
        for (int step = 0; step < kMostEnding * 2; ++step) {
            const size_t at = NodeAt(map, node);
            if (BranchCount(map, at) == 0) break;
            node = BranchAt(map, at, 0).next;
            if (node < 0 || node >= NodeCount(map) || PlayingPart(map, node) != head) break;
            const int segment = SegmentOf(map, node);
            if (segment == kFireSample) {
                if (step > 0) break;
                swap = node;
            } else if (HasAudio(segment) && gEndingCount[theme] < kMostEnding) {
                gEnding[theme][gEndingCount[theme]++] = node;
            }
        }
        return swap;
    }

    void FindTracks(const uint8_t* map) {
        gTrackMap = map;
        for (int theme = 0; theme < kThemes; ++theme) gTrackStart[theme] = gTrackLast[theme] = -1;
        const int nodes = NodeCount(map);
        if (U32(map, 0) != kMapMagic || nodes > kMostNodes) return;
        std::memset(gSegmentNode, 0xFF, sizeof(gSegmentNode));
        std::memset(gNodeLayer, 0xFF, sizeof(gNodeLayer));
        std::memset(gWalkSeen, 0, sizeof(gWalkSeen));
        for (int node = 0; node < nodes; ++node) {
            const int segment = SegmentOf(map, node);
            if (!HasAudio(segment) || segment >= kMostSegments || PursuitTheme(PlayingPart(map, node)) < 0) continue;
            if (gSegmentNode[segment] < 0) gSegmentNode[segment] = static_cast<int16_t>(node);
        }
        for (int segment = 0; segment < kMostSegments; ++segment) {
            if (gSegmentNode[segment] < 0) continue;
            const int theme = TrackTheme(PlayingPart(map, gSegmentNode[segment]));
            if (theme < 0) gSegmentNode[segment] = -1;
            else if (segment > gTrackLast[theme]) gTrackLast[theme] = segment;
        }
        int stamp = 0;
        for (int theme = 0; theme < kThemes; ++theme) {
            const int heads[] = { kChaseHeads[theme], kPullingAwayHeads[theme] };
            for (int head : heads) {
                if (head >= nodes || SegmentOf(map, head) != kHeadSample) continue;
                const size_t at = NodeAt(map, head);
                for (int b = 0; b < BranchCount(map, at); ++b) {
                    const Branch branch = BranchAt(map, at, b);
                    const int low = branch.low < 0 ? 0 : branch.low;
                    WalkLayer(map, head, (low + branch.high) / 2, ++stamp);
                }
            }
            const int chase = kChaseHeads[theme];
            if (chase >= nodes || SegmentOf(map, chase) != kHeadSample || BranchCount(map, NodeAt(map, chase)) == 0) continue;
            const int start = SegmentOf(map, BranchAt(map, NodeAt(map, chase), 0).next);
            if (!HasAudio(start) || start >= kMostSegments || gSegmentNode[start] < 0 || start > gTrackLast[theme]) continue;
            gTrackStart[theme] = start;
            LOG("full track: theme %d plays its chase and pulling-away segments %d-%d in order, then loops to %d", theme + 1, start, gTrackLast[theme], start);
        }
        for (int theme = 0; theme < kThemes; ++theme) {
            gEndingCount[theme] = 0;
            gEndingSwap[theme][0] = WalkEnding(map, kChaseEndingHeads[theme], theme);
            gEndingSwap[theme][1] = WalkEnding(map, kPullingAwayEndingHeads[theme], theme);
            if (gEndingSwap[theme][0] < 0 || gEndingSwap[theme][1] < 0) gEndingCount[theme] = 0;
            LOG("theme %d ending: %d segments, %d-%d", theme + 1, gEndingCount[theme], gEndingCount[theme] ? SegmentOf(map, gEnding[theme][0]) : -1,
                gEndingCount[theme] ? SegmentOf(map, gEnding[theme][gEndingCount[theme] - 1]) : -1);
        }
    }

    int EndingTheme(int part) {
        const int chase = ThemeOfPart(part, kChaseEndingHeads);
        return chase >= 0 ? chase : ThemeOfPart(part, kPullingAwayEndingHeads);
    }

    int EndingPick(int theme, int node) {
        if (gEndingCount[theme] == 0) return -1;
        if (node == gEndingSwap[theme][0] || node == gEndingSwap[theme][1]) return gEnding[theme][0];
        for (int i = 0; i + 1 < gEndingCount[theme]; ++i)
            if (gEnding[theme][i] == node) return gEnding[theme][i + 1];
        return -1;
    }

    bool TrackActive() {
        for (int theme = 0; theme < kThemes; ++theme)
            if (gTrackStart[theme] >= 0) return true;
        return false;
    }

    int NextTrackNode(int theme, int segment) {
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        int at = segment;
        for (int guard = gTrackStart[theme]; guard <= gTrackLast[theme]; ++guard) {
            at = at >= gTrackLast[theme] ? gTrackStart[theme] : at + 1;
            const int node = gSegmentNode[at];
            if (node >= 0 && (node < vanillaNodes || ThemeOn(theme))) return node;
        }
        return -1;
    }

    int GamePick(const uint8_t* map, int node, int value, int flag, bool paused) {
        const int used = Floored(map, node, value);
        int next = gPickBranch(node, used, flag);
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        if (next >= vanillaNodes && node < vanillaNodes) {
            if (paused || !ThemeOn(ThemeOf(node))) {
                const int kept = KeptTarget(map, node, used, next);
                if (kept >= 0) {
                    LOG(paused ? "paused: segment %d stays in the game's own loop at node %d (segment %d) instead of going into node %d"
                               : "unused theme off: segment %d goes on to node %d (segment %d) instead of the restored part at node %d",
                        SegmentOf(map, node), kept, SegmentOf(map, kept), next);
                    next = kept;
                }
            }
        } else if (next >= vanillaNodes && node >= vanillaNodes && SegmentOf(map, node) == kRandomSample) {
            const int game = FirstGameTarget(map, node);
            if (game >= 0 && !RestoredPickOn(map, game)) {
                LOG("switched off: random pick node %d goes to the game's node %d (segment %d) instead of node %d", node, game, SegmentOf(map, game), next);
                next = game;
            }
        }
        return next;
    }

    int TrackPick(const uint8_t* map, int theme, int node, int value, int flag, bool paused) {
        const int segment = SegmentOf(map, node);
        if (paused) {
            if (!HasAudio(segment) || gNodeLayer[node] < 0) return GamePick(map, node, value, flag, true);
            return GamePick(map, node, gNodeLayer[node], flag, true);
        }
        const int vanillaNodes = static_cast<int>(gSettings.VanillaNodes);
        int next = -1;
        if (segment == kHeadSample) {
            const int resume = gTrackTheme == theme && gTrackSegment >= 0 ? gSegmentNode[gTrackSegment] : -1;
            next = resume >= 0 && (resume < vanillaNodes || ThemeOn(theme)) ? resume : gSegmentNode[gTrackStart[theme]];
            LOG(next == resume ? "full track: theme %d resumes at segment %d" : "full track: theme %d starts at segment %d", theme + 1, SegmentOf(map, next));
        } else if (HasAudio(segment) && segment >= gTrackStart[theme] && segment <= gTrackLast[theme]) {
            next = NextTrackNode(theme, segment);
        }
        if (next < 0) return GamePick(map, node, value, flag, false);
        gTrackTheme = theme;
        gTrackSegment = SegmentOf(map, next);
        return next;
    }

    int __cdecl PickBranchHook(int node, int value, int flag) {
        const uint8_t* map = CurrentMap();
        if (map == nullptr) return gPickBranch(node, value, flag);
        const bool paused = GamePaused();
        if (paused != gWasPaused) {
            LOG(paused ? "game paused: the music loops where it is (intensity %d outside the full track)" : "game unpaused: the music goes on", gHeldIntensity);
            gWasPaused = paused;
        }
        if (!paused) gHeldIntensity = value;
        else if (gHeldIntensity >= 0) value = gHeldIntensity;
        if (map != gTrackMap) FindTracks(map);
        const int theme = TrackTheme(PlayingPart(map, node));
        const int ending = EndingTheme(PlayingPart(map, node));
        const int endingNext = ending >= 0 ? EndingPick(ending, node) : -1;
        if (endingNext >= 0) LOG("theme %d ending: node %d (segment %d) goes on to node %d (segment %d)", ending + 1, node, SegmentOf(map, node), endingNext, SegmentOf(map, endingNext));
        const int next = endingNext >= 0 ? endingNext
                       : theme >= 0 && gTrackStart[theme] >= 0 ? TrackPick(map, theme, node, value, flag, paused) : GamePick(map, node, value, flag, paused);
#if defined(_DEBUG)
        static int lastPart = -2;
        const int part = PlayingPart(map, node);
        const bool afterStart = gPursuitStartTick != 0 && GetTickCount() - gPursuitStartTick < 30000;
        if (part != lastPart || afterStart || paused) {
            LOG("music part %d -> %d at node %d (segment %d) -> node %d (segment %d), intensity %d, pick flag %d, music type %d, volume %d, paused %d",
                lastPart, part, node, SegmentOf(map, node), next, SegmentOf(map, next), value, flag, MusicType(), MusicVolume(), paused);
            lastPart = part;
        }
        if (next >= static_cast<int>(gSettings.VanillaNodes) && node < static_cast<int>(gSettings.VanillaNodes))
            LOG("album insert: segment %d -> node %d (segment %d)", SegmentOf(map, node), next, SegmentOf(map, next));
        if (SegmentOf(map, node) == kRandomSample)
            LOG("random pick at node %d with %d (flag %d): node %d (segment %d)", node, value, flag, next, SegmentOf(map, next));
#endif
        return next;
    }

    bool SeedRandom() {
        uint8_t current[sizeof(kRandomSeedPrologue)] = {};
        if (!Read(gSettings.RandomSeed, current, sizeof(current)) || std::memcmp(current, kRandomSeedPrologue, sizeof(current)) != 0) return false;
        LARGE_INTEGER counter = {};
        QueryPerformanceCounter(&counter);
        const uint32_t seed = counter.LowPart ^ static_cast<uint32_t>(counter.HighPart) ^ GetTickCount() ^ (GetCurrentProcessId() << 16);
        reinterpret_cast<RandomSeedFn>(gSettings.RandomSeed)(static_cast<int>(seed));
        LOG("PathFinder random generator seeded with 0x%08X", seed);
        return true;
    }

    bool InstallPickBranch() {
        uint8_t current[sizeof(kPickBranchPrologue)] = {};
        if (!Read(gSettings.PickBranch, current, sizeof(current)) || std::memcmp(current, kPickBranchPrologue, sizeof(current)) != 0) return false;
        gTrampoline = static_cast<uint8_t*>(VirtualAlloc(nullptr, sizeof(kPickBranchPrologue) + kJumpSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (gTrampoline == nullptr) return false;
        std::memcpy(gTrampoline, kPickBranchPrologue, sizeof(kPickBranchPrologue));
        const uintptr_t back = gSettings.PickBranch + sizeof(kPickBranchPrologue);
        const uintptr_t trampolineJump = reinterpret_cast<uintptr_t>(gTrampoline) + sizeof(kPickBranchPrologue);
        gTrampoline[sizeof(kPickBranchPrologue)] = kJump;
        const int32_t backRelative = static_cast<int32_t>(back - (trampolineJump + kJumpSize));
        std::memcpy(gTrampoline + sizeof(kPickBranchPrologue) + 1, &backRelative, sizeof(backRelative));
        gPickBranch = reinterpret_cast<PickBranchFn>(gTrampoline);

        uint8_t code[sizeof(kPickBranchPrologue)];
        std::memset(code, 0x90, sizeof(code));
        code[0] = kJump;
        const int32_t hookRelative = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&PickBranchHook) - (gSettings.PickBranch + kJumpSize));
        std::memcpy(code + 1, &hookRelative, sizeof(hookRelative));
        if (!Write(gSettings.PickBranch, code, sizeof(code))) {
            VirtualFree(gTrampoline, 0, MEM_RELEASE);
            gTrampoline = nullptr;
            return false;
        }
        return true;
    }

}

namespace SFXCTL_Pathfinder {

    bool Init(HMODULE module) {
        OpenLog(module);
        LOG("AudioDirectorsCut %s", kVersion);
        LoadSettings(module);
        gPatched = InstallPickBranch();
        LOG(gPatched ? "PathFinder branch picker hooked" : "the bytes at PickBranch don't match, nothing patched");
        if (!SeedRandom()) LOG("the bytes at RandomSeed don't match, the random picks keep the game's fixed start");
        return gPatched;
    }

    void Restore() {
        if (gPatched) Write(gSettings.PickBranch, kPickBranchPrologue, sizeof(kPickBranchPrologue));
        gPatched = false;
        CloseLog();
    }

}
