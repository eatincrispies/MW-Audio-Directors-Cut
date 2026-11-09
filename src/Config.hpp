#pragma once
#include <windows.h>

#define RESTORED_THEMES                  3
#define MOST_SONGS                       30
#define SONG_TEXT                        128
#define DEF_PLAY_TEXT                    8

namespace Config {

    enum Section {
        SECTION_ROOT = 0,
        SECTION_OTHER,
        SECTION_INTERRUPT_RAM,
        SECTION_INTERRUPT_RAM_HIGH,
        SECTION_CALL_FOR_EV,
        SECTION_SONG
    };

    struct Song {
        char  Album[SONG_TEXT];
        char  Artist[SONG_TEXT];
        char  SongName[SONG_TEXT];
        char  DefPlay[DEF_PLAY_TEXT];
        DWORD PathEvent;
    };

    struct Settings {
        bool  UnusedTheme[RESTORED_THEMES];
        bool  UnusedCooldownTheme;
        bool  UnusedBustedTheme;
        bool  SongEndingTransitions;
        float InterruptRamMinPlayerSpeed;
        float InterruptRamHighMinPlayerSpeed;
        bool  CallForEVUnitsDown;
        bool  CallForEVHeliDown;
        Song  Songs[MOST_SONGS];
        int   SongCount;

        Settings();
    };

    extern Settings gSettings;

    void Load(HMODULE module);

}
