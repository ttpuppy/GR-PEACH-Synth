// Patch.h
// フェーズ3「パッチ管理」〜フェーズ4で使う音色パラメータ一式。
// 検討記録6.3「レジスタマップの早期設計」に対応：
// 各パラメータのレンジをここで一元管理する。
// 将来FPGA実装に移行する際は、このstructのフィールドがそのまま
// MCU->FPGAレジスタマップの元になる想定。
//
// 【今回の変更（波形検証セッション）】
// 1. delayMix / chorusMix のstructデフォルトを 0.25 -> 0.0 に変更
//    （エフェクトはデフォルトOFF）。
//    理由：全プリセットに常時コーラスが掛かっており、コーラスの
//    ウェット側は変調ディレイによる実質ピッチシフト
//    （depth0.3/rate0.6Hzで最大約±40セント）になるため、ドライとの
//    干渉でスコープ上に振幅のうねり（ビート）が現れていた。
//    「X-MODでチューンが効いている」ように聞こえた正体はこれ。
//    ディレイ(280ms)も常時ONで、波形に過去のコピーが重畳していた。
//    エフェクトを使いたいプリセットでは個別に delayMix / chorusMix を
//    明示設定する方針に変更（下記 SYNC LEAD / XMOD BASS 参照）。
// 2. Preset 3 に検証用 "RAW SAW" を追加。
//    OSC1のみ・フィルター全開・エフェクトなしの素のsaw。
//    スコープでオシレーター単体の波形品質を確認するためのパッチ。

#pragma once
#include "SynthConfig.h"
#include "Oscillator.h"
#include "Filter.h"
#include "EnvelopeLfo.h"
#include "ModMatrix.h"
#include <cstring>

namespace synth {

struct Patch {
    char name[16] = "INIT";

    // OSC1 / OSC2
    WaveShape osc1Shape = WaveShape::Saw;
    WaveShape osc2Shape = WaveShape::Saw;
    float osc1PulseWidth = 0.5f;
    float osc2PulseWidth = 0.5f;
    float osc2DetuneSemi = 0.0f;   // OSC2の半音単位デチューン(-12..+12)
    float osc2FineCents = 0.0f;    // 微調整(セント)
    bool  oscSyncEnabled = false;  // OSC2->OSC1 ハードシンク
    float crossModAmount = 0.0f;   // OSC間クロスモジュレーション量
    float oscMix = 0.5f;           // 0=OSC1のみ, 1=OSC2のみ

    // フィルター
    FilterModel filterModel = FilterModel::Ladder;
    SvfMode svfMode = SvfMode::LowPass;
    float filterCutoffHz = 4000.0f;
    float filterResonance = 0.2f;
    float filterEgAmount = 0.5f;   // フィルターEGのカットオフへの掛かり量(-1..1)
    float filterKeyTrack = 0.3f;   // キーボードトラッキング(0..1)

    // EG (Filter用, Amp用)
    float filterEgAttackMs = 2.0f, filterEgDecayMs = 200.0f, filterEgSustain = 0.3f, filterEgReleaseMs = 300.0f;
    float ampEgAttackMs = 2.0f,    ampEgDecayMs = 150.0f,    ampEgSustain = 0.8f,    ampEgReleaseMs = 250.0f;

    // LFO
    float lfo1RateHz = 4.0f;
    LfoShape lfo1Shape = LfoShape::Triangle;
    float lfo2RateHz = 0.5f;
    LfoShape lfo2Shape = LfoShape::Sine;

    // モジュレーションマトリクス（簡易版）
    ModSlot modSlots[kNumModSlots];

    // ポルタメント（フェーズ1由来）
    float portamentoMs = 0.0f;

    // エフェクト（フェーズ4）
    // 【変更】mix系のデフォルトは0（OFF）。TIME/FEEDBACK/RATE/DEPTHは
    // 「ONにしたときの初期値」として従来値を残してある。
    float delayTimeMs = 280.0f;
    float delayFeedback = 0.35f;
    float delayMix = 0.0f;         // 旧: 0.25f（常時ON） -> デフォルトOFF
    float chorusRateHz = 0.6f;
    float chorusDepth = 0.3f;
    float chorusMix = 0.0f;        // 旧: 0.25f（常時ON） -> デフォルトOFF

    // マスター
    float masterVolume = 0.8f;
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
        // 【変更】エフェクトはデフォルトOFFになったため、素の
        // 2オシレーターsaw + Ladder LPF の音になる。
        presets_[0] = Patch{};
        std::strncpy(presets_[0].name, "INIT SAW", sizeof(presets_[0].name) - 1);

        // Preset 1: シンクリード
        // 【変更】従来どおりの音の厚みを保つため、ディレイ/コーラスを
        // このプリセットでは明示的にONにする（旧デフォルト値と同じ量）。
        presets_[1] = Patch{};
        presets_[1].oscSyncEnabled = true;
        presets_[1].osc2DetuneSemi = 7.0f;
        presets_[1].filterCutoffHz = 2500.0f;
        presets_[1].filterResonance = 0.5f;
        presets_[1].delayMix = 0.25f;
        presets_[1].chorusMix = 0.25f;
        std::strncpy(presets_[1].name, "SYNC LEAD", sizeof(presets_[1].name) - 1);

        // Preset 2: クロスモッドベース
        // 【変更】同上、エフェクトを明示ON。
        presets_[2] = Patch{};
        presets_[2].crossModAmount = 0.4f;
        presets_[2].filterModel = FilterModel::StateVariable;
        presets_[2].svfMode = SvfMode::LowPass;
        presets_[2].filterCutoffHz = 800.0f;
        presets_[2].delayMix = 0.25f;
        presets_[2].chorusMix = 0.25f;
        std::strncpy(presets_[2].name, "XMOD BASS", sizeof(presets_[2].name) - 1);

        // Preset 3: 【追加】検証用 RAW SAW
        // スコープでオシレーター波形そのものを確認するためのパッチ。
        // - OSC1のみ（oscMix=0）: 2オシレーター加算による段差を排除
        // - フィルター全開・EGアマウント0・レゾナンス0: LPFによる
        //   エッジ鈍り/リンギングを排除
        // - エフェクトなし（structデフォルトで既にOFF）
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
