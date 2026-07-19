// Pcm5102Ssif5Output.h
// PCM5102（I2S DAC）をSSIF5経由で駆動するAudioOutputHAL実装。
// PCM5102自体はI2Cレジスタを持たないため、ここでの初期化はSSIF5側の
// I2S信号生成のみで完結する（PCM5102モジュール側のFMT/DEMP/XSMT/FLT等の
// ハードウェアピンは、多くの市販モジュールで基板上プルアップ/GND固定済み。
// 使用するモジュールの実配線を確認すること）。

#pragma once
#include "../hal/AudioOutputHAL.h"
#include "../ssif/Ssif5Driver.h"
#include "../dsp/SynthConfig.h"

namespace synth {

class Pcm5102Ssif5Output : public AudioOutputHAL {
public:
    bool init(uint32_t sampleRateHz) override {
        return driver_.init(sampleRateHz);
    }

    void start(AudioFillCallback callback) override {
        fillCallback_ = callback;
        running_ = true;
    }

    void stop() override { running_ = false; }

    // メインループから周期的に呼び出す。1ブロック分（kBlockSize フレーム）を
    // 生成してポーリング送信する。ブロックサイズは dsp/SynthConfig.h の
    // kBlockSize（デフォルト64）に従う。
    void pumpOnce() {
        if (!running_ || !fillCallback_ || !driver_.isReady()) return;
        int16_t block[kBlockSize * 2];
        fillCallback_(block, kBlockSize);
        driver_.writeBlockPolling(block, kBlockSize);
    }

    bool isReady() const { return driver_.isReady(); }

private:
    ssif::Ssif5Driver driver_;
    AudioFillCallback fillCallback_;
    bool running_ = false;
};

} // namespace synth
