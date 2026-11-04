//Controlled A50E entry with actual shared/local metadata; no campaign ancestry claim.
#include "stellar7/gameplay.hpp"
#include "playable_startup_resources.hpp"
#ifdef STELLAR7_ENHANCED
#include "failure_page.hpp"
#include "world_presentation.hpp"
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {if(!condition) {throw std::runtime_error(message);}}
std::vector<stellar7::Model3d> models(const std::filesystem::path& root,const char* name) {
    std::ifstream file(root/name,std::ios::binary);require(bool(file),"missing model resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}
}
namespace stellar7 {
struct GameplayModelTestAccess {
    static void snapshot(GameplayModel& model,Level1Entity actor) {
        model.level2PresentationEntities_={actor};model.level2PresentationActiveListOrder_={0};
    }
    static std::unique_ptr<GameplayModel> minion(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
        Level1Entity actor{};actor.type=47;actor.position={0,16000,0};
        actor.collisionRadius=campaignModelRadius(shared,local,47);actor.stateBytes4dTo52={255,1,0,0,4,0};
        actor.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
        actor.animatedModelPresenterEnabled=false;
        auto model=std::make_unique<GameplayModel>();model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(2,{actor},StellarRng{},shared,local);model->completeLevel3Preloop();
        model->level2ActiveListOrder_=model->level2AllocationListOrder_;return model;
    }
    static std::unique_ptr<GameplayModel> entry(const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local,const std::uint8_t behavior) {
        Level1Entity guardian{};guardian.type=44;guardian.position={0,16000,behavior==1?500:2100};
        guardian.collisionRadius=campaignModelRadius(shared,local,44);
        guardian.stateBytes4dTo52={255,20,2,0,5,behavior};guardian.behaviorTimer=behavior==1?20:0;
        guardian.animatedModelPresenterEnabled=behavior==0;
        guardian.renderProfile=behavior==0?WorldActorRenderProfile::heapSelectorDeferredAnimation:WorldActorRenderProfile::heapSelectorNoCallback;
        auto model=std::make_unique<GameplayModel>();model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(2,{guardian},StellarRng{},shared,local);
        model->completeLevel3Preloop();model->level2ActiveListOrder_=model->level2AllocationListOrder_;
        return model;
    }
};
}
int main(int argc,char** argv) {
    if(argc<2||argc>3) {return 2;}
    try {
        const auto shared=models(argv[1],"COCKPIT.RES"),local=models(argv[1],"LEVEL3.RES");
        const bool selectorOnly=argc==3;
        if(!selectorOnly) {
            const auto assets=naturalRoute::assets(argv[1]);
            auto minion=stellar7::GameplayModelTestAccess::minion(shared,local);
            for(unsigned frame=0;frame<12;++frame) {
                minion->advancePlayableCampaign({});
                const auto& actor=minion->level2Entities().front();
                std::cout<<"minion frame="<<frame<<" native="<<unsigned(actor.stateBytes4dTo52[2])
                    <<" enhanced="<<unsigned(actor.presentationModelAnimationVariant)<<'\n';
                require(actor.stateBytes4dTo52[2]==((frame+1)&3),"enhanced gait changed original four-pose selector");
                require(actor.presentationModelAnimationVariant==((frame+1)&7),"enhanced minion did not retain its full eight-pose gait");
#ifdef STELLAR7_ENHANCED
                if(frame==3) {
                    const auto original=actor;auto partial=original;partial.presentationModelAnimationVariant=0;
                    stellar7::GameplayModelTestAccess::snapshot(*minion,partial);
                    const auto nativePartial=stellar7::renderPlayableLevel2ViewportIndexed(assets,*minion);
                    stellar7::IndexedScreen enhancedPartial;
                    {stellar7::WorldPresentationCapture capture({},stellar7::PresentationGeometry::spatial);enhancedPartial=stellar7::renderPlayableLevel2ViewportIndexed(assets,*minion);}
                    stellar7::GameplayModelTestAccess::snapshot(*minion,original);
                    const auto nativeFull=stellar7::renderPlayableLevel2ViewportIndexed(assets,*minion);
                    stellar7::IndexedScreen enhancedFull;
                    {stellar7::WorldPresentationCapture capture({},stellar7::PresentationGeometry::spatial);enhancedFull=stellar7::renderPlayableLevel2ViewportIndexed(assets,*minion);}
                    require(nativePartial.pixels==nativeFull.pixels,"enhanced gait changed faithful indexed animation");
                    const auto& a=enhancedPartial.presentation->chase->spatial->surfaces;
                    const auto& b=enhancedFull.presentation->chase->spatial->surfaces;bool moved=false;
                    for(std::size_t i=0;i<std::min(a.size(),b.size());++i) {
                        if(a[i].type!=47||b[i].type!=47) {continue;}
                        for(std::size_t j=0;j<std::min(a[i].points.size(),b[i].points.size());++j) {
                            moved|=a[i].points[j].x!=b[i].points[j].x||a[i].points[j].y!=b[i].points[j].y||a[i].points[j].z!=b[i].points[j].z;
                        }
                    }
                    require(moved,"enhanced renderer ignored the second half of minion gait");
                    std::cout<<"enhanced minion mesh and faithful indexed comparison passed\n";
                }
#endif
                minion->finishLevel3Radar();minion->finishPlayableCampaignFrame();
            }
        }
        for(const auto state:{std::uint8_t{0},std::uint8_t{1}}) {
            if(selectorOnly&&state==0) {continue;}
            auto model=stellar7::GameplayModelTestAccess::entry(shared,local,state);
            for(unsigned frame=0;frame<4;++frame) {
                model->advancePlayableCampaign({});
                const auto& actor=model->level2Entities().front();
                require(!actor.animatedModelPresenterEnabled
                    &&actor.renderProfile==stellar7::WorldActorRenderProfile::heapSelectorNoCallback,
                    "A50E retained generic130C animation callback");
                const auto expected=std::max(0,5-std::abs(int(actor.behaviorTimer)/2-5));
                require(actor.modelAnimationVariant==expected&&actor.presentationModelAnimationVariant==expected,
                    "A50E arm selector was overwritten by generic animation");
                model->finishLevel3Radar();
                require(model->level2Entities().front().modelAnimationVariant==expected,"radar advanced A50E arm selector");
                model->finishPlayableCampaignFrame();
            }
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
