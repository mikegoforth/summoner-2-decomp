#ifndef GAME_TIMESTAMP_H
#define GAME_TIMESTAMP_H

#include "types.h"

#define MAX_TIME 1072800000

void UpdateTicker(int delta_milliseconds);
void IncrementPauseCounter(void);
void DecrementPauseCounter(void);

class GameTimer {
    int value;

public:
    GameTimer() { value = -1; }
    void SetTimeout(int delta_milliseconds);
    void SetRandomTimeout(int low_milliseconds, int high_milliseconds);
    bool elapsed();
    int GetRemainingTime();
    int GetElapsedTime();
    bool valid();
};

class RealtimeTimer {
    int value;

public:
    RealtimeTimer() { value = -1; }
    void SetTimeout(int delta_milliseconds);
    void SetRandomTimeout(int low_milliseconds, int high_milliseconds);
    bool elapsed();
    int GetRemainingTime();
    int GetElapsedTime();
    bool valid();
};

extern int Timestamp_ticker;
extern int Timestamp_realtime_ticker;
extern int Timestamp_paused;

#endif // GAME_TIMESTAMP_H
