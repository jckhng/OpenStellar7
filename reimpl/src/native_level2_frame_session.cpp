#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/level2_frame.hpp"
#include "stellar7/level2_first_tail_work.hpp"
#include "stellar7/level2_periodic_tail_work.hpp"
#include "stellar7/level2_projectile_prefix_receipt.hpp"
#include "stellar7/weapon_lamp_work.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::int64_t flipHorizon = 2 * workHorizon;
constexpr std::int64_t paletteHorizon = 2 * workHorizon;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::size_t forwardKeyIndex = 4;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
enum class Level2FrameEntry { first, retained };

bool released(const GameplayInput& input) {
    return !input.left && !input.right && !input.forward && !input.reverse
        && !input.fire && !input.exit && !input.confirm;
}
bool combatControls(const GameplayInput& input) {
    return !input.exit && !input.confirm;
}
std::array<bool,9> heldKeys(const GameplayInput& input) {
    return {false,input.reverse,false,false,input.forward,false,input.left,input.right,input.fire};
}
std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}

//Pure model/physical-page planning from the actual prefix entry. This executor
//has no DISPLAY initial state, controller mutation, synth or clock reference.
class Level2FramePlanner final:public Level2FrameRenderer {
public:
    Level2FramePlanner(GameplayModel& model,const GameplayAssets& assets,const CockpitSettings& settings,
        const CockpitPageState& pages,const RadarState& radar,const IndexedPages& pixels,
        const FrameController& controller,Level2FrameEntry entry,const std::vector<std::size_t>& allocations)
        :model_(model),assets_(assets),settings_(settings),pages_(pages),radar_(radar),pixels_(pixels),
        controller_(controller),entry_(entry),allocations_(allocations) {}
    Level2PrefixExecution prefixExecution() const noexcept override { return Level2PrefixExecution::source; }
    Level2SourceDomain sourceDomain() const noexcept override {
        return entry_==Level2FrameEntry::first ? Level2SourceDomain::constructorControls : Level2SourceDomain::periodicLifecycle;
    }
    std::optional<StationaryLevel2PrefixWork> prepare(const StationaryLevel2PrefixInput& input) override {
        require(Stage::prefix);
        StationaryLevel2PrefixWork prefix;
        if (entry_==Level2FrameEntry::first) {
            const auto first=level2ResetPreloopPrefixOperations({input,allocations_});
            visibility=first.visibility;
            prefix={first.player,first.actors,first.activeOrder,first.operations};
        } else {
            constexpr auto sharedCount=Level2HostileProjectileModel::sharedCount;
            if(assets_.models.size()<sharedCount
                || (assets_.campaignLevels.empty() ? assets_.sharedModelRegistry[0]!=sharedCount
                    : assets_.campaignLevels.front().models.empty()
                        || assets_.campaignLevels.front().models.size()>assets_.models.size()
                        || assets_.models.size()-assets_.campaignLevels.front().models.size()!=sharedCount)) {
                throw std::invalid_argument("Level2 hostile prefix lacks its decoded shared model bank");
            }
            const auto& model19=assets_.models[Level2HostileProjectileModel::type];
            const Level2HostileProjectileModel hostile{sharedCount,{model19.scaleShift,model19.collisionRadius}};
            ownedShared_.assign(assets_.models.begin(),assets_.models.begin()+sharedCount);
            const auto descriptor=[&](const std::size_t type) {
                const auto& model=ownedShared_.at(type);
                return Level1EnemyProjectileModel{model.scaleShift,model.collisionRadius};
            };
            Level2PeriodicOrdinaryProfile periodic{sharedCount,descriptor(1),descriptor(4),descriptor(17),descriptor(20),{}};
            prefix=model_.recoveredLevel2PeriodicProjectilePrefix(assets_.scoreFont,pages_,periodic,&hostile,
                Level1PlayerCombatDomain::expiryAndLoss);
            if(!prefix.projectileReceipt) {throw std::logic_error("Level2 combat prefix returned no source receipt");}
            prefixScores=prefix.projectileReceipt->work.scoreSteps;
            prefixPalettes=prefix.projectileReceipt->work.damage.paletteSteps;
            for(const auto& score:prefixScores) {
                pixels_.apply(score.work.effects);pages_=score.work.pages;
            }
            prefixReceipt=prefix.projectileReceipt;
        }
        prefixOperations=prefix.operations;
        prefixRngCalls=prefixReceipt ? prefixReceipt->work.damage.combat.prefix.rng.calls() : model_.level2Rng().calls();
        pages_.beginFrame(); //0251/0256/0259 are already counted in the prefix.
        stage_=Stage::render;
        return prefix;
    }
    void render(Level2RenderState frame) override {
        require(Stage::render);
        const auto& panorama=assets_.horizonFramesForLevel(1);
        if (panorama.size()<4) { throw std::invalid_argument("recovered Level2 requires its loaded panorama pages"); }
        const HorizonWorkState horizon{24,145,static_cast<std::int16_t>(frame.player.yaw),settings_.viewSize,
            panorama.front().width,panorama.front().height,HorizonBackground::solid,
            campaignHorizonOverlayEnabled(1) ? HorizonOverlay::enabled : HorizonOverlay::disabled,64,78};
        auto next=std::make_unique<Level2FramePlan>(entry_==Level2FrameEntry::first
            ? planRecoveredLevel2ConstructorFrame(assets_,frame,model_.level2LoopTick(),model_.playerArmor(),
                horizon,settings_,pages_,radar_,pixels_,Level2FrameDomain::retainedForward,model_.recoveredLevel2RenderKeys())
            : planRecoveredLevel2PeriodicFrame(assets_,frame,model_.level2LoopTick(),model_.playerArmor(),
                horizon,settings_,pages_,radar_,pixels_,model_.recoveredLevel2RenderKeys(),
                prefixReceipt->work.damage.combat.prefix.allocationOrder,Level2RadarDomain::periodicLifecycle));
        if (entry_==Level2FrameEntry::first && next->radar.pageCopy) {
            throw std::invalid_argument("recovered quiet Level2 frame excludes a new armor-strip copy");
        }
        commitLevel2Frame(*next,frame,pages_,radar_,pixels_);
        if(next->combat) {
            model_.commitRecoveredLevel2CombatMain(*next->combat);
            model_.commitRecoveredLevel2CombatRadar(next->radar);
        }
        pages_.flip();
        framePlan=std::move(next);
        stage_=Stage::tail;
    }
    std::optional<FrameTailState> finish(const FrameTailState& state,const FrameTailGuards& incoming) override {
        require(Stage::tail);
        if (state.loopTick!=controller_.expectedLoopTick()
            || incoming.firstFrame!=(entry_==Level2FrameEntry::first)) {
            throw std::invalid_argument("recovered Level2 tail lost its actual source entry");
        }
        auto guards=controller_.nativeGuards(incoming,incoming.heldKeys);
        if (guards.periodic) { guards.periodic->enemyFlag=framePlan->radar.enemyFlag; }
        if (entry_==Level2FrameEntry::first) {
            auto first=level2FirstFrameTailOperations(state,guards);
            beforeEntryFade=std::move(first.beforeEntryFade);
            tail=std::move(first.tail);
        } else {
            const auto requestLamp=[&](WeaponLampState requested) {
                const auto dirty=model_.weaponIndicatorVisible()
                    ? static_cast<std::uint16_t>(model_.weaponReadyIndicator()) : std::uint16_t{0xffff};
                auto next=weaponLampOperations(dirty,requested,
                    assets_.artGroups.at(4).at(requested==WeaponLampState::ready?40:39),pages_);
                const auto operations=next.operations;pages_=next.pages;pixels_.apply(next.effects);
                lampEffects.insert(lampEffects.end(),next.effects.begin(),next.effects.end());
                model_.commitRecoveredLevel2WeaponLamp(next);++lampCalls;
                return operations;
            };
            std::optional<Level1TailFireAction> fire;
            if(guards.heldKeys[8]) {
                auto firing=level1PrimaryFireOperations(model_.recoveredLevel2PeriodicPrimaryInput(assets_,Level1PlayerCombatDomain::expiryAndLoss));
                model_.commitRecoveredLevel2PeriodicPrimary(assets_,firing,Level1PlayerCombatDomain::expiryAndLoss);
                auto lamp=firing.launch ? requestLamp(WeaponLampState::unavailable) : std::vector<NormalWorkOperation>{};
                fire=Level1TailFireAction{std::move(firing),std::move(lamp)};
            }
            const auto timers=fire ? std::array<std::uint8_t,3>{fire->work.mainCooldown,fire->work.weaponTimers[0],fire->work.weaponTimers[1]}
                : state.weaponTimers;
            std::vector<std::vector<NormalWorkOperation>> ready;
            for(std::size_t index=0;index<level1TailReadyLampCalls(timers);++index) {ready.push_back(requestLamp(WeaponLampState::ready));}
            auto complete=level2PeriodicFrameTailOperations(state,guards,[&](const FrameTailState& recentered) {
                return level2PeriodicSpawnOperations(model_.recoveredLevel2PeriodicInput(recentered),model_.level2Rng(),ownedShared_);
            },fire,ready,[&](const FrameTailState& recentered,const Level2PeriodicSpawnWork& periodic,std::uint8_t cachedFlag) {
                return level2ArrivalOperations(model_.recoveredLevel2ArrivalInput(recentered,periodic,cachedFlag),periodic.rng,ownedShared_);
            });
            lifecycle_=std::move(complete.lifecycle);
            if(complete.periodic) {
                periodic_=Level2PeriodicTailReceipt{std::move(*complete.periodic),&ownedShared_,std::move(complete.arrival),
                    *guards.periodic->enemyFlag};
                if(periodic_->work.spawn) {
                    if(!complete.periodicOperationOffset) {throw std::logic_error("Level2 periodic child lost its source offset");}
                    auto reset=periodic_->work.spawn->cacheReset;
                    reset.operationOffset+=*complete.periodicOperationOffset;
                    tailCacheResets.push_back(reset);
                }
                if(complete.arrivalCacheReset) {tailCacheResets.push_back(*complete.arrivalCacheReset);}
            } else {ordinary_=Level2OrdinaryTailReceipt{&*lifecycle_};}
            tail=lifecycle_->work;
            for(const auto& reset:tailCacheResets) {
                if(reset.operationOffset>tail.operations.size()) {throw std::logic_error("Level2 periodic cache reset is outside its source tail");}
            }
        }
        stage_=Stage::done;
        return tail.state;
    }
    const Level2LifecycleTailWork* lifecycleTail() const noexcept override {
        return lifecycle_ ? &*lifecycle_ : nullptr;
    }
    const Level2PeriodicTailReceipt* periodicTail() const noexcept override {
        return periodic_ ? &*periodic_ : nullptr;
    }
    const Level2OrdinaryTailReceipt* ordinaryTail() const noexcept override {
        return ordinary_ ? &*ordinary_ : nullptr;
    }
    void applyPeriodicCacheReset() {
        if(cacheResetApplied_>=tailCacheResets.size()) {throw std::logic_error("Level2 periodic cache reset lost its source boundary");}
        radar_.initializeAllocation(tailCacheResets[cacheResetApplied_++].physicalKey);
    }
    void requireComplete() const { require(Stage::done); }
    const CockpitPageState& pages() const noexcept { return pages_; }
    const RadarState& radar() const noexcept { return radar_; }
    const IndexedPages& pixels() const noexcept { return pixels_; }
    std::shared_ptr<const Level2ProjectilePrefixReceipt> prefixReceipt;
    std::vector<Level1PrefixScoreStep> prefixScores;
    std::vector<Level1PrefixPaletteStep> prefixPalettes;
    std::vector<IndexedPageEffect> lampEffects;
    std::size_t lampCalls=0;
    std::vector<NormalWorkOperation> prefixOperations;
    std::uint32_t prefixRngCalls=0;
    std::optional<Level2PreloopVisibilityReceipt> visibility;
    std::unique_ptr<Level2FramePlan> framePlan;
    std::vector<NormalWorkOperation> beforeEntryFade;
    FrameTailWork tail{};
    std::vector<Level2PeriodicCacheReset> tailCacheResets;
private:
    enum class Stage { prefix,render,tail,done };
    void require(Stage expected) const {
        if (stage_!=expected) { throw std::logic_error("recovered Level2 renderer hooks are out of source order"); }
    }
    GameplayModel& model_;
    const GameplayAssets& assets_;
    const CockpitSettings& settings_;
    CockpitPageState pages_;
    RadarState radar_;
    IndexedPages pixels_;
    const FrameController& controller_;
    Level2FrameEntry entry_;
    const std::vector<std::size_t>& allocations_;
    Stage stage_=Stage::prefix;
    std::optional<Level2LifecycleTailWork> lifecycle_;
    std::optional<Level2PeriodicTailReceipt> periodic_;
    std::optional<Level2OrdinaryTailReceipt> ordinary_;
    std::vector<Model3d> ownedShared_;
    std::size_t cacheResetApplied_=0;
};
}

NativeSessionDelivery NativeGameplaySession::advanceRecoveredLevel2Frame(const FrameCopyProfile copyProfile,
    const std::size_t workPacketBudget,const GameplayInput& input) {
    if (input.systemAction != GameplaySystemAction::none) {
        throw std::invalid_argument("source Level2 frames reject queued system actions");
    }
    const bool first=model_.phase()==GameplayPhase::level2PreloopReturn;
    const auto* preloop=recoveredLevel2Retry_ ? &recoveredLevel2Retry_->preloop
        : recoveredLevel2Transition_ ? &recoveredLevel2Transition_->preloop : nullptr;
    if (playableCampaign_ || !preloop || !recoveredLevel2Transition_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || initialized_
        || (first ? !released(input) : !combatControls(input))
        || workPacketBudget==0 || workPacketBudget>maximumPacketBudget || model_.campaignLevel()!=1
        || (!first && model_.phase()!=GameplayPhase::level2)
        || assets_.campaignLevels.size()<=1 || !campaignResources_ || campaignResources_->level!=1
        || campaignResources_->bitmaps!=&assets_.campaignLevels[1].bitmapGroups
        || campaignResources_->models!=&assets_.campaignLevels[1].models
        || campaignResources_->music!=&assets_.campaignLevels[1].musicSequence
        || pageProjection_!=(first ? NativePageProjection::retainedRecoveredLevel2Preloop
            : NativePageProjection::retainedRecoveredLevel2Frame)
        || recoveredController_->expectedLoopTick()!=model_.level2LoopTick()
        || level2Cursor_!=model_.level2AudioEvents().size()
        || model_.level2Player().weaponSlotTimers[2]!=0
        || (first && (model_.level2LoopTick()!=0 || recoveredLevel2Frame_
            || !preloop->firstEntryDisplay))
        || (!first && (!recoveredLevel2Frame_ || recoveredLevel2Frame_->returnedLoopTick!=model_.level2LoopTick()))) {
        throw std::invalid_argument("recovered Level2 frame requires its qualified transition/arrow-frame owners");
    }
    const auto held=recoveredController_->augmentGuards({}).heldKeys;
    for (std::size_t index=0;index<held.size();++index) {
        if (index!=forwardKeyIndex && index!=1 && index!=6 && index!=7 && index!=8 && held[index]) {
            throw std::invalid_argument("recovered Level2 arrow frames exclude other retained controller keys");
        }
    }
    const auto keys=heldKeys(input);
    recoveredController_->nativeGuards({},keys);
    const auto postcopy=frameCopyOperations(copyProfile);
    auto staged=std::make_unique<GameplayModel>(model_);
    auto planner=std::make_unique<Level2FramePlanner>(*staged,assets_,*assets_.cockpitSettings,pages_,*radar_,*pixels_,
        *recoveredController_,first ? Level2FrameEntry::first : Level2FrameEntry::retained,
        preloop->allocationOrder);
    staged->tickWithRenderer(input,*planner);
    planner->requireComplete();
    const auto lethal=planner->lifecycleTail() && planner->lifecycleTail()->failureRequested;
    const auto returnedPhase=lethal ? GameplayPhase::failurePreExplosionBlackout : GameplayPhase::level2;
    if (staged->phase()!=returnedPhase || staged->ticks()!=model_.ticks()+1
        || staged->level2LoopTick()!=static_cast<std::uint16_t>(model_.level2LoopTick()+1)
        || staged->level2AudioEvents().size()<level2Cursor_) {
        throw std::logic_error("recovered Level2 frame failed its complete source model boundary");
    }
    const auto& plan=*planner->framePlan;
    const PageFlipClockState flipState{PageFlipSynchronization::enabled,plan.pages.backPage(),plan.pages.frontPage(),
        0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram};
    const auto& stagedEvents=staged->level2AudioEvents();
    const std::vector<Level1AudioEvent> deliveredEvents(stagedEvents.begin()+level2Cursor_,stagedEvents.end());
    //Both passes traverse identical source work, including the real first9D9E
    //child between032C and032F. All admissions run before the live clock changes.
    const auto execute=[&](auto render,auto display,auto upload,auto provideFont,auto scoreComplete,auto cacheReset) {
        std::size_t eventOrdinal=level2Cursor_;
        const auto renderOrdered=[&](const std::vector<NormalWorkOperation>& operations) {
            for(const auto& operation:operations) {
                if(operation.kind!=NormalWorkKind::gameplayLevel2PrimaryFireEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2TargetHitEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2EnemyFireEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2EnemyType20Event
                    && operation.kind!=NormalWorkKind::gameplayLevel2SolidContactEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2PlayerCombatEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2ArrivalEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2ArrivalStopEvent
                    && operation.kind!=NormalWorkKind::gameplayLevel2GuardianDefeatEvent) {continue;}
                if(operation.count!=1 || eventOrdinal>=stagedEvents.size()
                    || stagedEvents[eventOrdinal].loopTick!=model_.level2LoopTick()
                    || stagedEvents[eventOrdinal].eventId!=operation.value) {
                    throw std::logic_error("Level2 source audio marker differs from canonical event order");
                }
                ++eventOrdinal;
            }
            render(operations);
        };
        std::size_t prefixOffset=0,scoreIndex=0,paletteIndex=0;
        while(scoreIndex<planner->prefixScores.size() || paletteIndex<planner->prefixPalettes.size()) {
            auto next=planner->prefixOperations.size()+1;
            if(scoreIndex<planner->prefixScores.size()) {next=std::min(next,planner->prefixScores[scoreIndex].operationOffset);}
            if(paletteIndex<planner->prefixPalettes.size()) {next=std::min(next,planner->prefixPalettes[paletteIndex].operationOffset);}
            if(next<prefixOffset || next>planner->prefixOperations.size()) {
                throw std::logic_error("Level2 prefix child has an invalid operation offset");
            }
            renderOrdered(std::vector<NormalWorkOperation>(planner->prefixOperations.begin()+prefixOffset,
                planner->prefixOperations.begin()+next));
            const auto hasPalette=paletteIndex<planner->prefixPalettes.size() && planner->prefixPalettes[paletteIndex].operationOffset==next;
            const auto hasScore=scoreIndex<planner->prefixScores.size() && planner->prefixScores[scoreIndex].operationOffset==next;
            if(hasPalette && hasScore) {throw std::logic_error("ambiguous Level2 excluded child order");}
            prefixOffset=next;
            if(hasPalette) {upload(planner->prefixPalettes[paletteIndex++].request);continue;}
            const auto& score=planner->prefixScores[scoreIndex++].work;
            if(score.fontSelections.size()!=2 || score.fontSelections[0].selection!=ScoreFontSelection::score
                || score.fontSelections[1].selection!=ScoreFontSelection::primary
                || score.drawPage!=pages_.backPage() || score.copyPage!=pages_.frontPage()) {
                throw std::logic_error("Level2 score child lost source7970 roles/font order");
            }
            std::size_t offset=0;
            for(const auto& font:score.fontSelections) {
                if(font.guestElapsedQualified || font.operationOffset<offset || font.operationOffset>score.operations.size()) {
                    throw std::logic_error("Level2 score font provider order is invalid");
                }
                renderOrdered(std::vector<NormalWorkOperation>(score.operations.begin()+offset,
                    score.operations.begin()+font.operationOffset));
                provideFont(font);offset=font.operationOffset;
            }
            renderOrdered(std::vector<NormalWorkOperation>(score.operations.begin()+offset,score.operations.end()));
            scoreComplete(score);
        }
        renderOrdered(std::vector<NormalWorkOperation>(planner->prefixOperations.begin()+prefixOffset,planner->prefixOperations.end()));
        renderOrdered(plan.operations);
        display();
        renderOrdered(postcopy.operations);
        renderOrdered(planner->beforeEntryFade);
        if (first) {
            for (std::uint16_t intensity=0;intensity<64;intensity+=4) {
                renderOrdered(std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,intensity==0 ? 20U : 25U}});
                upload(PaletteRequest{0,vgaPaletteColors,0,static_cast<std::uint8_t>(intensity)});
            }
            renderOrdered(std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,9}});
        }
        std::size_t tailOffset=0;
        for(const auto& reset:planner->tailCacheResets) {
            const auto offset=reset.operationOffset;
            if(offset<tailOffset) {throw std::logic_error("Level2 tail cache resets lost source order");}
            renderOrdered(std::vector<NormalWorkOperation>(planner->tail.operations.begin()+tailOffset,planner->tail.operations.begin()+offset));
            cacheReset();
            tailOffset=offset;
        }
        renderOrdered(std::vector<NormalWorkOperation>(planner->tail.operations.begin()+tailOffset,planner->tail.operations.end()));
        if(eventOrdinal!=stagedEvents.size()) {throw std::logic_error("Level2 source omitted a canonical audio event");}
    };
    auto phase=*audio_->workPhase_;
    auto palette=*recoveredStartupPalette_;
    execute([&](const auto& operations) {
        for (const auto& packet:packets(operations,workPacketBudget)) {
            phase=normalWorkTiming(phase,packet,workHorizon).phase;
        }
    },[&]() { phase=pageFlipClockTiming(phase,flipState,flipHorizon).timing.phase; },[&](PaletteRequest request) {
        const auto uploaded=paletteClockTiming(phase,palette,request,paletteHorizon);
        phase=uploaded.timing.phase;palette=uploaded.state;
    },[](const ScoreFontProviderRead&) {},[](const ScoreWork&) {},[]() {});
    NativeLevel2FrameRun run;
    run.firstSample=clock_.elapsedSamples();run.loopTick=model_.level2LoopTick();
    run.returnedLoopTick=staged->level2LoopTick();run.firstFrame=first;
    run.input=input;run.audioEvents=deliveredEvents;run.lampCalls=planner->lampCalls;
    run.prefixRngCalls=planner->prefixRngCalls;run.groundRngCalls=plan.ground.rng.calls();
    run.returnedRngCalls=staged->level2Rng().calls();run.firstVisibility=planner->visibility;
    phase=*audio_->workPhase_;palette=*recoveredStartupPalette_;
    std::vector<NativeTimedFrame> presentations;
    auto prefixPixels=std::make_unique<IndexedPages>(*pixels_);
    bool displayed=false;
    const auto publish=[&]() {
        IndexedScreen indexed;indexed.pixels.reserve(screenWidth*screenHeight);
        for (std::int16_t y=0;y<screenHeight;++y) {
            for (std::int16_t x=0;x<screenWidth;++x) {
                indexed.pixels.push_back(displayed ? plan.pixels.read(plan.pages.backPage(),{x,y})
                    : prefixPixels->read(pages_.frontPage(),{x,y}));
            }
        }
        indexed.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        presentations.push_back({clock_.elapsedSamples(),std::move(indexed)});
    };
    const auto append=[&](ClockedPresentationWork work) {
        phase=work.timing.phase;
        run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
        run.work.push_back(std::move(work));
    };
    execute([&](const auto& operations) {
        for (const auto& packet:packets(operations,workPacketBudget)) { append(clock_.renderWork(phase,packet,workHorizon)); }
    },[&]() {
        auto flipped=clock_.renderPageFlip(phase,flipState,flipHorizon);
        append(flipped.work);displayed=true;publish();run.flip=std::move(flipped);
    },[&](PaletteRequest request) {
        auto uploaded=clock_.renderPalette(phase,palette,request,paletteHorizon);
        palette=uploaded.state;append(uploaded.work);publish();run.palettes.push_back(std::move(uploaded));
    },[&](const ScoreFontProviderRead& font) {run.scoreFontProviders.push_back(font);},
        [&](const ScoreWork& score) {prefixPixels->apply(score.effects);},[&]() {planner->applyPeriodicCacheReset();});
    recoveredController_->completeNativeTail(run.loopTick,planner->tail,keys);
    model_=std::move(*staged);
    pages_=planner->pages();pixels_=planner->pixels();radar_=planner->radar();
    incomingIndicator_={plan.cockpit.state.indicatorX,plan.cockpit.state.indicatorVelocity};
    audio_->workPhase_=std::move(phase);*recoveredStartupPalette_=std::move(palette);
    pageProjection_=NativePageProjection::retainedRecoveredLevel2Frame;
    frame_.pixels=*retainedPagePixels(NativePageRole::front);
    const auto& colors=recoveredStartupPalette_->dac.colors;
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
    pageScore_=model_.score();level2Cursor_=model_.level2AudioEvents().size();
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,deliveredEvents};
    delivery.presentations=std::move(presentations);recoveredLevel2Frame_=std::move(run);
    return delivery;
}
} //namespace stellar7
