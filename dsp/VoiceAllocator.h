// VoiceAllocator.h
// フェーズ2「ポリフォニー化」〜フェーズ4のボイス管理。
// kNumVoices (4〜8、mbed_app.jsonのnum-voicesで設定) 分のVoiceを保持し、
// MIDIノートオン/オフをボイスに割り当てる。あふれた場合は最も古いノートを奪う
// （オルガン式ではなく、シンプルなFIFOスティール）。

#pragma once
#include "SynthConfig.h"
#include "Voice.h"
#include <array>

namespace synth {

class VoiceAllocator {
public:
    void setSampleRate(float sr) {
        for (auto& v : voices_) v.setSampleRate(sr);
    }

    void applyPatch(const Patch& p) {
        for (auto& v : voices_) v.applyPatch(p);
    }

    void noteOn(int note, float velocity01) {
        // 同じノートが既に鳴っていれば再トリガー
        for (int i = 0; i < kNumVoices; ++i) {
            if (voices_[i].isActive() && voices_[i].currentNote() == note) {
                voices_[i].noteOn(note, velocity01);
                bumpAge(i);
                return;
            }
        }
        // 空いているボイスを探す
        for (int i = 0; i < kNumVoices; ++i) {
            if (!voices_[i].isActive()) {
                voices_[i].noteOn(note, velocity01);
                bumpAge(i);
                return;
            }
        }
        // 全ボイス使用中: 最も古いボイスを奪う
        int oldest = 0;
        uint32_t oldestAge = age_[0];
        for (int i = 1; i < kNumVoices; ++i) {
            if (age_[i] < oldestAge) { oldestAge = age_[i]; oldest = i; }
        }
        voices_[oldest].noteOn(note, velocity01);
        bumpAge(oldest);
    }

    void noteOff(int note) {
        for (auto& v : voices_) {
            if (v.gateOn() && v.currentNote() == note) v.noteOff();
        }
    }

    void allNotesOff() {
        for (auto& v : voices_) v.noteOff();
    }

    // 全ボイスをミックスして1サンプル返す（呼び出し側でマスターゲイン/エフェクトを適用）
    //
    // 【今回修正・重大なバグ】
    // 従来は kNumVoices（固定の総ボイス数、例:8）で常に割っていたため、
    // 実際に鳴っているボイス数とスケーリングが噛み合っていなかった。
    //
    // 【訂正】当初 1/sqrt(activeCount) の「イコールパワー」スケーリングに
    // 変更しようとしたが、これは複数ボイスの位相がランダムな場合の
    // 平均的な音量感（RMS）を揃えるための手法であり、瞬間的なピーク値の
    // クリップ防止には効かないことに気づいた（sqrt(N)スケーリングでも、
    // 全ボイスの位相が瞬間的に揃った最悪ケースでは必要なヘッドルームが
    // sqrt(N)倍のまま残ってしまい、Nが増えるほどクリップしやすくなる）。
    //
    // そのため、ピーク値を確実に抑えられる「単純な線形平均」
    // (sum / activeCount) に変更した。各ボイスの出力が±1.0の範囲に
    // 収まっている前提で、この方式なら合算後も±1.0を超えない
    // （superposition的な最悪ケースでも、割った時点でちょうど
    // 1ボイス分の振幅に戻るため）。単音の音量は以前(1/sqrt(8))より
    // 大きくなる（少し小さいという報告の解消）。和音を弾いた時の
    // 音圧が単純加算ほど盛り上がらなくなる代わりに、クリップに
    // よる歪みノイズは大幅に減るはず。
    //
    // なお、フィルターの自己発振等で個々のボイス出力が±1.0を超える
    // 瞬間があっても、SynthEngine::processSample()側のソフトクリップ
    // (tanh)が最終的な安全網として働く。
    float processMix(float modWheel01) {
        float sum = 0.0f;
        int activeCount = 0;
        for (auto& v : voices_) {
            sum += v.process(modWheel01);
            if (v.isActive()) ++activeCount;
        }
        if (activeCount < 1) activeCount = 1;
        return sum / static_cast<float>(activeCount);
    }

private:
    void bumpAge(int idx) { age_[idx] = ++ageCounter_; }

    std::array<Voice, kNumVoices> voices_;
    std::array<uint32_t, kNumVoices> age_{};
    uint32_t ageCounter_ = 0;
};

} // namespace synth
