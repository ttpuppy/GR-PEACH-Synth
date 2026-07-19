// Patch.h
// フェーズ3「パッチ管理」〜フェーズ4で使う音色パラメータ一式。
// 検討記録6.3「レジスタマップの早期設計」に対応：
// 各パラメータのレンジをここで一元管理する。
// 将来FPGA実装に移行する際は、このstructのフィールドがそのまま
// MCU->FPGAレジスタマップの元になる想定。
//
// 【今回の変更（SoundDefaults.h整合）】
// 前回の変更でPatch構造体のデフォルト値をリテラル直書きにしたため、
// dsp/SoundDefaults.h の音色define群が未参照（死にコード）になっていた。
// 「音生成専任構成では音作りをSoundDefaults.hのdefine編集で行う」という
// README/main.cppの運用に合わせ、デフォルトメンバ初期化子を再び
// SYNTH_* define参照に戻した。エフェクトのデフォルトOFF
// （delayMix/chorusMix = 0.0）はSoundDefaults.h側の値として反映済み。
// 検証用 "RAW SAW" プリセット（Preset 3）と、SYNC LEAD / XMOD BASSでの
// エフェクト明示ONは維持している。

#pragma once
#include "SynthConfig.h"
#include "SoundDefaults.h"
#include "Oscillator.h"
#include "Filter.h"
#include "EnvelopeLfo.h"
#include "ModMatrix.h"
#include <cstring>

namespace synth {

struct Patch {
    char name[16] = "INIT";

    // OSC1 / OSC2
    WaveShape osc1Shape = SYNTH_OSC1_SHAPE;
    WaveShape osc2Shape = SYNTH_OSC2_SHAPE;
    float osc1PulseWidth = SYNTH_OSC1_PULSE_WIDTH;
    float osc2PulseWidth = SYNTH_OSC2_PULSE_WIDTH;
    float osc2DetuneSemi = SYNTH_OSC2_DETUNE_SEMI; // OSC2の半音単位デチューン(-12..+12)
    float osc2FineCents = SYNTH_OSC2_FINE_CENTS;   // 微調整(セント)
    bool  oscSyncEnabled = SYNTH_OSC_SYNC_ENABLED; // OSC2->OSC1 ハードシンク
    float crossModAmount = SYNTH_CROSS_MOD_AMOUNT; // OSC間クロスモジュレーション量
    float oscMix = SYNTH_OSC_MIX;                  // 0=OSC1のみ, 1=OSC2のみ

    // フィルター
    FilterModel filterModel = SYNTH_FILTER_MODEL;
    SvfMode svfMode = SYNTH_FILTER_SVF_MODE;
    float filterCutoffHz = SYNTH_FILTER_CUTOFF_HZ;
    float filterResonance = SYNTH_FILTER_RESONANCE;
    float filterEgAmount = SYNTH_FILTER_EG_AMOUNT; // フィルターEGのカットオフへの掛かり量(-1..1)
    float filterKeyTrack = SYNTH_FILTER_KEY_TRACK; // キーボードトラッキング(0..1)

    // EG (Filter用, Amp用)
    float filterEgAttackMs = SYNTH_FILTER_EG_ATTACK_MS;
    float filterEgDecayMs = SYNTH_FILTER_EG_DECAY_MS;
    float filterEgSustain = SYNTH_FILTER_EG_SUSTAIN;
    float filterEgReleaseMs = SYNTH_FILTER_EG_RELEASE_MS;
    float ampEgAttackMs = SYNTH_AMP_EG_ATTACK_MS;
    float ampEgDecayMs = SYNTH_AMP_EG_DECAY_MS;
    float ampEgSustain = SYNTH_AMP_EG_SUSTAIN;
    float ampEgReleaseMs = SYNTH_AMP_EG_RELEASE_MS;

    // LFO
    float lfo1RateHz = SYNTH_LFO1_RATE_HZ;
    LfoShape lfo1Shape = SYNTH_LFO1_SHAPE;
    float lfo2RateHz = SYNTH_LFO2_RATE_HZ;
    LfoShape lfo2Shape = SYNTH_LFO2_SHAPE;

    // モジュレーションマトリクス（簡易版）
    ModSlot modSlots[kNumModSlots];

    // ポルタメント（フェーズ1由来）
    float portamentoMs = SYNTH_PORTAMENTO_MS;

    // エフェクト（フェーズ4）
    // mix系のデフォルトは0（OFF）。TIME/FEEDBACK/RATE/DEPTHは
    // 「ONにしたときの初期値」（値の根拠と変更経緯はSoundDefaults.h参照）。
    float delayTimeMs = SYNTH_DELAY_TIME_MS;
    float delayFeedback = SYNTH_DELAY_FEEDBACK;
    float delayMix = SYNTH_DELAY_MIX;
    float chorusRateHz = SYNTH_CHORUS_RATE_HZ;
    float chorusDepth = SYNTH_CHORUS_DEPTH;
    float chorusMix = SYNTH_CHORUS_MIX;

    // マスター
    float masterVolume = SYNTH_MASTER_VOLUME;
};

// 起動時にRAM上に保持する簡易プリセットバンク。
// フェーズ3で言及した「patch management」の最小実装。
// SDカード等への永続化は将来拡張（要検討・未実装）。
class PatchBank {
public:
    static constexpr int kNumPresets = 16;

    Patch& at(int index) { return presets_[index % kNumPresets]; }
    const Patch& at(int index) const { return presets_[index % kNumPresets]; }

    void initDefaults() {
        // Preset 0: monologueクラス基本Saw音色
        // SoundDefaults.hのdefine値そのまま（＝音生成専任構成の「基準音色」。
        // 音作りはSoundDefaults.hを編集して再ビルドする）。
        presets_[0] = Patch{};
        std::strncpy(presets_[0].name, "INIT SAW", sizeof(presets_[0].name) - 1);

        // Preset 1: シンクリード
        // プリセット固有のキャラクター付けはここでリテラル上書きする
        // （SoundDefaults.hは「デフォルト」の一元管理であり、プリセット
        // 個別の音色差分まではdefine化しない方針）。
        // エフェクトはこのプリセットでは明示的にON（旧デフォルト値と同じ量）。
        presets_[1] = Patch{};
        presets_[1].oscSyncEnabled = true;
        presets_[1].osc2DetuneSemi = 7.0f;
        presets_[1].filterCutoffHz = 2500.0f;
        presets_[1].filterResonance = 0.5f;
        presets_[1].delayMix = 0.25f;
        presets_[1].chorusMix = 0.25f;
        std::strncpy(presets_[1].name, "SYNC LEAD", sizeof(presets_[1].name) - 1);

        // Preset 2: クロスモッドベース（同上、エフェクト明示ON）
        presets_[2] = Patch{};
        presets_[2].crossModAmount = 0.4f;
        presets_[2].filterModel = FilterModel::StateVariable;
        presets_[2].svfMode = SvfMode::LowPass;
        presets_[2].filterCutoffHz = 800.0f;
        presets_[2].delayMix = 0.25f;
        presets_[2].chorusMix = 0.25f;
        std::strncpy(presets_[2].name, "XMOD BASS", sizeof(presets_[2].name) - 1);

        // Preset 3: 検証用 RAW SAW
        // スコープでオシレーター波形そのものを確認するためのパッチ。
        // - OSC1のみ（oscMix=0）: 2オシレーター加算による段差を排除
        // - フィルター全開・EGアマウント0・レゾナンス0・キートラック0:
        //   LPFによるエッジ鈍り/リンギングを排除
        // - エフェクトなし（デフォルトで既にOFF）
        // これでスコープにほぼ理想的なsaw（polyBLEPでエッジがわずかに
        // 丸まる程度）が出ればオシレーターは健全と判断できる。
        presets_[3] = Patch{};
        presets_[3].oscMix = 0.0f;
        presets_[3].filterCutoffHz = 18000.0f;
        presets_[3].filterEgAmount = 0.0f;
        presets_[3].filterResonance = 0.0f;
        presets_[3].filterKeyTrack = 0.0f;
        std::strncpy(presets_[3].name, "RAW SAW", sizeof(presets_[3].name) - 1);

        for (int i = 4; i < kNumPresets; ++i) {
            presets_[i] = Patch{};
        }
    }

private:
    Patch presets_[kNumPresets];
};

} // namespace synth
