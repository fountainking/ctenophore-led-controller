#ifndef BEAT_SYNCHRONIZER_H
#define BEAT_SYNCHRONIZER_H

#include <Arduino.h>
#include <functional>
#include "../config/Constants.h"

// Drift-free beat timing system
class BeatSynchronizer {
private:
  unsigned long nextBeatTime = 0;
  unsigned long lastDriftCorrection = 0;
  unsigned long interval = 0;
  bool isActive = false;
  std::function<void()> onBeat;

public:
  BeatSynchronizer() {}

  // Set beat callback
  void setOnBeat(std::function<void()> callback) {
    onBeat = callback;
  }

  // Start beat synchronization
  void start(unsigned long beatInterval, unsigned long currentTime) {
    interval = beatInterval;
    nextBeatTime = currentTime + interval;
    isActive = true;
    lastDriftCorrection = currentTime;

    Serial.println("🎵 Beat sync started!");
  }

  // Stop beat synchronization
  void stop() {
    isActive = false;
    Serial.println("⏹️ Beat sync stopped");
  }

  // Update beat timing (call every frame)
  void update(unsigned long currentTime) {
    if (!isActive || interval == 0) return;

    // Check if it's time for a beat
    if (currentTime >= nextBeatTime) {
      // Trigger beat callback
      if (onBeat) {
        onBeat();
      }

      // ADDITIVE timing - prevents drift
      nextBeatTime += interval;

      // Drift correction every 10 seconds
      if (currentTime - lastDriftCorrection > TempoConfig::DRIFT_CORRECTION_INTERVAL_MS) {
        long drift = (long)nextBeatTime - (long)currentTime;

        // If drift > 25% of beat interval, resync
        if (abs(drift) > interval / 4) {
          Serial.print("⚡ Drift correction: "); Serial.print(drift); Serial.println("ms");
          nextBeatTime = currentTime + interval;
        }

        lastDriftCorrection = currentTime;
      }

      // Safety check: if nextBeatTime is way in the past, resync
      if (nextBeatTime < currentTime - interval) {
        Serial.println("⚡ Major resync needed");
        nextBeatTime = currentTime + interval;
      }
    }
  }

  // Resync to manual tap (for stride tracking)
  void resyncToTap(unsigned long tapTime, unsigned long newInterval) {
    if (newInterval > 0) {
      interval = newInterval;
    }
    nextBeatTime = tapTime + interval;
    Serial.println("⚡ Beat resynced to tap!");
  }

  // Update interval without disrupting timing
  void updateInterval(unsigned long newInterval) {
    if (newInterval > 0) {
      interval = newInterval;
    }
  }

  // Getters
  bool getIsActive() const { return isActive; }
  unsigned long getInterval() const { return interval; }
  unsigned long getNextBeatTime() const { return nextBeatTime; }
  unsigned long getTimeUntilBeat(unsigned long currentTime) const {
    if (!isActive || nextBeatTime <= currentTime) return 0;
    return nextBeatTime - currentTime;
  }
};

#endif // BEAT_SYNCHRONIZER_H
