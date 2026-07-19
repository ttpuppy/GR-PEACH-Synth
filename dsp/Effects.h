// Effects.h
// 検討記録6.1データフロー「RZ/A1H（実行時）: 戻ってきたポリフォニック音声に
// エフェクト適用（10MB SRAMをディレイ/リバーブ等のバッファとして活用）」に対応。
// GR-PEACH単体構成ではボイス生成もRZ/A1H側で行うため、
// ここでは「ミックス後のステレオ信号に対するポストエフェクト」として実装する。
//
// フェーズ4「マルチエフェクト2系統」の簡易版として、ディレイとコーラスの2系統を用意。

#pragma once
#include "SynthConfig.h"
#include "Patch.h"
#include <vector>
#include <cmath>

namespace synth {

// 最大1.5秒分のディレイバッファ（48kHz時 約72000サンプル ≒ 281KB/ch）。
// RZ/A1Hの10MB SRAMなら十分確保可能（検討記録2.1のFPGA内蔵メモリ33.8KBでは不可能な規模）。
constexpr int kMaxDelaySamples = static_cast<int>(kSampleRateHzF * 1.5f);

class StereoDelay {
public:
    void init() {
        bufL_.assign(kMaxDelaySamples, 0.0f);
        bufR_.assign(kMaxDelaySamples, 0.0f);
        writePos_ = 0;
    }

    void setTimeMs(float ms) {
        int samples = static_cast<int>(clampf(ms, 1.0f, 1500.0f) * 0.001f * kSampleRateHzF);
        delaySamples_ = samples;
    }
    void setFeedback(float fb) { feedback_ = clampf(fb, 0.0f, 0.95f); }
    void setMix(float mix) { mix_ = clampf(mix, 0.0f, 1.0f); }

    void process(float inL, float inR, float& outL, float& outR) {
        int readPos = writePos_ - delaySamples_;
        if (readPos < 0) readPos += kMaxDelaySamples;

        float dl = bufL_[readPos];
        float dr = bufR_[readPos];

        bufL_[writePos_] = inL + dl * feedback_;
        bufR_[writePos_] = inR + dr * feedback_;

        outL = inL * (1.0f - mix_) + dl * mix_;
        outR = inR * (1.0f - mix_) + dr * mix_;

        writePos_++;
        if (writePos_ >= kMaxDelaySamples) writePos_ = 0;
    }

private:
    std::vector<float> bufL_, bufR_;
    int writePos_ = 0;
    int delaySamples_ = static_cast<int>(0.28f * kSampleRateHzF);
    float feedback_ = 0.3f;
    float mix_ = 0.25f;
};

// 短いモジュレーテッド・ディレイによるコーラス
class Chorus {
public:
    void init() {
        constexpr int kChorusBufSize = static_cast<int>(kSampleRateHzF * 0.05f); // 50ms
        buf_.assign(kChorusBufSize, 0.0f);
        bufSize_ = kChorusBufSize;
    }

    void setRateHz(float hz) { lfoPhaseInc_ = hz / kSampleRateHzF; }
    void setDepth(float d) { depth_ = clampf(d, 0.0f, 1.0f); }
    void setMix(float mix) { mix_ = clampf(mix, 0.0f, 1.0f); }

    float process(float in) {
        buf_[writePos_] = in;

        float lfo = std::sin(kTwoPi * lfoPhase_);
        lfoPhase_ += lfoPhaseInc_;
        lfoPhase_ -= std::floor(lfoPhase_);

        float centerDelay = bufSize_ * 0.5f;
        float modDepth = bufSize_ * 0.4f * depth_;
        float readPosF = static_cast<float>(writePos_) - (centerDelay + lfo * modDepth);
        while (readPosF < 0.0f) readPosF += static_cast<float>(bufSize_);

        int i0 = static_cast<int>(readPosF);
        int i1 = (i0 + 1) % bufSize_;
        float frac = readPosF - static_cast<float>(i0);
        float wet = buf_[i0] * (1.0f - frac) + buf_[i1] * frac;

        writePos_ = (writePos_ + 1) % bufSize_;
        return in * (1.0f - mix_) + wet * mix_;
    }

private:
    std::vector<float> buf_;
    int bufSize_ = 1;
    int writePos_ = 0;
    float lfoPhase_ = 0.0f;
    float lfoPhaseInc_ = 0.6f / kSampleRateHzF;
    float depth_ = 0.3f;
    float mix_ = 0.25f;
};

class EffectsChain {
public:
    void init() { delay_.init(); chorusL_.init(); chorusR_.init(); }

    void applyPatch(const Patch& p) {
        delay_.setTimeMs(p.delayTimeMs);
        delay_.setFeedback(p.delayFeedback);
        delay_.setMix(p.delayMix);
        chorusL_.setRateHz(p.chorusRateHz);
        chorusL_.setDepth(p.chorusDepth);
        chorusL_.setMix(p.chorusMix);
        chorusR_.setRateHz(p.chorusRateHz * 1.07f); // L/Rでわずかにレートをずらして広がりを出す
        chorusR_.setDepth(p.chorusDepth);
        chorusR_.setMix(p.chorusMix);
    }

    void process(float inL, float inR, float& outL, float& outR) {
        float cL = chorusL_.process(inL);
        float cR = chorusR_.process(inR);
        delay_.process(cL, cR, outL, outR);
    }

private:
    StereoDelay delay_;
    Chorus chorusL_, chorusR_;
};

} // namespace synth
