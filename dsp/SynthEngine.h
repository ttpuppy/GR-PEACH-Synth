// SynthEngine.h
// VoiceAllocator + EffectsChain + PatchBank + Arpeggiator を束ねるトップレベルクラス。
// MIDI/パネル入力からのイベントをここで受け取り、オーディオコールバックから
// processBlock() を呼び出してPCMを生成する。

#pragma once
#include "SynthConfig.h"
#include "VoiceAllocator.h"
#include "Effects.h"
#include "Patch.h"
#include "Arpeggiator.h"

namespace synth {

class SynthEngine {
public:
    void init() {
        voices_.setSampleRate(kSampleRateHzF);
        effects_.init();
        patchBank_.initDefaults();
        selectPatch(0);
        arp_.setRateHz(8.0f);
    }

    void selectPatch(int index) {
        currentPatchIndex_ = index;
        const Patch& p = patchBank_.at(index);
        voices_.applyPatch(p);
        effects_.applyPatch(p);
    }

    Patch& currentPatch() { return patchBank_.at(currentPatchIndex_); }

    // MIDI/鍵盤からのイベント
    void noteOn(int note, float velocity01) {
        if (arpMode_ != ArpMode::Off) {
            arp_.heldNoteOn(note);
        } else {
            voices_.noteOn(note, velocity01);
        }
    }
    void noteOff(int note) {
        if (arpMode_ != ArpMode::Off) {
            arp_.heldNoteOff(note);
        } else {
            voices_.noteOff(note);
        }
    }

    void setModWheel(float v01) { modWheel_ = clampf(v01, 0.0f, 1.0f); }

    void setArpMode(ArpMode m) {
        arpMode_ = m;
        arp_.setMode(m);
        if (m == ArpMode::Off) voices_.allNotesOff();
    }
    void setArpRateHz(float hz) { arp_.setRateHz(hz); }
    bool arpModeIsOn() const { return arpMode_ != ArpMode::Off; }

    // オーディオコールバックから1サンプルごとに呼ぶ（ブロック単位で呼ぶ場合はループで包む）
    void processSample(float& outL, float& outR) {
        if (arpMode_ != ArpMode::Off) {
            int note; bool gate;
            if (arp_.tick(note, gate)) {
                if (gate) voices_.noteOn(note, 0.9f);
                else voices_.noteOff(note);
            }
        }

        float mono = voices_.processMix(modWheel_);
        float dryL = mono, dryR = mono;
        float wetL, wetR;
        effects_.process(dryL, dryR, wetL, wetR);

        float master = currentPatch().masterVolume;
        // 【今回追加】ボイスミックスの正規化(VoiceAllocator::processMix参照)を
        // 修正したことで通常はクリップしないはずだが、フィルターの自己発振や
        // ディレイ/コーラスのフィードバックによる一時的なオーバーシュートに
        // 備え、ハードクリップ(clampf)ではなくtanhによる緩やかなソフト
        // クリップに変更した。ハードクリップは倍音を大量に追加して
        // 耳障りな歪みになりやすいが、ソフトクリップは音楽的に自然な
        // 飽和感に近づく。
        outL = std::tanh(wetL * master);
        outR = std::tanh(wetR * master);
    }

    // ブロック単位でint16_tインターリーブPCMバッファを埋める（AudioOutputHAL用）
    void processBlock(int16_t* interleavedStereo, size_t frames) {
        for (size_t i = 0; i < frames; ++i) {
            float l, r;
            processSample(l, r);
            interleavedStereo[i * 2 + 0] = static_cast<int16_t>(l * 32767.0f);
            interleavedStereo[i * 2 + 1] = static_cast<int16_t>(r * 32767.0f);
        }
    }

private:
    VoiceAllocator voices_;
    EffectsChain effects_;
    PatchBank patchBank_;
    Arpeggiator arp_;
    ArpMode arpMode_ = ArpMode::Off;
    int currentPatchIndex_ = 0;
    float modWheel_ = 0.0f;
};

} // namespace synth
