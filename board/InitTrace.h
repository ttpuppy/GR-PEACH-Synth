// InitTrace.h
// ハードウェアオブジェクトの生成を1つずつトレースするための共通ヘルパー。
// RZ/A1Hは "pinmap not found for peripheral" のようなエラーが発生すると、
// エラーメッセージだけを出して停止する（どのオブジェクトの生成中だったかは
// 分からない）。そこで、各ペリフェラルの生成直前・直後に必ずこのマクロを通し、
// シリアルログから原因ピンを特定できるようにする。
//
// 使い方:
//   TRACE_NEW(gOled, Ssd1306Oled, "SSD1306 OLED (I2C SDA=kOledSda, SCL=kOledScl)",
//             board::kOledSda, board::kOledScl, board::kOledI2cAddress7bit);
//
// 上記は概念的な例。実際にはコンストラクタの引数の数が可変なため、
// 本プロジェクトでは以下の簡潔な2行パターンを各生成箇所で手書きする方針とする
// （マクロで無理に汎用化するとエラー時のスタックトレースが読みにくくなるため）。
//
//   printf("[init] creating <名前> (<ピン情報>)...\r\n");
//   gXxx = new Xxx(...);
//   printf("[init] <名前> OK\r\n");
//
// このファイルは上記パターンで使う共通のprintf文言ヘルパーのみ提供する。

#pragma once
#include "mbed.h"

namespace board {

inline void traceInitBegin(const char* label) {
    printf("[init] creating %s ...\r\n", label);
}

inline void traceInitOk(const char* label) {
    printf("[init] %s OK\r\n", label);
}

// PinNameを人間が読める形にできる範囲で出力する（Port/Bit番号までは分解しない簡易版）。
// mbed-osのPinName列挙値は数値でしかないため、意味のある名前はコード側のコメントで
// 補足する運用とする。
inline void tracePinValue(const char* pinLabel, PinName pin) {
    printf("       %s = PinName(%d)\r\n", pinLabel, static_cast<int>(pin));
}

} // namespace board
