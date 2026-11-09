#pragma once
#include <windows.h>

namespace EAXCop {

    enum Impact {
        IMPACT_REAR_ENDED  = 1,
        IMPACT_T_BONED     = 3,
        IMPACT_HEAD_ON     = 5,
        IMPACT_SIDE_SWIPED = 7
    };

    enum Intensity {
        INTENSITY_NORMAL = 1,
        INTENSITY_HIGH   = 2
    };

    enum EVType {
        EV_MULTIPLE_UNITS_DOWN = 1,
        EV_GAS_STATION_FIRE    = 2,
        EV_HELI_DOWN           = 4
    };

    enum Subject {
        ABOUT_SELF       = 0,
        ABOUT_OTHER_UNIT = 1
    };

}

namespace SndStrmWrapper {

    enum RequestState {
        REQUEST_QUEUED  = 0,
        REQUEST_LOADING = 1,
        REQUEST_PLAYING = 2,
        REQUEST_DONE    = 3
    };

    struct StreamStatus {
        int requests;
        int head;
        int buffered;
    };

    struct RequestStatus {
        int state;
        int played;
        int buffered;
        int length;
    };

}

namespace SoundAI {

    enum Reaction {
        REACT_NONE = 0,
        REACT_RAMMED_NORMAL,
        REACT_RAMMED_HIGH,
        REACT_INTERRUPT_RAM,
        REACT_INTERRUPT_RAM_HIGH
    };

    enum PostValidateResult {
        POST_VALIDATE_KEEP  = 0,
        POST_VALIDATE_DITCH = 2
    };

    bool Init();

}
