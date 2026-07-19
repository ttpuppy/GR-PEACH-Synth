// Arpeggiator.h
// フェーズ3「アルペジエーター、簡易シーケンサ」の最小実装。
// 保持中のノートをUp/Down/UpDownの順で自動再生する。

#pragma once
#include "SynthConfig.h"
#include <vector>
#include <algorithm>

namespace synth {

enum class ArpMode : uint8_t { Off, Up, Down, UpDown };

class Arpeggiator {
public:
    void setMode(ArpMode m) { mode_ = m; stepIndex_ = 0; }
    void setRateHz(float hz) { stepIntervalSamples_ = static_cast<uint32_t>(kSampleRateHzF / std::max(0.1f, hz)); }

    void heldNoteOn(int note) {
        if (std::find(held_.begin(), held_.end(), note) == held_.end()) held_.push_back(note);
        std::sort(held_.begin(), held_.end());
    }
    void heldNoteOff(int note) {
        held_.erase(std::remove(held_.begin(), held_.end(), note), held_.end());
    }

    // 呼び出し側(オーディオコールバックまたは制御レートタスク)から毎サンプル/毎ブロック呼ぶ。
    // ノートオン/オフが発生したフレームで true を返し、note に発音すべきノートをセットする。
    // triggeredOff が true の場合は直前のアルペジオノートをオフにすべきタイミング。
    bool tick(int& noteOut, bool& gateOn) {
        if (mode_ == ArpMode::Off || held_.empty()) return false;

        if (sampleCounter_ == 0) {
            buildSequenceIfNeeded();
            if (!sequence_.empty()) {
                if (gatePhase_) {
                    noteOut = sequence_[stepIndex_];
                    gateOn = true;
                    gatePhase_ = false;
                    sampleCounter_ = static_cast<uint32_t>(stepIntervalSamples_ * 0.6f); // ゲート長60%
                } else {
                    noteOut = sequence_[stepIndex_];
                    gateOn = false;
                    gatePhase_ = true;
                    stepIndex_ = (stepIndex_ + 1) % sequence_.size();
                    sampleCounter_ = static_cast<uint32_t>(stepIntervalSamples_ * 0.4f);
                }
                return true;
            }
        }
        sampleCounter_ = sampleCounter_ > 0 ? sampleCounter_ - 1 : 0;
        return false;
    }

private:
    void buildSequenceIfNeeded() {
        sequence_.clear();
        if (held_.empty()) return;
        switch (mode_) {
            case ArpMode::Up:
                sequence_ = held_;
                break;
            case ArpMode::Down:
                sequence_.assign(held_.rbegin(), held_.rend());
                break;
            case ArpMode::UpDown:
                sequence_ = held_;
                if (held_.size() > 2) {
                    for (size_t i = held_.size() - 2; i >= 1; --i) sequence_.push_back(held_[i]);
                }
                break;
            default: break;
        }
        if (stepIndex_ >= sequence_.size()) stepIndex_ = 0;
    }

    ArpMode mode_ = ArpMode::Off;
    std::vector<int> held_;
    std::vector<int> sequence_;
    size_t stepIndex_ = 0;
    uint32_t stepIntervalSamples_ = kSampleRateHzF / 8.0f;
    uint32_t sampleCounter_ = 0;
    bool gatePhase_ = true;
};

} // namespace synth
