#include "native_level2_frame_fixture.hpp"
#include "world_presentation.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
using namespace stellar7;
void ppm(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels) {
    std::ofstream out(path,std::ios::binary);out<<"P6\n1280 800\n255\n";
    for(auto pixel:pixels) {const char bytes[]{static_cast<char>(pixel>>16),static_cast<char>(pixel>>8),static_cast<char>(pixel)};out.write(bytes,3);}
    assert(out.good());
}
}
int main(int argc,char** argv) {
    assert(argc==3);using namespace stellar7;
    const auto begin=std::chrono::steady_clock::now();
    auto source=test::makePlayableLevel2Session(std::filesystem::u8path(argv[1]));
    auto enhanced=test::makePlayableLevel2Session(std::filesystem::u8path(argv[1]));
    std::size_t faces=0,subpixelFaces=0,qualifiedFaces=0;IndexedScreen previous;
    for(unsigned tick=0;tick<3;++tick) {
        GameplayInput input{};input.left=true;input.fire=tick==1;
        const auto a=source->advancePlayableCampaign(input,10000);
        NativeSessionDelivery b;
        {WorldPresentationCapture capture;b=enhanced->advancePlayableCampaign(input,10000);}
        assert(a.firstSample==b.firstSample&&a.samples==b.samples&&a.events.size()==b.events.size());
        for(std::size_t i=0;i<a.events.size();++i) {assert(a.events[i].loopTick==b.events[i].loopTick&&a.events[i].eventId==b.events[i].eventId);}
        assert(a.presentations.size()==b.presentations.size());
        for(std::size_t i=0;i<a.presentations.size();++i) {
            assert(a.presentations[i].sampleEnd==b.presentations[i].sampleEnd
                &&a.presentations[i].indexed.pixels==b.presentations[i].indexed.pixels
                &&a.presentations[i].indexed.palette==b.presentations[i].indexed.palette);
        }
        assert(source->frame().pixels==enhanced->frame().pixels&&source->frame().palette==enhanced->frame().palette);
        assert(!source->frame().presentation&&enhanced->frame().presentation);
        const auto& scene=*enhanced->frame().presentation;
        for(const auto& draw:scene.draws) {
            if(!draw.face||!draw.face->identity) {continue;}
            const auto& identity=*draw.face->identity;
            assert(identity.incarnation);
            bool matched=false;
            const auto& actors=enhanced->model().level2PresentationEntities();
            for(std::size_t index=0;index<actors.size();++index) {
                matched=matched||(enhanced->model().level2PresentationPoolSlot(index)==identity.allocation
                    &&enhanced->model().level2PresentationActorIncarnation(index)==identity.incarnation
                    &&actors[index].type==identity.type);
            }
            assert(matched);++qualifiedFaces;
        }
        for(const auto& draw:scene.draws) {if(draw.face) {++faces;for(auto point:draw.face->points) {
            if(point.x!=static_cast<int>(point.x)||point.y!=static_cast<int>(point.y)) {++subpixelFaces;break;}
        }}}
        auto rngA=source->model().level2Rng(),rngB=enhanced->model().level2Rng();
        assert(rngA.calls()==rngB.calls());for(std::size_t i=0;i<StellarRng::wordCount;++i) {assert(rngA.next()==rngB.next());}
        assert(source->model().level2LoopTick()==enhanced->model().level2LoopTick()
            &&source->model().score()==enhanced->model().score()&&source->model().playerArmor()==enhanced->model().playerArmor());
        const auto actor=[](const Level1Entity& a) {
            return std::tie(a.allocationIndex,a.flags,a.type,a.position.x,a.position.y,a.position.z,
                a.pitch,a.roll,a.yaw,a.pitchVelocity,a.rollVelocity,a.yawVelocity,a.xVelocity,a.yVelocity,a.zVelocity,
                a.forwardVelocity,a.stateBytes4dTo52,a.behaviorTimer,a.modelAnimationVariant,a.weaponSlotTimers);
        };
        assert(actor(source->model().level2Player())==actor(enhanced->model().level2Player()));
        const auto& actorsA=source->model().level2Entities();const auto& actorsB=enhanced->model().level2Entities();
        assert(actorsA.size()==actorsB.size());
        for(std::size_t i=0;i<actorsA.size();++i) {assert(actor(actorsA[i])==actor(actorsB[i]));}
        for(const auto role:{NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {assert(source->retainedPagePixels(role)==enhanced->retainedPagePixels(role));}
        const auto& writesA=source->synth().registerWrites();const auto& writesB=enhanced->synth().registerWrites();
        assert(writesA.size()==writesB.size());for(std::size_t i=0;i<writesA.size();++i) {
            assert(writesA[i].registerNumber==writesB[i].registerNumber&&writesA[i].value==writesB[i].value);
        }
        previous=enhanced->frame();
    }
    assert(faces>0&&subpixelFaces>0&&qualifiedFaces>0);
    const auto renderBegin=std::chrono::steady_clock::now();std::uint64_t checksum=0;
    for(unsigned i=0;i<120;++i) {const auto pixels=renderWorldPresentation(previous,4);checksum+=pixels[i];}
    const auto renderMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderBegin).count()/120;
    const auto output=std::filesystem::u8path(argv[2]);
    ppm(output/"enhanced.ppm",renderWorldPresentation(previous,4));
    auto classic=previous;classic.presentation.reset();ppm(output/"classic.ppm",renderWorldPresentation(classic,4));
    std::cout<<"Retained3frames: indexed/palette, samples, ordered events, OPL, RNG, pages unchanged; faces="<<faces
        <<" subpixel_faces="<<subpixelFaces<<" render4x_ms="<<renderMs<<" checksum="<<checksum
        <<" total_seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<'\n';
}
