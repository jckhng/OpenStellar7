#include "stellar7/level1_transition_work.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
Level1TransitionWork level1SuccessTransitionOperations(const Level1TransitionProfile& input) {
    const auto roles=input.cachePage==0xa400 && ((input.frontPage==0xa000&&input.backPage==0xa800)
        ||(input.frontPage==0xa800&&input.backPage==0xa000));
    if(input.levelIndex!=0 || !input.successReturned || input.missionActive || input.deathRequested || input.exitRequested
        || input.armor<0 || input.armor>680 || input.score<1000 || input.score>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        || input.introduction!=0 || input.selectedSystem!=-1 || !roles
        || std::any_of(input.systemLevels.begin(),input.systemLevels.end(),[](auto value){return value<1||value>3;})) {
        throw std::invalid_argument("unsupported retained Level1 success-to-Level2 transition");
    }
    Level1TransitionWork result{};
    result.observation={1,input.score,static_cast<std::int16_t>(std::min(680,input.armor+80)),input.systemTimers};
    for(std::size_t i=0;i<result.observation.systemTimers.size();++i) {
        if(i!=3) {result.observation.systemTimers[i]=0;} //Source48D4 omits12D4.
    }
    const auto plain=[&](std::uint64_t count) {
        if(result.segments.empty() || result.segments.back().kind!=RetryWorkKind::ordinary) {result.segments.push_back({});}
        auto& out=result.segments.back().operations;
        if(!out.empty()&&out.back().kind==NormalWorkKind::instructions) {out.back().count+=count;}
        else {out.push_back({NormalWorkKind::instructions,count});}
    };
    const auto child=[&](RetryWorkKind kind,std::uint16_t cs,std::uint16_t entry,std::uint16_t resume,bool qualified) {
        result.segments.push_back({kind,cs,entry,resume,{},{},qualified});
    };
    plain(5); //03EB,03EE/03F3 and0428/042C. No failed-mission/Continue branch.
    child(RetryWorkKind::releasePaletteProvider,0x1764,0x54dc,0x0431,false);
    plain(4);child(RetryWorkKind::detachLevelResourceProvider,0x0824,0xc01a,0x043d,false);
    plain(4);child(RetryWorkKind::releaseDetachedResourceProvider,0x0824,0xbfba,0x0447,false);
    plain(3);child(RetryWorkKind::releaseMusic,0x1ebc,0x018c,0x0453,false);
    plain(7);child(RetryWorkKind::stopPendingSound,0x0824,0x51a6,0x0469,true);
    plain(6); //0469 fade1,046F impulse0,0475 level0->1,0479..0480.
    plain(2+2+5+7); //0186/0190 live guards;019A fade0;4E06..4E12.
    child(RetryWorkKind::selectScoreFontProvider,0x1764,0x4848,0x4e17,false);
    plain(3); //4E17..4E1F introduction0 skips optional scene.
    child(RetryWorkKind::levelResourceBootstrapProvider,0x0824,0x4e80,0x4f50,false);
    plain(1);child(RetryWorkKind::stopPendingSound,0x0824,0x51a6,0x4f55,true);
    plain(2);child(RetryWorkKind::modelResourcesProvider,0x0824,0x4a34,0x4f59,false);
    plain(2);result.poolResetSegment=result.segments.size();
    child(RetryWorkKind::modelReloadProvider,0x0824,0x48d4,0x4f5f,false);
    plain(2);child(RetryWorkKind::releaseDetachedResourceProvider,0x1764,0x08f6,0x4f65,false);
    plain(3);child(RetryWorkKind::levelMusicResourcesProvider,0x0824,0x6e6c,0x4f6f,false);
    plain(1);result.constructorsSegment=result.segments.size();
    child(RetryWorkKind::retainedConstructorCompletionProvider,0x0824,0x4f72,0x508f,false);
    plain(3);result.loaderPagesSegment=result.segments.size();
    child(RetryWorkKind::loaderPages,0x0824,0x50c4,0x5095,false);
    plain(1);child(RetryWorkKind::musicActivation,0x0824,0x5098,0x50af,false);
    plain(6+11);child(RetryWorkKind::palette,0x3568,0x077a,0x50bb,true);
    result.segments.back().palette=PaletteRequest{0,256,0,0};
    plain(3+6);child(RetryWorkKind::directReset,0x0824,0x01a8,0x01b4,false);
    plain(3);child(RetryWorkKind::scoreHudProvider,0x0824,0x7970,0x01ba,false);
    plain(2);result.groundSegment=result.segments.size();
    child(RetryWorkKind::retainedGroundInitializationProvider,0x0824,0x4c16,0x01c0,false);
    plain(1);child(RetryWorkKind::statusHudProvider,0x0824,0x8f1c,0x01c3,false);
    plain(2+1);child(RetryWorkKind::controllerRebindProvider,0x0824,0x01d0,0x01d0,false);
    return result;
}

Level2PoolResetWork level2PoolReset(const Level2PoolResetInput& input,const Model3d& playerModel) {
    if(input.armor<0 || input.armor>680 || playerModel.scaleShift!=0 || playerModel.collisionRadius==0) {
        throw std::invalid_argument("unsupported Level2 retained pool reset profile");
    }
    Level2PoolResetWork result{input,{},{}};
    const auto plain=[&](std::uint64_t count) {
        if(result.steps.empty() || !std::holds_alternative<std::vector<NormalWorkOperation>>(result.steps.back())) {
            result.steps.emplace_back(std::vector<NormalWorkOperation>{});
        }
        auto& ops=std::get<std::vector<NormalWorkOperation>>(result.steps.back());
        if(!ops.empty() && ops.back().kind==NormalWorkKind::instructions) {ops.back().count+=count;}
        else {ops.push_back({NormalWorkKind::instructions,count});}
    };
    const auto clear=[&]() {
        plain(2+8); //Caller PUSH/CALL49F4; prefix through ES/DI setup.
        std::get<std::vector<NormalWorkOperation>>(result.steps.back()).push_back({NormalWorkKind::repeatString,99});
        plain(4); //49F4 return.
    };
    const auto reset=[&](Level1Entity& actor,std::uint8_t category) {
        const auto identity=actor.allocationIndex;actor={};actor.allocationIndex=identity;
        actor.stateBytes4dTo52[0]=category;actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::noHandles;
    };
    const auto release=[&](const Level1Entity& actor,Level2ResetPool pool,std::size_t slot) {
        if(actor.renderProfile==WorldActorRenderProfile::unknown) {throw std::invalid_argument("reset reached unknown selector ancestry");}
        if(actor.primaryType11AnimationSelector || actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback) {
            plain(2+8+2); //Caller PUSH/CALL13AC; nonalias branch through CALLDCCA.
            const Level2SelectorReleaseReceipt receipt{pool,slot};
            result.selectorReleases.push_back(receipt);result.steps.emplace_back(receipt);
            plain(28); //13C3..13DC, wholeDCC6(-1)/F051, caller ADDSP.
        } else if(actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback) {
            plain(2+15+1); //13AC's retained actor+4F alias skips heap release.
        }
    };
    plain(3); //48D4..48D6.
    for(std::size_t i=0;i<result.state.ordinary.size();++i) {
        plain(3);release(input.ordinary[i],Level2ResetPool::ordinary,i);clear();plain(6);
        reset(result.state.ordinary[i],255);
    }
    plain(1); //48FB fixed pointer.
    for(std::size_t i=0;i<8;++i) {
        plain(3);release(input.fixed[i],Level2ResetPool::fixed,i);clear();plain(6);
        reset(result.state.fixed[i],254);
    }
    plain(1); //4920 projectile pointer.
    for(std::size_t i=0;i<8;++i) {
        plain(1);clear();plain(8);
        reset(result.state.projectiles[i],253);result.state.projectiles[i].renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    }
    plain(1); //493E effect pointer.
    for(std::size_t i=0;i<8;++i) {
        plain(3);release(input.effects[i],Level2ResetPool::effect,i);clear();plain(7);
        reset(result.state.effects[i],252);result.state.effects[i].flags=0x88;
    }
    plain(8);clear(); //4966..497F globals and PUSHplayer; CALL49F4 inside clear.
    plain(4+48+14+3+(input.armor+80>680?1:0)+9+3); //4986..49F2, real shared-model registry, armor and timers.
    reset(result.state.player,0);result.state.player.collisionRadius=playerModel.collisionRadius;result.state.player.stateBytes4dTo52[1]=1;
    result.state.armor=static_cast<std::int16_t>(std::min(680,input.armor+80));
    for(std::size_t i=0;i<result.state.systemTimers.size();++i) {if(i!=3) {result.state.systemTimers[i]=0;}}
    return result;
}

Level2LoaderActors level2LoaderActors(const StellarRng& currentRng,const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local) {
    const auto definition=campaignLevelDefinition(1,0);
    const auto& table=definition.table;
    if(shared.size()!=44 || table.actors.size()!=7 || campaignModelRadius(shared,local,0)==0
        || currentRng.indexA()>=StellarRng::wordCount*2 || currentRng.indexB()>=StellarRng::wordCount*2
        || currentRng.indexA()%2 || currentRng.indexB()%2
        || shared[30].scaleShift!=0 || shared[30].flags!=0 || shared[30].collisionRadius==0) {
        throw std::invalid_argument("unsupported destination Level2 loaded model/table profile");
    }
    std::vector<std::uint8_t> types;
    for(const auto& entry:table.actors) {
        if(entry.type!=8&&entry.type!=13&&entry.type!=29) {throw std::invalid_argument("unexpected Level2 actor family");}
        if(entry.type!=29) {types.push_back(entry.type);}
    }
    const CampaignOrdinaryConstructor constructor(1,types,shared,local);
    class Loader final:public CampaignActorLoader {
    public:
        Loader(StellarRng& rng,const CampaignOrdinaryConstructor& constructor,const std::vector<Model3d>& shared,
            const std::vector<Model3d>& local):rng_(rng),constructor_(constructor),shared_(shared),local_(local) {}
        Actor allocateOrdinary(std::uint8_t type) override {
            auto actor=constructor_.construct(type,rng_);actor.allocationIndex=actors_.size();
            actors_.push_back({Level2LoaderPool::ordinary,ordinary_++,actor});return actors_.size()-1;
        }
        Actor allocateFixed(std::uint8_t type) override {
            if(type!=30) {throw std::invalid_argument("unsupported Level2 fixed constructor");}
            Level1Entity actor{};actor.allocationIndex=actors_.size();actor.type=type;actor.collisionRadius=campaignModelRadius(shared_,local_,type);
            actor.stateBytes4dTo52={254,100,0,0,0,0};actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::noHandles;
            actors_.push_back({Level2LoaderPool::fixed,fixed_++,actor});return actors_.size()-1;
        }
        void write(Actor actor,CampaignActorField field,std::int16_t value) override {
            auto& item=actors_.at(actor).actor;
            if(field==CampaignActorField::auxiliaryWord1a&&value==0) {return;}
            if(field==CampaignActorField::classification&&value==-2) {item.stateBytes4dTo52[0]=254;return;}
            throw std::invalid_argument("unexpected Level2 fixed write");
        }
        void setPosition(Actor actor,std::int16_t x,std::int16_t y) override {actors_.at(actor).actor.position={campaignCoordinate(x),campaignCoordinate(y),0};}
        void setYaw(Actor actor,std::uint16_t yaw) override {actors_.at(actor).actor.yaw=yaw;}
        void presentFixed(Actor) override {throw std::invalid_argument("Level2 has no animated fixed presenter");}
        void aliasAnimation(Actor) override {throw std::invalid_argument("Level2 has no fixed inline alias");}
        std::vector<Level2LoaderActorReceipt> take() {return std::move(actors_);}
        std::size_t ordinaryCount() const {return ordinary_;}
    private:
        StellarRng& rng_;
        const CampaignOrdinaryConstructor& constructor_;
        const std::vector<Model3d>& shared_;
        const std::vector<Model3d>& local_;
        std::vector<Level2LoaderActorReceipt> actors_;
        std::size_t ordinary_=0,fixed_=0;
    };
    auto rng=currentRng;Loader loader(rng,constructor,shared,local);initializeCampaignActors(table,loader);
    const auto count=loader.ordinaryCount();
    if(count!=5) {throw std::invalid_argument("Level2 source ordinary count changed");}
    auto result=Level2LoaderActors{rng,loader.take(),static_cast<std::int16_t>(count),static_cast<std::int16_t>(count),{}};
    const auto plain=[&](std::uint64_t count) {
        auto& ops=result.operations;
        if(!ops.empty() && ops.back().kind==NormalWorkKind::instructions) {ops.back().count+=count;}
        else {ops.push_back({NormalWorkKind::instructions,count});}
    };
    auto traceRng=currentRng;
    const auto random=[&]() {
        plain(20+(traceRng.indexA()==0?1:0)+(traceRng.indexB()==0?1:0));return traceRng.next();
    };
    plain(4); //4F72..4F81 nonempty table.
    for(std::size_t slot=0;slot<5;++slot) {
        const bool type13=slot>=3;
        plain(12+10+2+7*slot+3+6+2+8); //4F83 ordinary branch;4370 scan;49F4 prefix.
        result.operations.push_back({NormalWorkKind::repeatString,99});
        plain(4+7+48+6); //49F4 return;43DF..4401 with wholeD03C/C05C.
        plain(5);const auto placement=random(); //4862..4869.
        plain(3+((placement&1)?5:3)+2+((placement&2)?1:0)+2+((placement&4)?1:0));
        plain(4+9+8+9+10); //489B..48D3, two whole417A conversions.
        plain(5); //4404..4411, neitherType11 nor12.
        if(type13) {
            plain(2+1);const auto spin=random();plain(6+((spin&1)?1:0)); //4434..4449.
        } else {plain(4);}
        plain(2+1);random();plain(3+9+4+(type13?0:4)+2+7+8); //444C..44B9.
        plain(6+37); //504B..5058 plus positions/loop (417A twice).
    }
    for(std::size_t slot=0;slot<2;++slot) {
        plain(11); //4F83..4F9D fixedType29 -> destination30.
        plain(6+7*slot+3+14+48+14); //Whole4506; no inherited selector after48D4.
        plain(9); //4FA0..4FC1 normalLevel2 fixed branch.
        plain(slot==0?37:36); //505B..508C, final branch returns at508F.
    }
    return result;
}
Level2GroundInitializationWork level2RetainedGroundInitializationOperations(const StellarRng& currentRng) {
    constexpr std::int16_t horizontalRange=0x127;
    constexpr std::int16_t verticalRange=0x38;
    constexpr std::int16_t verticalOrigin=0x5a;
    constexpr std::int16_t coordinateScale=0x20;
    if(currentRng.indexA()>=StellarRng::wordCount*2 || currentRng.indexB()>=StellarRng::wordCount*2
        || currentRng.indexA()%2 || currentRng.indexB()%2) {
        throw std::invalid_argument("invalid retained ground RNG source indices");
    }
    auto rng=currentRng;
    std::array<GroundPoint,Level1GroundState::pointCount> points{};
    std::vector<NormalWorkOperation> operations;
    const auto plain=[&](std::uint64_t count){operations.push_back({NormalWorkKind::instructions,count});};
    const auto random=[&](){plain(20+(rng.indexA()==0?1:0)+(rng.indexB()==0?1:0));return static_cast<std::int16_t>(rng.next());};
    plain(3+2); //4C16..4C1C retained allocation;4C2D/4C33.
    for(auto& point:points) {
        plain(2+12); //4C35/36;4DC8..4DDE including CALLB39E.
        const auto x=random();plain(6+1); //4DE1..4DED second CALL.
        const auto y=random();plain(7+5); //4DF0..4E05.
        point={static_cast<std::int16_t>((x%horizontalRange)*coordinateScale),
            static_cast<std::int16_t>((y%verticalRange+verticalOrigin)*coordinateScale)};
        plain(4); //4C39..4C41 loop including final comparison.
    }
    plain(2);return {rng,Level1GroundState(points),std::move(operations)};
}
} //namespace stellar7
