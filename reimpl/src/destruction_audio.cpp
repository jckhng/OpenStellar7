#include "destruction_audio.hpp"
#include "world_presentation.hpp"
#include <algorithm>
#include <cmath>

namespace stellar7 {
void DestructionAudio::reset() {voices_.clear();lastSerial_=lastTick_=0;}
std::vector<std::int16_t> DestructionAudio::mix(const NativeSessionDelivery& delivery,const DestructionSound sound) {
    constexpr std::size_t maximumVoices=4;
    constexpr double circle=6.283185307179586;
    constexpr auto rate=SoundBlasterSynth::sampleRate;
    for(const auto& frame:delivery.presentations) {
        if(!frame.indexed.presentation) {continue;}
        const auto& scene=*frame.indexed.presentation;
        if(scene.worldTick<lastTick_) {reset();}
        lastTick_=scene.worldTick;
        for(const auto& cue:scene.destructions) {
            if(cue.id<=lastSerial_) {continue;}
            lastSerial_=cue.id;
            if(sound==DestructionSound::muted||scene.worldTick!=cue.born) {continue;}
            const bool guardian=cue.type==44||cue.type==45;
            voices_.push_back({cue.id,frame.sampleEnd,static_cast<std::uint64_t>(rate*(guardian?0.45:0.28)),
                guardian?4600.0:2600.0});
        }
    }
    auto output=delivery.samples;
    if(sound==DestructionSound::muted) {voices_.clear();return output;}
    for(std::size_t sample=0;sample<output.size();++sample) {
        const auto cursor=delivery.firstSample+sample;double accent=0;
        // Apply voice limits at the event timestamp, independent of delivery chunk size.
        while(std::count_if(voices_.begin(),voices_.end(),[&](const auto& voice) {
            return voice.start<=cursor&&cursor-voice.start<voice.length;
        })>maximumVoices) {
            const auto oldest=std::find_if(voices_.begin(),voices_.end(),[&](const auto& voice) {
                return voice.start<=cursor&&cursor-voice.start<voice.length;
            });
            voices_.erase(oldest);
        }
        for(const auto& voice:voices_) {
            if(cursor<voice.start||cursor-voice.start>=voice.length) {continue;}
            const auto offset=cursor-voice.start;
            auto hash=static_cast<std::uint32_t>(offset)^static_cast<std::uint32_t>(voice.id*0x9e3779b9U);
            hash^=hash>>16;hash*=0x7feb352dU;hash^=hash>>15;
            const auto noise=(hash&65535)/32767.5-1;
            const auto seconds=double(offset)/rate;
            const auto attack=std::min(1.0,seconds/0.003);
            const auto envelope=attack*std::exp(-seconds*12)*(1-double(offset)/voice.length);
            const auto boom=std::sin(circle*(90*seconds-70*seconds*seconds));
            accent+=voice.gain*envelope*(boom*0.65+noise*0.35);
        }
        // Preserve source samples when idle and leave headroom for its loudest passages.
        const auto headroom=1-std::abs(int(output[sample]))/32768.0;
        output[sample]=static_cast<std::int16_t>(std::clamp(std::lround(output[sample]+accent*headroom),-32768L,32767L));
    }
    const auto end=delivery.firstSample+delivery.samples.size();
    voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[&](const auto& voice) {
        return end>=voice.start&&end-voice.start>=voice.length;
    }),voices_.end());
    return output;
}
}
