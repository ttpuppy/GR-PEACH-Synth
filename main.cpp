// main.cpp
// GR-PEACH（RZ/A1H単体）を「音生成・音出し専任」としたフェーズ4エントリーポイント。
// パネル入力(エンコーダー/OLED/タクトスイッチ)は持たない。
//
// 構成:
//   - オーディオ出力: PCM5102（I2S）をSSIF0経由・Renesas公式R_BSP_Ssif
//     ドライバ(DMAベース)で駆動する（codec/Pcm5102Ssif0Output.h）。
//     【今回変更】当初はSSIF5を生レジスタで直接叩く実装(ssif/Ssif5Driver.h)
//     を使っていたが、別チャット「GY-5102へのSSIF音声出力検証」で
//     SSIF0+R_BSP_Ssifの組み合わせでの実機音出しが確認されたため、
//     動作実績のあるこちらに切り替えた。ssif/Ssif5Driver.h自体は
//     mbed-os非依存というポータビリティ上のメリットがあるため削除せず
//     残してあるが、現時点では未使用（README.md参照）。
//   - MIDI入力: USB0をUSB-MIDIデバイスとして使用（usb/UsbMidiInput.h）
//   - USER_BUTTON0: MIDI入力なしで音声パスを確認するためのテストノート再生
//   - オンボードLED: P6_12(USER_LED)はハートビート、P6_13-15は用途未定
//     （ユーザー仕様どおり、本コードでは初期化のみ行い消灯のままにしている）
//
// 音色パラメータは dsp/SoundDefaults.h のdefineを編集して再ビルドすることで
// 調整する（パネルUIを持たないため）。
//
// 【重要】ハードウェアに触れるオブジェクトはポインタ化してmain()内で1つずつ
// 生成し、board::traceInitBegin/traceInitOk でシリアルに進捗を出す
// （過去に発生した "pinmap not found" のデバッグ手法を踏襲）。

#include "mbed.h"
#include "dsp/SynthEngine.h"
#include "dsp/SoundDefaults.h"
#include "codec/Pcm5102Ssif0Output.h"
#include "usb/UsbMidiInput.h"
#include "board/BoardConfig.h"
#include "board/InitTrace.h"

using namespace synth;

static SynthEngine gEngine; // ハードウェア非依存

static Pcm5102Ssif0Output* gAudio = nullptr;
static UsbMidiInput* gUsbMidi = nullptr;
static DigitalOut* gLedUser = nullptr;
static DigitalOut* gLedRed = nullptr;
static DigitalOut* gLedGreen = nullptr;
static DigitalOut* gLedBlue = nullptr;
static InterruptIn* gUserButton = nullptr;

static volatile bool gTestNoteRequested = false;
static volatile bool gMidiActivityPending = false;

static void audioFillCallback(int16_t* interleavedStereo, size_t frames) {
    gEngine.processBlock(interleavedStereo, frames);
}

static void setupUsbMidi() {
    gUsbMidi->onNoteOn([](uint8_t /*ch*/, uint8_t note, uint8_t vel) {
        gEngine.noteOn(note, static_cast<float>(vel) / 127.0f);
        gMidiActivityPending = true;
    });
    gUsbMidi->onNoteOff([](uint8_t /*ch*/, uint8_t note) {
        gEngine.noteOff(note);
    });
    gUsbMidi->onCc([](uint8_t /*ch*/, uint8_t controller, uint8_t value) {
        if (controller == 1) { // モジュレーションホイール
            gEngine.setModWheel(static_cast<float>(value) / 127.0f);
        }
    });
    gUsbMidi->onProgramChange([](uint8_t /*ch*/, uint8_t program) {
        gEngine.selectPatch(program % PatchBank::kNumPresets);
    });
}

// USER_BUTTON0割り込みハンドラ: フラグを立てるだけにして、実際のノート再生は
// メインループ側で行う（割り込みコンテキストでの重い処理を避けるため）。
static void onUserButtonPressed() {
    gTestNoteRequested = true;
}

int main() {
    printf("\r\n=== GR-PEACH Analog Modeling Synth - Phase 4 (Audio-only, SSIF0/PCM5102) ===\r\n");
    printf("sample rate = %lu Hz, voices = %d\r\n",
           static_cast<unsigned long>(kSampleRateHz), kNumVoices);

    gEngine.init();
    printf("[init] SynthEngine OK (no hardware pins used)\r\n");

    // ---- ここから1つずつハードウェアペリフェラルを生成する ----
    // エラーで停止した場合、直前に出力された [init] 行が原因のペリフェラルを示す。

    board::traceInitBegin("Pcm5102Ssif0Output (SSIF0, R_BSP_Ssif, DMAベース)");
    printf("       pins: P4_4=SCK(D5), P4_5=WS(D4), P4_7=TxD(D2)\r\n");
    printf("             (別チャットで実機の音出し確認済みの構成)\r\n");
    gAudio = new Pcm5102Ssif0Output();
    board::traceInitOk("Pcm5102Ssif0Output object");

    board::traceInitBegin("UsbMidiInput (USB0, USBMIDI)");
    gUsbMidi = new UsbMidiInput();
    board::traceInitOk("UsbMidiInput");

    board::traceInitBegin("Onboard LEDs (P6_12-15)");
    board::tracePinValue("kLedUser",  board::kLedUser);
    board::tracePinValue("kLedRed",   board::kLedRed);
    board::tracePinValue("kLedGreen", board::kLedGreen);
    board::tracePinValue("kLedBlue",  board::kLedBlue);
    gLedUser = new DigitalOut(board::kLedUser, 0);
    gLedRed = new DigitalOut(board::kLedRed, 0);     // 用途未定、消灯のまま
    gLedGreen = new DigitalOut(board::kLedGreen, 0); // 用途未定、消灯のまま
    gLedBlue = new DigitalOut(board::kLedBlue, 0);   // 用途未定、消灯のまま
    board::traceInitOk("Onboard LEDs");

    board::traceInitBegin("USER_BUTTON0 (test note trigger)");
    board::tracePinValue("kUserButton0", board::kUserButton0);
    gUserButton = new InterruptIn(board::kUserButton0);
    gUserButton->fall(&onUserButtonPressed); // アクティブLow前提、要実機確認
    board::traceInitOk("USER_BUTTON0");

    printf("[init] all peripherals created successfully.\r\n");

    // ---- ソフトウェア初期化 ----
    setupUsbMidi();

    bool audioOk = gAudio->init(kSampleRateHz);
    if (!audioOk) {
        printf("[WARN] SSIF0 init failed. Likely cause: kSampleRateHz must be\r\n");
        printf("       44100 (see dsp/SynthConfig.h) - or SSIF channel resolution\r\n");
        printf("       mismatch (check pin wiring). Continuing without audio output\r\n");
        printf("       (USB MIDI / test button still work).\r\n");
    } else {
        gAudio->start(audioFillCallback);
        printf("[OK] SSIF0 audio streaming started (R_BSP_Ssif, DMA-based,\r\n");
        printf("     verified working configuration).\r\n");
    }

    uint32_t tick = 0;
    while (true) {
        gUsbMidi->poll(); // USBMIDIは割り込み駆動のため実質no-op

        if (gTestNoteRequested) {
            gTestNoteRequested = false;
            printf("[button] test note ON (note=%d, vel=%d)\r\n",
                   SYNTH_TEST_NOTE_MIDI_NUMBER, SYNTH_TEST_NOTE_VELOCITY);
            gEngine.noteOn(SYNTH_TEST_NOTE_MIDI_NUMBER,
                            static_cast<float>(SYNTH_TEST_NOTE_VELOCITY) / 127.0f);
            ThisThread::sleep_for(std::chrono::milliseconds(SYNTH_TEST_NOTE_DURATION_MS));
            gEngine.noteOff(SYNTH_TEST_NOTE_MIDI_NUMBER);
        }

        if (gMidiActivityPending) {
            gMidiActivityPending = false;
            // LED_BLUEは用途未定のため、ここでは光らせず何もしない。
            // 将来「MIDI入力時に点滅」に使う場合はここで *gLedBlue を操作する。
        }

        // 注記: Pcm5102Ssif0Outputは専用オーディオスレッドがwrite()を
        // ループ呼び出しして連続再生するため、メインループ側からの
        // 周期的なポンプ呼び出しは不要（過去にpumpOnce()方式だった名残の
        // 削除。SSIF5生レジスタ版のポーリング実装と違い、こちらは
        // R_BSP_Ssifの内部キュー機構が自然にパイプライン動作する）。

        // 【今回追加】オーディオスレッドがpublishした計測統計をここでprintfする。
        // （printfをオーディオスレッド内で行うと約145msのブロッキング
        //   グリッチが発生することが過去の検証で確認済みのため、出力は
        //   必ずメインループ側で行う。codec/Pcm5102Ssif0Output.h参照）
        {
            Pcm5102Ssif0Output::AudioTimingStats ts;
            if (gAudio->fetchTimingStats(ts)) {
                printf("[AudioTiming] budget=%luus avg=%luus max=%luus over_budget=%lu/%lu\r\n",
                       static_cast<unsigned long>(ts.budgetUs),
                       static_cast<unsigned long>(ts.avgUs),
                       static_cast<unsigned long>(ts.maxUs),
                       static_cast<unsigned long>(ts.overBudget),
                       static_cast<unsigned long>(ts.blocks));
            }
        }

        if ((tick++ % 500) == 0) {
            *gLedUser = !*gLedUser;
        }
        ThisThread::sleep_for(1ms);
    }
}
