// EnvelopeLfo.h
// EG(ADSR) x2（アンプ用・フィルター用）と LFO を提供。

#pragma once
#include "SynthConfig.h"

namespace synth {

enum class EgStage : uint8_t { Idle, Attack, Decay, Sustain, Release };

class Envelope {
public:
    void setSampleRate(float sr) { sampleRateHz_ = sr; }

    void setAttackMs(float ms)  { attackMs_ = ms;  updateRates(); }
    void setDecayMs(float ms)   { decayMs_ = ms;   updateRates(); }
    void setSustain(float lvl)  { sustain_ = clampf(lvl, 0.0f, 1.0f); }
    void setReleaseMs(float ms) { releaseMs_ = ms; updateRates(); }

    void noteOn() {
        stage_ = EgStage::Attack;
    }
    void noteOff() {
        if (stage_ != EgStage::Idle) stage_ = EgStage::Release;
    }

    bool isActive() const { return stage_ != EgStage::Idle; }

    // 【今回追加】steps: 一度に進めるサンプル数相当。コントロールレートで
    // （例えば16サンプルに1回)呼ぶ場合、steps=16を渡すことでアタック/
    // ディケイ/リリースの実時間の長さが変わらないようにする
    // (このパラメータを付けずに単純に呼び出し頻度だけ減らすと、
    // 設定したms値よりも実際には何倍も長くかかってしまうバグになる)。
    float process(int steps = 1) {
        switch (stage_) {
            case EgStage::Idle:
                value_ = 0.0f;
                break;
            case EgStage::Attack:
                value_ += attackInc_ * steps;
                if (value_ >= 1.0f) { value_ = 1.0f; stage_ = EgStage::Decay; }
                break;
            case EgStage::Decay:
                value_ -= decayInc_ * steps;
                if (value_ <= sustain_) { value_ = sustain_; stage_ = EgStage::Sustain; }
                break;
            case EgStage::Sustain:
                value_ = sustain_;
                break;
            case EgStage::Release:
                value_ -= releaseInc_ * steps;
                if (value_ <= 0.0f) { value_ = 0.0f; stage_ = EgStage::Idle; }
                break;
        }
        return value_;
    }

private:
    void updateRates() {
        attackInc_  = 1.0f / std::max(1.0f, attackMs_  * 0.001f * sampleRateHz_);
        decayInc_   = 1.0f / std::max(1.0f, decayMs_   * 0.001f * sampleRateHz_);
        releaseInc_ = 1.0f / std::max(1.0f, releaseMs_ * 0.001f * sampleRateHz_);
    }

    float sampleRateHz_ = kSampleRateHzF;
    float attackMs_ = 5.0f, decayMs_ = 100.0f, sustain_ = 0.7f, releaseMs_ = 200.0f;
    float attackInc_ = 0.01f, decayInc_ = 0.001f, releaseInc_ = 0.001f;
    float value_ = 0.0f;
    EgStage stage_ = EgStage::Idle;
};

enum class LfoShape : uint8_t { Sine, Triangle, Square, SampleHold };

class Lfo {
public:
    void setSampleRate(float sr) { sampleRateHz_ = sr; }
    void setRateHz(float hz) { phaseInc_ = hz / sampleRateHz_; }
    void setShape(LfoShape s) { shape_ = s; }

    // 【今回追加】steps: Envelope::process()と同じ理由でコントロールレート
    // 対応。LFOの実際の周波数がsteps倍遅くならないよう補正する。
    float process(int steps = 1) {
        float out = 0.0f;
        float advance = phaseInc_ * static_cast<float>(steps);
        switch (shape_) {
            case LfoShape::Sine:
                out = std::sin(kTwoPi * phase_);
                break;
            case LfoShape::Triangle:
                out = 4.0f * std::fabs(phase_ - std::floor(phase_ + 0.5f)) - 1.0f;
                break;
            case LfoShape::Square:
                out = phase_ < 0.5f ? 1.0f : -1.0f;
                break;
            case LfoShape::SampleHold:
                if (phase_ + advance >= 1.0f) {
                    heldValue_ = (static_cast<float>(rngState_ = rngState_ * 1103515245u + 12345u) /
                                  static_cast<float>(0xFFFFFFFFu)) * 2.0f - 1.0f;
                }
                out = heldValue_;
                break;
        }
        phase_ += advance;
        phase_ -= std::floor(phase_);
        return out;
    }

private:
    float sampleRateHz_ = kSampleRateHzF;
    float phase_ = 0.0f;
    float phaseInc_ = 2.0f / kSampleRateHzF;
    float heldValue_ = 0.0f;
    uint32_t rngState_ = 22222;
    LfoShape shape_ = LfoShape::Triangle;
};

} // namespace synth
