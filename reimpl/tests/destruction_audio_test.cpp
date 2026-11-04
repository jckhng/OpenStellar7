#include "destruction_audio.hpp"
#include "world_presentation.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
NativeSessionDelivery sequence() {
    NativeSessionDelivery delivery;delivery.samples.assign(24000,0);
    for(std::uint64_t id=1;id<=6;++id) {
        auto scene=std::make_shared<WorldPresentation>();scene->worldTick=id;
        scene->destructions={{id,id,0,{0,7000,1000},id==6?std::int16_t{44}:std::int16_t{1},1000,6}};
        IndexedScreen frame{{}, {},scene};delivery.presentations.push_back({100+id*100,std::move(frame)});
    }
    return delivery;
}
}
int main() {
    try {
        auto delivery=sequence();const auto original=delivery.samples;
        DestructionAudio full,partitioned,muted;
        const auto expected=full.mix(delivery,DestructionSound::enabled);
        require(delivery.samples==original,"enhanced audio mutated source PCM");
        require(std::count_if(expected.begin(),expected.end(),[](auto sample) {return sample!=0;})>1000,"destruction accent was inaudible");
        require(std::all_of(expected.begin(),expected.begin()+200,[](auto value) {return value==0;}),"death audio preceded its presentation timestamp");
        std::vector<std::int16_t> actual;
        for(std::size_t start=0;start<delivery.samples.size();start+=257) {
            const auto end=std::min(start+257,delivery.samples.size());NativeSessionDelivery part;part.firstSample=start;
            part.samples.assign(original.begin()+start,original.begin()+end);
            for(const auto& mark:delivery.presentations) {if(mark.sampleEnd>=start&&mark.sampleEnd<=end) {part.presentations.push_back(mark);}}
            const auto mixed=partitioned.mix(part,DestructionSound::enabled);actual.insert(actual.end(),mixed.begin(),mixed.end());
        }
        require(actual==expected,"audio accents depended on delivery chunk size or voice-limit scheduling");
        require(muted.mix(delivery,DestructionSound::muted)==original,"Sounds Off leaked enhanced audio");
        NativeSessionDelivery later;later.firstSample=original.size();later.samples.assign(4000,0);
        auto repeat=delivery.presentations.back();repeat.sampleEnd=later.firstSample;later.presentations={repeat};
        require(full.mix(later,DestructionSound::enabled)==later.samples,"repeated publication retriggered destruction audio");
        require(muted.mix(later,DestructionSound::enabled)==later.samples,"unmuting replayed an old death");
        full.reset();require(full.mix(delivery,DestructionSound::enabled)==expected,"new game did not reset enhanced audio identity");
        std::cout<<"Sample-timed death accents: overlap, voice cap, chunk invariance, mute, deduplication, reset and source PCM pass\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
