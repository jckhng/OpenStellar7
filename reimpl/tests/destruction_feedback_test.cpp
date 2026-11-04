#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include "world_presentation.hpp"
#include "chase_view.hpp"
#include "destruction_audio.hpp"
#include "stellar7/audio_direct_work.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
void ppm(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels) {
    std::ofstream file(path,std::ios::binary);file<<"P6\n1280 800\n255\n";
    for(const auto color:pixels) {const char bytes[]{char(color>>16),char(color>>8),char(color)};file.write(bytes,3);}
    requireCampaignComparison(bool(file),"cannot write destruction preview");
}
void visibility(const std::filesystem::path& root,const std::filesystem::path& output) {
    auto source=test::makeDestructionSession(root,0,test::DestructionTarget::ordinary,7000,1000,test::DestructionPool::full);
    auto enhanced=test::makeDestructionSession(root,0,test::DestructionTarget::ordinary,7000,1000,test::DestructionPool::full);
    test::CampaignBudgetComparison comparison;std::size_t deaths=0;
    for(unsigned step=0;step<16;++step) {
        const auto a=source->advancePlayableCampaign({},10000);
        NativeSessionDelivery b;
        {WorldPresentationCapture capture;b=enhanced->advancePlayableCampaign({},257);}
        comparison.compare(*source,*enhanced,a,b,test::CampaignComparisonStage::boundary);
        deaths+=std::count_if(b.events.begin(),b.events.end(),[](const auto& event) {return event.eventId==6;});
        if(step==0) {
            requireCampaignComparison(enhanced->model().destructions().size()==1,"lethal hit omitted its retained death observation");
            const auto& state=enhanced->synth().driverState();
            requireCampaignComparison(std::any_of(state.directTimers.begin(),state.directTimers.end(),[](auto timer) {return timer!=0;}),
                "original death event did not reach the driver's effect voices");
        }
        if(step==5) {
            const auto& frame=enhanced->frame();const auto image=renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase);
            const auto view=chaseCamera(enhanced->model().level2Player(),enhanced->model().level2PresentationEntities());
            const auto point=projectToViewport(transformToView({2000,7000,1000},view),view);
            unsigned fire=0;
            for(int y=point.y*4-36;y<=point.y*4+36;++y) {
                for(int x=point.x*4-36;x<=point.x*4+36;++x) {
                    if(x<0||x>=1280||y<0||y>=800) {continue;}
                    const auto color=image[y*1280+x];
                    if(((color>>16)&255)>180&&((color>>8)&255)>100&&(color&255)<150) {++fire;}
                }
            }
            if(!output.empty()) {std::filesystem::create_directories(output);ppm(output/"retained-destruction.ppm",image);}
            requireCampaignComparison(fire>12,"confirmed kill lost its chase destruction cue after legacy pool exhaustion/retirement");
            const auto ticks=enhanced->model().ticks();
            for(unsigned repeat=0;repeat<5;++repeat) {
                requireCampaignComparison(renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase)==image,
                    "display refresh advanced the destruction effect");
            }
            requireCampaignComparison(enhanced->model().ticks()==ticks,"destruction drawing advanced the world");
        }
    }
    requireCampaignComparison(enhanced->model().destructions().empty(),"retained destruction did not expire");
    requireCampaignComparison(deaths==1,"confirmed kill omitted or repeated its original death sound event");
    const auto& hit=enhanced->model().combatFeedback().hit;
    requireCampaignComparison(hit&&hit->amount>0&&!hit->active(0,enhanced->model().ticks()),
        "confirmed primary hit feedback was omitted or did not expire");
    std::cout<<"Confirmed kill with occupied legacy pool: retained cue and one original audio event; source twins agree\n";
}
void wav(const std::filesystem::path& path,const std::vector<std::int16_t>& samples) {
    std::ofstream file(path,std::ios::binary);
    const auto word=[&](std::uint32_t value,unsigned bytes) {for(unsigned i=0;i<bytes;++i) {file.put(char(value>>(i*8)));}};
    file.write("RIFF",4);word(36+static_cast<std::uint32_t>(samples.size()*2),4);file.write("WAVEfmt ",8);word(16,4);
    word(1,2);word(1,2);word(SoundBlasterSynth::sampleRate,4);word(SoundBlasterSynth::sampleRate*2,4);word(2,2);word(16,2);
    file.write("data",4);word(static_cast<std::uint32_t>(samples.size()*2),4);
    for(auto sample:samples) {word(static_cast<std::uint16_t>(sample),2);}
    requireCampaignComparison(bool(file),"cannot write enhanced destruction audio preview");
}
void variants(const std::filesystem::path& root,const std::filesystem::path& output) {
    for(const auto kind:{test::DestructionTarget::ordinary,test::DestructionTarget::guardian}) {
        for(const int distance:{3500,18000}) {
            auto session=test::makeDestructionSession(root,0,kind,distance,1000,test::DestructionPool::available);
            DestructionAudio mixer;std::uint64_t id=0;std::size_t changed=0;std::vector<std::int16_t> pcm;
            for(unsigned frame=0;frame<13;++frame) {
                const auto first=session->elapsedSamples();
                NativeSessionDelivery delivery;{WorldPresentationCapture capture;delivery=session->advancePlayableCampaign({});}
                if(frame==0) {
                    const auto& cues=session->model().destructions();requireCampaignComparison(cues.size()==1,"near/far/guardian hit omitted a death cue");
                    id=cues.front().id;
                    const auto expected=kind==test::DestructionTarget::guardian?12:6;
                    requireCampaignComparison(cues.front().event==expected&&std::count_if(delivery.events.begin(),delivery.events.end(),[&](auto event) {
                        return event.eventId==expected;
                    })==1,"kill's original sound ID/order disagrees with its death observation");
                }
                const auto copy=delivery.samples;const auto mixed=mixer.mix(delivery,DestructionSound::enabled);
                pcm.insert(pcm.end(),mixed.begin(),mixed.end());
                requireCampaignComparison(copy==delivery.samples,"enhanced mix changed retained native samples");
                for(std::size_t i=0;i<mixed.size();++i) {changed+=mixed[i]!=copy[i];}
                if(!output.empty()&&distance==3500) {
                    const auto folder=output/(kind==test::DestructionTarget::guardian?"guardian-frames":"ordinary-frames");
                    std::filesystem::create_directories(folder);
                    ppm(folder/(std::to_string(100+frame).substr(1)+".ppm"),renderWorldPresentation(session->frame(),4,nullptr,0,EnhancedView::chase));
                }
                if(frame==5) {
                    const auto& scene=*session->frame().presentation->chase;
                    std::size_t lastFire=0,firstHull=scene.draws.size();
                    for(std::size_t i=0;i<scene.draws.size();++i) {
                        const auto& face=scene.draws[i].face;
                        if(!face||!face->identity) {continue;}
                        if(face->identity->group==std::numeric_limits<std::size_t>::max()-2) {lastFire=i;}
                        if(face->identity->allocation==std::numeric_limits<std::size_t>::max()
                            &&face->identity->group<std::numeric_limits<std::size_t>::max()-2) {firstHull=std::min(firstHull,i);}
                    }
                    requireCampaignComparison(lastFire>0&&lastFire<firstHull,"destruction was absent or overlaid the nearer Raven");
                    requireCampaignComparison(session->model().destructions().front().id==id,"effect-slot reuse changed the retained death identity");
                    if(!output.empty()) {
                        ppm(output/(std::string(kind==test::DestructionTarget::guardian?"guardian-":"ordinary-")+std::to_string(distance)+".ppm"),
                            renderWorldPresentation(session->frame(),4,nullptr,0,EnhancedView::chase));
                    }
                }
                const auto paced=session->pacePlayable(first+SoundBlasterSynth::sampleRate/35);
                const auto tail=mixer.mix(paced,DestructionSound::enabled);pcm.insert(pcm.end(),tail.begin(),tail.end());
            }
            if(!output.empty()&&distance==3500) {wav(output/(kind==test::DestructionTarget::guardian?"guardian.wav":"ordinary.wav"),pcm);}
            requireCampaignComparison(changed>100,"confirmed destruction had no enhanced audio accent");
            requireCampaignComparison(session->model().destructions().empty(),"death cue persisted past its lifetime");
        }
    }
}
void rapid(const std::filesystem::path& root) {
    auto a=test::makeDestructionSession(root,1,test::DestructionTarget::ordinary,7000,0,test::DestructionPool::available);
    auto b=test::makeDestructionSession(root,1,test::DestructionTarget::ordinary,7000,0,test::DestructionPool::available);
    test::CampaignBudgetComparison compare;
    for(unsigned kill=0;kill<3;++kill) {
        if(kill!=0) {
            test::prepareNextDestruction(*a,test::DestructionTarget::ordinary,7000+int(kill)*1500,0);
            test::prepareNextDestruction(*b,test::DestructionTarget::ordinary,7000+int(kill)*1500,0);
        }
        const auto x=a->advancePlayableCampaign({},10000);NativeSessionDelivery y;
        {WorldPresentationCapture capture;y=b->advancePlayableCampaign({},257);}
        compare.compare(*a,*b,x,y,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(b->model().destructions().size()==kill+1,"rapid kills discarded or duplicated a retained destruction");
    }
    // A full recovered effect bank selects a replacement voice, rather than dropping the call.
    auto incoming=b->synth().callbackWorkState();incoming.voices.directTimers={1,100,100};
    const auto replaced=soundBlasterPlayableDirectAdvance(incoming,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayPrimaryFire);
    requireCampaignComparison(replaced.state.voices.directTimers[0]==gameplayPrimaryFireDirectRecord[2]
        &&replaced.state.voices.directTimers[1]==100&&replaced.state.voices.directTimers[2]==100,
        "recovered three-voice replacement policy changed");
    std::cout<<"Near/far, airborne/ground, guardian, rapid deaths, painter order and recovered audio replacement pass\n";
}
void nonlethal(const std::filesystem::path& root) {
    auto session=test::makeDestructionSession(root,0,test::DestructionTarget::survivor,7000,1000,test::DestructionPool::available);
    DestructionAudio mixer;
    for(unsigned frame=0;frame<3;++frame) {
        NativeSessionDelivery delivery;{WorldPresentationCapture capture;delivery=session->advancePlayableCampaign({});}
        requireCampaignComparison(session->model().destructions().empty(),"nonlethal hit emitted a destruction");
        requireCampaignComparison(mixer.mix(delivery,DestructionSound::enabled)==delivery.samples,"nonlethal hit emitted death audio");
        if(frame==0) {
            const auto& hit=session->model().combatFeedback().hit;
            requireCampaignComparison(hit&&hit->amount==1&&hit->active(0,session->model().ticks()),
                "nonlethal primary hit omitted its confirmed hit marker");
            requireCampaignComparison(std::any_of(delivery.events.begin(),delivery.events.end(),[](auto event) {return event.eventId==5;}),
                "nonlethal control did not hit its target");
        }
    }
}
}
int main(int argc,char** argv) {
    if(argc<2||argc>3) {return 2;}
    try {
        const auto output=argc==3?std::filesystem::u8path(argv[2]):std::filesystem::path{};
        visibility(argv[1],output);variants(argv[1],output);rapid(argv[1]);nonlethal(argv[1]);
    }
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
