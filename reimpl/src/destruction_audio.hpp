#pragma once
#include "stellar7/native_session.hpp"

namespace stellar7 {
enum class DestructionSound {enabled,muted};
// Enhanced host mix only; the recovered synthesizer and its PCM remain unchanged.
class DestructionAudio {
public:
    std::vector<std::int16_t> mix(const NativeSessionDelivery& delivery,DestructionSound sound);
    void reset();
private:
    struct Voice {std::uint64_t id,start,length;double gain;};
    std::vector<Voice> voices_;
    std::uint64_t lastSerial_=0,lastTick_=0;
};
}
