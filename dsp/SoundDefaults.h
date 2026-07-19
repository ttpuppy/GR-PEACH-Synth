// SoundDefaults.h
// 音色（Patch）のデフォルト値をdefineで一元管理する。
// パネルUI/OLEDを持たない「音生成専任」構成のため、音作りはこのファイルの
// define値を書き換えてビルドし直す運用とする。
// dsp/Patch.h の Patch構造体のデフォルトメンバ初期化子がこれらのdefineを参照する。
//
// 【今回の変更（波形検証セッション）】
// SYNTH_DELAY_MIX / SYNTH_CHORUS_MIX を 0.25 -> 0.0 に変更（エフェクトは
// デフォルトOFF）。
// 理由：全プリセットに常時コーラスが掛かっており、コーラスのウェット側は
// 変調ディレイによる実質ピッチシフト（depth0.3/rate0.6Hzで最大約±40セント）
// になるため、ドライとの干渉でスコープ上に振幅のうねり（ビート）が出ていた。
// ディレイ(280ms)も常時ONで、波形に過去のコピーが重畳していた。
// TIME/FEEDBACK/RATE/DEPTH系のdefineは「ONにしたときの初期値」として従来値の
// まま残す。エフェクトを使うプリセットは PatchBank::initDefaults() 内で
// 個別に delayMix / chorusMix を設定する（dsp/Patch.h 参照）。

#pragma once

// ---- OSC1 / OSC2 ----
#define SYNTH_OSC1_SHAPE        WaveShape::Saw
#define SYNTH_OSC2_SHAPE        WaveShape::Saw
#define SYNTH_OSC1_PULSE_WIDTH  0.5f
#define SYNTH_OSC2_PULSE_WIDTH  0.5f
#define SYNTH_OSC2_DETUNE_SEMI  0.0f
#define SYNTH_OSC2_FINE_CENTS   0.0f
#define SYNTH_OSC_SYNC_ENABLED  false
#define SYNTH_CROSS_MOD_AMOUNT  0.0f
#define SYNTH_OSC_MIX           0.5f

// ---- フィルター ----
#define SYNTH_FILTER_MODEL          FilterModel::Ladder
#define SYNTH_FILTER_SVF_MODE       SvfMode::LowPass
#define SYNTH_FILTER_CUTOFF_HZ      4000.0f
#define SYNTH_FILTER_RESONANCE      0.2f
#define SYNTH_FILTER_EG_AMOUNT      0.5f
#define SYNTH_FILTER_KEY_TRACK      0.3f

// ---- エンベロープ（フィルター用） ----
#define SYNTH_FILTER_EG_ATTACK_MS   2.0f
#define SYNTH_FILTER_EG_DECAY_MS    200.0f
#define SYNTH_FILTER_EG_SUSTAIN     0.3f
#define SYNTH_FILTER_EG_RELEASE_MS  300.0f

// ---- エンベロープ（アンプ用） ----
#define SYNTH_AMP_EG_ATTACK_MS      2.0f
#define SYNTH_AMP_EG_DECAY_MS       150.0f
#define SYNTH_AMP_EG_SUSTAIN        0.8f
#define SYNTH_AMP_EG_RELEASE_MS     250.0f

// ---- LFO ----
#define SYNTH_LFO1_RATE_HZ   4.0f
#define SYNTH_LFO1_SHAPE     LfoShape::Triangle
#define SYNTH_LFO2_RATE_HZ   0.5f
#define SYNTH_LFO2_SHAPE     LfoShape::Sine

// ---- ポルタメント ----
#define SYNTH_PORTAMENTO_MS  0.0f

// ---- エフェクト ----
// mix系はデフォルト0（OFF）。TIME/FEEDBACK/RATE/DEPTHはONにしたときの初期値。
#define SYNTH_DELAY_TIME_MS   280.0f
#define SYNTH_DELAY_FEEDBACK  0.35f
#define SYNTH_DELAY_MIX       0.0f    // 旧: 0.25f（常時ON） -> デフォルトOFF
#define SYNTH_CHORUS_RATE_HZ  0.6f
#define SYNTH_CHORUS_DEPTH    0.3f
#define SYNTH_CHORUS_MIX      0.0f    // 旧: 0.25f（常時ON） -> デフォルトOFF

// ---- マスター ----
#define SYNTH_MASTER_VOLUME   0.8f

// ---- テストノート（USER_BUTTON0で再生する確認用の音） ----
#define SYNTH_TEST_NOTE_MIDI_NUMBER  60    // 中央ド(C4)
#define SYNTH_TEST_NOTE_VELOCITY     100
#define SYNTH_TEST_NOTE_DURATION_MS  500
