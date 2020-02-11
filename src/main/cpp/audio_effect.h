/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EFFECT_PROCESSOR_H
#define EFFECT_PROCESSOR_H

#include <SLES/OpenSLES_Android.h>
#include <cstdint>
#include <atomic>
#include <mutex>

class AudioFormat {
 protected:
  int32_t sampleRate_ = SL_SAMPLINGRATE_48;
  int32_t channelCount_ = 2;
  SLuint32 format_ = SL_PCMSAMPLEFORMAT_FIXED_16;

  AudioFormat(int32_t sampleRate, int32_t channelCount, SLuint32 format)
      : sampleRate_(sampleRate), channelCount_(channelCount), format_(format){};

  virtual ~AudioFormat() {}
};

/**
 * An audio delay effect:
 *   - decay is for feedback(echo)weight
 *   - delay time is adjustable
 */
class AudioDelay : public AudioFormat {
 public:
  ~AudioDelay();

  explicit AudioDelay(int32_t sampleRate, int32_t channelCount, SLuint32 format,
                      size_t delayTimeLInMs, size_t delayTimeRInMs);                                                        //, float Weight
  bool setDelayTime(size_t delayTimeLInMiliSec, size_t delayTimeRInMiliSec);
  size_t getDelayTime(void) const;
  void setDecayWeight(float weight);
  float getDecayWeight(void) const;
  void process(int16_t *liveAudio, int32_t numFrames);

 private:
  size_t delayTimeL_ = 0;
  float decayWeight_ = 0.5;
  void *bufferL_ = nullptr;
  size_t bufCapacityL_ = 0;
  size_t bufSizeL_ = 0;
  size_t curPosL_ = 0;

  size_t delayTimeR_ = 0;
  void *bufferR_ = nullptr;
  size_t bufCapacityR_ = 0;
  size_t bufSizeR_ = 0;
  size_t curPosR_ = 0;

  std::mutex lock_;
  int32_t feedbackFactor_;
  int32_t liveAudioFactor_;
  void allocateBufferL(void);
  void allocateBufferR(void);
};
#endif  // EFFECT_PROCESSOR_H
