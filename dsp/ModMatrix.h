// ModMatrix.h
// multi/polyの159x35モジュレーションマトリクスは本ボード規模では非現実的なため、
// 検討記録5章の方針どおり「積み上げ」の現実的な範囲として、
// 固定ソース x 固定デスティネーションの簡易マトリクス（数スロット）で実装する。

#pragma once
#include "SynthConfig.h"
#include <array>

namespace synth {

enum class ModSource : uint8_t {
    None = 0,
    Lfo1,
    Lfo2,
    FilterEg,
    AmpEg,
    Velocity,
    ModWheel,
    Count
};

enum class ModDest : uint8_t {
    None = 0,
    Osc1Pitch,
    Osc2Pitch,
    Osc1PulseWidth,
    FilterCutoff,
    FilterResonance,
    Osc2CrossMod,
    Amp,
    Count
};

struct ModSlot {
    ModSource source = ModSource::None;
    ModDest dest = ModDest::None;
    float amount = 0.0f; // -1..1
};

constexpr int kNumModSlots = 8;

class ModMatrix {
public:
    ModSlot slots[kNumModSlots];

    // sourceValues はそのサンプル/ブロックで計算済みの各ソース値（-1..1程度）を渡す
    float getModulation(ModDest dest, const std::array<float, static_cast<size_t>(ModSource::Count)>& sourceValues) const {
        float sum = 0.0f;
        for (const auto& s : slots) {
            if (s.dest == dest && s.source != ModSource::None) {
                sum += sourceValues[static_cast<size_t>(s.source)] * s.amount;
            }
        }
        return sum;
    }
};

} // namespace synth
