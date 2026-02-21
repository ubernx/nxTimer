module;

#include <windows.h>
#include <chrono>
#include <thread>
#include <cstring>
#include <atomic>

export module TimerWorker;

import GameMemory;
import GameAddresses;
import Settings;

// Atomic here because the moment one thread writes those and another reads, has to be atomic to avoid UB
export struct TimerState {

    std::atomic<bool>   timerRunning{false};
    std::atomic<bool>   gameTimePaused{true};
    std::atomic<double> accumulatedTime{0.0};
    std::atomic<bool>   displayTotal{false};
    std::atomic<size_t> currentSplitIndex{0};

} timerState;

// + or += aren't inherently atomic for doubles, making own atomic adder
inline void atomicAdd(std::atomic<double>& a, double val) {

    double old = a.load(std::memory_order_relaxed);
    double desired;

    do {

        desired = old + val;

    } while (!a.compare_exchange_weak(old, desired, std::memory_order_relaxed));

}

export void TimerWorker() {

    timerState.timerRunning      = false;
    timerState.gameTimePaused    = true;
    timerState.accumulatedTime   = 0.0;
    timerState.currentSplitIndex = 0;

    auto previousTimePoint = std::chrono::steady_clock::now();
    auto nextTick= previousTimePoint;

    bool gameWasNotReady        = false;
    bool wasRunningLastFrame    = false;
    bool wasPausedLastFrame     = true;

    std::atomic<bool> finalSplitTriggered{false}; // Track if we've already done final split (atomic to guarantee single increment)


    while (true) {

        nextTick += std::chrono::microseconds(500); // 0.5ms target 2000hz

        // Try to get addresses even if the games not running
        if (!isGameReady()) {

            setupVersionOffsets();
            gameWasNotReady = true;
            finalSplitTriggered.store(false); // Reset on game disconnect
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;

        }

        if (gameWasNotReady) {

            previousTimePoint = std::chrono::steady_clock::now();
            nextTick = previousTimePoint;
            gameWasNotReady = false;

        }

        readGameMemorySnapshot();

        // Compute Changed states

        bool loadingChanged = snapShotCurrent.loading != snapShotPrevious.loading;

        bool pausedChanged = snapShotCurrent.isPaused != snapShotPrevious.isPaused;

        bool globalTimerChanged = snapShotCurrent.globalTimer != snapShotPrevious.globalTimer;

        // FINAL SPLIT DETECTION (latch on the raw 5-bytes == "final")
        const bool endIsFinalRaw = (std::memcmp(snapShotCurrent.EndRaw, "final", 5) == 0);
        if (endIsFinalRaw) {
            // Only let one iteration perform the split/stop. compare_exchange guarantees only one will win.
            bool expected = false;
            if (finalSplitTriggered.compare_exchange_strong(expected, true)) {

                if (timerState.timerRunning.load()) {
                    timerState.currentSplitIndex++;
                }
                timerState.timerRunning = false;
                timerState.displayTotal = true;
                timerState.gameTimePaused = true;

            }
        }



        // START LOGIC (block auto-start only if final split has latched)
        if (!finalSplitTriggered.load() && !timerState.timerRunning) {

            timerState.displayTotal = false;

            // Case 1
            if (snapShotCurrent.loading && loadingChanged) {

                timerState.timerRunning    = true;
                timerState.gameTimePaused  = true;
                timerState.accumulatedTime = 0.0;
                timerState.currentSplitIndex = 1; // index 0 is not assigned to any split, index 1 is the topest split
                finalSplitTriggered.store(false);

            }

            // Case 2
            if (!snapShotCurrent.isPaused && pausedChanged && snapShotCurrent.loading) {

                timerState.timerRunning    = true;
                timerState.gameTimePaused  = false;
                timerState.accumulatedTime = 0.0;
                timerState.currentSplitIndex = 1; // index 0 is not assigned to any split, index 1 is the topest split
                finalSplitTriggered.store(false);

            }

        }


        // isLoading LOGIC (calculate before split logic)

        bool isLoading =
            !snapShotCurrent.loading ||
            (snapShotCurrent.sync > versionOffsets.syncLowerBound &&
             snapShotCurrent.sync < versionOffsets.syncUpperBound) ||
            snapShotCurrent.prompt ||
            (!snapShotCurrent.isPaused &&
             snapShotCurrent.sync == 0 &&
             !globalTimerChanged);


        // AUTO-SPLIT LOGIC: Split when timer transitions from running to paused (loading starts)
        // Only if timer is still running (not stopped by final split)

        if (        timerState.timerRunning &&
                        wasRunningLastFrame &&
                        !wasPausedLastFrame &&
                                  isLoading &&
         (snapShotPrevious.focusState == 1) &&
         (snapShotCurrent.focusState != 1)) {
            timerState.currentSplitIndex++;
        }



        // Update pause state (only if timer is still running)

        if (timerState.timerRunning) {
            timerState.gameTimePaused = isLoading;
        }



        // Manual Key Handling


        if (GetAsyncKeyState(settings.timer_reset) & 1) {

            timerState.timerRunning    = false;
            timerState.accumulatedTime = 0.0;
            timerState.currentSplitIndex = 0;
            finalSplitTriggered.store(false);

        }

        if (GetAsyncKeyState(settings.timer_start_split) & 1) {

            if (!timerState.timerRunning) {

                timerState.timerRunning = true;
                timerState.accumulatedTime = 0.0;
                timerState.currentSplitIndex = 0;
                finalSplitTriggered.store(false);

            } else timerState.currentSplitIndex++;

        }

        if (GetAsyncKeyState(settings.timer_skip) & 1) timerState.currentSplitIndex++;

        if ((GetAsyncKeyState(settings.timer_undo) & 1) && (timerState.currentSplitIndex > 0)) timerState.currentSplitIndex--;

        // Accurate Time Accumulation (delta-based)

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> delta = now - previousTimePoint;
        previousTimePoint = now;

        if (timerState.timerRunning && !timerState.gameTimePaused) atomicAdd(timerState.accumulatedTime, delta.count());

        // Copy snapshot for next iteration

        snapShotPrevious = snapShotCurrent;
        wasRunningLastFrame = timerState.timerRunning.load();
        wasPausedLastFrame = timerState.gameTimePaused.load();

        // Sleep until next cycle

        std::this_thread::sleep_until(nextTick);
    }

}
