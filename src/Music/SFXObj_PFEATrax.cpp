#include "SFXObj_PFEATrax.hpp"
#include "SFXCTL_Pathfinder.hpp"
#include "../Config.hpp"
#include "../Memory.hpp"
#include <string.h>

namespace {

    #define PLAYLIST_DEFAULTS_SITE           0x005AB7BC
    #define INITIALIZE_EATRAX_SITE           0x004DF330
    #define INTERACTIVE_STOP_SITE            0x004DF258
    #define MAX_SONGS                        0x008F42C4
    #define SONGS                            0x009123CC
    #define SONGS_INSERT                     0x005AAC30
    #define FE_DATABASE                      0x0091CF90
    #define PATH_STOP                        0x00829310
    #define PATH_TRACK_STATUS                0x00829210
    #define PATH_SET_NAMED_VALUE             0x00829640
    #define PATH_TRACK_OF                    0x0082CAA0
    #define PATH_PROJECTS                    0x009C2954

    #define SONGS_FIRST                      0x004
    #define SONGS_LAST                       0x008
    #define DATABASE_PROFILE                 0x010
    #define PROFILE_PLAYLIST                 0x324
    #define PREV_MUSIC_TYPE                  0x14C
    #define TRACK_NODE                       0x004
    #define PLAYLIST_SLOTS                   30
    #define PATH_PROJECT_COUNT               4
    #define PATH_TRACK_COUNT                 24

    #define MAP_EVENT_COUNT                  0x0F
    #define MAP_VARIABLE_COUNT               0x11
    #define MAP_EVENT_TABLE                  0x1C
    #define MAP_VARIABLE_TABLE               0x24
    #define EVENT_ID                         12
    #define EVENT_ACTION_COUNT               15
    #define EVENT_HEADER_SIZE                20
    #define EVENT_ID_MASK                    0x00FFFFFF
    #define ACTION_SIZE                      12
    #define ACTION_TRACKS                    0x11000001
    #define ACTION_TYPE                      5
    #define ACTION_FORM                      6
    #define ACTION_FIRST                     8
    #define ACTION_SECOND                    10
    #define ACTION_TYPE_MASK                 0x7F
    #define ACTION_CONDITION                 1
    #define ACTION_OPERANDS_MASK             0x0F
    #define ACTION_BUILTIN_IS_CONSTANT       0x0D
    #define ACTION_CLAUSE_SHIFT              4
    #define ACTION_CLAUSE_MASK               7
    #define BUILTIN_PART                     3
    #define VARIABLE_SIZE                    20
    #define VARIABLE_NAME_SIZE               16

    #define SONG_END_JUMP_EVENT              0x007B768A
    #define PURSUIT_THEME_EVENT              0x006E7283
    #define SONG_END_VARIABLE                "songend"
    #define MOST_SONG_HEADS                  64
    #define LOAD_SIZE                        5

    const BYTE kMaxSongsLoad[]               = { 0xA1, 0xC4, 0x42, 0x8F, 0x00 };
    const BYTE kFEDatabaseLoad[]             = { 0xA1, 0x90, 0xCF, 0x91, 0x00 };
    const BYTE kPathStopCall[]               = { 0xE8, 0xB3, 0xA0, 0x34, 0x00 };

    struct SongInfo {
        const char* SongName;
        const char* Artist;
        const char* Album;
        const char* DefPlay;
        int         PathEvent;
    };

    struct JukeboxEntry {
        DWORD SongIndex;
        BYTE  Playability;
        BYTE  Padding[3];
    };

    typedef void  (__thiscall* SongsInsertMethod)(void* songs, SongInfo** where, unsigned int count, SongInfo* const* value);
    typedef int   (__cdecl* PathStopFunction)(int track);
    typedef int   (__cdecl* PathTrackStatusFunction)(unsigned int track);
    typedef int   (__cdecl* PathSetNamedValueFunction)(int track, const char* name, int value);
    typedef DWORD (__cdecl* PathTrackOfFunction)(int track);

    SongInfo    gSongInfo[MOST_SONGS];
    int         gVanillaSongs                = -1;
    DWORD       gPlaylistDefaultsBack        = PLAYLIST_DEFAULTS_SITE + LOAD_SIZE;
    DWORD       gInitializeEATraxBack        = INITIALIZE_EATRAX_SITE + LOAD_SIZE;
    const BYTE* gSongMap                     = NULL;
    bool        gSongEndReady                = false;
    int         gSongHeads[MOST_SONG_HEADS];
    int         gSongHeadCount               = 0;

    BYTE Playable(const char* defPlay) {
        if (strcmp(defPlay, "FE") == 0) return SFXObj_PFEATrax::PLAY_MENU;
        if (strcmp(defPlay, "IG") == 0) return SFXObj_PFEATrax::PLAY_RACE;
        if (strcmp(defPlay, "AL") == 0) return SFXObj_PFEATrax::PLAY_ALL;
        return SFXObj_PFEATrax::PLAY_OFF;
    }

    void __cdecl AddSongs() {
        const Config::Settings& s = Config::gSettings;
        __try {
            int* maxSongs = (int*)MAX_SONGS;
            SongInfo** first = *(SongInfo***)(SONGS + SONGS_FIRST);
            SongInfo** last = *(SongInfo***)(SONGS + SONGS_LAST);
            if (gVanillaSongs >= 0 || s.SongCount == 0 || first == NULL || last - first != *maxSongs) return;
            gVanillaSongs = *maxSongs;
            int room = PLAYLIST_SLOTS - gVanillaSongs;
            int added = 0;
            for (int i = 0; i < s.SongCount && added < room; ++i) {
                SongInfo* info = &gSongInfo[added];
                info->SongName = s.Songs[i].SongName;
                info->Artist = s.Songs[i].Artist;
                info->Album = s.Songs[i].Album;
                info->DefPlay = s.Songs[i].DefPlay;
                info->PathEvent = (int)s.Songs[i].PathEvent;
                SongInfo** end = *(SongInfo***)(SONGS + SONGS_LAST);
                ((SongsInsertMethod)SONGS_INSERT)((void*)SONGS, end, 1, &info);
                ++added;
            }
            *maxSongs = gVanillaSongs + added;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void __cdecl FixPlaylist() {
        __try {
            int total = *(const int*)MAX_SONGS;
            DWORD database = *(const DWORD*)FE_DATABASE;
            if (database == 0 || total <= 0 || total > PLAYLIST_SLOTS) return;
            DWORD profile = *(const DWORD*)(database + DATABASE_PROFILE);
            if (profile == 0) return;
            JukeboxEntry* playlist = (JukeboxEntry*)(profile + PROFILE_PLAYLIST);
            SongInfo* const* songs = *(SongInfo* const* const*)(SONGS + SONGS_FIRST);
            int vanilla = (gVanillaSongs >= 0) ? gVanillaSongs : total;
            bool present[PLAYLIST_SLOTS];
            bool kept[PLAYLIST_SLOTS];
            memset(present, 0, sizeof(present));
            memset(kept, 0, sizeof(kept));
            for (int slot = 0; slot < total; ++slot) {
                DWORD index = playlist[slot].SongIndex;
                if (index < (DWORD)total && !present[index]) {
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
                const SongInfo* song = (songs != NULL) ? songs[index] : NULL;
                playlist[slot].SongIndex = (DWORD)index;
                playlist[slot].Playability = (song != NULL && song->DefPlay != NULL) ? Playable(song->DefPlay) : (BYTE)SFXObj_PFEATrax::PLAY_OFF;
                kept[slot] = true;
                present[index] = true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    bool HasSongEndVariable(const BYTE* map) {
        DWORD variables = SFXCTL_Pathfinder::MapDword(map, MAP_VARIABLE_TABLE);
        for (int v = 0; v < map[MAP_VARIABLE_COUNT]; ++v)
            if (strncmp((const char*)(map + variables + VARIABLE_SIZE * v), SONG_END_VARIABLE, VARIABLE_NAME_SIZE) == 0) return true;
        return false;
    }

    bool IsSongHeadTest(const BYTE* map, DWORD at) {
        BYTE form = map[at + ACTION_FORM];
        int clause = (form >> ACTION_CLAUSE_SHIFT) & ACTION_CLAUSE_MASK;
        return SFXCTL_Pathfinder::MapDword(map, at) == ACTION_TRACKS
            && (map[at + ACTION_TYPE] & ACTION_TYPE_MASK) == ACTION_CONDITION
            && (form & ACTION_OPERANDS_MASK) == ACTION_BUILTIN_IS_CONSTANT
            && (clause == SFXObj_PFEATrax::CLAUSE_IF || clause == SFXObj_PFEATrax::CLAUSE_ELSE_IF)
            && SFXCTL_Pathfinder::MapWord(map, at + ACTION_FIRST) == BUILTIN_PART;
    }

    void ScanSongEndings(const BYTE* map) {
        gSongMap = map;
        gSongEndReady = false;
        gSongHeadCount = 0;
        if (SFXCTL_Pathfinder::MapDword(map, 0) != PATH_MAP_MAGIC) return;
        bool themes = false;
        DWORD events = SFXCTL_Pathfinder::MapDword(map, MAP_EVENT_TABLE);
        for (int e = 0; e < map[MAP_EVENT_COUNT]; ++e) {
            DWORD record = (DWORD)SFXCTL_Pathfinder::MapWord(map, events + 2 * e) * 4;
            DWORD id = SFXCTL_Pathfinder::MapDword(map, record + EVENT_ID) & EVENT_ID_MASK;
            if (id == PURSUIT_THEME_EVENT) themes = true;
            if (id != SONG_END_JUMP_EVENT) continue;
            for (int a = 0; a < map[record + EVENT_ACTION_COUNT]; ++a) {
                DWORD at = record + EVENT_HEADER_SIZE + ACTION_SIZE * a;
                if (IsSongHeadTest(map, at) && gSongHeadCount < MOST_SONG_HEADS)
                    gSongHeads[gSongHeadCount++] = SFXCTL_Pathfinder::MapWord(map, at + ACTION_SECOND);
            }
        }
        gSongEndReady = HasSongEndVariable(map) && themes && gSongHeadCount > 0;
    }

    const BYTE* MapOfTrack(DWORD track) {
        for (int p = 0; p < PATH_PROJECT_COUNT; ++p) {
            DWORD project = ((const DWORD*)PATH_PROJECTS)[p];
            if (project == 0) continue;
            for (int t = 0; t < PATH_TRACK_COUNT; ++t)
                if (*(const DWORD*)(project + PATH_PROJECT_TRACKS + sizeof(DWORD) * t) == track)
                    return *(const BYTE* const*)(project + PATH_PROJECT_MAP);
        }
        return NULL;
    }

    bool SongPlaying(int track, const BYTE* self) {
        __try {
            int before = *(const int*)(self + PREV_MUSIC_TYPE);
            int status = ((PathTrackStatusFunction)PATH_TRACK_STATUS)((unsigned int)track);
            DWORD data = ((PathTrackOfFunction)PATH_TRACK_OF)(track);
            const BYTE* map = (data != 0) ? MapOfTrack(data) : NULL;
            if (map != NULL && map != gSongMap) ScanSongEndings(map);
            int node = (data != 0) ? *(const short*)(data + TRACK_NODE) : NO_NODE;
            int part = (map != NULL) ? SFXCTL_Pathfinder::PlayingPart(map, node) : NO_NODE;
            bool song = false;
            for (int i = 0; i < gSongHeadCount && !song; ++i) song = (gSongHeads[i] == part);
            bool playing = status == SFXObj_PFEATrax::TRACK_QUEUEING || status == SFXObj_PFEATrax::TRACK_READY
                        || status == SFXObj_PFEATrax::TRACK_PLAYING || status == SFXObj_PFEATrax::TRACK_FADING;
            return before == SFXObj_PFEATrax::MUSIC_LICENSED && playing && map != NULL && gSongEndReady && song;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    int __cdecl StopOrEndSong(int track, const BYTE* self) {
        SFXCTL_Pathfinder::RestartTrack();
        bool ending = Config::gSettings.SongEndingTransitions && SongPlaying(track, self);
        ((PathSetNamedValueFunction)PATH_SET_NAMED_VALUE)(track, SONG_END_VARIABLE, ending ? 1 : 0);
        if (ending) return 0;
        return ((PathStopFunction)PATH_STOP)(track);
    }

    __declspec(naked) void PlaylistDefaultsHook() {
        __asm {
            pushfd
            pushad
            call AddSongs
            popad
            popfd
            mov  eax, dword ptr ds:[MAX_SONGS]
            jmp  dword ptr [gPlaylistDefaultsBack]
        }
    }

    __declspec(naked) void InitializeEATraxHook() {
        __asm {
            pushfd
            pushad
            call FixPlaylist
            popad
            popfd
            mov  eax, dword ptr ds:[FE_DATABASE]
            jmp  dword ptr [gInitializeEATraxBack]
        }
    }

    __declspec(naked) void InteractiveStopCall() {
        __asm {
            push esi
            push dword ptr [esp + 8]
            call StopOrEndSong
            add  esp, 8
            ret
        }
    }

}

namespace SFXObj_PFEATrax {

    bool Init() {
        bool songs = false;
        if (Config::gSettings.SongCount > 0) {
            int saved = Memory::Patches();
            songs = Memory::PatchBranch(PLAYLIST_DEFAULTS_SITE, kMaxSongsLoad, sizeof(kMaxSongsLoad), OPCODE_JUMP, (const void*)&PlaylistDefaultsHook)
                 && Memory::PatchBranch(INITIALIZE_EATRAX_SITE, kFEDatabaseLoad, sizeof(kFEDatabaseLoad), OPCODE_JUMP, (const void*)&InitializeEATraxHook);
            if (!songs) Memory::Unpatch(saved);
        }
        bool endings = Memory::PatchBranch(INTERACTIVE_STOP_SITE, kPathStopCall, sizeof(kPathStopCall), OPCODE_CALL, (const void*)&InteractiveStopCall);
        return songs && endings;
    }

}
