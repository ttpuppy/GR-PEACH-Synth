// ChipRegs.h
// SSIF5を駆動するために必要な、CPG（クロック・リセット制御）とGPIO/PFC
// （ピン機能マルチプレクサ）のレジスタアドレス定義。
//
// 【出典・信頼度】
// ユーザー提供の「RZ/A1H Software Package for GR-PEACH V1.70」に含まれる
// 以下の公式ヘッダファイルから、実際に構造体をコンパイルして offsetof() で
// 算出した値（推測ではない）:
//   - renesas/application/system/iodefines/cpg_iodefine.h  (struct st_cpg)
//   - renesas/application/system/iodefines/gpio_iodefine.h (struct st_gpio)
//   - renesas/application/inc/Renesas_RZ_A1.h（ビット定義マクロ）
//
// CPGベースアドレス: 0xFCFE0010
// GPIOベースアドレス: 0xFCFE3004
// （このGPIOベースアドレスは、本プロジェクトのユーザーが以前別のRZ/A1H基板
//   （MP-RZA1H/FPGA-01）のLEDブリンク検証で確認済みの値と一致しており、
//   同一チップファミリとして整合性がある）

#pragma once
#include <cstdint>

namespace chip_regs {

// ---- CPG (Clock Pulse Generator) ----
constexpr std::uint32_t kCpgBaseAddr = 0xFCFE0010u;

// CONFIRMED: offsetof(struct st_cpg, STBCR11) / SWRSTCR1 を実機コンパイルして算出
constexpr std::uint32_t kOffsetStbcr11   = 0x430; // モジュールストップ制御11（クロック供給停止/再開）
constexpr std::uint32_t kOffsetSwrstcr1  = 0x450; // ソフトウェアリセット制御1

// STBCR11: 該当ビットが1=クロック停止、0=クロック供給
// （出典: renesas/drivers/r_ssif/src/lld/ssif.c の
//   `CPGSTBCR11 &= ~gb_cpg_stbcr_bit[ch];` で有効化していることから極性を確認）
constexpr std::uint8_t kStbcr11BitSsif0 = 0x20u; // MSTP115
constexpr std::uint8_t kStbcr11BitSsif1 = 0x10u; // MSTP114
constexpr std::uint8_t kStbcr11BitSsif2 = 0x08u; // MSTP113
constexpr std::uint8_t kStbcr11BitSsif3 = 0x04u; // MSTP112
constexpr std::uint8_t kStbcr11BitSsif4 = 0x02u; // MSTP111
constexpr std::uint8_t kStbcr11BitSsif5 = 0x01u; // MSTP110 ★SSIF5はこのビット★

// SWRSTCR1: ソフトウェアリセット。該当ビットを1->0とパルスすることでリセット
// （出典: renesas/drivers/r_ssif/src/lld/ssif.c の SSIF_Reset() 関数の手順を
//   そのまま踏襲: セット→ダミーリード→クリア→ダミーリード）
constexpr std::uint8_t kSwrstcr1BitSsif0 = 0x40u; // SRST16
constexpr std::uint8_t kSwrstcr1BitSsif1 = 0x20u; // SRST15
constexpr std::uint8_t kSwrstcr1BitSsif2 = 0x10u; // SRST14
constexpr std::uint8_t kSwrstcr1BitSsif3 = 0x08u; // SRST13
constexpr std::uint8_t kSwrstcr1BitSsif4 = 0x04u; // SRST12
constexpr std::uint8_t kSwrstcr1BitSsif5 = 0x02u; // SRST11 ★SSIF5はこのビット★
// 【更新】AXTALE(AUDIO_X1有効化)について、公式ハードウェアマニュアルの
// 第6章「クロックパルス発振器」を確認したが、AUDIO_X1に対する明示的な
// 「発振有効化ビット」の記載は見当たらなかった。水晶発振子が物理的に
// 実装されていれば電源投入時から発振する一般的なクロック端子として
// 扱われている可能性が高く（GR-PEACHのオンボードWM8978がSSIF0で
// AUDIO_X1を使って動作している実績とも整合する）、本ビットは
// 不要である可能性が高いと判断し、現状は叩いていない。
constexpr std::uint8_t kSwrstcr1BitAudioX1Enable = 0x80u; // 現状未使用（上記参照）

// ---- GPIO / PFC (Pin Function Controller) ----
constexpr std::uint32_t kGpioBaseAddr = 0xFCFE3004u;

// CONFIRMED: offsetof(struct st_gpio, ...) を実機コンパイルして算出（Port2用）
// これらのオフセットはP0〜P11共通の規則的な配置になっているため、他ポートでも
// 「ポート番号n」に対して同じ規則で算出可能（本ファイルはSSIF5用にPort2のみ収録）。
constexpr std::uint32_t kOffsetP2     = 0x4;
constexpr std::uint32_t kOffsetPM2    = 0x304;
constexpr std::uint32_t kOffsetPMC2   = 0x404;
constexpr std::uint32_t kOffsetPFC2   = 0x504;
constexpr std::uint32_t kOffsetPFCE2  = 0x604;
constexpr std::uint32_t kOffsetPFCAE2 = 0xA04;
constexpr std::uint32_t kOffsetPIBC2  = 0x4004;
constexpr std::uint32_t kOffsetPBDC2  = 0x4104;
constexpr std::uint32_t kOffsetPIPC2  = 0x4204;

// ---- SSIF5のピン(P2_4=SCK, P2_5=WS, P2_7=TxD) ----
// 【CONFIRMED: GR-PEACH回路図(X28A-M01-E/F, x28am01ef.pdf)で確認済み】
// SHT.4「Expansion I/F」のCPUピン一覧、およびSHT.6「PINMAP」のCN11ブロックで、
// 以下がそのまま記載されている:
//   P2_4/D20/.../SSISCK5/...   (SSIF5ビットクロック)
//   P2_5/D21/.../SSIWS5/...    (SSIF5ワードセレクト)
//   P2_6/D22/.../SSIRxD5/...   (SSIF5受信データ、本用途では未使用)
//   P2_7/D23/.../SSITxD5/...   (SSIF5送信データ)
// これらはCN11「Arduino Compatible Pin Socket/Header」に、拡張ピン番号
// D20/D21/D22/D23として実際に引き出されている（標準Arduino UnoのD0-D15を
// 超えるGR-PEACH独自拡張ピン）。CN11上の物理ピン番号(1-8)の並び順は
// PDFのテキスト抽出では確実に復元できなかったため、実際の配線時は
// 回路図の図（テキストではなく図面そのもの）または基板のシルク印刷で
// D20-D23の位置を直接確認すること。
//
// 【前回未確認としていた点は解消】
// 「P2_4-P2_7がチップ仕様としてSSIF5に対応することは確認できたが、
//   GR-PEACH基板上で実際に引き出されているかは未確認」としていたが、
// 上記の通り回路図で実際に確認できた。
constexpr int kSsif5PinBitSck = 4; // P2_4 (SSISCK5, CN11 = D20)
constexpr int kSsif5PinBitWs  = 5; // P2_5 (SSIWS5,  CN11 = D21)
constexpr int kSsif5PinBitTxd = 7; // P2_7 (SSITxD5, CN11 = D23)
// P2_6(SSIRxD5, CN11 = D22)は再生専用のため未使用。

} // namespace chip_regs
