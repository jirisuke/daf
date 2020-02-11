/*
 * Copyright 2018 The Android Open Source Project
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
#include "audio_effect.h"
#include "audio_common.h"
#include <climits>
#include <cstring>

/*
 * Mixing Audio in integer domain to avoid FP calculation
 *   (FG * ( MixFactor * 16 ) + BG * ( (1.0f-MixFactor) * 16 )) / 16
 */
static const int32_t kFloatToIntMapFactor = 128;
static const uint32_t kMsPerSec = 1000;
/**
 * Constructor for AudioDelay
 * @param sampleRate
 * @param channelCount
 * @param format
 * @param delayTimeInMs
 */
AudioDelay::AudioDelay(int32_t sampleRate, int32_t channelCount,                                    //AudioDelayオブジェクトにサンプリング周波数、チャンネル数、format、遅延時間代入されて呼び出すとき
                       SLuint32 format, size_t delayTimeLInMs, size_t delayTimeRInMs                                        //ここ重要なのに分からない。コロンの使い方？
                       )                                                                            //float decayWeight
    : AudioFormat(sampleRate, channelCount, format),                                                //decayWeight_(decayWeight)
      delayTimeL_(delayTimeLInMs), delayTimeR_(delayTimeRInMs){                                                                   //音の重さ　とは？
    feedbackFactor_ = static_cast<int32_t>(decayWeight_ * kFloatToIntMapFactor);                    //これとると遅延反応しなくなる kFloatToIntMapFactorのポインタ。何のアドレス示してる？
  liveAudioFactor_ = kFloatToIntMapFactor - feedbackFactor_;
  allocateBufferL();
  allocateBufferR();                                                                                 //OuterClass::InnerClass　名前区切りにコｒン２つ
}                                                                                                   //(){}の形分からない

/**
 * Destructor
 */
AudioDelay::~AudioDelay() {
  if (bufferR_) delete static_cast<uint8_t*>(bufferR_);
  if (bufferL_) delete static_cast<uint8_t*>(bufferL_);                                           //AudioDelay()で呼び出すと何するのこれ？
}

/**
 * Configure for delay time ( in miliseconds ), dynamically adjustable
 * @param delayTimeInMS in miliseconds
 * @return true if delay time is set successfully
 */
bool AudioDelay::setDelayTime(size_t delayTimeLInMS, size_t delayTimeRInMS) {
  if ((delayTimeLInMS == delayTimeL_) && (delayTimeRInMS == delayTimeR_)) return true;

  std::lock_guard<std::mutex> lock(lock_);

  if (bufferL_) {
    delete static_cast<uint8_t*>(bufferL_);
    bufferL_ = nullptr;
  }
  if (bufferR_) {
      delete static_cast<uint8_t*>(bufferR_);
      bufferR_ = nullptr;
  }

  delayTimeL_ = delayTimeLInMS;                                                                       //audio_effectオブジェクトのフィールド値にaudio_mainに代入？
  allocateBufferL();
  delayTimeR_ = delayTimeRInMS;
  allocateBufferR();
  return ((bufferL_ != nullptr) && (bufferR_ != nullptr));
}

/**
 * Internal helper function to allocate buffer for the delay
 *  - calculate the buffer size for the delay time
 *  - allocate and zero out buffer (0 means silent audio)
 *  - configure bufSize_ to be size of audioFrames
 */
void AudioDelay::allocateBufferL(void) {                                                           //遅延させるためのバッファ？　遅延時間に応じたバッファサイズ算出
  float floatDelayTime = (float)delayTimeL_ / kMsPerSec;
  float fNumFrames = floatDelayTime * (float)sampleRate_ / kMsPerSec;
  size_t sampleCount = static_cast<uint32_t>(fNumFrames + 0.5f) * channelCount_;

  uint32_t bytePerSample = format_ / 8;
  assert(bytePerSample <= 4 && bytePerSample);

  uint32_t bytePerFrame = channelCount_ * bytePerSample;

  // get bufCapacity in bytes
  bufCapacityL_ = sampleCount * bytePerSample;
  bufCapacityL_ =
      ((bufCapacityL_ + bytePerFrame - 1) / bytePerFrame) * bytePerFrame;

  bufferL_ = new uint8_t[bufCapacityL_];
  assert(bufferL_);

  memset(bufferL_, 0, bufCapacityL_);
  curPosL_ = 0;

  // bufSize_ is in Frames ( not samples, not bytes )
  bufSizeL_ = bufCapacityL_ / bytePerFrame;
}

void AudioDelay::allocateBufferR(void) {                                                           //遅延させるためのバッファ？　遅延時間に応じたバッファサイズ算出
    float floatDelayTime = (float)delayTimeR_ / kMsPerSec;
    float fNumFrames = floatDelayTime * (float)sampleRate_ / kMsPerSec;
    size_t sampleCount = static_cast<uint32_t>(fNumFrames + 0.5f) * channelCount_;

    uint32_t bytePerSample = format_ / 8;
    assert(bytePerSample <= 4 && bytePerSample);

    uint32_t bytePerFrame = channelCount_ * bytePerSample;

    // get bufCapacity in bytes
    bufCapacityR_ = sampleCount * bytePerSample;
    bufCapacityR_ =
            ((bufCapacityR_ + bytePerFrame - 1) / bytePerFrame) * bytePerFrame;

    bufferR_ = new uint8_t[bufCapacityR_];
    assert(bufferR_);

    memset(bufferR_, 0, bufCapacityR_);
    curPosR_ = 0;

    // bufSize_ is in Frames ( not samples, not bytes )
    bufSizeR_ = bufCapacityR_ / bytePerFrame;
}


size_t AudioDelay::getDelayTime(void) const { return delayTimeL_; }

/**
 * setDecayWeight(): set the decay factor
 * ratio: value of 0.0 -- 1.0f;
 *
 * the calculation is in integer ( not in float )
 * for performance purpose
 */

void AudioDelay::process(int16_t* liveAudio, int32_t numFrames) {                                   //liveoudio18,numframe 192       L146から
  // return;                                                                                        // ここでreturnすると、遅延バッファの処理をしない。

  if (feedbackFactor_ == 0 || bufSizeL_  < numFrames || bufSizeR_  < numFrames) {                                         //0msで何もしない
    return;
  }

  if (!lock_.try_lock()) {
    return;
  }

  if (numFrames + curPosL_ > bufSizeL_) {                                                           //遅延0.1sでバッファサイズ4800sample
    curPosL_ = 0;
  }

  if (numFrames + curPosR_ > bufSizeR_) {                                                           //遅延0.2sでバッファサイズ9600sample
    curPosR_ = 0;
  }

  // process every sample
  int32_t sampleCount = channelCount_ * numFrames;                                                      //samplecount= 2チャンネル分*バッファサイズ192Sample = 384では？
  int16_t* samplesL = &static_cast<int16_t*>(bufferL_)[curPosL_ * channelCount_];
  int16_t* samplesR = &static_cast<int16_t*>(bufferR_)[curPosR_ * channelCount_];                    //右用リングバッファは、右用のカーソル位置　*  チャンネルいくつ数えたか
  for (size_t idx = 0; idx < sampleCount; idx++) {

    // Pure delay
      int16_t tmp = liveAudio[idx];
      if(idx % 2 == 0){                          //左                                                     //バッファ？liveAudioデータ値　から　左リングバッファsamples　へ　移す
          liveAudio[idx] = samplesL[idx];
//          samplesL[idx] = tmp;                                                                           //samples[idx] = tmp;   左チャンネル音声データのみ、バッファから遅延用バッファへ移す　なら、　右チャンネル音声データは　だだ漏れでは？

      }else{                                       //右
          liveAudio[idx] = samplesR[idx];
//          samplesR[idx] = tmp;
      }
      samplesL[idx] = tmp;
      samplesR[idx] = tmp;
  }                                                                                                 //バッファ一杯192sample分のやりとり。合わせてSampleCount384個分。48khzのデータをバッファに192sample分溜めるまでに理論値4msかかる。往復で8ms。
                                                                                                    //
  curPosL_ += numFrames;                                                                            //遅延用バッファL一周4800Sample分遅延させた音声データをバッファ一杯192sample分だけバッファへ渡し、バッファから192sample分だけ受け取る。　約24回後に0.1s遅延
  curPosR_ += numFrames;                                                                            //遅延用バッファL一周9600Sample分遅延させた音声データをバッファ一杯192sample分だけバッファへ渡し、バッファから192sample分だけ受け取る。　約24回後に0.2s遅延
  lock_.unlock();
}
