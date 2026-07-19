// Pcm5102Ssif0Output.h
// PCM5102(GY-PCM5102)をSSIF0経由・Renesas公式R_BSP_Ssifドライバ(DMAベース)で
// 駆動する実装。
//
// 【今回の経緯・SSIF5からSSIF0+R_BSPへの切替について】
// 当初はorder.txtの要望（mbed-os依存を減らし将来のFreeRTOS移行に備える）に
// 沿って、SSIF5を生レジスタで直接叩く実装(ssif/Ssif5Driver.h)を採用していた。
// しかし別チャット「GY-5102へのSSIF音声出力検証」で、SSIF0 + Renesas公式
// R_BSP_Ssifドライバ(DMAベース、mbed-gr-libs由来)の組み合わせで実機の
// 音出しに成功したことが確認された。本ファイルはその動作確認済み構成を
// そのまま採用したものである。
//
// 【重要】ssif/Ssif5Driver.h（生レジスタ版）は削除していない。
// ポータビリティ要件（mbed-os非依存）を将来また優先したくなった場合の
// 足がかりとして残してある。ただし現時点でSsif5Driver.h側は実機での
// 音出し確認ができていない（SSIFSR.TDEビット位置の修正は別チャットの
// 検証より前に行ったもので、その後の実機再検証はまだ）。
// 現在main.cppはこちら(Pcm5102Ssif0Output, SSIF0)をデフォルトで使用する。
//
// 【NC_BSSについて（今回追加、重要）】
// R_BSP_SsifはDMAで直接メモリを読みに行くため、CPUキャッシュに書いた
// 最新データが実メモリに反映されないまま(ライトバック前に)DMAが読みに
// 行ってしまう問題が起きる。これを避けるため、オーディオバッファは
// 非キャッシュ領域"NC_BSS"セクションにグローバル配列として配置する
// 必要がある（クラスメンバ変数にはセクション属性を個別指定できない
// ため、グローバル配列にせざるを得ない）。
//
// 【ピン割り当て: SSIF channel 0（動作実績あり）】
//   SCK(ビットクロック) = P4_4 = Arduino D5
//   WS (LR/ワードクロック) = P4_5 = Arduino D4
//   TxD(シリアルデータ出力) = P4_7 = Arduino D2
//   RxDは使用しない(PCM5102は出力専用DACのため不要、NCを渡す)
//
// 【クロック設定】
// 44100Hz固定（GR-PEACHのオーディオ用水晶(22.5792MHz)から整数分周できる
// のはこの系列のみ。48000Hzは非対応、詳細はdsp/SynthConfig.hのコメント参照）。

#pragma once
#include "mbed.h"
#include "rtos.h"
#include "../hal/AudioOutputHAL.h"
#include "R_BSP_Ssif.h"

namespace synth {

namespace pcm5102_ssif0_detail {

constexpr int kBlockFrames = 64;  // dsp/SynthConfig.h の kBlockSize と合わせる
constexpr int kNumBuffers  = 4;   // リングバッファ面数（アンダーラン対策）

// 【重要】NC_BSS配置。クラスメンバではなく名前空間スコープのグローバル配列にする。
__attribute__((section("NC_BSS")))
inline int16_t g_audioBuffers[kNumBuffers][kBlockFrames * 2]; // interleaved stereo

} // namespace pcm5102_ssif0_detail

class Pcm5102Ssif0Output : public AudioOutputHAL {
public:
    Pcm5102Ssif0Output()
        : ssif_(P4_4 /*SCK=D5*/, P4_5 /*WS=D4*/, P4_7 /*TxD=D2*/, NC /*RxD未使用*/, NC /*audio_clk未使用、AUDIO_X1直結*/),
          audioThread_(osPriorityHigh, kAudioThreadStackSize) {
    }

    bool init(uint32_t sampleRateHz) override {
        if (sampleRateHz != 44100) {
            printf("[Pcm5102Ssif0Output] ERROR: sampleRateHz must be 44100 "
                   "(requested %lu). See dsp/SynthConfig.h comments.\r\n",
                   static_cast<unsigned long>(sampleRateHz));
            return false;
        }
        if (ssif_.GetSsifChNo() != 0) {
            printf("[Pcm5102Ssif0Output] ERROR: expected SSIF channel 0, "
                   "got %ld. Check pin wiring / PinMap.\r\n",
                   static_cast<long>(ssif_.GetSsifChNo()));
            return false;
        }

        ssif_channel_cfg_t cfg;
        cfg.enabled                = true;
        cfg.int_level               = 0x80;
        cfg.slave_mode              = false; // RZ/A1H側がクロックマスター（PCM5102はスレーブ専用のため必須）
        cfg.sample_freq             = 44100u;
        cfg.clk_select              = SSIF_CFG_CKS_AUDIO_X1; // オンボード22.5792MHz水晶を直接使用
        cfg.multi_ch                = SSIF_CFG_MULTI_CH_1;
        cfg.data_word               = SSIF_CFG_DATA_WORD_16; // メモリ上は16bit PCM
        cfg.system_word              = SSIF_CFG_SYSTEM_WORD_32; // ワイヤ上は32bitスロット（標準I2S）
        cfg.bclk_pol                = SSIF_CFG_FALLING;
        cfg.ws_pol                  = SSIF_CFG_WS_LOW;
        cfg.padding_pol             = SSIF_CFG_PADDING_LOW;
        cfg.serial_alignment        = SSIF_CFG_DATA_FIRST;
        cfg.parallel_alignment      = SSIF_CFG_LEFT;
        cfg.ws_delay                = SSIF_CFG_DELAY; // 1クロック遅延=標準I2Sフォーマット(PCM5102のデフォルトFMT)
        cfg.noise_cancel            = SSIF_CFG_ENABLE_NOISE_CANCEL;
        cfg.tdm_mode                = SSIF_CFG_DISABLE_TDM;
        cfg.romdec_direct.mode      = SSIF_CFG_DISABLE_ROMDEC_DIRECT;
        cfg.romdec_direct.p_cbfunc  = NULL;

        ssif_.init(&cfg, pcm5102_ssif0_detail::kNumBuffers, 0 /* 再生専用のため読み込みバッファ不要 */);
        return true;
    }

    void start(AudioFillCallback callback) override {
        fillCallback_ = callback;
        running_ = true;
        audioThread_.start(mbed::callback(this, &Pcm5102Ssif0Output::audioThreadLoop));
    }

    void stop() override {
        running_ = false;
        audioThread_.join();
    }

private:
    static const uint32_t kAudioThreadStackSize = 8192; // フルDSPエンジンのコールスタック分に余裕を持たせる

    // 専用スレッドからwrite()を単純にループ呼び出しするだけ。
    // write()自体がドライバ内部のキュー空き待ち(セマフォ)で自然にブロック
    // するため、これだけで正しくパイプライン動作する
    // （Renesas公式EasyPlaybackと同じ使い方。p_notify_funcはNULLでよい。
    //  コールバックから次のwrite()を呼ぶと自己デッドロックになるため厳禁。
    //  詳細はcodec/Pcm5102Ssif.h(別プロジェクト)で既に踏んだ経緯を参照）。
    //
    // 【今回追加】ボイス数を増やすとノイズが乗るという実機報告を受け、
    // fillCallback_()（=SynthEngine::processBlock()、DSP処理本体）に
    // かかる実時間を計測し、1ブロック分のリアルタイム予算
    // (kBlockFrames/サンプルレート、64フレーム@44100Hzなら約1.45ms)を
    // 超えていないか一定間隔でシリアルに出力するようにした。
    // 超えている場合、CPU処理がリアルタイム再生に追いつけていないこと
    // （ノイズの直接原因）が実測で確認できる。
    void audioThreadLoop() {
        rbsp_data_conf_t conf;
        conf.p_notify_func = NULL;
        conf.p_app_data = NULL;

        const uint32_t kBudgetUs =
            (pcm5102_ssif0_detail::kBlockFrames * 1000000u) / 44100u; // ≈1451us

        Timer timer;
        uint32_t maxUs = 0;
        uint64_t sumUs = 0;
        uint32_t sampleCount = 0;
        uint32_t overBudgetCount = 0;
        uint32_t reportCounter = 0;

        int idx = 0;
        while (running_) {
            timer.reset();
            timer.start();
            int16_t* buf = pcm5102_ssif0_detail::g_audioBuffers[idx];
            fillCallback_(buf, pcm5102_ssif0_detail::kBlockFrames);
            timer.stop();
            uint32_t elapsedUs = static_cast<uint32_t>(timer.elapsed_time().count());

            if (elapsedUs > maxUs) maxUs = elapsedUs;
            sumUs += elapsedUs;
            ++sampleCount;
            if (elapsedUs > kBudgetUs) ++overBudgetCount;

            ssif_.write(buf, sizeof(pcm5102_ssif0_detail::g_audioBuffers[idx]), &conf);
            idx = (idx + 1) % pcm5102_ssif0_detail::kNumBuffers;

            // 約2秒(44100Hz/64フレーム毎ブロックなので約1378ブロック)ごとに統計を出力
            if (++reportCounter >= 1378) {
                reportCounter = 0;
                printf("[AudioTiming] budget=%luus avg=%luus max=%luus over_budget=%lu/%lu\r\n",
                       static_cast<unsigned long>(kBudgetUs),
                       static_cast<unsigned long>(sumUs / sampleCount),
                       static_cast<unsigned long>(maxUs),
                       static_cast<unsigned long>(overBudgetCount),
                       static_cast<unsigned long>(sampleCount));
                maxUs = 0;
                sumUs = 0;
                sampleCount = 0;
                overBudgetCount = 0;
            }
        }
    }

    R_BSP_Ssif ssif_;
    Thread audioThread_;
    AudioFillCallback fillCallback_;
    volatile bool running_ = false;
};

} // namespace synth
