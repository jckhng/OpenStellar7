#include "native_level2_frame_fixture.hpp"
#include "stellar7/blackout_work.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
using namespace stellar7;
void same(const NativeGameplaySession& a,const NativeGameplaySession& b,
    const NativeSessionDelivery& left,const NativeSessionDelivery& right) {
    assert(left.firstSample==right.firstSample && left.samples==right.samples);
    assert(left.presentations.size()==right.presentations.size());
    const auto frame=[](const IndexedScreen& x,const IndexedScreen& y) {
        assert(x.pixels==y.pixels && x.palette.size()==y.palette.size());
        for (std::size_t index=0;index<x.palette.size();++index) {
            assert(x.palette[index].red==y.palette[index].red && x.palette[index].green==y.palette[index].green
                && x.palette[index].blue==y.palette[index].blue);
        }
    };
    for (std::size_t index=0;index<left.presentations.size();++index) {
        assert(left.presentations[index].sampleEnd>=left.firstSample
            && left.presentations[index].sampleEnd<=left.firstSample+left.samples.size());
        if (index!=0) {assert(left.presentations[index].sampleEnd>=left.presentations[index-1].sampleEnd);}
        assert(left.presentations[index].sampleEnd==right.presentations[index].sampleEnd);
        frame(left.presentations[index].indexed,right.presentations[index].indexed);
    }
    frame(a.frame(),b.frame());
    for (const auto role:{NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {
        assert(a.retainedPagePixels(role)==b.retainedPagePixels(role));
    }
    assert(a.model().phase()==b.model().phase() && a.model().ticks()==b.model().ticks());
    assert(a.model().presentationFrame()==b.model().presentationFrame() && a.elapsedSamples()==b.elapsedSamples());
    assert(a.synth().driverState().normalClockCycles==b.synth().driverState().normalClockCycles);
    const auto& oplA=a.synth().registerWrites();const auto& oplB=b.synth().registerWrites();
    assert(oplA.size()==oplB.size());
    for (std::size_t index=0;index<oplA.size();++index) {
        assert(oplA[index].registerNumber==oplB[index].registerNumber && oplA[index].value==oplB[index].value);
    }
    const auto voiceA=a.synth().sampledVoiceObservation(),voiceB=b.synth().sampledVoiceObservation();
    assert(voiceA.phase==voiceB.phase && voiceA.transferred==voiceB.transferred && voiceA.clockPhase==voiceB.clockPhase
        && voiceA.dacValue==voiceB.dacValue && voiceA.speakerEnabled==voiceB.speakerEnabled);
}
void run(const std::filesystem::path& resources) {
    auto a=test::makePlayableEndingSession(resources),b=test::makePlayableEndingSession(resources);
    const auto initialSamples=a->elapsedSamples(),initialCycles=a->synth().driverState().normalClockCycles;
    const auto rng=a->model().level2Rng();
    bool loaded=false,playing=false,teardown=false;
    double maximumAdvanceMilliseconds=0;
    for (unsigned step=0;step<50;++step) {
        const auto before=a->model().phase();
        const auto frontBefore=before==GameplayPhase::endingTeardown ? a->frame().pixels : std::vector<std::uint8_t>{};
        NativeSessionDelivery left,right;
        try {
            const auto started=std::chrono::steady_clock::now();
            left=a->advancePlayableCampaign({},10000);
            maximumAdvanceMilliseconds=std::max(maximumAdvanceMilliseconds,
                std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
            right=b->advancePlayableCampaign({},257);
        } catch (const std::exception&) {
            std::cerr<<"ending step="<<step<<" phase="<<static_cast<int>(a->model().phase())
                <<" page="<<a->model().presentationFrame()<<'\n';
            throw;
        }
        same(*a,*b,left,right);
        if (before==GameplayPhase::endingTeardown) {
            assert(left.presentations.size()>=2*blackoutPaletteSteps);
            for (std::size_t index=0;index<blackoutPaletteSteps;++index) {
                assert(left.presentations[index].indexed.pixels==frontBefore);
            }
            std::size_t distinct=0;auto prior=left.firstSample;
            for (const auto& shown:left.presentations) {
                if (shown.sampleEnd>prior) {++distinct;}
                prior=shown.sampleEnd;
            }
            assert(distinct>=2*blackoutPaletteSteps);
        }
        const auto voice=a->synth().sampledVoiceObservation();
        loaded=loaded || voice.phase==SampledVoicePhase::loaded;
        playing=playing || (voice.phase==SampledVoicePhase::playing && voice.transferred>0);
        teardown=teardown || a->model().phase()==GameplayPhase::endingTeardown;
        if (a->model().phase()!=GameplayPhase::endingReturn) {continue;}
        assert(step==45 && loaded && playing && teardown);
        assert(a->model().campaignLevel()==7 && a->model().presentationFrame()==endingScenePageCount+1);
        assert(a->elapsedSamples()>initialSamples && a->synth().driverState().normalClockCycles>initialCycles);
        assert(a->synth().driverState().clockSource==SoundBlasterClockSource::normalWork);
        assert(voice.phase==SampledVoicePhase::unloaded);
        auto expected=rng,actual=a->model().level2Rng();
        assert(expected.calls()==actual.calls());
        for (std::size_t index=0;index<StellarRng::wordCount;++index) {assert(expected.next()==actual.next());}
        const auto front=a->retainedPagePixels(NativePageRole::front);
        assert(front && std::all_of(front->begin(),front->end(),[](const auto pixel) {return pixel==0;}));
        std::cout<<"Controlled actual final-bank entry -> SCENED0..43/VOICE/two fades -> endingReturn; calls="<<step+1
            <<" samples="<<a->elapsedSamples()-initialSamples<<" max_advance_ms="<<maximumAdvanceMilliseconds<<'\n';
        const auto advance=[&](const GameplayInput& input=GameplayInput{}) {
            const auto x=a->advancePlayableCampaign(input,10000),y=b->advancePlayableCampaign(input,257);
            same(*a,*b,x,y);
        };
        advance();assert(a->model().phase()==GameplayPhase::nativeMenu);
        advance();
        GameplayInput confirm{};confirm.confirm=true;
        advance(confirm);advance();
        assert(a->model().phase()==GameplayPhase::playableLoaderReturn && a->model().campaignLevel()==0);
        advance();advance();
        GameplayInput moving{};moving.forward=true;moving.left=true;moving.fire=true;
        advance(moving);advance(moving);
        assert(a->model().phase()==GameplayPhase::playable && a->model().level2LoopTick()==3);
        assert(a->model().playerArmor()==680 && a->model().score()==0);
        std::cout<<"Retained endingReturn -> title/released Begin/480 actual IRQs -> actual Level1 +3 PASS\n";
        return;
    }
    throw std::runtime_error("playable ending did not reach its retained return");
}
}
int main(int argc,char** argv) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    if (argc!=2) {return 2;}
    try {run(argv[1]);return 0;}
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
