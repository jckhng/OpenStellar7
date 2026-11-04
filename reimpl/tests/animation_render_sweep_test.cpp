// Controlled source callback/presenter entries; no natural campaign ancestry claim.
#include "stellar7/gameplay.hpp"
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "campaign_guardian.hpp"
#include "failure_page.hpp"
#include "world_presentation.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
}
namespace stellar7 {
struct GameplayModelTestAccess {
    static std::unique_ptr<GameplayModel> entry(const GameplayAssets& assets,unsigned bank,std::int16_t type) {
        constexpr std::size_t sharedModelCount=44;
        const std::vector<Model3d> shared(assets.models.begin(),assets.models.begin()+sharedModelCount);
        Level1Entity actor{};actor.type=type;actor.position={0,12000,500};
        actor.collisionRadius=campaignModelRadius(shared,assets.campaignLevels[bank].models,type);
        actor.stateBytes4dTo52={255,100,2,0,5,0};
        if(type==24) {actor.stateBytes4dTo52[0]=0xfc;}
        actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
        actor.animatedModelPresenterEnabled=true;
        auto model=std::make_unique<GameplayModel>();model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(static_cast<std::uint8_t>(bank),{actor},StellarRng{},shared,assets.campaignLevels[bank].models);
        if(bank==2) {model->completeLevel3Preloop();} else {model->completePlayableCampaignPreloop();}
        model->level2ActiveListOrder_=model->level2AllocationListOrder_;
        model->phase_=GameplayPhase::playable;
        return model;
    }
    static Level1Entity& actor(GameplayModel& model) {return model.level2Entities_.front();}
    static void callback(GameplayModel& model) {
        if(actor(model).type==24) {model.advanceLevel3Animation();} else {model.advanceLevel2GuardianAnimation();}
    }
    static void main(GameplayModel& model) {model.presentLevel2({},nullptr,false);}
    static void snapshot(GameplayModel& model) {model.snapshotLevel2Presentation({});}
};
}
namespace {
using namespace stellar7;
const Model3d& resource(const GameplayAssets& assets,unsigned bank,std::int16_t type) {
    return type<44?assets.models.at(type):assets.campaignLevels.at(bank).models.at(type-44);
}
void visibleSelectors(const GameplayAssets& assets,GameplayModel& model,const Model3d& mesh) {
    unsigned observed=0;
    std::array<bool,retainedAnimationSelectorCapacity> seen{};
    for(const std::uint16_t yaw:{0,0x2000,0x4000,0x6000}) {
        auto& actor=GameplayModelTestAccess::actor(model);actor.yaw=yaw;
        GameplayModelTestAccess::snapshot(model);
        const auto before=actor;const auto rng=model.level2Rng().calls();
        IndexedScreen indexed;
        {WorldPresentationCapture capture({},PresentationGeometry::spatial);indexed=renderPlayableLevel2ViewportIndexed(assets,model);}
        require(indexed.presentation&&indexed.presentation->chase,"selector survey lacks both presentation paths");
        for(const auto* scene:{indexed.presentation.get(),indexed.presentation->chase.get()}) {
            for(const auto& draw:scene->draws) {
                if(!draw.face||!draw.face->identity||draw.face->identity->type!=actor.type) {continue;}
                const auto& id=*draw.face->identity;
                if(id.group>=std::numeric_limits<std::size_t>::max()-1) {continue;}
                const auto lod=std::find_if(mesh.lods.begin(),mesh.lods.end(),[&](const auto& value) {return value.threshold==id.lod;});
                require(lod!=mesh.lods.end()&&id.group<lod->groups.size(),"selector survey lost original group identity");
                const auto& group=lod->groups[id.group];
                if(group.animationOffset<0) {continue;}
                const auto offset=static_cast<std::size_t>(group.animationOffset);
                const auto selected=offset==0?actor.presentationModelAnimationVariant:actor.presentationModelAnimationTail.at(offset-1);
                const auto expected=selected<group.primitives.size()?selected:0;
                require(id.variant==expected,"rendered group ignored its retained heap selector");seen.at(offset)=true;++observed;
            }
        }
        require(test::campaignActorFields(before)==test::campaignActorFields(actor)&&rng==model.level2Rng().calls(),
            "render observation advanced animation or RNG");
    }
    require(observed>0,"selector survey did not draw any animated original group");
    for(unsigned i=0;i<mesh.flags;++i) {require(seen.at(i),"selector survey omitted a selector-bearing group");}
}
void contactSelectors(const GameplayAssets& assets) {
    auto model=GameplayModelTestAccess::entry(assets,3,44);
    auto& actor=GameplayModelTestAccess::actor(*model);
    actor.modelAnimationVariant=actor.presentationModelAnimationVariant=3;
    actor.modelAnimationTail[0]=actor.presentationModelAnimationTail[0]=3;
    actor.behaviorTimer=1;actor.collisionContactIsPlayer=true;
    CampaignGuardianContext context;context.contact=CampaignGuardianContact{0,0,true};
    CampaignGuardianCallbacks callbacks;int damage=0;callbacks.changePlayerArmor=[&](std::int16_t value) {damage+=value;};
    std::cout<<"contact entry type="<<actor.type<<" radius="<<actor.collisionRadius<<" category="<<unsigned(actor.stateBytes4dTo52[0])
        <<" class="<<unsigned(actor.stateBytes4dTo52[4])<<" behavior="<<unsigned(actor.stateBytes4dTo52[5])<<'\n';
    StellarRng rng;advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,callbacks);
    require(damage==-100&&actor.modelAnimationVariant==0&&actor.modelAnimationTail[0]==3&&!actor.animatedModelPresenterEnabled,
        "Level4 contact reset a neighboring selector or retained its callback");
    GameplayModelTestAccess::main(*model);
    require(actor.presentationModelAnimationTail[0]==3,"main display lost the paused secondary selector");
    visibleSelectors(assets,*model,resource(assets,3,44));
    std::cout<<"Level4 first-selector contact reset preserves secondary pose in main/chase\n";
}
void pausedSelectors(const GameplayAssets& assets) {
    auto model=GameplayModelTestAccess::entry(assets,1,45);
    auto& actor=GameplayModelTestAccess::actor(*model);
    actor.modelAnimationVariant=actor.presentationModelAnimationVariant=3;
    actor.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;actor.animatedModelPresenterEnabled=false;
    GameplayModelTestAccess::main(*model);
    require(actor.modelAnimationVariant==3,"paused guardian advanced or reset its heap");
    visibleSelectors(assets,*model,resource(assets,1,45));
    std::cout<<"Paused Type45 retains its selected geometry\n";
}
void cadence(const GameplayAssets& assets) {
    unsigned entries=0,geometryEntries=0;
    for(unsigned bank=0;bank<7;++bank) {
        std::vector<std::int16_t> types{11,12};
        if(bank==2||bank==5) {types.push_back(24);}
        if(!assets.campaignLevels[bank].models.empty()&&assets.campaignLevels[bank].models.front().flags) {types.push_back(44);}
        if(bank==1) {types.push_back(45);}
        for(const auto type:types) {
            const auto& mesh=resource(assets,bank,type);
            require(mesh.flags>0&&mesh.flags<=retainedAnimationSelectorCapacity&&mesh.animationLimits.size()==mesh.flags,
                "callback survey requires complete supported selector metadata");
            require(std::all_of(mesh.animationLimits.begin(),mesh.animationLimits.end(),[&](auto v) {return v==mesh.animationLimits[0];}),
                "callback survey encountered unequal heap limits");
            auto model=GameplayModelTestAccess::entry(assets,bank,type);auto& actor=GameplayModelTestAccess::actor(*model);
            actor.modelAnimationVariant=actor.presentationModelAnimationVariant=1;
            for(unsigned i=1;i<mesh.flags;++i) {actor.modelAnimationTail.at(i-1)=actor.presentationModelAnimationTail.at(i-1)=1;}
            std::array<std::uint8_t,retainedAnimationSelectorCapacity> expected{};expected.fill(1);unsigned countdown=0;
            for(unsigned call=0;call<7;++call) {
                if(countdown==0) {
                    countdown=2;
                    for(unsigned i=0;i<mesh.flags;++i) {expected[i]=static_cast<std::uint8_t>((expected[i]+1)%mesh.animationLimits[i]);}
                } else {--countdown;}
                GameplayModelTestAccess::callback(*model);
                require(actor.modelAnimationVariant==expected[0]&&actor.stateBytes4dTo52[3]==countdown,"heap callback changed its native countdown");
                for(unsigned i=1;i<mesh.flags;++i) {require(actor.modelAnimationTail[i-1]==expected[i],"heap callback skipped a secondary selector");}
            }
            actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
            GameplayModelTestAccess::main(*model);
            require(actor.presentationModelAnimationVariant==expected[0],"main snapshot lost the selected pose");
            for(unsigned i=1;i<mesh.flags;++i) {require(actor.presentationModelAnimationTail[i-1]==expected[i],"main snapshot lost secondary animation state");}
            std::cout<<"geometry entry bank="<<bank<<" type="<<type<<" selected="<<unsigned(actor.presentationModelAnimationVariant)<<'\n';
            // FC24 has no qualified incarnation; its MAIN/radar/pixels use the actual-bank type24_radar test.
            if(type!=24) {visibleSelectors(assets,*model,mesh);++geometryEntries;}
            ++entries;
        }
    }
    std::cout<<"Heap callback countdown/freeze/snapshot entries="<<entries<<" tagged main/chase geometry entries="<<geometryEntries<<" banks=7\n";
}
}
int main(int argc,char** argv) try {
    require(argc==3,"resources and boundary required");const auto assets=naturalRoute::assets(argv[1]);const std::string mode=argv[2];
    if(mode=="selectors") {contactSelectors(assets);} else if(mode=="paused") {pausedSelectors(assets);} else if(mode=="cadence") {cadence(assets);} else {return 2;}
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
