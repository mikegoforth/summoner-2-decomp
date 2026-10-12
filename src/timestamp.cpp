#include "game/timestamp.h"

// TODO: name once its unit is split (likely rand()).
extern "C" unsigned int func_00191210(void);

int Timestamp_ticker = 1;
int Timestamp_realtime_ticker = 1;
int Timestamp_paused;

void UpdateTicker(int delta_milliseconds) {
    int remainingTime;
    int remainingRealTime;
    if (!Timestamp_paused) {
        remainingTime = MAX_TIME - Timestamp_ticker;
        if (delta_milliseconds > remainingTime) {
            Timestamp_ticker = delta_milliseconds - remainingTime;
        } else {
            Timestamp_ticker += delta_milliseconds;
        }
    }
    remainingRealTime = MAX_TIME - Timestamp_realtime_ticker;
    if (delta_milliseconds > remainingRealTime) {
        Timestamp_realtime_ticker = delta_milliseconds - remainingRealTime;
    } else {
        Timestamp_realtime_ticker += delta_milliseconds;
    }
}

void IncrementPauseCounter() { Timestamp_paused += 1; }

void DecrementPauseCounter() { Timestamp_paused -= 1; }

void GameTimer::SetTimeout(int delta_milliseconds) {
    int maxTime;
    if (delta_milliseconds >= 0) {
        maxTime = (MAX_TIME - Timestamp_ticker);
        if (delta_milliseconds > maxTime) {
            value = delta_milliseconds - maxTime;
            return;
        }
        value = Timestamp_ticker + delta_milliseconds;
        return;
    }
    value = Timestamp_ticker + delta_milliseconds;
    if (value < 0) {
        value += MAX_TIME;
    }
}

void GameTimer::SetRandomTimeout(int low_milliseconds, int high_milliseconds) {
    SetTimeout(((func_00191210() % (high_milliseconds - low_milliseconds + 1)) + low_milliseconds));
}

bool GameTimer::elapsed() {
    int remainingTimerTime;
    int timeSinceTimerFinished;
    if (value < 0) {
        return false;
    }
    if (value > Timestamp_ticker) {
        remainingTimerTime = value - Timestamp_ticker;
        if (remainingTimerTime > MAX_TIME / 2) {
            return true;
        }
        return false;
    }
    timeSinceTimerFinished = Timestamp_ticker - value;
    if (timeSinceTimerFinished > MAX_TIME / 2) {
        return false;
    }
    return true;
}

int GameTimer::GetRemainingTime() {
    int remainingTimerTime;
    int timeSinceTimerFinished;
    if (value < 0) {
        return MAX_TIME;
    }
    if (value > Timestamp_ticker) {
        remainingTimerTime = value - Timestamp_ticker;
        if (remainingTimerTime > MAX_TIME / 2) {
            return -(MAX_TIME - value + Timestamp_ticker);
        }
        return remainingTimerTime;
    }
    timeSinceTimerFinished = Timestamp_ticker - value;
    if (timeSinceTimerFinished > MAX_TIME / 2) {
        return (MAX_TIME - Timestamp_ticker) + value;
    }
    return -(timeSinceTimerFinished);
}

int GameTimer::GetElapsedTime() { return -GetRemainingTime(); }

bool GameTimer::valid() {
    if (value < 0) {
        return false;
    }
    return true;
}

void RealtimeTimer::SetTimeout(int delta_milliseconds) {
    int maxTime;
    if (delta_milliseconds >= 0) {
        maxTime = (MAX_TIME - Timestamp_realtime_ticker);
        if (delta_milliseconds > maxTime) {
            value = delta_milliseconds - maxTime;
            return;
        }
        value = Timestamp_realtime_ticker + delta_milliseconds;
        return;
    }
    value = Timestamp_realtime_ticker + delta_milliseconds;
    if (value < 0) {
        value += MAX_TIME;
    }
}

void RealtimeTimer::SetRandomTimeout(int low_milliseconds, int high_milliseconds) {
    SetTimeout(((func_00191210() % (high_milliseconds - low_milliseconds + 1)) + low_milliseconds));
}

bool RealtimeTimer::elapsed() {
    int remainingTimerTime;
    int timeSinceTimerFinished;
    if (value < 0) {
        return false;
    }
    if (value > Timestamp_realtime_ticker) {
        remainingTimerTime = value - Timestamp_realtime_ticker;
        if (remainingTimerTime > MAX_TIME / 2) {
            return true;
        }
        return false;
    }
    timeSinceTimerFinished = Timestamp_realtime_ticker - value;
    if (timeSinceTimerFinished > MAX_TIME / 2) {
        return false;
    }
    return true;
}

int RealtimeTimer::GetRemainingTime() {
    int remainingTimerTime;
    int timeSinceTimerFinished;
    if (value < 0) {
        return MAX_TIME;
    }
    if (value > Timestamp_realtime_ticker) {
        remainingTimerTime = value - Timestamp_realtime_ticker;
        if (remainingTimerTime > MAX_TIME / 2) {
            return -(MAX_TIME - value + Timestamp_realtime_ticker);
        }
        return remainingTimerTime;
    }
    timeSinceTimerFinished = Timestamp_realtime_ticker - value;
    if (timeSinceTimerFinished > MAX_TIME / 2) {
        return (MAX_TIME - Timestamp_realtime_ticker) + value;
    }
    return -(timeSinceTimerFinished);
}

int RealtimeTimer::GetElapsedTime() { return -GetRemainingTime(); }

bool RealtimeTimer::valid() {
    if (value < 0) {
        return false;
    }
    return true;
}
