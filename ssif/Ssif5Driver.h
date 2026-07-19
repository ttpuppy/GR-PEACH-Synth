// Ssif5Driver.h
// RZ/A1H SSIF5を直接レジスタ操作で駆動する、mbed-os完全非依存のドライバ。
//
// 【更新】ユーザー提供の「RZ/A1H Software Package for GR-PEACH V1.70」から
// SSIF5のレジスタベースアドレス・オフセット・ビット位置・クロック有効化
// 手順・チップレベルのピン割り当てが確認できたため、当初mbed-osの
// pin_function()に頼っていたピン設定も含め、本ファイルは完全にmbed-os非依存の
// 生レジスタアクセスのみで構成した（<mbed.h>を一切includeしていない）。
// これにより、将来FreeRTOSベースへ移行する際もこのファイルをほぼそのまま
// 持っていける。
//
// 【信頼度】
// - レジスタベースアドレス・オフセット・SSICR/SSIFCR/SSISRのビット位置・
//   CPGのクロック/リセット制御・GPIO/PFCのレジスタオフセットは、いずれも
//   公式ソフトウェアパッケージのソースコードから直接確認済み（詳細は
//   ssif/ChipRegs.h, ssif/Ssif5Regs.h のコメント参照）。
// - SSIFSRのビット位置（FIFO空き検出）と、GR-PEACH基板上でのP2_4/P2_5/P2_7の
//   実際の引き出し先は未確認（詳細は各ファイルのコメント参照）。

#pragma once
#include <cstdint>
#include <cstddef>
#include "../board/BoardConfig.h"
#include "Ssif5Regs.h"
#include "ChipRegs.h"

namespace ssif {

class Ssif5Driver {
public:
    // sampleRateHz: 再生サンプルレート。GR-PEACHのオーディオ用水晶(AUDIO_X1,
    // 22.5792MHz)からビットクロックを整数分周できるのは44100Hz系列のみ
    // のため、事実上44100固定（詳細はssif/Ssif5Regs.hのkSsicrValCkdvDiv8
    // コメント参照）。
    bool init(std::uint32_t sampleRateHz) {
        if (sampleRateHz != 44100) {
            // 【今回追加】従来はこのチェックが無く、48000Hz指定でも
            // "成功"扱いになっていたが、実際にはCKDV(クロック分周比)を
            // 一切設定していなかったため常に分周比1のまま鳴らそうとする
            // バグがあった（=意図したサンプルレートと無関係に、常に
            // 誤ったビットクロックで出力される状態）。分周比の計算表は
            // 44100Hz用の値しか用意していないため、他のレートは明示的に
            // 拒否するようにした。
            return false;
        }
        sampleRateHz_ = sampleRateHz;

        if (board::kSsif5BaseAddr == 0) {
            return false; // 未設定（本ドライバでは既定値を確認済みのため通常発生しない）
        }

        configurePins();
        configureClockGating();
        configureReset();
        configureRegisters();
        return true;
    }

    // インターリーブ済みステレオPCM（16bit）をブロック単位でSSIF5へポーリング送信する。
    // DMAは使わず、FIFOに空きが出るたびに書き込むシンプルなポーリング実装。
    void writeBlockPolling(const int16_t* interleavedStereo, std::size_t frames) {
        if (!isReady()) return;
        for (std::size_t i = 0; i < frames * 2; ++i) {
            if (!waitForTxFifoSpace()) {
                // 【更新】以前はSSIFSRのTDEビット位置が未確認だったが、
                // 公式ハードウェアマニュアル(19.3.6節)でbit16と確認できた
                // （ssif/Ssif5Regs.hのkSsifsrBitTDE参照）。以前bit0(実際は
                // RDF)を見ていたバグにより、この分岐が常に真になり
                // 実質無音になっていたと考えられる。
                return;
            }
            writeReg(ssif_regs::kOffsetSSIFTDR, static_cast<std::uint32_t>(
                static_cast<std::int32_t>(interleavedStereo[i]) << 16)); // 上位16bitに配置(左詰め)
        }
    }

    bool isReady() const { return ready_; }

private:
    // ---- ピン機能(PFC)設定 ----
    // 公式ソフトウェアパッケージ renesas/drivers/r_ssif/src/lld/ssif_portsetting.c
    // の SSIF_CHNUM_5 (TARGET_BOARD_RSK) ケースの手順をそのまま踏襲し、
    // GPIO.PxxN構造体アクセスを直接アドレス演算に置き換えたもの。
    // P2_4=SCK, P2_5=WS, P2_7=TxD（Alternative Mode 4）。
    // 受信は本用途では不要なためP2_6(RxD)の設定は省略している。
    void configurePins() {
        setPinAltMode4(chip_regs::kSsif5PinBitSck, /*isOutput=*/true);
        setPinAltMode4(chip_regs::kSsif5PinBitWs,  /*isOutput=*/true);
        setPinAltMode4(chip_regs::kSsif5PinBitTxd, /*isOutput=*/true);
    }

    // Port2の指定ビットをAlternative Mode 4（SSIF5系統）・入出力設定にする。
    // 手順は公式パッケージのSSIF_PortSetting()内の一連のPIBC/PBDC/PM/PMC/
    // PIPC/PFC/PFCE/PFCAEレジスタ操作を忠実に再現している。
    void setPinAltMode4(int bit, bool isOutput) {
        using namespace chip_regs;
        const std::uint16_t mask = static_cast<std::uint16_t>(1u << bit);

        writeGpio16(kOffsetPIBC2, readGpio16(kOffsetPIBC2) & static_cast<std::uint16_t>(~mask));
        writeGpio16(kOffsetPBDC2, readGpio16(kOffsetPBDC2) & static_cast<std::uint16_t>(~mask));
        writeGpio16(kOffsetPM2,   readGpio16(kOffsetPM2)   | mask);
        writeGpio16(kOffsetPMC2,  readGpio16(kOffsetPMC2)  & static_cast<std::uint16_t>(~mask));
        writeGpio16(kOffsetPIPC2, readGpio16(kOffsetPIPC2) & static_cast<std::uint16_t>(~mask));

        if (isOutput) {
            writeGpio16(kOffsetPBDC2, readGpio16(kOffsetPBDC2) | mask);
        } else {
            writeGpio16(kOffsetPBDC2, readGpio16(kOffsetPBDC2) & static_cast<std::uint16_t>(~mask));
        }
        writeGpio16(kOffsetPFC2,   readGpio16(kOffsetPFC2)   | mask); // Alt Mode 4: PFC=1
        writeGpio16(kOffsetPFCE2,  readGpio16(kOffsetPFCE2)  | mask); // PFCE=1
        writeGpio16(kOffsetPFCAE2, readGpio16(kOffsetPFCAE2) & static_cast<std::uint16_t>(~mask)); // PFCAE=0

        writeGpio16(kOffsetPIPC2, readGpio16(kOffsetPIPC2) | mask);
        writeGpio16(kOffsetPMC2,  readGpio16(kOffsetPMC2)  | mask);
    }

    // ---- CPGクロック供給有効化 ----
    // 出典: renesas/drivers/r_ssif/src/lld/ssif.c
    //   CPGSTBCR11 &= ~gb_cpg_stbcr_bit[ch];  (該当ビットを0にしてクロック供給)
    void configureClockGating() {
        std::uint8_t stbcr11 = readCpg8(chip_regs::kOffsetStbcr11);
        stbcr11 = static_cast<std::uint8_t>(stbcr11 & ~chip_regs::kStbcr11BitSsif5);
        writeCpg8(chip_regs::kOffsetStbcr11, stbcr11);
        (void)readCpg8(chip_regs::kOffsetStbcr11); // ダミーリード（公式コードと同様の作法）
    }

    // ---- ソフトウェアリセット（パルス） ----
    // 出典: renesas/drivers/r_ssif/src/lld/ssif.c の SSIF_Reset()
    void configureReset() {
        std::uint8_t swrstcr1 = readCpg8(chip_regs::kOffsetSwrstcr1);
        writeCpg8(chip_regs::kOffsetSwrstcr1,
                  static_cast<std::uint8_t>(swrstcr1 | chip_regs::kSwrstcr1BitSsif5));
        (void)readCpg8(chip_regs::kOffsetSwrstcr1);

        swrstcr1 = readCpg8(chip_regs::kOffsetSwrstcr1);
        writeCpg8(chip_regs::kOffsetSwrstcr1,
                  static_cast<std::uint8_t>(swrstcr1 & ~chip_regs::kSwrstcr1BitSsif5));
        (void)readCpg8(chip_regs::kOffsetSwrstcr1);
    }

    void configureRegisters() {
        using namespace ssif_regs;
        // FIFOリセット
        writeReg(kOffsetSSIFCR, kSsifcrBitTFRST | kSsifcrBitRFRST);
        writeReg(kOffsetSSIFCR, 0);

        // I2Sマスター送信: CKS=0(AUDIO_X1), CHNL=1(2ch/ステレオ), DWL=16bit,
        // SWL=32bit, CKDV=分周比8(44100Hz用, 下記参照), SCKD=1/SWSD=1
        // (マスター、自ら出力), DEL=1(I2Sは1bit遅延), TEN=1(送信許可)
        //
        // 【今回修正】以前はCKDV(クロック分周比、bit4-7)を一切設定して
        // いなかった。CKDVは初期値0(=分周比1相当)のまま放置されており、
        // これだと実際のビットクロックが44100Hz用に意図した周波数の
        // 8倍の速さで出力されてしまい、PCM5102側が正しく認識できず
        // 無音・または異常なピッチになっていた可能性が高い。
        std::uint32_t ssicr = 0;
        ssicr |= (kSsicrValDwl16bit << kSsicrShiftDWL) & kSsicrBitDwlMask;
        ssicr |= (kSsicrValSwl32bit << kSsicrShiftSWL) & kSsicrBitSwlMask;
        ssicr |= (kSsicrValCkdvDiv8 << kSsicrShiftCKDV) & kSsicrBitCkdvMask;
        ssicr |= (1u << kSsicrShiftCHNL) & kSsicrBitChnlMask; // 2ch
        ssicr |= (1u << kSsicrShiftSCKD); // マスター: BCLK自ら出力
        ssicr |= (1u << kSsicrShiftSWSD); // マスター: WS自ら出力
        ssicr |= (1u << kSsicrShiftDEL);  // I2Sフォーマット(1bit遅延)
        ssicr |= (1u << kSsicrShiftTEN);  // 送信許可
        writeReg(kOffsetSSICR, ssicr);

        ready_ = true;
    }

    bool waitForTxFifoSpace() {
        using namespace ssif_regs;
        constexpr std::uint32_t kTimeoutSpins = 1000000; // 実クロックに応じて要調整
        for (std::uint32_t spins = 0; spins < kTimeoutSpins; ++spins) {
            if (readReg(kOffsetSSIFSR) & kSsifsrBitTDE) return true;
        }
        return false; // タイムアウト
    }

    // ---- SSIF5レジスタ(32bit)アクセス ----
    void writeReg(std::uint32_t offset, std::uint32_t value) {
        *reinterpret_cast<volatile std::uint32_t*>(board::kSsif5BaseAddr + offset) = value;
    }
    std::uint32_t readReg(std::uint32_t offset) {
        return *reinterpret_cast<volatile std::uint32_t*>(board::kSsif5BaseAddr + offset);
    }

    // ---- CPGレジスタ(8bit)アクセス ----
    void writeCpg8(std::uint32_t offset, std::uint8_t value) {
        *reinterpret_cast<volatile std::uint8_t*>(chip_regs::kCpgBaseAddr + offset) = value;
    }
    std::uint8_t readCpg8(std::uint32_t offset) {
        return *reinterpret_cast<volatile std::uint8_t*>(chip_regs::kCpgBaseAddr + offset);
    }

    // ---- GPIO/PFCレジスタ(16bit)アクセス ----
    void writeGpio16(std::uint32_t offset, std::uint16_t value) {
        *reinterpret_cast<volatile std::uint16_t*>(chip_regs::kGpioBaseAddr + offset) = value;
    }
    std::uint16_t readGpio16(std::uint32_t offset) {
        return *reinterpret_cast<volatile std::uint16_t*>(chip_regs::kGpioBaseAddr + offset);
    }

    std::uint32_t sampleRateHz_ = 44100;
    bool ready_ = false;
};

} // namespace ssif
