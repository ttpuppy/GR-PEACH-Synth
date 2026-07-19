// SynthConfig.h
// 検討記録(synth_design_review.md) セクション4.2「GR-PEACH（RZ/A1H単体・ソフトウェアDSP構成）」
// を踏まえたフェーズ4実装のグローバル設定。
//
// 注記: 検討記録6.3では将来的なFPGA(HDL)移植を見据えて固定小数点統一を推奨しているが、
// 本実装はGR-PEACH単体（RZ/A1H、Cortex-A9、VFP搭載）でのソフトウェアDSPに限定するため、
// 開発速度と可読性を優先してfloat(単精度)で統一する。
// 将来FPGA側エンジンを実装する際は、本ファイルのSampleフラグや各DSPモジュールの
// インターフェースはそのままに、内部演算をQ15/Q31固定小数点に置き換えることを想定している。

#pragma once

#include <cstdint>
#include <cmath>

// 【今回修正】48000 -> 44100
// GR-PEACHのオーディオ用水晶(AUDIO_X1, 22.5792MHz)からSSIF5のビット
// クロックを整数分周できるのは44.1kHz系列のみ(22,579,200 / (32bit x 2ch
// x 44100Hz) = 8、割り切れる)。48000Hzでは割り切れず
// (22,579,200 / (32bit x 2ch x 48000Hz) = 7.35)、ssif/Ssif5Driver.h::init()
// が明示的にfalseを返すようにしたため、44100に変更した。
#ifndef SYNTH_SAMPLE_RATE_HZ
#define SYNTH_SAMPLE_RATE_HZ 44100
#endif

#ifndef SYNTH_NUM_VOICES
#define SYNTH_NUM_VOICES 8   // フェーズ4目安: 4〜8（トレードオフあり）
#endif

namespace synth {

using Sample = float; // -1.0 .. +1.0 を基本レンジとする正規化サンプル

// ARM Compiler 6 (armclang) の標準ライブラリでは <cmath> から M_PI が
// 常に見えるとは限らない（POSIX拡張マクロ依存のため、ライブラリ設定次第で未定義になる）。
// そのため M_PI に依存せず、自前の定数を用意して全DSPモジュールから参照する。
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

constexpr uint32_t kSampleRateHz = SYNTH_SAMPLE_RATE_HZ;
constexpr float kSampleRateHzF   = static_cast<float>(SYNTH_SAMPLE_RATE_HZ);
constexpr float kInvSampleRateHzF = 1.0f / kSampleRateHzF;
constexpr int kNumVoices = SYNTH_NUM_VOICES;

// 【今回追加】コントロールレート分周比。
// フィルターのカットオフ/レゾナンス計算・モジュレーションマトリクス評価・
// ピッチ計算(fastPow2)・LFO/フィルターEGの評価など、「人間の耳には
// そこまで高速な更新が必要ない」パラメータは、毎サンプルではなく
// kControlRateDivサンプルに1回だけ再計算する。16サンプル@44.1kHzでも
// 約0.36ms間隔（≈2.76kHz相当）で更新されるため、通常の演奏では
// 聴感上の違いはほぼ皆無なはずである。一方オシレーターの波形生成・
// フィルターの実際の信号処理・アンプEGは、音質に直結するため
// 引き続き毎サンプル処理する。
constexpr int kControlRateDiv = 16;

// オーディオ処理ブロックサイズ（コーデックDMA/バッファリング単位）
// GR-PEACHのSRAMは10MB使えるため、ディレイ/リバーブ用バッファは別途大きく確保する。
constexpr int kBlockSize = 64;

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// 【今回追加】std::pow(2.0f, x)の高速近似。
// dsp/Voice.h::process()（毎サンプル×ボイス数だけ呼ばれるホットパス）で
// ピッチのモジュレーション計算に std::pow(2.0f, semitone/12.0f) を
// 使っていたが、std::powは超越関数のライブラリ呼び出しで比較的重く、
// 「単音はきれいだが2音以上でパチパチ/ジジジというノイズが乗る」という
// 症状（実機で報告あり）は、ボイス数に比例してCPU処理時間が伸び、
// リアルタイム再生の締め切りに間に合わなくなる典型的な兆候と考えられる。
// 整数部をfloatのビット表現に直接埋め込むテクニック(いわゆる
// "fast exp2")で、ライブラリ呼び出し無しに同等の精度（オーディオ用途で
// 聴感上問題ないレベル）を実現する。
inline float fastPow2(float x) {
    x = clampf(x, -126.0f, 126.0f); // floatの指数部で表現できる範囲にクランプ
    float xi = std::floor(x);
    float xf = x - xi;
    // 2^xf (xf∈[0,1)) の多項式近似（4次、オーディオ用途で十分な精度）
    float poly = 1.0f + xf * (0.6931471805599453f
                 + xf * (0.2401793883296778f
                 + xf * (0.05585549467516293f
                 + xf * (0.008989340744587916f))));
    // 【修正】以前はfloatのビット表現に整数部を直接加算する手法を使って
    // いたが、xiが負の場合に「符号付き整数の左シフト」が発生し、これは
    // C++の未定義動作(UB)だった。ピッチが下方向に変調されるたびにこの
    // 経路を通るため、コンパイラの最適化次第で不正確な値が返ることが
    // あり、これが「2〜3音でチリチリ/ジジジというノイズが乗る」症状の
    // 直接原因だったと考えられる（実測ではCPU処理時間は予算に対し
    // 十分な余裕があったため、CPU負荷が原因ではなくこちらが真因）。
    // std::ldexp(x, n) は x * 2^n を計算する標準ライブラリ関数で、
    // 内部的には指数部の調整のみで済むため std::pow(2,x) より十分軽い。
    return std::ldexp(poly, static_cast<int>(xi));
}

inline float midiNoteToHz(float note) {
    // A4 = MIDIノート69 = 440Hz
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

// 一次ローパス（ワンポール）スムージング。
// 検討記録6.3「ジッパーノイズ対策」に対応する、パラメータ急変対策の補間器。
class OnePoleSmoother {
public:
    void setTimeConstantMs(float ms) {
        float t = ms <= 0.0f ? 0.0001f : ms * 0.001f;
        coeff_ = std::exp(-1.0f / (t * kSampleRateHzF));
    }
    void reset(float value) { value_ = value; target_ = value; }
    void setTarget(float target) { target_ = target; }
    float process() {
        value_ = target_ + coeff_ * (value_ - target_);
        return value_;
    }
    float value() const { return value_; }

private:
    float value_ = 0.0f;
    float target_ = 0.0f;
    float coeff_ = 0.999f;
};

} // namespace synth
