#include "stellar7/success_initial_work.hpp"
#include <cassert>
#include <stdexcept>
#include <vector>

using namespace stellar7;

namespace {
SuccessControlInput fixture(const SuccessPaletteChannel channel,const std::uint16_t front) {
    const auto back=static_cast<std::uint16_t>(front==0xa000 ? 0xa800 : 0xa000);
    return {{SuccessControlStage::inactive,channel,SuccessPaletteCompletionSource::soundHandle},
        {back,front,0xa400,front,back},0,true};
}
std::uint64_t dispatches(const SuccessControlWork& work) {
    std::uint64_t result=0;
    for(const auto& step:work.steps) {for(const auto& op:step.operations) {assert(op.kind==NormalWorkKind::instructions);result+=op.count;}}
    return result;
}
std::vector<SuccessControlChild> children(const SuccessControlWork& work) {
    std::vector<SuccessControlChild> result;
    for(const auto& step:work.steps) {if(step.child) {result.push_back(*step.child);}}
    return result;
}
template<class Function> void rejects(Function function) {
    bool rejected=false;
    try {function();} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
}

namespace {
void sameStep(const stellar7::SuccessControlStep& a,const stellar7::SuccessControlStep& b) {
    assert(a.operations.size()==b.operations.size() && a.stores.size()==b.stores.size() && a.child.has_value()==b.child.has_value());
    for (std::size_t i=0;i<a.operations.size();++i) {
        assert(a.operations[i].kind==b.operations[i].kind && a.operations[i].count==b.operations[i].count
            && a.operations[i].value==b.operations[i].value);
    }
    for (std::size_t i=0;i<a.stores.size();++i) {
        assert(a.stores[i].kind==b.stores[i].kind && a.stores[i].value==b.stores[i].value && a.stores[i].sourcePc==b.stores[i].sourcePc);
    }
    if (a.child) {
        assert(a.child->kind==b.child->kind && a.child->sourceCaller==b.child->sourceCaller
            && a.child->sourceSegment==b.child->sourceSegment && a.child->sourceEntry==b.child->sourceEntry
            && a.child->sourceReturn==b.child->sourceReturn && a.child->arguments==b.child->arguments
            && a.child->guestElapsedQualified==b.child->guestElapsedQualified);
    }
}
void composition() {
    for (const auto channel:{SuccessPaletteChannel::green,SuccessPaletteChannel::blue}) {
        for (const auto front:{0xa000,0xa800}) {
            const auto input=fixture(channel,static_cast<std::uint16_t>(front));
            const auto whole=successEntryOperations(input);
            const auto prefix=successInitialEntryOperations(input);
            assert(prefix.control.stage==SuccessControlStage::awaitingPalette);
            assert(prefix.steps.back().child && prefix.steps.back().child->sourceEntry==0x705c);
            assert(children(prefix).size()==7 && dispatches(prefix)==854);
            assert(prefix.initializedPalette && prefix.paletteInitializationStepOffset);
            assert(prefix.initializedPalette->values==whole.initializedPalette->values);
            auto next=input;next.control=prefix.control;next.pages=prefix.pages;
            SuccessPaletteClockTiming returned{};
            returned.state=initialSuccessPalette(channel);returned.boundary=SuccessPaletteBoundary::initialUploadComplete;
            const auto suffix=successInitialPaletteReturnOperations(next,returned);
            assert(suffix.control.stage==SuccessControlStage::polling && dispatches(suffix)==11);
            assert(!suffix.initializedPalette && !suffix.paletteInitializationStepOffset);
            assert(children(suffix).size()==2 && children(suffix)[0].kind==SuccessControlChildKind::planarCopy
                && children(suffix)[1].kind==SuccessControlChildKind::countdownStore);
            assert(prefix.steps.size()+suffix.steps.size()==whole.steps.size());
            for(std::size_t i=0;i<whole.steps.size();++i) {
                sameStep(whole.steps[i],i<prefix.steps.size() ? prefix.steps[i] : suffix.steps[i-prefix.steps.size()]);
            }
            auto done=next;done.control=suffix.control;
            rejects([&]{successInitialPaletteReturnOperations(done,returned);});
            rejects([&]{successInitialEntryOperations(next);});
            auto wrong=returned;wrong.boundary=SuccessPaletteBoundary::repeat;
            rejects([&]{successInitialPaletteReturnOperations(next,wrong);});
            wrong=returned;wrong.boundary=SuccessPaletteBoundary::presentationComplete;
            rejects([&]{successInitialPaletteReturnOperations(next,wrong);});
            wrong=returned;wrong.state.values[0]-=1;
            rejects([&]{successInitialPaletteReturnOperations(next,wrong);});
            wrong=returned;wrong.state.directions[0]=-1;
            rejects([&]{successInitialPaletteReturnOperations(next,wrong);});
            auto invalid=next;invalid.levelIndex=1;
            rejects([&]{successInitialPaletteReturnOperations(invalid,returned);});
            rejects([&]{successPaletteReturnOperations(next,returned);}); //Old return API stays strict.
        }
    }
}
}
int main() { composition();return 0; }
