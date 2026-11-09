#pragma once
#include <windows.h>

namespace MusicFlow {

    enum State {
        STATE_CHASE        = 0,
        STATE_CLOSING_IN   = 1,
        STATE_PULLING_AWAY = 2,
        STATE_COOLDOWN     = 3
    };

    enum PursuitStatus {
        PURSUIT_COOLDOWN = 2
    };

    bool Init();

}
