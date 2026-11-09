#pragma once
#include "SFXCTL_Pathfinder.hpp"

namespace SFXObj_PFEATrax {

    bool Init();
    void Restore();

}

namespace {

    constexpr uintptr_t kPlaylistDefaults   = 0x005AB7BCu;
    constexpr uintptr_t kInitializeEATrax   = 0x004DF330u;
    constexpr uintptr_t kMaxSongs           = 0x008F42C4u;
    constexpr uintptr_t kSongs              = 0x009123CCu;
    constexpr uintptr_t kSongsInsert        = 0x005AAC30u;
    constexpr uintptr_t kFEDatabase         = 0x0091CF90u;
    constexpr size_t    kSongsFirst         = 0x4;
    constexpr size_t    kSongsLast          = 0x8;
    constexpr size_t    kDatabaseProfile    = 0x10;
    constexpr size_t    kProfilePlaylist    = 0x324;
    constexpr int       kPlaylistSlots      = 30;
    constexpr uint8_t   kMaxSongsLoad[]     = { 0xA1, 0xC4, 0x42, 0x8F, 0x00 };
    constexpr uint8_t   kFEDatabaseLoad[]   = { 0xA1, 0x90, 0xCF, 0x91, 0x00 };
    constexpr uint8_t   kPushFlags          = 0x9C;
    constexpr uint8_t   kPushAll            = 0x60;
    constexpr uint8_t   kPopAll             = 0x61;
    constexpr uint8_t   kPopFlags           = 0x9D;
    constexpr uint8_t   kCall               = 0xE8;
    constexpr size_t    kStubSize           = 32;
    constexpr uintptr_t kInteractiveStop    = 0x004DF258u;
    constexpr uint8_t   kPathStopCall[]     = { 0xE8, 0xB3, 0xA0, 0x34, 0x00 };
    constexpr uintptr_t kPathStop           = 0x00829310u;
    constexpr uintptr_t kPathTrackStatus    = 0x00829210u;
    constexpr uintptr_t kPathSetNamedValue  = 0x00829640u;
    constexpr uintptr_t kPathTrackOf        = 0x0082CAA0u;
    constexpr uintptr_t kPathProjects       = 0x009C2954u;
    constexpr int       kProjectCount       = 4;
    constexpr int       kTrackCount         = 24;
    constexpr size_t    kTrackNode          = 4;
    constexpr size_t    kPrevMusicType      = 0x14C;
    constexpr int       kMusicLicensed      = 0;
    constexpr int       kTrackQueueing      = 2;
    constexpr int       kTrackReadyForNext  = 4;
    constexpr int       kTrackPlaying       = 5;
    constexpr int       kTrackFading        = 6;
    constexpr uint32_t  kSongEndJumpEvent   = 0x7B768Au;
    constexpr uint32_t  kPursuitThemeEvent  = 0x6E7283u;
    constexpr uint32_t  kEventIdMask        = 0xFFFFFFu;
    constexpr uint32_t  kActionTrackMask    = 0x11000001u;
    constexpr size_t    kEventHeaderSize    = 20;
    constexpr size_t    kActionSize         = 12;
    constexpr size_t    kVariableSize       = 20;
    constexpr size_t    kVariableNameSize   = 16;
    constexpr uint16_t  kPartBuiltIn        = 3;
    constexpr int       kMostSongHeads      = 64;
    constexpr const char* kSongEndVariable  = "songend";
    constexpr uint8_t   kPushEsi            = 0x56;
    constexpr uint8_t   kPushArgument[]     = { 0xFF, 0x74, 0x24, 0x08 };
    constexpr uint8_t   kDropArguments[]    = { 0x83, 0xC4, 0x08 };
    constexpr uint8_t   kReturn             = 0xC3;

    enum Playability {
        kPlayOff  = 0,
        kPlayMenu = 1,
        kPlayRace = 2,
        kPlayAll  = 3
    };

    struct SongInfo {
        const char* SongName;
        const char* Artist;
        const char* Album;
        const char* DefPlay;
        int         PathEvent;
    };

    struct JukeboxEntry {
        uint32_t SongIndex;
        uint8_t  PlayabilityField;
        uint8_t  Padding[3];
    };

    static_assert(sizeof(SongInfo) == 0x14, "Sound::stSongInfo is 0x14 bytes");
    static_assert(sizeof(JukeboxEntry) == 8, "a JukeboxEntry is 8 bytes");

    struct Patch {
        uintptr_t      site;
        const uint8_t* code;
        size_t         length;
        void (__cdecl* entry)();
        uint8_t*       stub;
    };

    using SongsInsertFn = void(__thiscall*)(void* songs, SongInfo** where, unsigned count, SongInfo* const* value);
    using PathStopFn = int(__cdecl*)(int track);
    using PathTrackStatusFn = int(__cdecl*)(unsigned track);
    using PathSetNamedValueFn = int(__cdecl*)(int track, const char* name, int value);
    using PathTrackOfFn = uintptr_t(__cdecl*)(int track);

    SongInfo       gSongInfo[kMostSongs] = {};
    int            gVanillaSongs = -1;
    int            gAddedSongs = 0;
    uint8_t*       gStopStub = nullptr;
    const uint8_t* gSongMap = nullptr;
    bool           gSongEndReady = false;
    int            gSongHeads[kMostSongHeads] = {};
    int            gSongHeadCount = 0;

    uint8_t Playable(const char* defPlay) {
        if (std::strcmp(defPlay, "FE") == 0) return kPlayMenu;
        if (std::strcmp(defPlay, "IG") == 0) return kPlayRace;
        if (std::strcmp(defPlay, "AL") == 0) return kPlayAll;
        return kPlayOff;
    }

    void __cdecl AddSongs() {
        __try {
            int* maxSongs = reinterpret_cast<int*>(kMaxSongs);
            SongInfo** first = *reinterpret_cast<SongInfo***>(kSongs + kSongsFirst);
            SongInfo** last = *reinterpret_cast<SongInfo***>(kSongs + kSongsLast);
            if (gVanillaSongs >= 0 || gSongCount == 0 || first == nullptr || last - first != *maxSongs) return;
            gVanillaSongs = *maxSongs;
            const int room = kPlaylistSlots - gVanillaSongs;
            int added = 0;
            for (int i = 0; i < gSongCount && added < room; ++i) {
                SongInfo* info = &gSongInfo[added];
                info->SongName = gSongs[i].SongName;
                info->Artist = gSongs[i].Artist;
                info->Album = gSongs[i].Album;
                info->DefPlay = gSongs[i].DefPlay;
                info->PathEvent = static_cast<int>(gSongs[i].PathEvent);
                SongInfo** end = *reinterpret_cast<SongInfo***>(kSongs + kSongsLast);
                reinterpret_cast<SongsInsertFn>(kSongsInsert)(reinterpret_cast<void*>(kSongs), end, 1, &info);
                ++added;
                LOG("EA Trax song %d: %s - %s (%s), PathEvent 0x%08X, %s", gVanillaSongs + added, info->SongName, info->Artist, info->Album, info->PathEvent, info->DefPlay);
            }
            if (gSongCount > room) LOG("the playlist has room for %d more songs, %d left out", room, gSongCount - room);
            gAddedSongs = added;
            *maxSongs = gVanillaSongs + added;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("adding the songs failed");
        }
    }

    void __cdecl FixPlaylist() {
        __try {
            const int total = *reinterpret_cast<const int*>(kMaxSongs);
            const uintptr_t database = *reinterpret_cast<const uintptr_t*>(kFEDatabase);
            if (database == 0 || total <= 0 || total > kPlaylistSlots) return;
            const uintptr_t profile = *reinterpret_cast<const uintptr_t*>(database + kDatabaseProfile);
            if (profile == 0) return;
            JukeboxEntry* playlist = reinterpret_cast<JukeboxEntry*>(profile + kProfilePlaylist);
            const int vanilla = gVanillaSongs >= 0 ? gVanillaSongs : total;
            bool present[kPlaylistSlots] = {};
            bool kept[kPlaylistSlots] = {};
            for (int slot = 0; slot < total; ++slot) {
                const uint32_t index = playlist[slot].SongIndex;
                if (index < static_cast<uint32_t>(total) && !present[index]) {
                    present[index] = true;
                    kept[slot] = true;
                }
            }
            for (int index = 0; index < total; ++index) {
                if (present[index]) continue;
                int slot = -1;
                for (int s = vanilla; s < total && slot < 0; ++s)
                    if (!kept[s]) slot = s;
                for (int s = 0; s < total && slot < 0; ++s)
                    if (!kept[s]) slot = s;
                if (slot < 0) break;
                SongInfo* const* songs = *reinterpret_cast<SongInfo* const* const*>(kSongs + kSongsFirst);
                const SongInfo* song = songs != nullptr ? songs[index] : nullptr;
                playlist[slot].SongIndex = static_cast<uint32_t>(index);
                playlist[slot].PlayabilityField = song != nullptr && song->DefPlay != nullptr ? Playable(song->DefPlay) : static_cast<uint8_t>(kPlayOff);
                kept[slot] = true;
                present[index] = true;
                LOG("playlist slot %d had no song of its own, now song %d", slot + 1, index + 1);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("checking the playlist failed");
        }
    }

    void ScanSongEndings(const uint8_t* map) {
        gSongMap = map;
        gSongEndReady = false;
        gSongHeadCount = 0;
        if (U32(map, 0) != kMapMagic) return;
        bool variable = false;
        const size_t variables = U32(map, 0x14 + 4 * 4);
        for (int v = 0; v < map[0x11] && !variable; ++v)
            variable = std::strncmp(reinterpret_cast<const char*>(map + variables + kVariableSize * v), kSongEndVariable, kVariableNameSize) == 0;
        bool themes = false;
        const size_t index = U32(map, 0x14 + 2 * 4);
        for (int e = 0; e < map[0x0F]; ++e) {
            const size_t record = static_cast<size_t>(U16(map, index + 2 * static_cast<size_t>(e))) * 4;
            const uint32_t id = U32(map, record + 12) & kEventIdMask;
            if (id == kPursuitThemeEvent) themes = true;
            if (id != kSongEndJumpEvent) continue;
            for (int a = 0; a < map[record + 15]; ++a) {
                const size_t at = record + kEventHeaderSize + kActionSize * static_cast<size_t>(a);
                const uint8_t kind = static_cast<uint8_t>((map[at + 6] >> 4) & 7);
                if (U32(map, at) == kActionTrackMask && (map[at + 5] & 0x7F) == 1 && (map[at + 6] & 0x0F) == 0x0D && (kind == 1 || kind == 2) &&
                    U16(map, at + 8) == kPartBuiltIn && gSongHeadCount < kMostSongHeads)
                    gSongHeads[gSongHeadCount++] = U16(map, at + 10);
            }
        }
        gSongEndReady = variable && themes && gSongHeadCount > 0;
        LOG(gSongEndReady ? "MW_Music map @%p has the song endings: %d songs end before the pursuit music"
                          : "MW_Music map @%p has no song endings, a pursuit stops the song as the game does", static_cast<const void*>(map), gSongHeadCount);
    }

    const uint8_t* MapOfTrack(uintptr_t track) {
        for (int p = 0; p < kProjectCount; ++p) {
            const uintptr_t project = reinterpret_cast<const uintptr_t*>(kPathProjects)[p];
            if (project == 0) continue;
            for (int t = 0; t < kTrackCount; ++t)
                if (*reinterpret_cast<const uintptr_t*>(project + kProjectTracks + sizeof(uintptr_t) * t) == track)
                    return *reinterpret_cast<const uint8_t* const*>(project + gSettings.ProjectMap);
        }
        return nullptr;
    }

    bool SongPlaying(int track, uintptr_t self) {
        __try {
            const int before = *reinterpret_cast<const int*>(self + kPrevMusicType);
            const int status = reinterpret_cast<PathTrackStatusFn>(kPathTrackStatus)(static_cast<unsigned>(track));
            const uintptr_t data = reinterpret_cast<PathTrackOfFn>(kPathTrackOf)(track);
            const uint8_t* map = data != 0 ? MapOfTrack(data) : nullptr;
            if (map != nullptr && map != gSongMap) ScanSongEndings(map);
            const int node = data != 0 ? *reinterpret_cast<const int16_t*>(data + kTrackNode) : -1;
            const int part = map != nullptr ? PlayingPart(map, node) : -1;
            const int volume = data != 0 ? *reinterpret_cast<const int8_t*>(data + kTrackVolume) : -1;
            bool song = false;
            for (int i = 0; i < gSongHeadCount && !song; ++i) song = gSongHeads[i] == part;
            const bool playing = status == kTrackQueueing || status == kTrackReadyForNext || status == kTrackPlaying || status == kTrackFading;
            LOG("pursuit start: music before %d (0 = EA Trax song), track %08X status %d, node %d in part %d (%s), volume %d, map %s",
                before, track, status, node, part, song ? "a song" : "not a song", volume, map == nullptr ? "not found" : gSongEndReady ? "has the song endings" : "has no song endings");
            return before == kMusicLicensed && playing && map != nullptr && gSongEndReady && song;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("pursuit start: reading the music track failed");
            return false;
        }
    }

    int __cdecl StopOrEndSong(int track, uintptr_t self) {
        gEATrax = self;
        gPursuitStartTick = GetTickCount();
        gTrackSegment = -1;
        const bool ending = gSwitches.SongEndings && SongPlaying(track, self);
        LOG("music type now %d, volume %d", MusicType(), MusicVolume());
        reinterpret_cast<PathSetNamedValueFn>(kPathSetNamedValue)(track, kSongEndVariable, ending ? 1 : 0);
        if (ending) {
            LOG("pursuit started during a song: the song plays its ending, then the pursuit music starts");
            return 0;
        }
        return reinterpret_cast<PathStopFn>(kPathStop)(track);
    }

    Patch gPatches[] = {
        { kPlaylistDefaults, kMaxSongsLoad,   sizeof(kMaxSongsLoad),   &AddSongs,    nullptr },
        { kInitializeEATrax, kFEDatabaseLoad, sizeof(kFEDatabaseLoad), &FixPlaylist, nullptr },
    };

    uint8_t* WriteBranch(uint8_t* at, uint8_t opcode, uintptr_t target) {
        *at++ = opcode;
        const int32_t relative = static_cast<int32_t>(target - (reinterpret_cast<uintptr_t>(at) + sizeof(int32_t)));
        std::memcpy(at, &relative, sizeof(relative));
        return at + sizeof(relative);
    }

    bool Install(Patch& patch) {
        uint8_t current[kJumpSize] = {};
        if (patch.length != kJumpSize || !Read(patch.site, current, sizeof(current)) || std::memcmp(current, patch.code, patch.length) != 0) return false;
        uint8_t* stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, kStubSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (stub == nullptr) return false;
        uint8_t* at = stub;
        *at++ = kPushFlags;
        *at++ = kPushAll;
        at = WriteBranch(at, kCall, reinterpret_cast<uintptr_t>(patch.entry));
        *at++ = kPopAll;
        *at++ = kPopFlags;
        std::memcpy(at, patch.code, patch.length);
        at += patch.length;
        WriteBranch(at, kJump, patch.site + patch.length);
        FlushInstructionCache(GetCurrentProcess(), stub, kStubSize);

        uint8_t jump[kJumpSize];
        jump[0] = kJump;
        const int32_t relative = static_cast<int32_t>(reinterpret_cast<uintptr_t>(stub) - (patch.site + kJumpSize));
        std::memcpy(jump + 1, &relative, sizeof(relative));
        if (!Write(patch.site, jump, sizeof(jump))) {
            VirtualFree(stub, 0, MEM_RELEASE);
            return false;
        }
        patch.stub = stub;
        return true;
    }

    bool InstallSongEndings() {
        uint8_t current[sizeof(kPathStopCall)] = {};
        if (!Read(kInteractiveStop, current, sizeof(current)) || std::memcmp(current, kPathStopCall, sizeof(current)) != 0) return false;
        uint8_t* stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, kStubSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (stub == nullptr) return false;
        uint8_t* at = stub;
        *at++ = kPushEsi;
        std::memcpy(at, kPushArgument, sizeof(kPushArgument));
        at += sizeof(kPushArgument);
        at = WriteBranch(at, kCall, reinterpret_cast<uintptr_t>(&StopOrEndSong));
        std::memcpy(at, kDropArguments, sizeof(kDropArguments));
        at += sizeof(kDropArguments);
        *at = kReturn;
        FlushInstructionCache(GetCurrentProcess(), stub, kStubSize);

        uint8_t call[sizeof(kPathStopCall)];
        call[0] = kCall;
        const int32_t relative = static_cast<int32_t>(reinterpret_cast<uintptr_t>(stub) - (kInteractiveStop + sizeof(call)));
        std::memcpy(call + 1, &relative, sizeof(relative));
        if (!Write(kInteractiveStop, call, sizeof(call))) {
            VirtualFree(stub, 0, MEM_RELEASE);
            return false;
        }
        gStopStub = stub;
        return true;
    }

    bool InstallSongs() {
        if (gSongCount == 0) {
            LOG("no songs in the TOML, EA Trax is left as the game has it");
            return false;
        }
        bool all = true;
        for (Patch& patch : gPatches) all = Install(patch) && all;
        LOG(all ? "EA Trax hooked: %d songs join the playlist" : "the EA Trax code doesn't match, the songs aren't added", gSongCount);
        if (!all)
            for (Patch& patch : gPatches)
                if (patch.stub != nullptr) {
                    Write(patch.site, patch.code, patch.length);
                    patch.stub = nullptr;
                }
        return all;
    }

}

namespace SFXObj_PFEATrax {

    bool Init() {
        const bool songs = InstallSongs();
        const bool endings = InstallSongEndings();
        LOG(endings ? "StartInteractiveMusic hooked: a song playing when a pursuit starts plays its ending first" : "the bytes at StartInteractiveMusic's PATH_stop don't match, songs stop as the game does");
        return songs && endings;
    }

    void Restore() {
        for (Patch& patch : gPatches)
            if (patch.stub != nullptr) {
                Write(patch.site, patch.code, patch.length);
                patch.stub = nullptr;
            }
        if (gStopStub != nullptr) {
            Write(kInteractiveStop, kPathStopCall, sizeof(kPathStopCall));
            gStopStub = nullptr;
        }
    }

}
