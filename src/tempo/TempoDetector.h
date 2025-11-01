#ifndef TEMPO_DETECTOR_H
#define TEMPO_DETECTOR_H

#include <Arduino.h>
#include "../config/Constants.h"

// Tempo detection from tap intervals
class TempoDetector {
private:
  // Tap history (circular buffer)
  unsigned long tapHistory[TempoConfig::PRESS_HISTORY_SIZE];
  int tapCount = 0;

  // Calculated tempo
  int bpm = 0;
  unsigned long interval = 0;
  bool isLocked = false;

public:
  TempoDetector() {
    reset();
  }

  // Add a new tap
  void addTap(unsigned long currentTime) {
    // Taps 1-4: Store directly
    if (tapCount < TempoConfig::PRESS_HISTORY_SIZE) {
      tapHistory[tapCount] = currentTime;
      tapCount++;
    }
    // Tap 5+: Shift window
    else {
      for (int i = 0; i < TempoConfig::PRESS_HISTORY_SIZE - 1; i++) {
        tapHistory[i] = tapHistory[i + 1];
      }
      tapHistory[TempoConfig::PRESS_HISTORY_SIZE - 1] = currentTime;
      tapCount = TempoConfig::PRESS_HISTORY_SIZE;
    }

    // Calculate tempo if we have enough taps
    if (tapCount >= TempoConfig::MIN_TAPS_FOR_PREDICTION) {
      calculateTempo();
    }
  }

  // Calculate tempo from tap history
  void calculateTempo() {
    if (tapCount < TempoConfig::MIN_TAPS_FOR_PREDICTION) return;

    unsigned long avgInterval = 0;
    int intervalCount = 0;

    // Tap 3: First prediction (2 intervals)
    if (tapCount == 3) {
      unsigned long interval1 = tapHistory[1] - tapHistory[0];
      unsigned long interval2 = tapHistory[2] - tapHistory[1];
      avgInterval = (interval1 + interval2) / 2;
      intervalCount = 2;
      Serial.println("🎯 FIRST PREDICTION!");
    }
    // Tap 4+: Exponential weighting (recent taps = higher influence)
    else {
      unsigned long interval1 = tapHistory[1] - tapHistory[0];
      unsigned long interval2 = tapHistory[2] - tapHistory[1];
      unsigned long interval3 = tapHistory[3] - tapHistory[2];

      // Exponential weighting: 1x, 2x, 4x
      avgInterval = (interval1 + interval2 * 2 + interval3 * 4) / 7;
      intervalCount = 3;
      isLocked = true;
      Serial.println("🔄 TEMPO ADJUSTED!");
    }

    // Convert to BPM
    interval = avgInterval;
    bpm = 60000 / avgInterval;

    // Clamp to reasonable range
    if (bpm < TempoConfig::MIN_BPM) {
      bpm = TempoConfig::MIN_BPM;
      interval = 60000 / TempoConfig::MIN_BPM;
    }
    if (bpm > TempoConfig::MAX_BPM) {
      bpm = TempoConfig::MAX_BPM;
      interval = 60000 / TempoConfig::MAX_BPM;
    }

    Serial.print("📊 BPM: "); Serial.print(bpm);
    Serial.print(" ("); Serial.print(interval); Serial.print("ms)");
    Serial.print(" | From "); Serial.print(intervalCount); Serial.println(" intervals");
  }

  // Reset tempo detection
  void reset() {
    tapCount = 0;
    bpm = 0;
    interval = 0;
    isLocked = false;
    for (int i = 0; i < TempoConfig::PRESS_HISTORY_SIZE; i++) {
      tapHistory[i] = 0;
    }
  }

  // Getters
  int getBPM() const { return bpm; }
  unsigned long getInterval() const { return interval; }
  int getTapCount() const { return tapCount; }
  bool isTempoLocked() const { return isLocked; }
  bool hasEnoughTaps() const { return tapCount >= TempoConfig::MIN_TAPS_FOR_PREDICTION; }
};

#endif // TEMPO_DETECTOR_H
