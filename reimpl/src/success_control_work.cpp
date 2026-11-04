#include "stellar7/success_control_work.hpp"
#include <stdexcept>

namespace stellar7 {
namespace {
void validate(const SuccessControlInput& input) {
    const auto& p=input.pages;
    if (input.levelIndex!=0 || !input.graphicsEnabled
        || (input.control.channel!=SuccessPaletteChannel::green && input.control.channel!=SuccessPaletteChannel::blue)
        || (input.control.completion!=SuccessPaletteCompletionSource::soundHandle
            && input.control.completion!=SuccessPaletteCompletionSource::timer)
        || p.cache!=0xa400 || (p.back!=0xa000 && p.back!=0xa800)
        || (p.front!=0xa000 && p.front!=0xa800) || p.back==p.front
        || (p.draw!=p.back && p.draw!=p.front && p.draw!=p.cache)
        || (p.copy!=p.back && p.copy!=p.front && p.copy!=p.cache)) {
        throw std::invalid_argument("unsupported recovered Level1 palette presenter context");
    }
}
void instructions(SuccessControlWork& work,const std::uint64_t count) {
    work.steps.push_back({{{NormalWorkKind::instructions,count}},{},{}});
}
void store(SuccessControlWork& work,const SuccessControlStoreKind kind,
    const std::uint16_t value,const std::uint16_t pc,const std::uint64_t count=2) {
    instructions(work,count);work.steps.back().stores.push_back({kind,value,pc});
    if (kind==SuccessControlStoreKind::drawPage) {work.pages.draw=value;}
    if (kind==SuccessControlStoreKind::copyPage) {work.pages.copy=value;}
}
enum class ChildQualification { pendingRetainedOwner, sourceQualified };
void child(SuccessControlWork& work,const SuccessControlChildKind kind,
    const std::uint16_t caller,const std::uint16_t segment,const std::uint16_t entry,
    const std::uint16_t returned,const std::array<std::int32_t,6> arguments={},const ChildQualification qualified=ChildQualification::pendingRetainedOwner) {
    work.steps.push_back({{},SuccessControlChild{kind,caller,segment,entry,returned,arguments,qualified==ChildQualification::sourceQualified},{}});
}
SuccessControlWork initial(const SuccessControlInput& input) {
    return {input.control,input.pages,{},{}};
}
}

SuccessControlWork successEntryOperations(const SuccessControlInput& input) {
    if (input.control.stage!=SuccessControlStage::inactive) {
        throw std::invalid_argument("success entry must execute once");
    }
    validate(input);auto work=initial(input);
    store(work,SuccessControlStoreKind::warningClear,0,0x6f5b,6); //Prologue and129C clear.
    instructions(work,1); //6F60 CALLF51A6; callee retains its independent owner.
    child(work,SuccessControlChildKind::entryStop,0x6f60,0x824,0x51a6,0x6f65);
    //Owns6F65..6F79 caller+02E6/handle publication/repeat1; not Begin5098.
    child(work,SuccessControlChildKind::activateSuccess,0x6f6c,0x1ebc,0x2e6,0x6f7e,{0,1,0x12bc,0,0,0});
    instructions(work,5); //6F7E..6F8B, enabled-graphics initialization.
    for (std::size_t i=0;i<48;++i) {instructions(work,17);} //6F8E..6FB3, either channel.
    work.initializedPalette=initialSuccessPalette(input.control.channel);
    work.paletteInitializationStepOffset=work.steps.size();
    store(work,SuccessControlStoreKind::drawPage,work.pages.cache,0x6fb8);
    instructions(work,1); //6FBB CALL6D46; matching cache relation.
    child(work,SuccessControlChildKind::cockpit,0x6fbb,0x824,0x6d46,0x6fbe,
        {work.pages.cache,work.pages.cache,0,0,0,0},ChildQualification::sourceQualified);
    store(work,SuccessControlStoreKind::copyPage,work.pages.cache,0x6fc1);
    store(work,SuccessControlStoreKind::drawPage,work.pages.back,0x6fc7);
    instructions(work,8); //6FCA..6FD6 dimensions,args,CALLF22A1.
    child(work,SuccessControlChildKind::planarCopy,0x6fd6,0x1764,0x22a1,0x6fdb,
        {0,0,320,146,work.pages.cache,work.pages.back},ChildQualification::sourceQualified);
    instructions(work,4); //6FDB cleanup/6FDE/6FE3 graphicsbranch/7043 CALL674E.
    child(work,SuccessControlChildKind::sight,0x7043,0x824,0x674e,0x7046,{},ChildQualification::sourceQualified);
    instructions(work,1); //7046 CALL698A.
    child(work,SuccessControlChildKind::radar,0x7046,0x824,0x698a,0x7049);
    store(work,SuccessControlStoreKind::drawPage,work.pages.front,0x704c);
    store(work,SuccessControlStoreKind::copyPage,work.pages.back,0x7052);
    instructions(work,2); //7055/705A enabled-graphics gate.
    //Qualified child includes705C..706C caller and516B; don't charge it twice.
    child(work,SuccessControlChildKind::residentPalette,0x7069,0x824,0x705c,0x706f,
        {static_cast<std::int32_t>(SuccessPaletteExecution::initial),
            static_cast<std::int32_t>(work.control.channel),static_cast<std::int32_t>(work.control.completion),0,0,0},ChildQualification::sourceQualified);
    instructions(work,9); //706F CALL8484 plus8484..8490 fullcopy setup/CALLF.
    child(work,SuccessControlChildKind::planarCopy,0x8490,0x1764,0x22a1,0x8495,
        {0,0,320,200,work.pages.back,work.pages.front},ChildQualification::sourceQualified);
    instructions(work,2); //8495 cleanup/8498 nearRET.
    //7072's instruction and actual live timer store are inseparable under IRQ.
    child(work,SuccessControlChildKind::countdownStore,0x7072,0x824,0x7072,0x7078,{992,0,0,0,0,0});
    work.control.stage=SuccessControlStage::polling;
    return work;
}

SuccessControlWork successPollOperations(const SuccessControlInput& input) {
    if (input.control.stage!=SuccessControlStage::polling) {
        throw std::invalid_argument("success poll requires completed entry/previous child");
    }
    validate(input);auto work=initial(input);
    child(work,SuccessControlChildKind::residentPalette,0x70e5,0x824,0x7078,0,
        {static_cast<std::int32_t>(SuccessPaletteExecution::advance),
            static_cast<std::int32_t>(work.control.channel),static_cast<std::int32_t>(work.control.completion),0,0,0},ChildQualification::sourceQualified);
    work.control.stage=SuccessControlStage::awaitingPalette;
    return work;
}

SuccessControlWork successPaletteReturnOperations(const SuccessControlInput& input,
    const SuccessPaletteClockTiming& paletteReturn) {
    if (input.control.stage!=SuccessControlStage::awaitingPalette) {
        throw std::invalid_argument("success palette return requires one pending child");
    }
    validate(input);
    if (paletteReturn.state.channel!=input.control.channel
        || (paletteReturn.boundary!=SuccessPaletteBoundary::repeat
            && paletteReturn.boundary!=SuccessPaletteBoundary::presentationComplete)) {
        throw std::invalid_argument("wrong source success palette return boundary");
    }
    auto work=initial(input);
    if (paletteReturn.boundary==SuccessPaletteBoundary::repeat) {
        work.control.stage=SuccessControlStage::polling;
        return work;
    }
    instructions(work,1); //7204 CALLF51A6.
    child(work,SuccessControlChildKind::returnStop,0x7204,0x824,0x51a6,0x7209);
    instructions(work,5); //7209..720E saved registers/nearRET to0784.
    work.control.stage=SuccessControlStage::finished;
    return work;
}

} // namespace stellar7
