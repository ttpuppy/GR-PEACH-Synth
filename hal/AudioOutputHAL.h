// AudioOutputHAL.h
// シンセエンジンとオーディオコーデック実装を分離するためのインターフェース。
// GR-PEACH実機ではcodec/AudioStream_GRPEACH.h が実装を提供する。
// PC上でのアルゴリズム検証（ゴールデンモデル）用にダミー実装を差し替えることも可能。

#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>

namespace synth {

// fill callback: interleaved stereo int16_t バッファに frames サンプル分書き込む
using AudioFillCallback = std::function<void(int16_t* interleavedStereo, size_t frames)>;

class AudioOutputHAL {
public:
    virtual ~AudioOutputHAL() = default;

    // コーデック初期化（I2C設定・I2S/SSIFクロック設定など）
    virtual bool init(uint32_t sampleRateHz) = 0;

    // オーディオコールバックを登録して再生開始
    virtual void start(AudioFillCallback callback) = 0;

    virtual void stop() = 0;
};

} // namespace synth
