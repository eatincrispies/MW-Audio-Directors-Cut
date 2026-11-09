#pragma once
#include <windows.h>

namespace SFXObj_PFEATrax {

    enum Playability {
        PLAY_OFF  = 0,
        PLAY_MENU = 1,
        PLAY_RACE = 2,
        PLAY_ALL  = 3
    };

    enum TrackStatus {
        TRACK_QUEUEING = 2,
        TRACK_READY    = 4,
        TRACK_PLAYING  = 5,
        TRACK_FADING   = 6
    };

    enum MusicType {
        MUSIC_LICENSED = 0
    };

    enum ActionClause {
        CLAUSE_IF      = 1,
        CLAUSE_ELSE_IF = 2
    };

    bool Init();

}
