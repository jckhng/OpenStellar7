#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "playable_detail_render.hpp"
#include "stellar7/indexed_render.hpp"

#include <iostream>
#include <type_traits>
#include <utility>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
//The integer permits the RED test to compile against the unchanged baseline.
constexpr auto preferencesAction=static_cast<GameplaySystemAction>(10);
constexpr std::size_t maximumCalls=10000;

class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;
        if(fail) {return {HighscoreSaveStatus::writeFailed,std::nullopt};}
        bytes=encodeHighscoreTable(table);return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    bool fail=false;
    unsigned calls=0;
    std::vector<std::uint8_t> bytes;
};

struct FrozenWorld {
    explicit FrozenWorld(const GameplayModel& model):loop(model.level2LoopTick()),ticks(model.ticks()),
        score(model.score()),armor(model.playerArmor()),rng(model.level2Rng()),failure(model.failureRng()),
        player(model.level2Player()),timers(model.level1State().systemTimers()),
        order(model.level2ActiveListOrder()),ground(model.level2Ground().points()) {
        actors=model.level2Entities();
    }
    void checkPhysics(const GameplayModel& model) const {
        const auto physics=[](const Level1Entity& actor) {
            return std::tie(actor.position.x,actor.position.y,actor.position.z,actor.pitch,actor.roll,actor.yaw,
                actor.pitchVelocity,actor.rollVelocity,actor.yawVelocity,actor.xVelocity,actor.yVelocity,
                actor.zVelocity,actor.forwardVelocity);
        };
        requireCampaignComparison(loop==model.level2LoopTick()&&ticks==model.ticks()&&score==model.score()
            &&armor==model.playerArmor()&&physics(player)==physics(model.level2Player())
            &&timers==model.level1State().systemTimers()&&order==model.level2ActiveListOrder(),
            "live return advanced world/player physics/system timers");
    }
    void check(const GameplayModel& model) const {
        checkPhysics(model);
        requireCampaignComparison(loop==model.level2LoopTick()&&ticks==model.ticks()&&score==model.score()
            &&armor==model.playerArmor()&&test::campaignActorFields(player)==test::campaignActorFields(model.level2Player())
            &&timers==model.level1State().systemTimers()&&order==model.level2ActiveListOrder()
            &&ground==model.level2Ground().points()&&actors.size()==model.level2Entities().size(),
            "live preferences advanced world/player/system timers or physical membership");
        for(std::size_t index=0;index<actors.size();++index) {
            requireCampaignComparison(test::campaignActorFields(actors[index])==test::campaignActorFields(model.level2Entities()[index]),
                "live preferences mutated a physical actor");
        }
        test::compareCampaignRng(rng,model.level2Rng(),test::CampaignComparisonStage::boundary);
        test::compareCampaignRng(failure,model.failureRng(),test::CampaignComparisonStage::boundary);
    }
    std::uint16_t loop;
    std::uint64_t ticks;
    std::uint32_t score;
    std::int32_t armor;
    StellarRng rng,failure;
    Level1Entity player;
    std::decay_t<decltype(std::declval<const GameplayModel&>().level1State().systemTimers())> timers;
    std::vector<std::size_t> order;
    std::decay_t<decltype(std::declval<const GameplayModel&>().level2Ground().points())> ground;
    std::vector<Level1Entity> actors;
};

void titleCache(const GameplayAssets& assets,const NativeGameplaySession& owner,
    const std::vector<std::uint8_t>& incoming,const CockpitIndicatorState indicator) {
    const auto roles=owner.pageObservation();
    CockpitWorkBitmaps art;
    art.strip=assets.artGroups.at(3).at(1);art.matchingPageBackground=assets.artGroups.at(3).at(2);
    art.indicator=assets.artGroups.at(4).at(6);
    for(std::size_t index=0;index<art.overlays.size();++index) {art.overlays[index]=assets.artGroups.at(4).at(index);}
    const auto matching=playableFullCockpitOperations({indicator.x,indicator.velocity,0,CockpitPageRelation::matching},art);
    const auto ordinary=playableFullCockpitOperations({indicator.x,indicator.velocity,0,CockpitPageRelation::differing},art);
    const auto expected=[&](const BitmapFont& font) {
        IndexedPages pages;
        for(std::int16_t y=0;y<200;++y) {
            for(std::int16_t x=0;x<320;++x) {pages.write(roles.cache,{x,y},incoming[y*320+x]);}
        }
        pages.apply(cockpitCachePageEffects(matching,ordinary,art,font,"SOL",239,roles.cache,roles.back,
            CockpitPageLayout::recoveredStartup,CockpitCacheDomain::retainedGameplay));
        return pages.snapshot(roles.cache);
    };
    const auto primary=expected(assets.titleFont),secondary=expected(assets.scoreFont);
    requireCampaignComparison(primary!=secondary,"actual primary/secondary title fonts do not distinguish this control");
    requireCampaignComparison(owner.retainedPagePixels(NativePageRole::cache)==std::optional{primary},
        "live50C4 title did not use retained primary12CA or altered omitted low-detail cache pixels");
    requireCampaignComparison(owner.model().level2CockpitIndicatorX()==indicator.x
        &&owner.model().level2CockpitIndicatorVelocity()==indicator.velocity,
        "full->low0 live close advanced disabled6D85 state");
}

void run(const std::filesystem::path& root) {
    auto assets=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(vga.at(0));
    const auto store=std::make_shared<MemoryStore>();
    RecoveredFrontend frontend(assets,art,copy,257,store,CampaignExecution::playable,
        NativePointerCapability::unavailable,NativeMenuFeatures::fullGraphicDetail);
    unsigned menuPolls=0;
    for(unsigned call=0;!frontend.session()&&call<maximumCalls;++call) {
        NativeMenuInput input;
        if(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=menuPolls++==1;}
        frontend.advanceStartup(input);
    }
    requireCampaignComparison(frontend.session()!=nullptr,"live preferences test missed actual Begin");
    for(unsigned index=0;index<3;++index) {frontend.advanceGameplay({});}
    const auto* owner=frontend.session();const auto* synth=&owner->synth();
    const auto before=owner->model().level2LoopTick();
    GameplayInput open;open.systemAction=preferencesAction;
    frontend.advanceGameplay(open);
    requireCampaignComparison(frontend.menuBackAvailable()&&frontend.session()==owner&&&owner->synth()==synth
        &&owner->model().level2LoopTick()==static_cast<std::uint16_t>(before+1),
        "F10 did not consume exactly one ordinary frame before the retained modal");
    const FrozenWorld frozen(owner->model());
    const auto cacheBefore=*owner->retainedPagePixels(NativePageRole::cache);
    const CockpitIndicatorState indicatorBefore{static_cast<std::int16_t>(owner->model().level2CockpitIndicatorX()),
        static_cast<std::int16_t>(owner->model().level2CockpitIndicatorVelocity())};
    const auto enteredSamples=frontend.elapsedSamples();
    const auto poll=[&](const GameplayInput input) {
        const auto beforeSamples=frontend.elapsedSamples();const auto delivery=frontend.advanceGameplay(input);
        requireCampaignComparison(delivery.firstSample==beforeSamples
            &&delivery.firstSample+delivery.samples.size()==frontend.elapsedSamples(),"live modal lost PCM epoch");
        requireCampaignComparison(frontend.session()==owner&&&owner->synth()==synth,"live modal replaced session or synth");
        if(frontend.menuBackAvailable()) {frozen.check(owner->model());}
        else {frozen.checkPhysics(owner->model());}
        return delivery;
    };
    for(unsigned index=0;index<32;++index) {poll({});}
    requireCampaignComparison(frontend.elapsedSamples()>enteredSamples,"paused gameplay stopped real device/audio time");
    GameplayInput down;down.reverse=true;poll(down);poll({}); //PLAY->detail.
    GameplayInput left;left.left=true;
    for(unsigned index=0;index<41;++index) {poll(left);}
    poll({});
    GameplayInput up;up.forward=true;poll(up);poll({}); //detail->PLAY.
    GameplayInput enter;enter.confirm=true;poll(enter);poll({});
    requireCampaignComparison(!frontend.menuBackAvailable()&&store->calls==1&&owner->highscoreTable()
        &&owner->highscoreTable()->header()[0]==0,"PLAY did not save/reload current live detail");
    titleCache(assets,*owner,cacheBefore,indicatorBefore);
    for(unsigned index=1;index<=2;++index) {
        frontend.advanceGameplay({});
        requireCampaignComparison(owner->model().level2CockpitIndicatorX()==indicatorBefore.x,
            "low0 adjacent ordinary frame advanced disabled indicator");
        requireCampaignComparison(owner->model().level2LoopTick()==static_cast<std::uint16_t>(frozen.loop+index),
            "live return lost an adjacent ordinary frame");
    }
    const auto saved=store->bytes;store->fail=true;
    frontend.advanceGameplay(open);
    const FrozenWorld retryEntry(owner->model());
    const auto allocation=owner->synth().driverState().sequenceAllocations;
    requireCampaignComparison(owner->synth().driverState().sequenceSelected,"live music fixture requires current publication");
    frontend.advanceGameplay(up);frontend.advanceGameplay({}); //PLAY->sounds.
    frontend.advanceGameplay(up);frontend.advanceGameplay({}); //sounds->music.
    frontend.advanceGameplay(enter);frontend.advanceGameplay({});
    requireCampaignComparison(owner->synth().driverState().sequenceAllocated&&!owner->synth().driverState().sequenceSelected,
        "live MUSIC Off did not suspend the retained descriptor");
    GameplayInput escape;escape.exit=true;
    frontend.advanceGameplay(escape);
    requireCampaignComparison(frontend.menuBackAvailable()&&store->calls==2&&store->bytes==saved,
        "failed live save closed modal or changed durable bytes");
    frontend.advanceGameplay(escape);
    requireCampaignComparison(!frontend.menuBackAvailable()&&store->calls==2&&store->bytes==saved,
        "failed-save Escape undo wrote again or retained modal");
    requireCampaignComparison(owner->synth().driverState().sequenceAllocated&&owner->synth().driverState().sequenceSelected
        &&owner->synth().driverState().sequenceAllocations==allocation,"live undo lost allocation/current music publication");
    retryEntry.checkPhysics(owner->model());
    std::cout<<"Actual Begin/F10 full->low0, primary title/cache residue, adjacent frozen indicator and failure undo PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
