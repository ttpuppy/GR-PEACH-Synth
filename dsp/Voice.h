// Voice.h
// 検討記録6.2「役割分担まとめ」における「RZ/A1H（開発時）: VCO/VCF/EG/LFOアルゴリズムの
// 固定小数点ソフトウェア・ゴールデンモデル」に相当する部分。
// GR-PEACH単体構成(4.2)では、このVoiceがそのまま実行時エンジンとしても使われる
// （FPGA移植時にはHDL側に置き換わり、RZ/A1H側はレジスタ書き込みに専念する設計）。

#pragma once
#include "SynthConfig.h"
#include "Oscillator.h"
#include "Filter.h"
#include "EnvelopeLfo.h"
#include "Patch.h"
#include <array>

namespace synth {

class Voice {
public:
    void setSampleRate(float sr) {
        sampleRateHz_ = sr;
        osc1_.setSampleRate(sr);
        osc2_.setSampleRate(sr);
        filter_.setSampleRate(sr);
        filterEg_.setSampleRate(sr);
        ampEg_.setSampleRate(sr);
        lfo1_.setSampleRate(sr);
        lfo2_.setSampleRate(sr);
        pitchGlide_.setTimeConstantMs(1.0f);
    }

    void applyPatch(const Patch& p) {
        patch_ = &p;
        osc1_.setShape(p.osc1Shape);
        osc2_.setShape(p.osc2Shape);
        osc1_.setPulseWidth(p.osc1PulseWidth);
        osc2_.setPulseWidth(p.osc2PulseWidth);
        osc2_.setCrossModAmount(p.crossModAmount);

        filter_.setModel(p.filterModel);
        filter_.setSvfMode(p.svfMode);
        filter_.setResonance(p.filterResonance);

        filterEg_.setAttackMs(p.filterEgAttackMs);
        filterEg_.setDecayMs(p.filterEgDecayMs);
        filterEg_.setSustain(p.filterEgSustain);
        filterEg_.setReleaseMs(p.filterEgReleaseMs);

        ampEg_.setAttackMs(p.ampEgAttackMs);
        ampEg_.setDecayMs(p.ampEgDecayMs);
        ampEg_.setSustain(p.ampEgSustain);
        ampEg_.setReleaseMs(p.ampEgReleaseMs);

        lfo1_.setRateHz(p.lfo1RateHz);
        lfo1_.setShape(p.lfo1Shape);
        lfo2_.setRateHz(p.lfo2RateHz);
        lfo2_.setShape(p.lfo2Shape);

        pitchGlide_.setTimeConstantMs(p.portamentoMs);
    }

    void noteOn(int midiNote, float velocity01) {
        note_ = midiNote;
        velocity_ = velocity01;
        float targetHz = midiNoteToHz(static_cast<float>(midiNote));
        if (!active_) {
            pitchGlide_.reset(targetHz);
        }
        pitchGlide_.setTarget(targetHz);
        filterEg_.noteOn();
        ampEg_.noteOn();
        active_ = true;
        gateOn_ = true;
        // 【今回追加】ノートオンの瞬間はコントロールレートの更新タイミングを
        // 待たず即座に1回評価しておく（新しいノート番号でのkeyTrack等を
        // 反映するタイミングが最大kControlRateDiv-1サンプル遅れるのを防ぐ）。
        controlRateCounter_ = 0;
    }

    void noteOff() {
        filterEg_.noteOff();
        ampEg_.noteOff();
        gateOn_ = false;
    }

    bool isActive() const { return active_ && (ampEg_.isActive() || gateOn_); }
    int  currentNote() const { return note_; }
    bool gateOn() const { return gateOn_; }

    // 1サンプル分の処理。modWheel01 はパネル/MIDIから供給される 0..1 の値。
    //
    // 【今回、コントロールレート化＋補間】
    // フィルターのカットオフ/レゾナンス設定・モジュレーションマトリクス
    // 評価・ピッチ計算(fastPow2)・フィルターEG/LFOの評価は、
    // kControlRateDiv(=16)サンプルに1回だけ行う。ただし、その結果を
    // そのまま16サンプル間ステップ状に適用すると、複数ボイスの更新
    // タイミングが互いにズレている（ノートを弾いたタイミングが違う
    // ため）ことで、それぞれの「カクつき」が混ざり合い、ブザーのような
    // 耳障りなノイズになることが実機テストで判明した（単音では目立たず
    // 複数音で顕著、というのがまさにこのパターンの特徴）。
    // そのため、重い計算の結果は「目標値」として保持し、次の
    // updateControlRate()が呼ばれるまでの16サンプルにかけて直線補間
    // (これ自体は非常に軽い演算)で滑らかに変化させるようにした。
    float process(float modWheel01) {
        if (!active_) return 0.0f;

        float baseHz = pitchGlide_.process(); // ポルタメントの滑らかさのため毎サンプル
        ampEgV_ = ampEg_.process(); // アンプへの直結量なので毎サンプル(ジッパーノイズ回避)。
                                     // モジュレーションソースとしても直近値をそのまま使う。

        if (controlRateCounter_ == 0) {
            updateControlRate(modWheel01);
        }
        float blend = static_cast<float>(controlRateCounter_) / static_cast<float>(kControlRateDiv);
        controlRateCounter_ = (controlRateCounter_ + 1) % kControlRateDiv;

        // 目標値へ向けて直線補間（軽い演算なので毎サンプル行っても問題ない）
        float pitchMult1 = prevPitchMult1_ + (targetPitchMult1_ - prevPitchMult1_) * blend;
        float pitchMult2 = prevPitchMult2_ + (targetPitchMult2_ - prevPitchMult2_) * blend;
        float pw1        = prevPw1_        + (targetPw1_        - prevPw1_)        * blend;
        float xmodAmt    = prevXmodAmt_    + (targetXmodAmt_    - prevXmodAmt_)    * blend;
        float cutoffHz   = prevCutoffHz_   + (targetCutoffHz_   - prevCutoffHz_)   * blend;
        float resonance  = prevResonance_  + (targetResonance_  - prevResonance_)  * blend;
        float ampMod     = prevAmpMod_     + (targetAmpMod_     - prevAmpMod_)     * blend;

        // OSC1/OSC2: 周波数は上記で補間した乗数を使う
        osc1_.setFrequency(baseHz * pitchMult1);
        osc1_.setPulseWidth(pw1);

        osc2_.setFrequency(baseHz * pitchMult2);
        osc2_.setCrossModAmount(xmodAmt);

        float osc2Out = osc2_.process(0.0f);

        // ハードシンク: OSC2の位相が1周する瞬間にOSC1をリセット
        if (patch_->oscSyncEnabled) {
            float p2 = osc2_.phase();
            if (p2 < lastOsc2Phase_) {
                osc1_.hardSync();
            }
            lastOsc2Phase_ = p2;
        }

        float osc1Out = osc1_.process(osc2Out); // クロスモジュレーション（OSC2->OSC1）

        float oscMixOut = osc1Out * (1.0f - patch_->oscMix) + osc2Out * patch_->oscMix;

        // フィルター係数は補間済みの値を毎サンプル設定する
        // （Ladderモデルのデフォルト実装では超越関数を含まないため、
        //   毎サンプル呼んでもコストは小さい。詳細はFilter.h参照）。
        filter_.setCutoffHz(clampf(cutoffHz, 20.0f, sampleRateHz_ * 0.45f));
        filter_.setResonance(clampf(resonance, 0.0f, 1.0f));
        float filtered = filter_.process(oscMixOut);

        float amp = clampf(ampEgV_ + ampMod, 0.0f, 1.0f) * velocity_;
        float out = filtered * amp;

        if (ampEgV_ <= 0.0f && !gateOn_) {
            active_ = false;
        }
        return out;
    }

private:
    // kControlRateDivサンプルに1回だけ呼ばれる、重い処理をまとめた部分。
    // 前回の目標値を「補間の開始点」に繰り上げてから、新しい目標値を計算する。
    void updateControlRate(float modWheel01) {
        prevPitchMult1_ = targetPitchMult1_;
        prevPitchMult2_ = targetPitchMult2_;
        prevPw1_        = targetPw1_;
        prevXmodAmt_    = targetXmodAmt_;
        prevCutoffHz_   = targetCutoffHz_;
        prevResonance_  = targetResonance_;
        prevAmpMod_     = targetAmpMod_;

        // LFO/フィルターEGはsteps=kControlRateDivを渡し、実際の周波数/
        // 時間が呼び出し頻度の低下で遅くならないよう補正する
        // (EnvelopeLfo.hのprocess(int steps)参照)。
        float lfo1v = lfo1_.process(kControlRateDiv);
        float lfo2v = lfo2_.process(kControlRateDiv);
        float filterEgV = filterEg_.process(kControlRateDiv);

        std::array<float, static_cast<size_t>(ModSource::Count)> srcVals{};
        srcVals[static_cast<size_t>(ModSource::Lfo1)] = lfo1v;
        srcVals[static_cast<size_t>(ModSource::Lfo2)] = lfo2v;
        srcVals[static_cast<size_t>(ModSource::FilterEg)] = filterEgV;
        srcVals[static_cast<size_t>(ModSource::AmpEg)] = ampEgV_; // 直近(このサンプル)の値をそのまま使う
        srcVals[static_cast<size_t>(ModSource::Velocity)] = velocity_;
        srcVals[static_cast<size_t>(ModSource::ModWheel)] = modWheel01;

        float pitchModSemi1 = 0.0f, pitchModSemi2 = 0.0f, pwMod = 0.0f, cutoffMod = 0.0f, resMod = 0.0f, xmodMod = 0.0f, ampMod = 0.0f;
        for (const auto& slot : patch_->modSlots) {
            if (slot.source == ModSource::None) continue;
            float v = srcVals[static_cast<size_t>(slot.source)] * slot.amount;
            switch (slot.dest) {
                case ModDest::Osc1Pitch: pitchModSemi1 += v * 12.0f; break;
                case ModDest::Osc2Pitch: pitchModSemi2 += v * 12.0f; break;
                case ModDest::Osc1PulseWidth: pwMod += v * 0.4f; break;
                case ModDest::FilterCutoff: cutoffMod += v; break;
                case ModDest::FilterResonance: resMod += v; break;
                case ModDest::Osc2CrossMod: xmodMod += v; break;
                case ModDest::Amp: ampMod += v; break;
                default: break;
            }
        }

        targetPitchMult1_ = fastPow2(pitchModSemi1 / 12.0f);
        float osc2Semi = patch_->osc2DetuneSemi + patch_->osc2FineCents / 100.0f + pitchModSemi2;
        targetPitchMult2_ = fastPow2(osc2Semi / 12.0f);
        targetPw1_ = patch_->osc1PulseWidth + pwMod;
        targetXmodAmt_ = clampf(patch_->crossModAmount + xmodMod, 0.0f, 1.0f);
        targetAmpMod_ = ampMod;

        // フィルター: キートラッキング + フィルターEG + マトリクス由来のcutoff変調
        float keyTrackHz = midiNoteToHz(static_cast<float>(note_)) * patch_->filterKeyTrack;
        targetCutoffHz_ = patch_->filterCutoffHz
                        + keyTrackHz
                        + patch_->filterEgAmount * filterEgV * 6000.0f
                        + cutoffMod * 6000.0f;
        targetResonance_ = clampf(patch_->filterResonance + resMod, 0.0f, 1.0f);
    }

    const Patch* patch_ = nullptr;
    Oscillator osc1_, osc2_;
    MultiModelFilter filter_;
    Envelope filterEg_, ampEg_;
    Lfo lfo1_, lfo2_;
    OnePoleSmoother pitchGlide_;

    float sampleRateHz_ = kSampleRateHzF;
    float lastOsc2Phase_ = 0.0f;
    int note_ = 60;
    float velocity_ = 1.0f;
    bool active_ = false;
    bool gateOn_ = false;

    // コントロールレート更新のカウンタ・アンプEG直近値
    int controlRateCounter_ = 0;
    float ampEgV_ = 0.0f;

    // 各パラメータの「補間前(prev)」「目標値(target)」。
    // 初期値はprev=targetで揃えておき、起動直後に不要な補間ジャンプが
    // 出ないようにしてある。
    float prevPitchMult1_ = 1.0f, targetPitchMult1_ = 1.0f;
    float prevPitchMult2_ = 1.0f, targetPitchMult2_ = 1.0f;
    float prevPw1_ = 0.5f,        targetPw1_ = 0.5f;
    float prevXmodAmt_ = 0.0f,    targetXmodAmt_ = 0.0f;
    float prevCutoffHz_ = 1000.0f, targetCutoffHz_ = 1000.0f;
    float prevResonance_ = 0.0f,  targetResonance_ = 0.0f;
    float prevAmpMod_ = 0.0f,     targetAmpMod_ = 0.0f;
};

} // namespace synth
