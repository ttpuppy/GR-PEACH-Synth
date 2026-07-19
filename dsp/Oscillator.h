// Oscillator.h
// フェーズ1〜4で使うVCOモデル。
// - 波形: saw / square(pulse width可変) / triangle / sine
// - PolyBLEPによるエイリアシング低減（帯域制限）
// - ハードシンク（他オシレーターの位相リセットを受ける）
// - クロスモジュレーション（他オシレーターの出力でFM/位相変調）
//   -> multi/polyのSync/Cross-modに相当する、フェーズ4の主要追加要素

#pragma once
#include "SynthConfig.h"

namespace synth {

enum class WaveShape : uint8_t {
    Saw = 0,
    Square,
    Triangle,
    Sine
};

class Oscillator {
public:
    void setSampleRate(float sr) { sampleRateHz_ = sr; }

    void setFrequency(float hz) {
        freqHz_ = hz;
        phaseInc_ = freqHz_ / sampleRateHz_;
    }

    void setShape(WaveShape s) { shape_ = s; }
    void setPulseWidth(float pw) { pulseWidth_ = clampf(pw, 0.02f, 0.98f); }

    // クロスモジュレーション量: 0=なし, 大きいほど強いFM/位相変調
    void setCrossModAmount(float amt) { crossModAmount_ = amt; }

    // ハードシンク: 呼び出されるたびに位相を0にリセットする
    void hardSync() { phase_ = 0.0f; }

    // 出力は -1..+1。modInput は他オシレーターの出力（クロスモジュレーション用、-1..1想定）。
    // syncSource が true の周期でリセットするのは呼び出し側（Voice）が hardSync() を呼ぶ設計。
    float process(float modInput = 0.0f) {
        float modPhase = phase_ + crossModAmount_ * modInput * 0.25f;
        modPhase -= std::floor(modPhase);

        float out = 0.0f;
        switch (shape_) {
            case WaveShape::Saw:
                out = 2.0f * modPhase - 1.0f;
                out -= polyBlep(modPhase, phaseInc_);
                break;
            case WaveShape::Square: {
                out = modPhase < pulseWidth_ ? 1.0f : -1.0f;
                out += polyBlep(modPhase, phaseInc_);
                float shiftPhase = modPhase + (1.0f - pulseWidth_);
                shiftPhase -= std::floor(shiftPhase);
                out -= polyBlep(shiftPhase, phaseInc_);
                break;
            }
            case WaveShape::Triangle: {
                // 積分した矩形波として近似（軽量・十分帯域制限された三角波）
                float sq = modPhase < 0.5f ? 1.0f : -1.0f;
                sq += polyBlep(modPhase, phaseInc_);
                float shiftPhase = modPhase + 0.5f;
                shiftPhase -= std::floor(shiftPhase);
                sq -= polyBlep(shiftPhase, phaseInc_);
                // リーキー積分器
                triIntegrator_ = triIntegrator_ * 0.9995f + sq * phaseInc_ * 4.0f;
                out = triIntegrator_;
                break;
            }
            case WaveShape::Sine:
                out = std::sin(kTwoPi * modPhase);
                break;
        }

        phase_ += phaseInc_;
        phase_ -= std::floor(phase_);
        return out;
    }

    float phase() const { return phase_; }

private:
    static float polyBlep(float t, float dt) {
        if (t < dt) {
            float x = t / dt;
            return x + x - x * x - 1.0f;
        } else if (t > 1.0f - dt) {
            float x = (t - 1.0f) / dt;
            return x * x + x + x + 1.0f;
        }
        return 0.0f;
    }

    float sampleRateHz_ = kSampleRateHzF;
    float freqHz_ = 440.0f;
    float phase_ = 0.0f;
    float phaseInc_ = 440.0f / kSampleRateHzF;
    float pulseWidth_ = 0.5f;
    float crossModAmount_ = 0.0f;
    float triIntegrator_ = 0.0f;
    WaveShape shape_ = WaveShape::Saw;
};

} // namespace synth
