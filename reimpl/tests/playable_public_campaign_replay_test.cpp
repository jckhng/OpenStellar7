#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include <array>
#include <iomanip>
#include <optional>
#include <iostream>
#include <sstream>

// Read-only physical owners, including inactive records and aliases.
namespace stellar7 {
struct GameplayModelTestAccess {
    static void compare(const GameplayModel& a, const GameplayModel& b) {
        const auto pool = [](const auto& x, const auto& y) {
            for (std::size_t i = 0; i < x.size(); ++i) {
                test::requireCampaignComparison(test::campaignActorFields(x[i]) == test::campaignActorFields(y[i]),
                    "replay budget changed a physical pool record");
            }
        };
        pool(a.level2ActorPool_, b.level2ActorPool_); pool(a.level2FixedPool_, b.level2FixedPool_);
        pool(a.level2ProjectilePool_, b.level2ProjectilePool_); pool(a.level2DeathEffectPool_, b.level2DeathEffectPool_);
        test::requireCampaignComparison(std::tie(a.level2ActorPoolActive_,a.level2FixedPoolKnown_,
            a.level2ProjectilePoolActive_,a.level2DeathEffectPoolActive_,a.level2EntityActorPoolSlots_,
            a.level2EntityFixedPoolSlots_,a.level2EntityProjectilePoolSlots_,a.level2EntityDeathEffectPoolSlots_,
            a.level2ActiveEnemyCount_,a.level2SpawnedEnemyCount_) == std::tie(b.level2ActorPoolActive_,b.level2FixedPoolKnown_,
            b.level2ProjectilePoolActive_,b.level2DeathEffectPoolActive_,b.level2EntityActorPoolSlots_,
            b.level2EntityFixedPoolSlots_,b.level2EntityProjectilePoolSlots_,b.level2EntityDeathEffectPoolSlots_,
            b.level2ActiveEnemyCount_,b.level2SpawnedEnemyCount_), "replay budget changed pool ownership/quota");
    }
};
}

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
void sameBytes(const std::vector<std::uint8_t>& actual, const std::filesystem::path& path) {
    requireCampaignComparison(actual==naturalRoute::read(path),"replay changed a sealed page/palette snapshot");
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

class Reference {
public:
    explicit Reference(const std::filesystem::path& folder):prefix_(firstLine(folder/"reference-prefix.txt")),
        pcm_(prefix_+".pcm",std::ios::binary),opl_(prefix_+".opl.bin",std::ios::binary),
        events_(prefix_+".events.jsonl"),boundaries_(csv(folder/"gameplay-boundaries.csv")) {
        requireCampaignComparison(bool(pcm_) && bool(opl_) && bool(events_),"missing sealed reference streams");
    }
    void delivery(const NativeSessionDelivery& value,const unsigned call,const RecoveredFrontend& frontend) {
        requireCampaignComparison(value.firstSample==samples_,"replay lost continuous startup PCM");
        for (const auto sample:value.samples) {
            const auto word=static_cast<std::uint16_t>(sample);
            requireCampaignComparison(pcm_.get()==(word&255) && pcm_.get()==(word>>8),"replay differs from sealed whole PCM");
        }
        samples_+=value.samples.size();
        for (const auto& event:value.events) {
            std::string line; requireCampaignComparison(bool(std::getline(events_,line)),"replay emitted extra event");
            requireCampaignComparison(field(line,"ordinal")==eventCount_++ && field(line,"call")==call
                && field(line,"deliveryFirstSample")==value.firstSample && field(line,"deliverySampleEnd")==samples_
                && field(line,"modelTick")== (frontend.session()?frontend.session()->model().ticks():0)
                && field(line,"loopTick")==event.loopTick && field(line,"eventId")==event.eventId,
                "replay differs from sealed event delivery/order");
            defeatEvents_+=event.eventId==12;
        }
        if (!frontend.session()) { return; }
        const auto& writes=frontend.session()->synth().registerWrites();
        requireCampaignComparison(writes.size()>=oplCursor_,"replay replaced OPL owner");
        for (;oplCursor_<writes.size();++oplCursor_) {
            requireCampaignComparison(opl_.get()==writes[oplCursor_].registerNumber && opl_.get()==writes[oplCursor_].value,
                "replay differs from sealed ordered OPL");
        }
    }
    void boundary(const char* stage,const unsigned call,const RecoveredFrontend& frontend) {
        while (boundaryCursor_<boundaries_.size() && boundaries_[boundaryCursor_][0]==stage
            && number(boundaries_[boundaryCursor_],1)==call) {
            const auto& row=boundaries_[boundaryCursor_++]; requireCampaignComparison(row.size()==17,"invalid replay boundary");
            requireCampaignComparison(number(row,3)==frontend.elapsedSamples(),"reference boundary sample epoch changed");
            std::ostringstream name; name<<prefix_<<".boundary-"<<std::setfill('0')<<std::setw(4)<<number(row,2);
            sameBytes(frontend.frame().pixels,name.str()+".frame.bin");
            std::vector<std::uint8_t> palette;
            for (const auto color:frontend.frame().palette) { palette.insert(palette.end(),{color.red,color.green,color.blue}); }
            sameBytes(palette,name.str()+".palette-rgb.bin");
            if (row[4].empty()) { continue; }
            requireCampaignComparison(frontend.session()!=nullptr,"reference session missing");
            const auto& session=*frontend.session(); const auto& model=session.model(); const auto pages=session.pageObservation();
            requireCampaignComparison(number(row,4)==model.campaignLevel()+1 && number(row,5)==static_cast<unsigned>(model.phase())
                && number(row,6)==model.level2LoopTick() && number(row,7)==model.level2Rng().calls()
                && number(row,8)==static_cast<unsigned>(pages.projection) && number(row,9)==pages.cache
                && number(row,10)==pages.back && number(row,11)==pages.front && number(row,12)==pages.copy
                && number(row,13)==pages.draw,"reference model/page roles changed");
            constexpr std::array<NativePageRole,3> roles{{NativePageRole::cache,NativePageRole::back,NativePageRole::front}};
            constexpr std::array<const char*,3> names{{"cache","back","front"}};
            for (std::size_t i=0;i<roles.size();++i) {
                const auto page=session.retainedPagePixels(roles[i]);
                requireCampaignComparison(bool(page)==bool(number(row,14+i)),"reference page ownership changed");
                if (page) { sameBytes(*page,name.str()+"."+names[i]+".bin"); }
            }
        }
    }
    void complete() {
        requireCampaignComparison(samples_==3187646 && eventCount_==234 && defeatEvents_==1 && oplCursor_==17769
            && boundaryCursor_==100 && pcm_.peek()==EOF && opl_.peek()==EOF && events_.peek()==EOF,
            "fullroute replay did not consume the sealed reference exactly");
    }
private:
    std::string prefix_; std::ifstream pcm_,opl_,events_; std::vector<Row> boundaries_;
    std::uint64_t samples_=0; std::size_t oplCursor_=0,eventCount_=0,boundaryCursor_=0,defeatEvents_=0;
};

void replay(const std::filesystem::path& resources,const std::filesystem::path& fixture) {
    auto data=naturalRoute::assets(resources);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(resources/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(resources/"STELLAR.RES"),{'V','G','A',':'});
    requireCampaignComparison(vga.size()==1,"missing actual VGA driver");
    const auto copy=decodeFrameCopyProfile(vga.front());
    auto left=std::make_unique<RecoveredFrontend>(data,startup,copy,10000,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    auto right=std::make_unique<RecoveredFrontend>(data,startup,copy,257,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    Reference reference(fixture); test::CampaignBudgetComparison comparison;
    beginRight(*right,comparison.rightPcm);
    const NativeGameplaySession* leftOwner=nullptr; const NativeGameplaySession* rightOwner=nullptr;
    unsigned startupCalls=0,gameplayCalls=0,guardianDamage=0,defeatAwards=0;
    std::optional<StellarRng> endingRng; std::optional<unsigned> endingWorld;
    for (const auto& row:csv(fixture/"inputs.csv")) {
        const auto call=static_cast<unsigned>(number(row,1)); const auto control=input(row); const auto name=text(row);
        NativeSessionDelivery a,b;
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
                reference.boundary("gameplay",0,*left);
            }
            const auto& model=leftOwner->model();
            requireCampaignComparison(number(row,2)==static_cast<unsigned>(model.phase()) && number(row,3)==model.campaignLevel()+1,
                "gameplay replay phase/bank drift");
            unsigned oldHealth=0;
            if (model.campaignLevel()==6) {
                for (const auto& actor:model.level2Entities()) {
                    if (actor.active() && actor.type==44 && actor.stateBytes4dTo52[0]==0xff && actor.stateBytes4dTo52[4]==5) {
                        oldHealth=actor.stateBytes4dTo52[1];
                    }
                }
            }
            if (call==3900) {
                requireCampaignComparison(control.confirm && leftOwner->highscoreNameEntry()
                    && leftOwner->highscoreNameEntry()->name()=="NATURAL", "name Enter did not submit actual NATURAL");
            }
            const auto oldScore=model.score();
            a=left->advanceGameplay(control,{},name); b=right->advanceGameplay(control,{},name);
            const auto& after=leftOwner->model();
            if (call==3899) {
                requireCampaignComparison(leftOwner->highscoreNameEntry() && leftOwner->highscoreNameEntry()->name()=="NATURAL"
                    && rightOwner->highscoreNameEntry() && rightOwner->highscoreNameEntry()->name()=="NATURAL",
                    "reconstructed text did not enter actual NATURAL");
            }
            if (oldHealth) {
                unsigned health=0;
                for (const auto& actor:after.level2Entities()) {
                    if (actor.active() && actor.type==44 && actor.stateBytes4dTo52[0]==0xff && actor.stateBytes4dTo52[4]==5) {
                        health=actor.stateBytes4dTo52[1];
                    }
                }
                requireCampaignComparison(health<=oldHealth,"actual guardian health increased"); guardianDamage+=oldHealth-health;
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
        if (row[0]=="startup") {
            comparison.leftPcm.insert(comparison.leftPcm.end(),a.samples.begin(),a.samples.end());
            if (left->session()) {
                requireCampaignComparison(comparison.leftPcm==comparison.rightPcm,"startup budgets changed whole PCM");
                NativeSessionDelivery boundary{}; boundary.firstSample=left->elapsedSamples();
                comparison.compare(*left->session(),*right->session(),boundary,boundary,test::CampaignComparisonStage::boundary);
                GameplayModelTestAccess::compare(left->session()->model(),right->session()->model());
                compareSourcePreloop(left->session()->model().level1State(),right->session()->model().level1State());
            }
        } else {
            comparison.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
            GameplayModelTestAccess::compare(left->session()->model(),right->session()->model());
            requireCampaignComparison(left->elapsedSamples()==right->elapsedSamples(),"frontend sample epoch drift");
        }
        reference.boundary(row[0].c_str(),call,*left);
        if (leftOwner) {
            requireCampaignComparison(left->session()==leftOwner && right->session()==rightOwner
                && leftOwner->model().remainingContinues()==2 && leftOwner->model().playerArmor()==680,
                "route changed owner or consumed armor/continue stock");
        }
    }
    requireCampaignComparison(startupCalls==118 && gameplayCalls==3904 && guardianDamage==20 && defeatAwards==1
        && endingRng && leftOwner && leftOwner->model().phase()==GameplayPhase::nativeMenu
        && leftOwner->model().score()==1890 && leftOwner->model().level2LoopTick()==2362,
        "actual public fullroute missed earned ending/title");
    reference.complete();
    std::cout<<"Actual public startup/blue1->4->7/20HP/SCENED/title budget twins:3904calls/3187646samples\n";
}
}
int main(int argc,char** argv) {
    if (argc!=3) { return 2; }
    try { replay(argv[1],argv[2]); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
