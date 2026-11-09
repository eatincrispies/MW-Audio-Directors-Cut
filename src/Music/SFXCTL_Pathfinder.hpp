#pragma once
#include <windows.h>

#define PATH_PROJECT_MAP                 0x018
#define PATH_PROJECT_TRACKS              0x040
#define PATH_MAP_MAGIC                   0x50464478
#define NO_NODE                          (-1)

namespace SFXCTL_Pathfinder {

    bool  Init();
    WORD  MapWord(const BYTE* map, DWORD at);
    DWORD MapDword(const BYTE* map, DWORD at);
    int   PlayingPart(const BYTE* map, int node);
    bool  TrackActive();
    bool  GamePaused();
    void  RestartTrack();

}
