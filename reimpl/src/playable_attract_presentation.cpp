#include "playable_attract.hpp"
#include "normal_work_packets.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon=120000,deviceHorizon=2*workHorizon;
constexpr std::size_t maximumWaitPolls=512;
std::int64_t paletteHorizon(const SoundBlasterClockPhase& phase) {
    return phase.callback && phase.callback->program ? deviceHorizon : workHorizon;
}
}
AttractPresentation::AttractPresentation(PlayableAttractOwners owners,NativeSessionDelivery& delivery,const std::size_t packetBudget,
    const NativeDeliverySink& sink):o(owners),budget(packetBudget),delivery_(delivery),sink_(sink),stream_(sink_) {
    if(!o.phase.playableAutomaticAttract || !o.phase.timer || !o.phase.callback || packetBudget==0 || packetBudget>10000
        || o.assets.cockpitDacPalette.size()!=vgaPaletteBytes) {
        throw std::invalid_argument("attract requires its explicit retained native owners and bounded work");
    }
    //A caller joining an existing receipt streams its prefix before this borrow.
    stream_.adopt(delivery_);
}
void AttractPresentation::append(ClockedPresentationWork work) {
    if(work.audio.firstSample!=delivery_.firstSample+delivery_.samples.size()) {throw std::logic_error("attract lost retained PCM epoch");}
    o.phase=work.timing.phase;
    delivery_.samples.insert(delivery_.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    if(o.synth.sampledVoiceObservation().phase==SampledVoicePhase::irqPending) {o.synth.serviceSampledVoiceIrq();}
    stream_.publish(delivery_);
}
void AttractPresentation::append(NativeSessionDelivery work) {
    if(work.firstSample!=delivery_.firstSample+delivery_.samples.size()) {throw std::logic_error("attract child lost retained PCM epoch");}
    delivery_.samples.insert(delivery_.samples.end(),work.samples.begin(),work.samples.end());
    delivery_.presentations.insert(delivery_.presentations.end(),work.presentations.begin(),work.presentations.end());
    stream_.publish(delivery_);
}
void AttractPresentation::normal(const std::vector<NormalWorkOperation>& operations) {
    for(const auto& packet:normalWorkPackets(operations,budget)) {append(o.clock.renderWork(o.phase,packet,workHorizon));}
}
void AttractPresentation::store(const std::uint16_t raw) {normal({{NormalWorkKind::playableAttractCountdownStore,1,raw}});}
std::uint16_t AttractPresentation::read(const NormalWorkKind kind) {
    auto work=o.clock.renderWork(o.phase,{{kind,1}},workHorizon);
    const auto value=kind==NormalWorkKind::playableAttractCreditsPoll ? work.timing.creditsCountdownRead : work.timing.countdownLastRead;
    if(!value) {throw std::logic_error("attract lost its actual countdown read");}
    append(std::move(work));return *value;
}
bool AttractPresentation::wait() {
    //Each poll executes five dispatches; leave room for the retained IRQ callback.
    auto work=o.clock.renderWork(o.phase,{{NormalWorkKind::playableAttractWaitPoll,
        std::min(budget,maximumWaitPolls)}},workHorizon);
    const auto done=work.timing.countdownReturned;append(std::move(work));return done;
}
void AttractPresentation::publish() {
    delivery_.presentations.push_back({o.clock.elapsedSamples(),{o.pixels.snapshot(o.pages.frontPage()),expandVgaDacPalette(
        std::vector<std::uint8_t>(o.palette.dac.colors.begin(),o.palette.dac.colors.end()))}});
    stream_.publish(delivery_);
}
void AttractPresentation::upload(const std::uint8_t intensity) {
    auto work=o.clock.renderPalette(o.phase,o.palette,{0,vgaPaletteColors,0,intensity},paletteHorizon(o.phase));
    o.palette=work.state;append(std::move(work.work));publish();
}
void AttractPresentation::palette(const std::vector<Rgb>& colors) {
    if(colors.size()!=vgaPaletteColors) {throw std::invalid_argument("attract requires an actual complete palette resource");}
    std::size_t index=0;
    for(const auto color:colors) {for(const auto value:{color.red,color.green,color.blue}) {o.palette.source[index++]=value>>2;}}
    upload(vgaComponentMaximum);
}
void AttractPresentation::cockpitPalette() {
    std::copy(o.assets.cockpitDacPalette.begin(),o.assets.cockpitDacPalette.end(),o.palette.source.begin());upload(vgaComponentMaximum);
}
void AttractPresentation::blackout() {
    const auto prior=o.pixels.snapshot(o.pages.frontPage());
    auto work=o.clock.renderBlackout(o.phase,{o.palette,{}},o.pages,o.pixels,paletteHorizon(o.phase),BlackoutPageLayout::recoveredStartup);
    for(auto& step:work.steps) {
        append(std::move(step.before));o.palette=step.palette.state;append(std::move(step.palette.work));
        delivery_.presentations.push_back({o.clock.elapsedSamples(),{prior,expandVgaDacPalette(
            std::vector<std::uint8_t>(o.palette.dac.colors.begin(),o.palette.dac.colors.end()))}});
    }
    o.palette=work.state.palette;append(std::move(work.clear));publish();
}
void AttractPresentation::fadeIn() {
    for(std::uint8_t intensity=0;intensity<=60;intensity+=4) {upload(intensity);}
}
void AttractPresentation::copy(const std::uint16_t from,const std::uint16_t to,const ProjectedPoint point,
    const std::uint16_t width,const std::uint16_t height) {
    o.pixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{from,to,point,width,height}});
}
void AttractPresentation::screen(const IndexedScreen& screen,const std::uint16_t page) {
    if(screen.pixels.size()!=320*200) {throw std::invalid_argument("attract requires an actual complete indexed screen");}
    for(std::int16_t y=0;y<200;++y) {for(std::int16_t x=0;x<320;++x) {o.pixels.write(page,{x,y},screen.pixels[y*320+x]);}}
}
}
