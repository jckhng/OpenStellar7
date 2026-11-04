#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include <array>
#include <algorithm>
#include <iomanip>
#include <optional>
#include <iostream>
#include <sstream>
#include <type_traits>

// Read-only physical owners, including inactive records and aliases.
namespace stellar7 {
struct GameplayModelTestAccess {
    static std::pair<bool,Level1Entity> physical(const GameplayModel& model,const std::string& pool,const std::size_t slot) {
        if (pool=="player") {return {true,model.level2Player()};}
        if (pool=="FF") {return {model.level2ActorPoolActive_.at(slot),model.level2ActorPool_.at(slot)};}
        if (pool=="FE") {return {model.level2FixedPoolKnown_.at(slot),model.level2FixedPool_.at(slot)};}
        if (pool=="FD") {return {model.level2ProjectilePoolActive_.at(slot),model.level2ProjectilePool_.at(slot)};}
        if (pool=="FC") {return {model.level2DeathEffectPoolActive_.at(slot),model.level2DeathEffectPool_.at(slot)};}
        throw std::runtime_error("unknown historical physical pool");
    }
    static std::pair<unsigned,unsigned> counts(const GameplayModel& model) {
        return {model.level2ActiveEnemyCount_,model.level2SpawnedEnemyCount_};
    }
    template<class Proxy> static void compareProxy(const Proxy& a,const Proxy& b) {
        if constexpr (std::is_same_v<Proxy,FixedPosition>) {
            test::requireCampaignComparison(a==b,"budget composition changed canonical proxy XYZ");
        } else {
            static_assert(std::is_same_v<Proxy,Level1Entity>,"update full canonical proxy comparison");
            test::requireCampaignComparison(test::campaignActorFields(a)==test::campaignActorFields(b),
                "budget composition changed full canonical proxy actor");
        }
    }
    static auto effectFields(const PlayableSystemEffect& effect) {
        std::array<std::uint64_t,4> result{{effect.index(),0,0,0}};
        std::visit([&](const auto& value) {
            using Effect=std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Effect,SystemAudio>) {
                result[1]=value.event;
            } else if constexpr (std::is_same_v<Effect,SystemDecoyStrip>
                || std::is_same_v<Effect,SystemDecoyPalette>) {
                result[1]=static_cast<unsigned>(value.mode);
            } else {
                result[1]=value.system;
                if constexpr (std::is_same_v<Effect,SystemBorder>) {result[2]=value.selected;}
                else if constexpr (std::is_same_v<Effect,SystemPanel>) {
                    result[2]=static_cast<unsigned>(value.style);result[3]=value.remainingCharges;
                } else {static_assert(std::is_same_v<Effect,SystemCopy>,"update pending system-effect comparison");}
            }
        },effect);
        return result;
    }
    static void compare(const GameplayModel& a, const GameplayModel& b) {
        const auto pool = [](const auto& x, const auto& y) {
            test::requireCampaignComparison(x.size()==y.size(),"budget composition changed pending actor count");
            for (std::size_t i = 0; i < x.size(); ++i) {
                test::requireCampaignComparison(test::campaignActorFields(x[i]) == test::campaignActorFields(y[i]),
                    "replay budget changed a physical pool record");
            }
        };
        pool(a.level2ActorPool_, b.level2ActorPool_); pool(a.level2FixedPool_, b.level2FixedPool_);
        pool(a.level2ProjectilePool_, b.level2ProjectilePool_); pool(a.level2DeathEffectPool_, b.level2DeathEffectPool_);
        pool(a.level2PendingTerminalEffectPresentations_,b.level2PendingTerminalEffectPresentations_);
        compareProxy(a.campaignPlayerProxy_,b.campaignPlayerProxy_);
        test::requireCampaignComparison(a.playableSystemEffects_.size()==b.playableSystemEffects_.size(),
            "budget composition changed pending system-effect count");
        for (std::size_t i=0;i<a.playableSystemEffects_.size();++i) {
            test::requireCampaignComparison(effectFields(a.playableSystemEffects_[i])==effectFields(b.playableSystemEffects_[i]),
                "budget composition changed pending system effect");
        }
        test::requireCampaignComparison(std::tie(a.level2ActorPoolActive_,a.level2FixedPoolKnown_,
            a.level2ProjectilePoolActive_,a.level2DeathEffectPoolActive_,a.level2EntityActorPoolSlots_,
            a.level2EntityFixedPoolSlots_,a.level2EntityProjectilePoolSlots_,a.level2EntityDeathEffectPoolSlots_,
            a.level2ActiveEnemyCount_,a.level2SpawnedEnemyCount_,a.playableSystemOldActorSlots_,a.level6GuardianTurn_,
            a.level2PendingTerminalEffectSlots_,a.level2DeathEffectPoolPendingPresenterRetirement_) == std::tie(b.level2ActorPoolActive_,b.level2FixedPoolKnown_,
            b.level2ProjectilePoolActive_,b.level2DeathEffectPoolActive_,b.level2EntityActorPoolSlots_,
            b.level2EntityFixedPoolSlots_,b.level2EntityProjectilePoolSlots_,b.level2EntityDeathEffectPoolSlots_,
            b.level2ActiveEnemyCount_,b.level2SpawnedEnemyCount_,b.playableSystemOldActorSlots_,b.level6GuardianTurn_,
            b.level2PendingTerminalEffectSlots_,b.level2DeathEffectPoolPendingPresenterRetirement_), "replay budget changed pool ownership/quota");
    }
};
}

#include "returned_title_menu_extension.hpp"

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
using Row = std::vector<std::string>;
std::vector<Row> csv(const std::filesystem::path& path) {
    std::ifstream file(path); requireCampaignComparison(bool(file), "missing replay CSV");
    std::vector<Row> result; std::string line; std::getline(file,line);
    while (std::getline(file,line)) {
        if (!line.empty() && line.back()=='\r') { line.pop_back(); }
        Row row; std::size_t start=0;
        for (;;) {
            const auto end=line.find(',',start); row.push_back(line.substr(start,end-start));
            if (end==std::string::npos) { break; }
            start=end+1;
        }
        result.push_back(std::move(row));
    }
    return result;
}
std::uint64_t number(const Row& row, const std::size_t column) { return std::stoull(row.at(column)); }
std::uint64_t field(const std::string& json, const char* key) {
    const auto token=std::string("\"")+key+"\":";
    const auto start=json.find(token); requireCampaignComparison(start!=std::string::npos,"missing reference event field");
    return std::stoull(json.substr(start+token.size()));
}
std::string firstLine(const std::filesystem::path& path) {
    std::ifstream file(path); std::string line; std::getline(file,line);
    if (!line.empty() && line.back()=='\r') { line.pop_back(); }
    requireCampaignComparison(!line.empty(),"missing reference prefix"); return line;
}
GameplayInput input(const Row& row) {
    requireCampaignComparison(row.size()==7 && number(row,4)<64 && number(row,5)<=2,"invalid replay input row");
    const auto bits=number(row,4); GameplayInput value{};
    value.left=bits&1; value.right=bits&2; value.forward=bits&4; value.reverse=bits&8;
    value.fire=bits&16; value.confirm=bits&32;
    value.systemAction=static_cast<GameplaySystemAction>(number(row,5)); return value;
}
NameEntryInput text(const Row& row) {
    NameEntryInput result{}; const auto& hex=row.at(6);
    requireCampaignComparison(hex.size()%2==0,"invalid replay name hex");
    for (std::size_t i=0;i<hex.size();i+=2) {
        result.characters.push_back(static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16)));
    }
    return result;
}
void beginRight(RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
    unsigned menuPoll=0;
    for (unsigned call=0;!frontend.session() && call<10000;++call) {
        NativeMenuInput menu{};
        if (frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision) { menu.confirm=menuPoll++==1; }
        const auto before=frontend.elapsedSamples(); const auto delivery=frontend.advanceStartup(menu);
        requireCampaignComparison(delivery.firstSample==before && before+delivery.samples.size()==frontend.elapsedSamples(),
            "right startup lost continuous PCM");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    requireCampaignComparison(frontend.session() && menuPoll==3,"right actual Begin missed initial source preloop");
}
void compareSourcePreloop(const Level1State& a,const Level1State& b) {
    requireCampaignComparison(a.levelIndex()==b.levelIndex() && a.loopTick()==b.loopTick()
        && a.score()==b.score() && a.playerArmor()==b.playerArmor() && a.activeListOrder()==b.activeListOrder()
        && a.ground().points()==b.ground().points() && a.entities().size()==b.entities().size()
        && test::campaignActorFields(a.player())==test::campaignActorFields(b.player()),
        "startup budgets changed initial source preloop");
    for (std::size_t i=0;i<a.entities().size();++i) {
        requireCampaignComparison(test::campaignActorFields(a.entities()[i])==test::campaignActorFields(b.entities()[i]),
            "startup budgets changed source constructor actor");
    }
    test::compareCampaignRng(a.rng(),b.rng(),test::CampaignComparisonStage::boundary);
}

//AD04 is a partial historical gameplay/event witness. New display/audio output
//compares fully between current budgets; no AD04 pixels or PCM are replaced.
class Historical {
public:
    explicit Historical(const std::filesystem::path& folder):models_(csv(folder/"semantic-boundaries.csv")),
        actors_(csv(folder/"physical-actors.csv")),sources_(csv(folder/"source-witnesses.csv")),
        events_(csv(folder/"ordered-events.csv")) {}
    void delivery(const NativeSessionDelivery& delivery,const unsigned call,const RecoveredFrontend& frontend) {
        requireCampaignComparison(delivery.firstSample==samples_,"replay lost continuous startup PCM");
        samples_+=delivery.samples.size();
        for (const auto& event:delivery.events) {
            requireCampaignComparison(eventCursor_<events_.size(),"extra historical event");
            const auto& row=events_[eventCursor_];
            requireCampaignComparison(number(row,0)==eventCursor_++ && number(row,1)==call
                && number(row,2)==(frontend.session()?frontend.session()->model().ticks():0)
                && number(row,3)==event.loopTick && number(row,4)==event.eventId,
                "historical ordered gameplay event diverged");
        }
    }
    void boundary(const unsigned call,const NativeGameplaySession& session,const std::uint32_t oldScore) {
        const auto& model=session.model();
        while (modelCursor_<models_.size() && number(models_[modelCursor_],0)==call) {
            const auto& row=models_[modelCursor_++];
            requireCampaignComparison(number(row,1)==static_cast<unsigned>(model.phase()) && number(row,2)==model.campaignLevel()+1
                && number(row,3)==model.level2LoopTick() && number(row,4)==model.ticks()
                && std::stoll(row.at(5))==model.playerArmor() && number(row,6)==model.score()
                && number(row,7)==model.remainingContinues() && number(row,8)==model.level2Rng().calls(),
                "historical semantic boundary diverged");
        }
        while (actorCursor_<actors_.size() && number(actors_[actorCursor_],0)==call) {
            const auto& row=actors_[actorCursor_++];
            const auto value=GameplayModelTestAccess::physical(model,row.at(1),number(row,2));
            const auto& a=value.second;
            //Only previously captured fields. In particular, no old Type24
            //modelAnimationVariant or stateBytes[3] value was captured/inferred.
            std::vector<std::string> actual;
            const auto add=[&](const auto item) {actual.push_back(std::to_string(item));};
            add(value.first);add(a.position.x);add(a.position.y);add(a.position.z);
            add(a.allocationIndex);add(a.type);add(a.stateBytes4dTo52[0]);add(a.stateBytes4dTo52[1]);
            add(a.stateBytes4dTo52[2]);add(a.stateBytes4dTo52[4]);add(a.stateBytes4dTo52[5]);add(a.flags);
            add(a.collisionRadius);add(a.yaw);add(a.forwardVelocity);add(a.yawVelocity);add(a.pitch);add(a.roll);
            add(a.pitchVelocity);add(a.rollVelocity);add(a.xVelocity);add(a.yVelocity);add(a.zVelocity);add(a.weaponCooldown);
            add(a.ownerIsPlayer);add(a.ownerAllocationIndex);add(a.collisionContactIsPlayer);add(a.collisionContactAllocationIndex);
            requireCampaignComparison(row.size()==actual.size()+3 && std::equal(actual.begin(),actual.end(),row.begin()+3),
                "historical captured physical actor diverged");
        }
        while (sourceCursor_<sources_.size() && number(sources_[sourceCursor_],0)==call) {
            const auto& row=sources_[sourceCursor_++];const auto counts=GameplayModelTestAccess::counts(model);
            requireCampaignComparison(number(row,1)==model.campaignLevel()+1 && number(row,2)==static_cast<unsigned>(model.phase())
                && number(row,3)==model.level2LoopTick() && number(row,4)==counts.first && number(row,5)==counts.second
                && std::stoll(row.at(6))==model.playerArmor() && number(row,7)==model.remainingContinues()
                && std::stoll(row.at(8))==static_cast<std::int64_t>(model.score())-oldScore,
                "historical quota/guardian/reward witness diverged");
        }
    }
    bool hasBoundary(const unsigned call) const {
        return (modelCursor_<models_.size() && number(models_[modelCursor_],0)==call)
            || (actorCursor_<actors_.size() && number(actors_[actorCursor_],0)==call)
            || (sourceCursor_<sources_.size() && number(sources_[sourceCursor_],0)==call);
    }
    std::size_t eventCount() const noexcept {return eventCursor_;}
    void complete() const {
        requireCampaignComparison(eventCursor_==events_.size() && eventCursor_==844 && modelCursor_==models_.size()
            && actorCursor_==actors_.size() && sourceCursor_==sources_.size(),"historical witnesses not consumed exactly");
    }
private:
    std::vector<Row> models_,actors_,sources_,events_;
    std::size_t modelCursor_=0,actorCursor_=0,sourceCursor_=0,eventCursor_=0;
    std::uint64_t samples_=0;
};

void replay(const std::filesystem::path& resources,const std::filesystem::path& fixture) {
    auto data=naturalRoute::assets(resources);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(resources/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(resources/"STELLAR.RES"),{'V','G','A',':'});
    requireCampaignComparison(vga.size()==1,"missing actual VGA driver");
    const auto copy=decodeFrameCopyProfile(vga.front());
    auto left=std::make_unique<RecoveredFrontend>(data,startup,copy,10000,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    auto right=std::make_unique<RecoveredFrontend>(data,startup,copy,257,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    Historical reference(fixture); test::CampaignBudgetComparison comparison;
    beginRight(*right,comparison.rightPcm);
    const NativeGameplaySession* leftOwner=nullptr; const NativeGameplaySession* rightOwner=nullptr;
    unsigned startupCalls=0,gameplayCalls=0,defeatAwards=0;
    std::size_t boundaryChecks=0,presentations=0;
    std::array<unsigned,7> guardianDamage{};
    std::optional<StellarRng> endingRng; std::optional<unsigned> endingWorld;
    for (const auto& row:csv(fixture/"inputs.csv")) {
        const auto call=static_cast<unsigned>(number(row,1)); const auto control=input(row); const auto name=text(row);
        NativeSessionDelivery a,b;
        const auto oldScore=leftOwner?leftOwner->model().score():0;
        const auto previousPhase=leftOwner?std::optional<GameplayPhase>(leftOwner->model().phase()):std::nullopt;
        const auto previousBank=leftOwner?leftOwner->model().campaignLevel():0;
        if (row[0]=="startup") {
            requireCampaignComparison(call==++startupCalls && !left->session()
                && number(row,2)==static_cast<unsigned>(left->startup().phase()),"startup replay phase drift");
            NativeMenuInput menu{}; menu.confirm=control.confirm;
            a=left->advanceStartup(menu);
        } else {
            requireCampaignComparison(row[0]=="gameplay" && call==++gameplayCalls && left->session() && right->session(),
                "invalid gameplay replay ordering");
            if (!leftOwner) {
                leftOwner=left->session(); rightOwner=right->session();
                reference.boundary(0,*leftOwner,leftOwner->model().score());
            }
            const auto& model=leftOwner->model();
            requireCampaignComparison(number(row,2)==static_cast<unsigned>(model.phase()) && number(row,3)==model.campaignLevel()+1,
                "gameplay replay phase/bank drift");
            unsigned oldHealth=0;
            const auto bank=model.campaignLevel();
            if (bank>=3 && bank<=6) {
                for (const auto& actor:model.level2Entities()) {
                    if (actor.active() && actor.type==44 && actor.stateBytes4dTo52[0]==0xff && actor.stateBytes4dTo52[4]==5) {
                        oldHealth=actor.stateBytes4dTo52[1];
                    }
                }
            }
            if (call==12683) {
                requireCampaignComparison(control.confirm && leftOwner->highscoreNameEntry()
                    && leftOwner->highscoreNameEntry()->name()=="NATURAL", "name Enter did not submit actual NATURAL");
            }
            a=left->advanceGameplay(control,{},name); b=right->advanceGameplay(control,{},name);
            const auto& after=leftOwner->model();
            if (call==12682) {
                requireCampaignComparison(leftOwner->highscoreNameEntry() && leftOwner->highscoreNameEntry()->name()=="NATURAL"
                    && rightOwner->highscoreNameEntry() && rightOwner->highscoreNameEntry()->name()=="NATURAL",
                    "recorded text did not enter actual NATURAL");
            }
            if (oldHealth) {
                unsigned health=0;
                for (const auto& actor:after.level2Entities()) {
                    if (actor.active() && actor.type==44 && actor.stateBytes4dTo52[0]==0xff && actor.stateBytes4dTo52[4]==5) {
                        health=actor.stateBytes4dTo52[1];
                    }
                }
                requireCampaignComparison(health<=oldHealth,"actual guardian health increased"); guardianDamage.at(bank)+=oldHealth-health;
                if (health==0) {
                    requireCampaignComparison(after.score()-oldScore==1000,"final defeat score changed"); ++defeatAwards;
                }
            }
            if (after.phase()==GameplayPhase::endingEntry && !endingRng) {
                endingRng=after.level2Rng(); endingWorld=after.level2LoopTick();
            }
            if (endingRng) {
                test::compareCampaignRng(*endingRng,after.level2Rng(),test::CampaignComparisonStage::boundary);
                requireCampaignComparison(*endingWorld==after.level2LoopTick(),"ending advanced retained world");
            }
        }
        reference.delivery(a,call,*left);
        presentations+=a.entry.size()+a.presentations.size();
        if (row[0]=="startup") {
            comparison.leftPcm.insert(comparison.leftPcm.end(),a.samples.begin(),a.samples.end());
            if (left->session()) {
                requireCampaignComparison(comparison.leftPcm==comparison.rightPcm,"startup budgets changed whole PCM");
                NativeSessionDelivery boundary{}; boundary.firstSample=left->elapsedSamples();
                comparison.compare(*left->session(),*right->session(),boundary,boundary,test::CampaignComparisonStage::boundary);
                ++boundaryChecks;
                GameplayModelTestAccess::compare(left->session()->model(),right->session()->model());
                compareSourcePreloop(left->session()->model().level1State(),right->session()->model().level1State());
            }
        } else {
            const auto& model=leftOwner->model();
            const bool boundary=!previousPhase || *previousPhase!=model.phase() || previousBank!=model.campaignLevel()
                || reference.hasBoundary(call) || endingRng.has_value() || call==12687;
            comparison.compare(*left->session(),*right->session(),a,b,boundary
                ?test::CampaignComparisonStage::boundary:test::CampaignComparisonStage::frame);
            boundaryChecks+=boundary;
            GameplayModelTestAccess::compare(left->session()->model(),right->session()->model());
            requireCampaignComparison(left->elapsedSamples()==right->elapsedSamples(),"frontend sample epoch drift");
        }
        if (row[0]=="gameplay") {reference.boundary(call,*leftOwner,oldScore);}
        if (leftOwner) {
            requireCampaignComparison(left->session()==leftOwner && right->session()==rightOwner
                && leftOwner->model().remainingContinues()==2 && leftOwner->model().playerArmor()>0,
                "route changed owner or consumed armor/continue stock");
        }
    }
    requireCampaignComparison(startupCalls==118 && gameplayCalls==12687 && guardianDamage[3]==40 && guardianDamage[4]==50
        && guardianDamage[5]==50 && guardianDamage[6]==20 && defeatAwards==4
        && endingRng && leftOwner && leftOwner->model().phase()==GameplayPhase::nativeMenu
        && leftOwner->model().score()==6545 && leftOwner->model().level2LoopTick()==3341
        && leftOwner->model().playerArmor()==456 && leftOwner->model().level1State().systemStatusLevels()[2]==1,
        "actual public fullroute missed earned ending/title");
    reference.complete();
    std::cout<<"{\"route\":\"actual-startup/blue1/GREEN4/5/6/7/SCENED/title\",\"calls\":"<<gameplayCalls
        <<",\"samples\":"<<comparison.leftPcm.size()<<",\"events\":"<<reference.eventCount()
        <<",\"oplWrites\":"<<leftOwner->synth().registerWrites().size()<<",\"leftPresentations\":"<<presentations
        <<",\"fullHistoryBoundaryChecks\":"<<boundaryChecks<<",\"gameplayFramePageComparisons\":"<<gameplayCalls<<"}\n";
    test::appendReturnedTitleMenu(*left,*right,comparison,data);
}
}
int main(int argc,char** argv) {
    if (argc!=3) { return 2; }
    try { replay(argv[1],argv[2]); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
