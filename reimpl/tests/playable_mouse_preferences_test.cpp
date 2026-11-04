//Private resource-backed proposal. Root owns compilation/execution.
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        bytes=encodeHighscoreTable(table);++calls;
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    std::vector<std::uint8_t> bytes;
    unsigned calls=0;
};
std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& assets,const NativeStartupAssets& art,
    const FrameCopyProfile copy,const NativePointerCapability capability,std::shared_ptr<HighscoreStore> store={}) {
    auto frontend=std::make_unique<RecoveredFrontend>(assets,art,copy,10000,std::move(store),
        CampaignExecution::playable,capability);
    NativeSessionDelivery last;
    for(unsigned call=0;frontend->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
        last=frontend->advanceStartup({});
    }
    require(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision,"pointer cold startup missed menu");
    if(capability==NativePointerCapability::portablePointer
        &&frontend->startup().recoveredMenuResources()->configuration.options[0]!=0) {
        const auto target=frontend->startup().recoveredMenuEntry()->pointer;
        require(last.pointerPosition&&last.pointerPosition->x==target.x&&last.pointerPosition->y==target.y,
            "initial enabled mouse lost source5B9E host positioning feedback");
    }
    return frontend;
}
NativeMenuInput pointer(const ProjectedPoint point,const bool down) {
    NativeMenuInput input;input.pointer=NativePointerInput{point,down,false};return input;
}
void pointerBudgetTwins(const GameplayAssets& assets,const NativeStartupAssets& art,const FrameCopyProfile copy) {
    auto leftStore=std::make_shared<MemoryStore>(),rightStore=std::make_shared<MemoryStore>();
    RecoveredFrontend left(assets,art,copy,10000,leftStore,CampaignExecution::playable,NativePointerCapability::portablePointer);
    RecoveredFrontend right(assets,art,copy,257,rightStore,CampaignExecution::playable,NativePointerCapability::portablePointer);
    std::vector<std::int16_t> leftPcm,rightPcm;
    const auto collect=[](const RecoveredFrontend& frontend,const NativeSessionDelivery& delivery,std::vector<std::int16_t>& pcm) {
        require(delivery.firstSample==pcm.size()&&delivery.firstSample+delivery.samples.size()==frontend.elapsedSamples(),
            "pointer budget route lost contiguous whole PCM");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    };
    const auto cold=[&](RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
        NativeSessionDelivery last;
        for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
            last=frontend.advanceStartup({});collect(frontend,last,pcm);
        }
        require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,
            "portable pointer budget cold startup missed menu");
        require(last.pointerPosition&&last.pointerPosition->x==163&&last.pointerPosition->y==69,
            "portable pointer cold budget lost initial host position");
    };
    cold(left,leftPcm);cold(right,rightPcm);
    const auto sameStartup=[&]() {
        require(left.startup().phase()==right.startup().phase()&&left.elapsedSamples()==right.elapsedSamples()
            &&left.startup().countdown()==right.startup().countdown()
            &&left.startup().menu().selection()==right.startup().menu().selection(),
            "pointer budgets changed retained startup/menu state");
        const auto& a=left.startup().recoveredMenuResources()->configuration;
        const auto& b=right.startup().recoveredMenuResources()->configuration;
        require(a.viewSize==b.viewSize&&a.options==b.options,"pointer budgets changed canonical preferences");
        require(left.startup().recoveredSettings()->settings.capability==StartupCapabilityProfile::portablePointer
            &&right.startup().recoveredSettings()->settings.capability==StartupCapabilityProfile::portablePointer,
            "pointer budgets lost explicit native capability provenance");
        for(const auto page:{NativeMenuPage::cache,NativeMenuPage::back,NativeMenuPage::front}) {
            require(left.startup().menuPageSegment(page)==right.startup().menuPageSegment(page)
                &&left.startup().menuPagePixels(page)==right.startup().menuPagePixels(page),
                "pointer budgets changed retained physical menu pages");
        }
        test::compareCampaignFrame(left.frame(),right.frame());
        require(leftPcm==rightPcm,"pointer cold/menu budgets changed whole ordered PCM");
    };
    sameStartup();
    test::CampaignBudgetComparison campaign;
    const auto step=[&](const NativeMenuInput input=NativeMenuInput{}) {
        const auto a=left.advanceStartup(input),b=right.advanceStartup(input);
        collect(left,a,leftPcm);collect(right,b,rightPcm);
        require(a.firstSample==b.firstSample&&a.samples==b.samples&&a.exitRequested==b.exitRequested
            &&a.events.size()==b.events.size()&&a.entry.size()==b.entry.size()
            &&a.presentations.size()==b.presentations.size()&&a.pointerPosition.has_value()==b.pointerPosition.has_value(),
            "pointer menu/Begin budgets changed ordered delivery");
        if(a.pointerPosition) {require(a.pointerPosition->x==b.pointerPosition->x&&a.pointerPosition->y==b.pointerPosition->y,
            "pointer budgets changed host cursor feedback");}
        for(std::size_t i=0;i<a.events.size();++i) {
            require(a.events[i].loopTick==b.events[i].loopTick&&a.events[i].eventId==b.events[i].eventId,
                "pointer budgets changed ordered audio events");
        }
        for(std::size_t i=0;i<a.entry.size();++i) {
            require(a.entry[i].elapsedNanoseconds==b.entry[i].elapsedNanoseconds&&a.entry[i].samples==b.entry[i].samples,
                "pointer budgets changed ordered entry audio");
            test::compareCampaignFrame(a.entry[i].indexed,b.entry[i].indexed);
        }
        for(std::size_t i=0;i<a.presentations.size();++i) {
            require(a.presentations[i].sampleEnd==b.presentations[i].sampleEnd,
                "pointer budgets changed GUI presentation endpoints");
            test::compareCampaignFrame(a.presentations[i].indexed,b.presentations[i].indexed);
        }
        require((left.session()!=nullptr)==(right.session()!=nullptr),"pointer Begin budgets changed adoption boundary");
        if(left.session()) {campaign.compare(*left.session(),*right.session(),a,b,test::CampaignComparisonStage::boundary);}
        else {sameStartup();}
    };
    const auto click=[&](const ProjectedPoint point) {step(pointer(point,true));step(pointer(point,false));};
    step({true,false,false});step();step({false,true,false});step();
    require(left.startup().menu().selection()==0,"budget route did not rearm pointer-mode keyboard navigation");
    click({163,126});click({60,84}); //PREFS, MOUSE Off.
    require(left.startup().recoveredMenuResources()->configuration.options[0]==0,"budget route did not toggle MOUSE Off");
    step({false,false,true});step(); //Retained logical pointer + Enter turns MOUSE On without packets.
    require(left.startup().recoveredMenuResources()->configuration.options[0]==1,"budget route did not restore MOUSE On");
    click({163,170});
    require(leftStore->calls==1&&rightStore->calls==1&&leftStore->bytes==rightStore->bytes,
        "pointer budgets changed exact preference save/reload bytes");
    click({163,151});click({164,173}); //SCORES and return.
    click({163,101});click({248,172});click({67,172}); //Actual Briefing model page and return.
    step(pointer({319,196},false));click({163,69});
    require(left.startup().recoveredBegin()->inputProfile==NativeMenuBeginProfile::portablePointer
        &&right.startup().recoveredBegin()->inputProfile==NativeMenuBeginProfile::portablePointer,
        "budget Begin lost native pointer dispatch provenance");
    for(unsigned call=0;!left.session()&&call<64;++call) {step();}
    require(left.session()&&right.session(),"pointer budget Begin did not retain loader/audio owners");
    const auto a=left.advanceGameplay({}),b=right.advanceGameplay({});
    collect(left,a,leftPcm);collect(right,b,rightPcm);
    campaign.compare(*left.session(),*right.session(),a,b,test::CampaignComparisonStage::boundary);
    require(leftPcm==rightPcm,"pointer cold/menu/Begin/adoption budgets changed whole PCM");
}

void run(const std::filesystem::path& root) {
    auto assets=naturalRoute::assets(root);
    assets.cockpitSettings->options[0]=1;
    assets.highscoreTable->setConfiguration(*assets.cockpitSettings);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(drivers.at(0));
    auto store=std::make_shared<MemoryStore>();
    auto frontend=menu(assets,art,copy,NativePointerCapability::portablePointer,store);
    require(frontend->startup().recoveredMenuResources()->configuration.options[0]==1
        &&frontend->startup().recoveredSettings()->settings.capability==StartupCapabilityProfile::portablePointer,
        "actual declared pointer capability was normalized to absent or lost its native receipt");
    const auto poll=[&](NativeMenuInput input) {
        const auto sample=frontend->elapsedSamples();const auto delivery=frontend->advanceStartup(input);
        require(delivery.firstSample==sample&&sample+delivery.samples.size()==frontend->elapsedSamples(),
            "native pointer input lost PCM ownership");
        return delivery;
    };
    const auto click=[&](const ProjectedPoint point) {poll(pointer(point,true));return poll(pointer(point,false));};
    poll({true,false,false});poll({});
    require(frontend->startup().menu().selection()==4,"main Up did not wrap BEGIN to EXIT");
    poll({false,true,false});poll({});
    require(frontend->startup().menu().selection()==0,"released pointer-mode poll did not rearm main keyboard navigation");
    NativeMenuInput cancel;
    cancel.up=cancel.down=cancel.confirm=cancel.left=cancel.right=cancel.cancel=true;
    cancel.pointer=NativePointerInput{{0,0},true,true,NativePointerPhase::cancel};
    const auto cancelRelease=[&](const ProjectedPoint point,const char* message) {
        poll(pointer(point,false));const auto idle=frontend->frame().pixels;
        poll(pointer(point,true));
        require(frontend->frame().pixels!=idle,"focus regression did not paint held widget");
        const auto cancelled=poll(cancel);
        require(!cancelled.exitRequested&&cancelled.pointerPosition
            &&cancelled.pointerPosition->x==point.x&&cancelled.pointerPosition->y==point.y
            &&frontend->frame().pixels==idle,message);
        require(!poll(pointer(point,false)).exitRequested&&frontend->frame().pixels==idle,message);
    };
    cancelRelease({163,176},"focus cancel activated main EXIT, changed focus, or moved retained cursor");
    require(frontend->startup().menu().selection()==0,"focus cancel consumed navigation keys");
    poll({false,false,true});poll(cancel);
    require(!poll({}).exitRequested,"cancelled held Enter activated retained EXIT");
    poll(pointer({163,126},false));poll({false,false,true});poll({});
    //No pointer sample: Enter must hit the retained free PREFS cursor, not keyboard BEGIN focus.
    cancelRelease({60,84},"focus cancel changed PREFS or closed its modal");
    require(frontend->startup().recoveredMenuResources()->configuration.options[0]==1,
        "focus-cancelled MOUSE press changed canonical configuration");
    click({60,84});
    require(frontend->startup().recoveredMenuResources()->configuration.options[0]==0,
        "MOUSE On->Off did not update canonical configuration");
    const auto sounds=frontend->startup().recoveredMenuResources()->configuration.options[3];
    click({178,113});
    require(frontend->startup().recoveredMenuResources()->configuration.options[3]==sounds
        &&frontend->startup().recoveredMenuResources()->configuration.options[0]==0,
        "disabled mouse still changed widgets");
    poll({false,false,true});poll({}); //Enter at retained logical MOUSE position, with no pointer sample.
    require(frontend->startup().recoveredMenuResources()->configuration.options[0]==1,
        "retained source pointer/Enter could not turn mouse back On");
    poll({true,false,false}); //Source keyboard focus remains EXIT after mouse activation; Up selects STORY.
    const auto feedback=poll({});
    require(feedback.pointerPosition&&feedback.pointerPosition->x==178&&feedback.pointerPosition->y==142,
        "mouse activation changed keyboard focus or keyboard navigation lost source5B9E feedback");
    click({163,170});
    require(store->calls==1&&!store->bytes.empty(),"pointer preferences EXIT did not save/reload once");
    const auto original=encodeHighscoreTable(*assets.highscoreTable);
    require(std::equal(original.begin()+highscoreHeaderBytes,original.end(),store->bytes.begin()+highscoreHeaderBytes),
        "mouse preference save changed score/name records");
    click({163,151}); //SCORES.
    cancelRelease({164,173},"focus-cancelled SCORES press returned to main");
    const auto hall=frontend->frame().pixels;
    click({0,0});poll(pointer({164,173},false));
    require(frontend->frame().pixels==hall,"outside Hall click activated CONTINUE");
    click({164,173});require(frontend->startup().menu().selection()==0,"Scores pointer CONTINUE did not return");
    click({163,101}); //BRIEFING.
    cancelRelease({248,172},"focus-cancelled BRIEFING press advanced or closed its page");
    const auto briefing=frontend->frame().pixels;
    click({248,172});require(frontend->frame().pixels!=briefing,"Briefing pointer CONTINUE did not advance real page");
    click({67,172});require(frontend->startup().menu().selection()==0,"Briefing pointer EXIT did not return");
    const auto edge=poll(pointer({319,196},false));
    require(edge.pointerPosition&&edge.pointerPosition->x==319&&edge.pointerPosition->y==196
        &&frontend->frame().pixels.size()==320U*200U,"legal source pointer edge was rejected or damaged page bounds");
    click({163,69});
    const auto* begin=frontend->startup().recoveredBegin();
    require(begin&&begin->inputProfile==NativeMenuBeginProfile::portablePointer
        &&begin->polls[0].sampleEnd==0&&begin->polls[1].sampleEnd==0,
        "pointer Begin fabricated Enter press/release work receipts");
    for(unsigned call=0;!frontend->session()&&call<10000;++call) {frontend->advanceStartup({});}
    require(frontend->session()!=nullptr,"pointer-enabled Begin did not retain actual Level1 loader owners");
    frontend->advanceGameplay({});
    auto saved=assets;saved.highscoreTable=decodeHighscoreTable(store->bytes);
    saved.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(store->bytes.begin(),store->bytes.begin()+highscoreHeaderBytes));
    auto present=menu(saved,art,copy,NativePointerCapability::portablePointer);
    auto absent=menu(saved,art,copy,NativePointerCapability::unavailable);
    require(present->startup().recoveredMenuResources()->configuration.options[0]==1
        &&absent->startup().recoveredMenuResources()->configuration.options[0]==0,
        "saved MOUSE On did not preserve actual capability or normalize absent cold host");
    const auto clickPresent=[&](const ProjectedPoint point) {
        present->advanceStartup(pointer(point,true));present->advanceStartup(pointer(point,false));
    };
    clickPresent({163,126});clickPresent({60,84}); //MOUSE Off.
    present->advanceStartup({true,false,false});present->advanceStartup({}); //EXIT keyboard focus -> STORY.
    present->advanceStartup({false,true,false});present->advanceStartup({}); //STORY -> EXIT.
    present->advanceStartup({false,false,true});present->advanceStartup({}); //Save/return BEGIN.
    require(present->startup().recoveredMenuResources()->configuration.options[0]==0,"native mouse-off UI state lost");
    present->advanceStartup({false,false,true});present->advanceStartup({}); //No samples; real retained cursor Enter.
    require(present->startup().recoveredBegin()
        &&present->startup().recoveredBegin()->inputProfile==NativeMenuBeginProfile::portablePointer,
        "capable MOUSE Off with absent packets fell back to first-widget Enter receipts");
    bool rejected=false;
    try {RecoveredFrontend strict(saved,art,copy,10000,{},CampaignExecution::sourceWork,NativePointerCapability::portablePointer);}
    catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"strict sourceWork admitted a portable pointer capability");
    pointerBudgetTwins(assets,art,copy);
    std::cout<<"MOUSE canonical toggle/native hits/save/cold capability/firstBegin/source rejection PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
