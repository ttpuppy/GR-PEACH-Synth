// Filter.h
// フェーズ4「フィルターモデル複数化」に対応する複数VCFモデル。
// - LadderFilter: Moog型4ポール自己発振対応ラダーフィルター（非線形tanhサチュレーション付き）
// - StateVariableFilter: マルチモード(LP/BP/HP/Notch)、2ポール、モデル切替が軽量
// どちらもゼロ遅延フィードバック(ZDF)を簡略化したTPT(Topology-Preserving Transform)構造で、
// カットオフ・レゾナンスの急変にも比較的追従性が良い。

#pragma once
#include "SynthConfig.h"

namespace synth {

enum class FilterModel : uint8_t {
    Ladder = 0,      // Moog系4ポールLP
    StateVariable    // マルチモード2ポール
};

enum class SvfMode : uint8_t { LowPass, BandPass, HighPass, Notch };

class LadderFilter {
public:
    void setSampleRate(float sr) { sampleRateHz_ = sr; }

    void setCutoffHz(float hz) {
        cutoffHz_ = clampf(hz, 20.0f, sampleRateHz_ * 0.45f);
        updateCoeffs();
    }
    void setResonance(float res01) {
        // 0..1 -> 自己発振寸前(約4.0)までのフィードバック量
        resonance_ = clampf(res01, 0.0f, 1.0f) * 4.2f;
    }

    float process(float in) {
        float g = g_;
        float fb = resonance_ * (stage_[3] - drive_ * in);
        float x = std::tanh(in - fb);

        stage_[0] += g * (x - stage_[0]);
        float s0 = std::tanh(stage_[0]);
        stage_[1] += g * (s0 - stage_[1]);
        float s1 = std::tanh(stage_[1]);
        stage_[2] += g * (s1 - stage_[2]);
        float s2 = std::tanh(stage_[2]);
        stage_[3] += g * (s2 - stage_[3]);

        return stage_[3];
    }

    void reset() { stage_[0] = stage_[1] = stage_[2] = stage_[3] = 0.0f; }

private:
    void updateCoeffs() {
        float wc = kTwoPi * cutoffHz_ / sampleRateHz_;
        g_ = clampf(wc * 0.6f, 0.0f, 0.85f); // 簡易プリウォーピング近似
    }

    float sampleRateHz_ = kSampleRateHzF;
    float cutoffHz_ = 1000.0f;
    float resonance_ = 0.0f;
    float drive_ = 0.9f;
    float g_ = 0.1f;
    float stage_[4] = {0, 0, 0, 0};
};

class StateVariableFilter {
public:
    void setSampleRate(float sr) { sampleRateHz_ = sr; }
    void setMode(SvfMode m) { mode_ = m; }

    void setCutoffHz(float hz) {
        cutoffHz_ = clampf(hz, 20.0f, sampleRateHz_ * 0.45f);
        updateCoeffs();
    }
    void setResonance(float res01) {
        // Q: 0.5(緩い)〜~12(鋭い/自己発振寸前)
        q_ = 0.5f + clampf(res01, 0.0f, 1.0f) * 11.5f;
        updateCoeffs();
    }

    float process(float in) {
        // TPT (Topology Preserving Transform) SVF, Andrew Simper方式の簡略版
        float hp = (in - (2.0f * damping_ + g_) * band_ - low_) * a1_;
        float bp = g_ * hp + band_;
        float lp = g_ * bp + low_;
        low_ = lp;
        band_ = bp;

        switch (mode_) {
            case SvfMode::LowPass:  return lp;
            case SvfMode::BandPass: return bp;
            case SvfMode::HighPass: return hp;
            case SvfMode::Notch:    return lp + hp;
        }
        return lp;
    }

    void reset() { low_ = band_ = 0.0f; }

private:
    void updateCoeffs() {
        g_ = std::tan(kPi * cutoffHz_ / sampleRateHz_);
        damping_ = 1.0f / (2.0f * q_);
        a1_ = 1.0f / (1.0f + 2.0f * damping_ * g_ + g_ * g_);
    }

    float sampleRateHz_ = kSampleRateHzF;
    float cutoffHz_ = 1000.0f;
    float q_ = 0.7f;
    float g_ = 0.1f;
    float damping_ = 0.7f;
    float a1_ = 1.0f;
    float low_ = 0.0f;
    float band_ = 0.0f;
    SvfMode mode_ = SvfMode::LowPass;
};

// Voice側から model を切り替えて使うための薄いラッパー
class MultiModelFilter {
public:
    void setSampleRate(float sr) {
        ladder_.setSampleRate(sr);
        svf_.setSampleRate(sr);
    }
    void setModel(FilterModel m) { model_ = m; }
    void setSvfMode(SvfMode m) { svf_.setMode(m); }

    // 【今回修正】以前はラダー/SVF両方のsetCutoffHz/setResonanceを常に
    // 呼んでいたため、使っていない方のモデルの係数再計算（SVF側は
    // std::tan()を含む）が毎サンプル×ボイス数だけ無駄に走っていた。
    // これは「単音はきれいだが2音以上でノイズが乗る」という実機報告の
    // 一因（CPU処理時間がボイス数に比例して増え、リアルタイム再生の
    // 締め切りに間に合わなくなる）と考えられるため、現在選択中の
    // モデルだけを更新するように変更した。
    void setCutoffHz(float hz) {
        if (model_ == FilterModel::Ladder) {
            ladder_.setCutoffHz(hz);
        } else {
            svf_.setCutoffHz(hz);
        }
    }
    void setResonance(float r) {
        if (model_ == FilterModel::Ladder) {
            ladder_.setResonance(r);
        } else {
            svf_.setResonance(r);
        }
    }

    float process(float in) {
        return model_ == FilterModel::Ladder ? ladder_.process(in) : svf_.process(in);
    }

    void reset() { ladder_.reset(); svf_.reset(); }

private:
    FilterModel model_ = FilterModel::Ladder;
    LadderFilter ladder_;
    StateVariableFilter svf_;
};

} // namespace synth
