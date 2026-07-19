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

// 【今回追加】DSP処理時間のGPIOプローブ。
// fillCallback_(=SynthEngine::processBlock)の実行中だけこのピンをHighにする。
// Analog Discovery 2のデジタルchで観測すれば、High時間=1ブロックの生成時間を
// 直接計測でき、リアルタイム予算(64フレーム@44.1kHz=約1.451ms)に対する
// 余裕がボイス数によってどう変わるかをスパイク発生と同時相関で確認できる。
// 計測が終わったらこのdefineをコメントアウトすればプローブごと消える。
// 注意: D2/D4/D5はSSIF0が使用中のため使用不可。D6で"pinmap not found"等が
// 出る場合は他の空きデジタルピンに変更すること（InitTraceのログで切り分け可能）。
#define AUDIO_TIMING_PROBE_PIN D6

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

    // 【今回追加】オーディオスレッドが約2秒ごとに publish する計測統計。
    // メインループ側から fetchTimingStats() で取得し、printfはそちらで行う
    // （printfをオーディオスレッドに置いてはならない。上記コメント参照）。
    struct AudioTimingStats {
        uint32_t budgetUs;    // 1ブロックのリアルタイム予算(us)
        uint32_t avgUs;       // 期間内の平均fill時間(us)
        uint32_t maxUs;       // 期間内の最大fill時間(us)
        uint32_t overBudget;  // 予算超過したブロック数
        uint32_t blocks;      // 期間内の総ブロック数
    };

    // 新しい統計ウィンドウが確定していればoutにコピーしてtrueを返す。
    // 単一writer(オーディオスレッド)/単一reader(メインループ)前提の
    // 簡易フラグ同期。ウィンドウは約2秒、読み出しは1msポーリングなので
    // 実用上の競合は無視できる（診断用途）。
    bool fetchTimingStats(AudioTimingStats& out) {
        if (!statsReady_) return false;
        out = publishedStats_;
        statsReady_ = false;
        return true;
    }

private:
    static const uint32_t kAudioThreadStackSize = 8192; // フルDSPエンジンのコールスタック分に余裕を持たせる

    // 専用スレッドからwrite()をループ呼び出しする。
    //
    // 【今回のバグ修正（ポリフォニックノイズの根本原因）】
    // 従来は「write()がドライバ内部のキュー空き待ちで自然にブロックする
    // ため、fill→writeの単純ループで正しくパイプライン動作する」と
    // 考えていたが、これは誤りだった。R_BSP_Aio.cpp の実装を確認した
    // ところ:
    //   - write(buf, size, &conf) は conf!=NULL の時点で非同期パス
    //     (aio_trans)に入り、DMA要求をゼロコピーでキューに積んで
    //     「即リターン」する（ブロックするのはmax_write_num本が
    //     転送中のときだけ）
    //   - 内部セマフォが返却されるのは各バッファの「DMA転送完了時」
    // このため定常状態では、write(buf[N-1])が返った直後の fill(buf[0])
    // が「DMAがまさに読んでいる最中の最古バッファ」を上書きしてしまい、
    // 1ブロックにつき1個の継ぎ目不連続（実機スパイクノイズ）が発生する。
    // 隣接ブロック間の波形差が大きいほど段差が大きくなるため、
    // ボイス数を増やすとノイズが顕著になる（実測と一致）。
    //
    // 修正: 各writeに完了通知コールバック(p_notify_func)を設定し、
    // 「空きバッファ数」を表すカウンティングセマフォ(bufFreeSem_)を
    // fillの前にacquireする。完了は投入順(FIFO)に発生するため、
    // acquire成功 = これから書くバッファのDMAが完了済み、が保証される。
    // コールバック内ではセマフォreleaseのみ行う（「コールバックから
    // 次のwrite()を呼ぶと自己デッドロック」の既知ルールは維持。
    // 詳細はcodec/Pcm5102Ssif.h(別プロジェクト)の経緯を参照）。
    //
    // 【今回追加】ボイス数を増やすとノイズが乗るという実機報告を受け、
    // fillCallback_()（=SynthEngine::processBlock()、DSP処理本体）に
    // かかる実時間を計測し、1ブロック分のリアルタイム予算
    // (kBlockFrames/サンプルレート、64フレーム@44100Hzなら約1.45ms)を
    // 超えていないか確認できるようにした。
    // 超えている場合、CPU処理がリアルタイム再生に追いつけていないこと
    // （ノイズの直接原因）が実測で確認できる。
    //
    // 【今回修正】統計のprintfをオーディオスレッド内から排除した。
    // 過去の検証で「audio threadでのprintfは約145msのブロッキング
    // グリッチを起こす」ことが確認済みであり、スレッド内printfでは
    // 2秒ごとに自らノイズを注入しながらノイズを測ることになるため。
    // 統計はpublishedStats_に書き出すだけにして、main.cppのメイン
    // ループがfetchTimingStats()で取得してprintfする方式に変更。
    void audioThreadLoop() {
        rbsp_data_conf_t conf;
        conf.p_notify_func = &Pcm5102Ssif0Output::onWriteComplete;
        conf.p_app_data = this;

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
            // これから書き込むバッファのDMA完了を待つ（上記コメント参照）。
            // acquire待ち時間はDSP処理時間ではないため、タイマー/プローブの
            // 計測区間には含めない。
            bufFreeSem_.acquire();

            timer.reset();
            timer.start();
            int16_t* buf = pcm5102_ssif0_detail::g_audioBuffers[idx];
#ifdef AUDIO_TIMING_PROBE_PIN
            timingProbe_.write(1); // High区間 = DSP処理時間（AD2デジタルchで観測）
#endif
            fillCallback_(buf, pcm5102_ssif0_detail::kBlockFrames);
#ifdef AUDIO_TIMING_PROBE_PIN
            timingProbe_.write(0);
#endif
            timer.stop();
            uint32_t elapsedUs = static_cast<uint32_t>(timer.elapsed_time().count());

            if (elapsedUs > maxUs) maxUs = elapsedUs;
            sumUs += elapsedUs;
            ++sampleCount;
            if (elapsedUs > kBudgetUs) ++overBudgetCount;

            ssif_.write(buf, sizeof(pcm5102_ssif0_detail::g_audioBuffers[idx]), &conf);
            idx = (idx + 1) % pcm5102_ssif0_detail::kNumBuffers;

            // 約2秒(44100Hz/64フレーム毎ブロックなので約1378ブロック)ごとに
            // 統計を確定してpublishする（printfはメインループ側で行う）。
            if (++reportCounter >= 1378) {
                reportCounter = 0;
                publishedStats_.budgetUs   = kBudgetUs;
                publishedStats_.avgUs      = static_cast<uint32_t>(sumUs / sampleCount);
                publishedStats_.maxUs      = maxUs;
                publishedStats_.overBudget = overBudgetCount;
                publishedStats_.blocks     = sampleCount;
                statsReady_ = true;
                maxUs = 0;
                sumUs = 0;
                sampleCount = 0;
                overBudgetCount = 0;
            }
        }
    }

    // DMA転送完了ごとに「空きバッファ」トークンを1つ返却する。
    // ドライバはSIGEV_THREAD相当のコンテキストからこれを呼ぶ。
    // ここでは絶対にwrite()を呼ばないこと（自己デッドロック）。
    static void onWriteComplete(void* /*p_data*/, int32_t /*result*/, void* p_app_data) {
        static_cast<Pcm5102Ssif0Output*>(p_app_data)->bufFreeSem_.release();
    }

    R_BSP_Ssif ssif_;
    Thread audioThread_;
    AudioFillCallback fillCallback_;
    volatile bool running_ = false;

    // 空きバッファ数を表すカウンティングセマフォ。初期値=全バッファ空き。
    // fill前にacquire、DMA完了コールバックでrelease。
    rtos::Semaphore bufFreeSem_{pcm5102_ssif0_detail::kNumBuffers,
                                pcm5102_ssif0_detail::kNumBuffers};

#ifdef AUDIO_TIMING_PROBE_PIN
    DigitalOut timingProbe_{AUDIO_TIMING_PROBE_PIN, 0};
#endif
    AudioTimingStats publishedStats_ = {};
    volatile bool statsReady_ = false;
};

} // namespace synth
