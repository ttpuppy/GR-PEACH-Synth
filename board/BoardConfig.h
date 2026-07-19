// BoardConfig.h
// GR-PEACHを「音生成・音出し専任」とした構成のピン定義集約ファイル。
// パネル入力(エンコーダー/OLED/タクトスイッチ)は本構成では一切使用しない
// （それらは別のコントロール系ボード/PC側に分離された前提）。
//
// 【ポータビリティ方針】
// GPIO（LED/ボタン）はmbed-osのDigitalOut/DigitalIn/PinNameのままにしている。
// 理由: 単純なGPIO on/offはどのRTOS/HALに移植してもほぼ1:1で書き換えられる
// 薄いラッパーであり、今の時点で生レジスタに置き換えるポータビリティ上の
// メリットがほとんどない。一方でSSIF（オーディオ）は`ssif/Ssif5Driver.h`で
// mbed-osへの依存を切り離した生レジスタアクセスとして実装している。
//
// 【SSIF5について: 確認済み情報の出典】
// レジスタベースアドレス・オフセット・ビット位置・チップレベルのピン割り当ては
// ユーザー提供の「RZ/A1H Software Package for GR-PEACH V1.70」の公式ソース
// コードから直接確認済み（詳細は ssif/ChipRegs.h, ssif/Ssif5Regs.h のコメント
// 参照）。さらに、そのチップ側ピン(P2_4/P2_5/P2_6/P2_7)がGR-PEACHの基板上で
// 実際にCN11コネクタ（Arduino互換拡張ピンD20-D23）へ引き出されていることも、
// GR-PEACH回路図（X28A-M01-E/F, x28am01ef.pdf）のSHT.4/SHT.6で確認済み。
// これで配線に関する未確認事項はすべて解消している（詳細はREADME.md参照）。

#pragma once
#include "mbed.h"
#include <cstdint>

namespace board {

// -------------------- CONFIRMED: オンボードLED --------------------
// ユーザー提供仕様どおり。前回検討時のPG_12〜15から本仕様ではP6_12〜15に修正。
constexpr PinName kLedUser  = P6_12; // ハートビート
constexpr PinName kLedRed   = P6_13; // 用途未定（予約）
constexpr PinName kLedGreen = P6_14; // 用途未定（予約）
constexpr PinName kLedBlue  = P6_15; // 用途未定（予約）

// -------------------- CONFIRMED: USER_BUTTON0 --------------------
// 用途: テストノート再生（MIDI入力なしで音声パスの動作確認を行うため）。
// mbed-osのGR-PEACHターゲット定義にUSER_BUTTON0というPinNameシンボルが
// 存在する前提でmain.cpp側は実装している。
// もしビルド時に "USER_BUTTON0 was not declared" 等のエラーが出た場合は、
// 実際のPinName（回路図で要確認）に置き換えること。
constexpr PinName kUserButton0 = USER_BUTTON0;

// -------------------- CONFIRMED: USB0 --------------------
// USB Serial（CDC）とUSB MIDIの共用端子として使用。
// mbed-osの標準USBMIDIクラスを使用（usb/UsbMidiInput.h参照）。
// 【既知の制約】同一物理ポート上でのCDC Serial + MIDIの真の複合(コンポジット)
// USBデバイス化には、mbed-osのUSBDeviceを直接継承したカスタムディスクリプタの
// 実装が必要（本フェーズではUSBMIDIのみを実装し、複合化はTODO。README参照）。

// -------------------- CONFIRMED: RFモジュールインターフェース --------------------
// 全ピン不使用（ここでは何も定義しない）。

// -------------------- CONFIRMED: SSIF5 <-> PCM5102 --------------------
// CONFIRMED: renesas/application/system/iodefines/ssif_iodefine.h より
// #define SSIF5 (*(struct st_ssif *)0xE820D800uL)
constexpr std::uint32_t kSsif5BaseAddr = 0xE820D800u;

// SSIF5のピン機能設定・クロック供給・リセットは、すべて ssif/Ssif5Driver.h と
// ssif/ChipRegs.h 内の生レジスタ操作で完結するため、ここでのPinName定義は
// 不要。ピン割り当て（P2_4/P2_5/P2_7、GR-PEACH基板上ではCN11のD20/D21/D23）
// はGR-PEACH回路図で確認済み（詳細は ssif/ChipRegs.h のコメント参照）。

} // namespace board
