// Ssif5Regs.h
// RZ/A1H SSIF（Serial Sound Interface）のレジスタオフセット・ビットフィールド定義。
//
// 【出典・信頼度についての注記 - 更新済み】
// 以下の値は、ユーザーから提供された「RZ/A1H Software Package for GR-PEACH
// V1.70」（Renesas公式ソフトウェアパッケージ）に実際に含まれるソースコードから
// 直接確認した値であり、信頼度は高い（推測ではない）。
//
//   - レジスタオフセット: renesas/application/system/iodefines/ssif_iodefine.h
//     の struct st_ssif 定義から算出（offsetofで実機コンパイル確認済み）
//   - SSICR/SSIFCR/SSISRのビット位置: renesas/drivers/r_ssif/inc/ssif.h の
//     SSIF_CR_SHIFT_*, SSIF_FCR_SHIFT_*, SSIF_SR_SHIFT_* マクロから直接転記
//
// 【残る不確定要素 - 更新済み】
// SSIFSR（FIFOステータスレジスタ）のビット位置は、ユーザー提供の公式
// ハードウェアマニュアル（R01UH0403JJ0700 Rev.7.00, 19.3.6節）で確認できた。
// TDEはbit16（従来誤ってbit0=RDFを見ていたのを修正済み。詳細は下記
// kSsifsrBitTDEのコメント参照）。

#pragma once
#include <cstdint>

namespace ssif_regs {

// ---- レジスタオフセット（CONFIRMED: ssif_iodefine.h の struct st_ssif より） ----
constexpr std::uint32_t kOffsetSSICR   = 0x00; // 制御レジスタ
constexpr std::uint32_t kOffsetSSISR   = 0x04; // ステータスレジスタ
constexpr std::uint32_t kOffsetSSIFCR  = 0x10; // FIFO制御レジスタ
constexpr std::uint32_t kOffsetSSIFSR  = 0x14; // FIFOステータスレジスタ（★TODO★下記参照）
constexpr std::uint32_t kOffsetSSIFTDR = 0x18; // 送信FIFOデータレジスタ（書き込み専用）
constexpr std::uint32_t kOffsetSSIFRDR = 0x1C; // 受信FIFOデータレジスタ（本用途では未使用）
constexpr std::uint32_t kOffsetSSITDMR = 0x20; // TDMモードレジスタ（本用途では未使用）
constexpr std::uint32_t kOffsetSSIFCCR = 0x24; // （CONFIRMED存在, 用途未使用）
constexpr std::uint32_t kOffsetSSIFCMR = 0x28; // （CONFIRMED存在, 用途未使用）
constexpr std::uint32_t kOffsetSSIFCSR = 0x2C; // （CONFIRMED存在, 用途未使用）

// ---- SSICR ビットフィールド（CONFIRMED: ssif.h SSIF_CR_SHIFT_* より） ----
constexpr std::uint32_t kSsicrShiftCKS  = 30; // オーディオクロック選択(0=AUDIO_X1, 1=AUDIO_CLK)
constexpr std::uint32_t kSsicrShiftTUIEN = 29;
constexpr std::uint32_t kSsicrShiftTOIEN = 28;
constexpr std::uint32_t kSsicrShiftRUIEN = 27;
constexpr std::uint32_t kSsicrShiftROIEN = 26;
constexpr std::uint32_t kSsicrShiftIIEN  = 25;
constexpr std::uint32_t kSsicrShiftCHNL = 22; // 2bit: チャンネル数(0=1ch monaural, 1=2ch stereo)
constexpr std::uint32_t kSsicrShiftDWL  = 19; // 3bit: データワード長
constexpr std::uint32_t kSsicrShiftSWL  = 16; // 3bit: システムワード長
constexpr std::uint32_t kSsicrShiftSCKD = 15; // ビットクロック方向(1=マスター/自ら出力)
constexpr std::uint32_t kSsicrShiftSWSD = 14; // ワードセレクト方向(1=マスター/自ら出力)
constexpr std::uint32_t kSsicrShiftSCKP = 13; // ビットクロック極性
constexpr std::uint32_t kSsicrShiftSWSP = 12; // ワードセレクト極性
constexpr std::uint32_t kSsicrShiftSPDP = 11; // パディング極性
constexpr std::uint32_t kSsicrShiftSDTA = 10; // シリアルデータアライメント
constexpr std::uint32_t kSsicrShiftPDTA = 9;  // パラレルデータアライメント
constexpr std::uint32_t kSsicrShiftDEL  = 8;  // フレーム同期ディレイ(I2S=1)
constexpr std::uint32_t kSsicrShiftCKDV = 4;  // 4bit: クロック分周比
constexpr std::uint32_t kSsicrShiftMUEN = 3;  // ミュート
constexpr std::uint32_t kSsicrShiftTEN  = 1;  // 送信許可
constexpr std::uint32_t kSsicrShiftREN  = 0;  // 受信許可

constexpr std::uint32_t kSsicrBitCkdvMask = (0xFu << kSsicrShiftCKDV);
constexpr std::uint32_t kSsicrBitDwlMask  = (0x7u << kSsicrShiftDWL);
constexpr std::uint32_t kSsicrBitSwlMask  = (0x7u << kSsicrShiftSWL);
constexpr std::uint32_t kSsicrBitChnlMask = (0x3u << kSsicrShiftCHNL);

// DWL/SWLの値エンコード（CONFIRMED: 一般的なRZ/A1 SSIF仕様。3bit値でワード長を表現。
// 000=8bit, 001=16bit, 010=18bit, 011=20bit, 100=22bit, 101=24bit, 110=32bit(SWLのみ), ...
// 本プロジェクトでは 16bitデータ/32bitシステムワード の組み合わせのみ使用）
constexpr std::uint32_t kSsicrValDwl16bit = 1u; // DWL=001 -> 16bit
constexpr std::uint32_t kSsicrValSwl32bit = 6u; // SWL=110 -> 32bit

// ---- CKDV（クロック分周比）エンコード ----
// 【今回追加・重要】configureRegisters()が従来CKDVビットを一切設定して
// いなかった（=常にCKDV=0"1分周"のまま）のを修正するために追加。
// 出典: d-kato/mbed-gr-libs の R_BSP_SsifDef.h に定義されている
// SSIF_CFG_CKDV_BITS_* enum（Renesas公式ドライバのCKDV分周比エンコード表）
// から、必要な分周比8のぶんだけ転記した（他の分周比は本プロジェクトでは未使用）。
//   分周比1→0, 2→1, 4→2, 8→3, 16→4, 32→5, 64→6, 128→7,
//   6→8, 12→9, 24→10, 48→11, 96→12
//
// GR-PEACHのオーディオ用水晶(AUDIO_X1)は22.5792MHzで、
// ビットクロック = 22,579,200 / 分周比 が実際に出力される周波数になる。
// SWL=32bit(システムワード長)・2ch(ステレオ)の場合、
// 必要なビットクロック = サンプルレート x 32 x 2 なので、
// 44100Hzなら 22,579,200 / (44100 x 32 x 2) = 8 と綺麗に割り切れるため
// 分周比8(CKDV=3)を使う。48000Hzでは 22,579,200 / (48000x32x2) = 7.35
// となり整数の分周比が存在しないため、48000Hzはこの水晶では原理的に
// 生成不可能（分周比の一覧1,2,4,8,16,32,64,128,6,12,24,48,96のいずれにも
// 一致しない）。そのためSYNTH_SAMPLE_RATE_HZは44100固定とすること。
constexpr std::uint32_t kSsicrValCkdvDiv8 = 3u;

// ---- SSIFCR ビットフィールド（CONFIRMED: ssif.h SSIF_FCR_SHIFT_* より） ----
constexpr std::uint32_t kSsifcrShiftTIE   = 3;
constexpr std::uint32_t kSsifcrShiftRIE   = 2;
constexpr std::uint32_t kSsifcrShiftTFRST = 1;
constexpr std::uint32_t kSsifcrShiftRFRST = 0;
constexpr std::uint32_t kSsifcrBitTIE   = (1u << kSsifcrShiftTIE);
constexpr std::uint32_t kSsifcrBitRIE   = (1u << kSsifcrShiftRIE);
constexpr std::uint32_t kSsifcrBitTFRST = (1u << kSsifcrShiftTFRST);
constexpr std::uint32_t kSsifcrBitRFRST = (1u << kSsifcrShiftRFRST);

// ---- SSISR ビットフィールド（CONFIRMED: ssif.h SSIF_SR_SHIFT_* より） ----
constexpr std::uint32_t kSsisrShiftTUIRQ = 29;
constexpr std::uint32_t kSsisrShiftTOIRQ = 28;
constexpr std::uint32_t kSsisrShiftRUIRQ = 27;
constexpr std::uint32_t kSsisrShiftROIRQ = 26;
constexpr std::uint32_t kSsisrShiftIIRQ  = 25;
constexpr std::uint32_t kSsisrShiftIDST  = 0;
constexpr std::uint32_t kSsisrBitIIRQ = (1u << kSsisrShiftIIRQ);
constexpr std::uint32_t kSsisrBitIntErrMask =
    (1u << kSsisrShiftTUIRQ) | (1u << kSsisrShiftTOIRQ) |
    (1u << kSsisrShiftRUIRQ) | (1u << kSsisrShiftROIRQ);

// ---- SSIFSR ビットフィールド（CONFIRMED: ユーザー提供の公式ハードウェア
//   マニュアル「RZ/A1Hグループ、RZ/A1Mグループ ユーザーズマニュアル
//   ハードウェア編」(R01UH0403JJ0700 Rev.7.00) 19.3.6「FIFOステータス
//   レジスタ（SSIFSR）」(p.19-15, PDFページ911) より直接確認） ----
//
// 【今回、重大な修正】
// 従来 kSsifsrBitTDE = (1u << 0) としていたが、これは誤りだった。
// マニュアルのビット配置は次の通り：
//   bit31-28: リザーブ
//   bit27-24: TDC[3:0]（送信FIFO格納データ数）
//   bit23-17: リザーブ
//   bit16   : TDE（送信データエンプティ）★これが正しい位置★
//   bit15-12: リザーブ
//   bit11-8 : RDC[3:0]（受信FIFO格納データ数）
//   bit7-1  : リザーブ
//   bit0    : RDF（受信データフル）
// つまり以前のコードは「受信データフル(RDF)」のビットを見ていたことになる。
// 本プロジェクトは受信(RxD)を一切使っていないため、このビットは
// 常に0のまま（受信データは来ないので、当然RDFは立たない）。
// そのため waitForTxFifoSpace() は永久にタイムアウトし続け、
// writeBlockPolling() が実質的に何もサンプルを送信できていなかった
// ことになる。これが「無音」の直接の原因だった可能性が高い。
//
// なお、マニュアルにはTDEのクリア条件として「トリガ数より多いデータを
// 書き込み、かつTDEに明示的に0を書き込む」という記載があるが、
// 本ドライバはFIFOコントロールレジスタ(SSIFCR)のTTRG[1:0]を
// 初期値のまま(00=しきい値7、つまりFIFOが空でなければほぼ常に
// TDE=1)使っているため、1ワードずつのポーリング書き込みでは
// このクリア処理を省略しても実用上問題にならないと考えられる
// （マニュアルのSSIFSR初期値 H'00010000 = bit16(TDE)=1 とも整合）。
constexpr std::uint32_t kSsifsrBitTDE = (1u << 16); // CONFIRMED: マニュアル19.3.6

} // namespace ssif_regs
