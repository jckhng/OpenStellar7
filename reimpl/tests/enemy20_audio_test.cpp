#include "stellar7/enemy20_audio_work.hpp"
#include "stellar7/bitmap_cache.hpp"
#include "stellar7/driver_sequence_work.hpp"
#include "stellar7/native_session.hpp"
namespace {
void enemy20LiveChecks(stellar7::PresentationClock&,stellar7::SoundBlasterSynth&,stellar7::SoundBlasterClockPhase&);
}
#define main included_enemy_event_main
#include "enemy_event_fixture.inc"
#undef main

namespace stellar7 {
struct Enemy20McbTestAccess {
    static SoundBlasterEnemy20McbByte nativeProcess(const SoundBlasterEnemy20McbByte& byte,
        const std::string_view name, const std::vector<std::uint8_t>& payload,const IndexedBitmap& recent,
        const std::uint16_t imageSegment=0x0824) {
        return byte.withNativeProcessIdentity(name,payload,recent,
            {imageSegment,SoundBlasterEnemy20McbByte::DosLoadPlacement::afterPsp});
    }
    static SoundBlasterEnemy20McbByte highLoad(const SoundBlasterEnemy20McbByte& byte,
        const std::vector<std::uint8_t>& payload,const IndexedBitmap& recent) {
        return byte.withNativeProcessIdentity("STELLAR7.EXE",payload,recent,
            {0x0824,SoundBlasterEnemy20McbByte::DosLoadPlacement::high});
    }
    static std::optional<SoundBlasterEnemy20McbByte> fromCache(const BitmapCache& cache) {
        return SoundBlasterEnemy20McbByte::fromCache(cache);
    }
};
}

namespace {
constexpr NormalWorkOperation enemy20{NormalWorkKind::gameplayEnemyFireEvent,1,gameplayEnemyType20Event};
constexpr std::size_t ownedSsm0Bytes=2523;

std::vector<std::uint8_t> ownedSsm0() {
    std::vector<std::uint8_t> bytes(ownedSsm0Bytes);
    bytes[0]=0x84;
    return bytes;
}

IndexedBitmap recentMcbFrame() {
    IndexedBitmap frame{40,11,std::vector<std::uint8_t>(40*11)};
    for (std::size_t index=0; index<21; ++index) {
        frame.pixels[4*(89+index)+3]=static_cast<std::uint8_t>(index+1);
    }
    return frame;
}

BitmapCache mcbCache(const std::uint8_t value) {
    std::vector<IndexedBitmap> frames{{120,56,std::vector<std::uint8_t>(120*56)},
        {56,6,std::vector<std::uint8_t>(56*6)},{120,18,std::vector<std::uint8_t>(120*18)}};
    frames[2].pixels[2038]=152;
    frames[2].pixels[2042]=value;
    frames[2].pixels[2046]=156;
    for (std::size_t index=0; index<8; ++index) {
        frames[2].pixels[4*(496+index)+2]=152;
    }
    return cacheBitmapGroup(frames,0);
}

void mcbChecks() {
    for(const auto value:{17,156,220}) {
        const auto cache=mcbCache(static_cast<std::uint8_t>(value));
        const auto byte=Enemy20McbTestAccess::fromCache(cache);
        assert(byte && byte->value()==value);
        auto state=history();
        auto payload=std::vector<std::uint8_t>(36);payload[0]=0x84;
        state.program->enemy20CallerWindow=level1Enemy20CallerWindow(payload);
        state.voices.directTimers={25,60,0};
        rejects([&]{soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
        state.program->enemy20McbByte=byte;
        const auto direct=soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);
        assert((direct.state.voices.directTimers==std::array<std::uint16_t,3>{25,60,40}));
        const auto attenuation=static_cast<std::uint8_t>(63-value);
        // Program selection reads the alias; the later direct note uses volume15.
        const auto found=std::find_if(direct.blocks.begin(),direct.blocks.end(),[&](const auto& block) {
            return block.kind==SoundBlasterWorkKind::registerWrite && block.registerNumber==0x55
                && block.value==attenuation;
        });
        assert(found!=direct.blocks.end());
        const auto work=normalWorkTiming(phaseFor(state),{enemy20},horizon);
        assert(callbackFields(*work.phase.callback)==callbackFields(direct.state));
        assert(work.phase.callback->program->enemy20McbByte==byte);
        const auto retry=soundBlasterSequenceOperations(assets().levelMusicSequence,DriverSequenceAdmission::inheritedChannels);
        assert(soundBlasterProgramSequenceState(*state.program,retry.state).enemy20McbByte==byte);
        state.velocities[8]=72;
        rejects([&]{soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
    }
    auto cache=mcbCache(156);
    cache.planes[2][1764+510]^=1;
    assert(!Enemy20McbTestAccess::fromCache(cache));
    cache=mcbCache(156);cache.planes[2][1764+496]^=1;
    assert(!Enemy20McbTestAccess::fromCache(cache));
    cache=mcbCache(156);++cache.frames[2].header.offset;
    assert(!Enemy20McbTestAccess::fromCache(cache));
    cache=mcbCache(156);cache.frames[2].bitmap.width=116;
    assert(!Enemy20McbTestAccess::fromCache(cache));
    cache=mcbCache(156);cache.frames.clear();
    assert(!Enemy20McbTestAccess::fromCache(cache));
}

void nativeProcessChecks() {
    const auto cache=mcbCache(156);
    const auto recent=recentMcbFrame();
    auto payload=ownedSsm0();
    for (std::size_t index=1; index<19; ++index) { payload[index]=static_cast<std::uint8_t>(index+17); }
    const auto reserved = *Enemy20McbTestAccess::fromCache(cache);
    const auto named = Enemy20McbTestAccess::nativeProcess(reserved, "STELLAR7.EXE",payload,recent);
    const auto other = Enemy20McbTestAccess::nativeProcess(reserved, "OTHER.EXE",ownedSsm0(),recentMcbFrame());
    assert(named.value() == reserved.value() && named != reserved && named != other);
    assert(named.isNativeIdentityOf(reserved) && !reserved.isNativeIdentityOf(named));
    assert(named.nativeProcessByte(38) == 'T' && other.nativeProcessByte(38) == 'T');
    assert(named.nativeProcessByte(37) == 'S' && other.nativeProcessByte(37) == 'O');
    assert(named.nativeProcessByte(29) == 'M');
    assert(named.nativeProcessByte(30) == 0x14 && named.nativeProcessByte(31) == 0x08);
    assert(named.nativeProcessByte(32) == 158 && named.nativeProcessByte(33) == 0);
    auto smallPayload=ownedSsm0();smallPayload.resize(36);
    const auto small=Enemy20McbTestAccess::nativeProcess(reserved,"STELLAR7.EXE",smallPayload,recent);
    assert(small.nativeProcessByte(32)==3 && small.nativeProcessByte(33)==0);
    assert(named.nativeProcessByte(34) == 152 && named.nativeProcessByte(35) == 156
        && named.nativeProcessByte(36) == 156);
    for (std::uint16_t index=0; index<21; ++index) { assert(named.nativeProcessByte(index)==index+1); }
    for (std::uint16_t index=21; index<29; ++index) { assert(named.nativeProcessByte(index)==152); }
    assert(!named.nativeProcessByte(45));
    assert(!reserved.nativeProcessByte(38));
    assert(!reserved.nativeProcessByte(29));
    const auto replacement = *Enemy20McbTestAccess::fromCache(mcbCache(156));
    assert(!named.isNativeIdentityOf(replacement));
    rejects([&] { Enemy20McbTestAccess::nativeProcess(named, "STELLAR7.EXE",ownedSsm0(),recentMcbFrame()); });
    rejects([&] { Enemy20McbTestAccess::nativeProcess(reserved, "",ownedSsm0(),recentMcbFrame()); });
    rejects([&] { Enemy20McbTestAccess::nativeProcess(reserved, "STELLAR7.EXE",{},recentMcbFrame()); });
    rejects([&] { Enemy20McbTestAccess::nativeProcess(reserved, "STELLAR7.EXE",ownedSsm0(),recentMcbFrame(),16); });
    rejects([&] { Enemy20McbTestAccess::highLoad(reserved,ownedSsm0(),recentMcbFrame()); });
    auto invalidRecent=recent; ++invalidRecent.width;
    rejects([&] { Enemy20McbTestAccess::nativeProcess(reserved,"STELLAR7.EXE",payload,invalidRecent); });
    const auto relocated=Enemy20McbTestAccess::nativeProcess(reserved,"STELLAR7.EXE",ownedSsm0(),recentMcbFrame(),0x0900);
    assert(relocated.nativeProcessByte(30)==0xf0 && relocated.nativeProcessByte(31)==0x08);
    auto state = history();
    state.program->enemy20CallerWindow = level1Enemy20CallerWindow(payload);
    state.voices.directTimers = {58,0,0};
    state.velocities[7] = 77;
    state.program->enemy20McbByte = reserved;
    rejects([&] { soundBlasterPlayableDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20); });
    state.program->enemy20McbByte = named;
    const auto direct = soundBlasterPlayableDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);
    assert(direct.state.voices.directTimers[1] == 40 && direct.state.program->enemy20McbByte == named);
    const auto attenuation = static_cast<std::uint8_t>(63 - *named.nativeProcessByte(38));
    assert(std::any_of(direct.blocks.begin(),direct.blocks.end(),[&](const auto& block) {
        return block.kind == SoundBlasterWorkKind::registerWrite && block.registerNumber == 0x54 && block.value == attenuation;
    }));
    auto allocated = state;
    allocated.velocities[6] = 99;
    allocated.velocities[7] = 91;
    allocated.velocities[8] = 59;
    allocated.voices.directTimers = {25,60,0};
    const auto third = soundBlasterPlayableDirectAdvance(allocated,gameplayEnemyType20DirectRecord,
        SoundBlasterDirectCaller::gameplayEnemyType20);
    assert(third.state.voices.directTimers[2] == 40);
    const auto expectedThirdLevel=static_cast<std::uint8_t>(
        (third.state.program->voices[8].carrierScale << 6) | static_cast<std::uint8_t>(63 - 'M'));
    assert(std::any_of(third.blocks.begin(),third.blocks.end(),[&](const auto& block) {
        return block.kind == SoundBlasterWorkKind::registerWrite && block.registerNumber == 0x55
            && block.value == expectedThirdLevel;
    }));
    const std::array<std::uint8_t,8> processName{'S','T','E','L','L','A','R','7'};
    const auto sourceByte=[&](const std::uint16_t index) -> std::uint8_t {
        if (index<21) { return recent.pixels[4*(89+index)+3]; }
        if (index<29) { return cache.planes[2][1764+496+index-21]; }
        if (index==29) { return 'M'; }
        if (index==30) { return static_cast<std::uint8_t>(0x0824-16); }
        if (index==31) { return static_cast<std::uint8_t>((0x0824-16)>>8); }
        if (index==32) { return static_cast<std::uint8_t>((payload.size()+15)/16); }
        if (index==33) { return static_cast<std::uint8_t>(((payload.size()+15)/16)>>8); }
        if (index<37) { return cache.planes[2][1764+509+index-34]; }
        if (index<45) { return processName[index-37]; }
        return payload[index-45];
    };
    for (std::uint16_t index=0; index<64; ++index) {
        allocated.velocities[8]=static_cast<std::uint8_t>(2*index+1);
        const auto projected=soundBlasterPlayableDirectAdvance(allocated,gameplayEnemyType20DirectRecord,
            SoundBlasterDirectCaller::gameplayEnemyType20);
        assert(projected.state.voices.directTimers[2]==40);
        std::vector<std::uint8_t> carrierWrites;
        for (const auto& block:projected.blocks) {
            if (block.kind==SoundBlasterWorkKind::registerWrite && block.registerNumber==0x55) {
                carrierWrites.push_back(block.value);
            }
        }
        assert(carrierWrites.size()>=2);
        const auto expected=static_cast<std::uint8_t>((projected.state.program->voices[8].carrierScale<<6)
            | static_cast<std::uint8_t>(63-sourceByte(index)));
        assert(carrierWrites[carrierWrites.size()-2]==expected);
    }
    allocated.velocities[8]=129;
    rejects([&] { soundBlasterPlayableDirectAdvance(allocated,gameplayEnemyType20DirectRecord,
        SoundBlasterDirectCaller::gameplayEnemyType20); });
    const auto retry = soundBlasterSequenceOperations(assets().levelMusicSequence,DriverSequenceAdmission::inheritedChannels);
    assert(soundBlasterProgramSequenceState(*state.program,retry.state).enemy20McbByte == named);
}

void controls20() {
    auto state=history();
    rejects([&]{soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
    auto payload=std::vector<std::uint8_t>(36);payload[0]=0x84;
    state.program->enemy20CallerWindow=level1Enemy20CallerWindow(payload);
    const auto direct=soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);
    //P29 cached free15 whole source control; its looked-up byte49 is independently
    //bound to the retained resource by this packet's loaded-layout audit.
    assert(blockHash(direct.blocks)==0xf5c031e9407ba3dcULL);
    assert((direct.state.voices.directTimers==std::array<std::uint16_t,3>{40,0,0}));
    assert(direct.state.program->channelPrograms[13]==42);
    const auto work=normalWorkTiming(phaseFor(state),{enemy20},horizon);
    assert(work.callbacks.empty()&&work.total.dispatches==3190);
    std::vector<std::uint16_t> writes;
    for(const auto& effect:work.soundBlasterEffects) {
        if(effect.kind==SoundBlasterForegroundKind::runtimeOplWrite) {writes.push_back(effect.value);}
    }
    assert(writes==expectedWrites(0));
    assert(callbackFields(*work.phase.callback)==callbackFields(direct.state));
    //Second free voice uses inherited104>>1=52, also inside the owned window.
    state.voices.directTimers={5,0,0};
    const auto second=soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);
    assert((second.state.voices.directTimers==std::array<std::uint16_t,3>{5,40,0}));
    assert(second.state.program->channelPrograms[14]==42);
    //Third voice71>>1=35 crosses DOS MCB storage and remains unsupported.
    state.voices.directTimers={5,4,0};
    rejects([&]{soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
    rejects([&]{normalWorkTiming(phaseFor(state),{enemy20},horizon);});
    state=history();state.program->enemy20CallerWindow=level1Enemy20CallerWindow(payload);
    rejects([&]{soundBlasterDirectAdvance(state,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire);});
    rejects([&]{soundBlasterDirectAdvance(state,gameplayEnemyType17DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
    auto fade=state;fade.fade=1;
    rejects([&]{soundBlasterDirectAdvance(fade,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
    auto wrong=phaseFor(state);wrong.sequenceSelection->songOrdinal=4;
    rejects([&]{normalWorkTiming(wrong,{enemy20},horizon);});
    for(const auto velocity:{90,126}) {
        auto boundary=state;boundary.velocities[6]=static_cast<std::uint8_t>(velocity);
        soundBlasterDirectAdvance(boundary,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);
    }
    for(const auto velocity:{88,128}) {
        auto outside=state;outside.velocities[6]=static_cast<std::uint8_t>(velocity);
        rejects([&]{soundBlasterDirectAdvance(outside,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyType20);});
    }
    const auto retry=soundBlasterSequenceOperations(assets().levelMusicSequence,DriverSequenceAdmission::inheritedChannels);
    const auto carried=soundBlasterProgramSequenceState(*state.program,retry.state);
    assert(carried.enemy20CallerWindow==state.program->enemy20CallerWindow);
    for(const auto until:{1,60}) {
        auto phase=phaseFor(state);const auto local=5000-phase.remainingCycles-phase.deferredCycles;
        phase.remainingCycles=until;phase.deferredCycles=5000-local-until;
        phase.devices->irq0Requested=true;phase.irqCheckRequested=true;phase.timer->callbackCount=1;
        const auto raced=normalWorkTiming(phase,{enemy20},horizon);
        assert(raced.callbacks.size()==1);
        assert(raced.phase.callback->program->enemy20CallerWindow==state.program->enemy20CallerWindow);
    }
    for(std::size_t size=0;size<36;++size) {
        const auto bad=std::vector<std::uint8_t>(size);
        rejects([&]{level1Enemy20CallerWindow(bad);});
    }
}

void enemy20LiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    const auto origin=synth.driverState().normalClockCycles;
    const auto samples=clock.elapsedSamples();
    const auto writes=synth.registerWrites().size();
    const auto before=callbackFields(synth.callbackWorkState());
    auto bad=phase;bad.sequenceSelection->songOrdinal=3;
    rejects([&]{bindLevel1Enemy20Audio(synth,bad);});
    bad=phase;++bad.callback->voices.notes[0];
    rejects([&]{bindLevel1Enemy20Audio(synth,bad);});
    assert(!synth.callbackWorkState().program->enemy20CallerWindow);
    phase=bindLevel1Enemy20Audio(synth,phase);
    const auto byte=Enemy20McbTestAccess::fromCache(mcbCache(156));assert(byte);
    phase=bindLevel1Enemy20McbAudio(synth,phase,*byte);
    rejects([&]{bindLevel1Enemy20McbAudio(synth,phase,*byte);});
    assert(phase.callback->program->enemy20CallerWindow==synth.callbackWorkState().program->enemy20CallerWindow);
    assert(callbackFields(synth.callbackWorkState())==before);
    assert(synth.driverState().normalClockCycles==origin&&clock.elapsedSamples()==samples&&synth.registerWrites().size()==writes);
    rejects([&]{bindLevel1Enemy20Audio(synth,phase);});
    const auto work=normalWorkTiming(phase,{enemy20},horizon);
    const auto window=phase.callback->program->enemy20CallerWindow;
    for(unsigned mutation=0;mutation<8;++mutation) {
        auto invalid=work;
        auto receipt=std::find_if(invalid.soundBlasterEffects.begin(),invalid.soundBlasterEffects.end(),
            [](const auto& effect){return effect.kind==SoundBlasterForegroundKind::gameplayEnemyType20DirectState;});
        assert(receipt!=invalid.soundBlasterEffects.end());
        if(mutation==0) {receipt->value=2;}
        if(mutation==1) {receipt->kind=SoundBlasterForegroundKind::gameplayEnemyDirectState;}
        if(mutation==2) {receipt->directState.reset();}
        if(mutation==3) {
            auto changed=std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
            ++changed->program->enemy20CallerWindow->payloadPrefix[4];receipt->directState=changed;
        }
        if(mutation==4) {invalid.phase.sequenceSelection->songOrdinal=4;}
        if(mutation==5) {invalid.soundBlasterEffects.front().value^=1;}
        if(mutation==6) {
            auto changed=std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
            changed->program->enemy20McbByte.reset();receipt->directState=changed;
        }
        if(mutation==7) {
            auto changed=std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
            changed->program->enemy20McbByte=Enemy20McbTestAccess::fromCache(mcbCache(156));receipt->directState=changed;
        }
        rejects([&]{synth.renderClocked(invalid,origin);});
        assert(callbackFields(synth.callbackWorkState())==before);
        assert(synth.callbackWorkState().program->enemy20CallerWindow==window);
        assert(synth.driverState().normalClockCycles==origin&&synth.registerWrites().size()==writes);
    }
    const auto rendered=clock.renderWork(phase,{enemy20},horizon);phase=rendered.timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    assert(synth.callbackWorkState().program->enemy20CallerWindow==window);
    auto next=rendered.audio.writes.begin();
    for(const auto& effect:work.soundBlasterEffects) {
        if(effect.kind!=SoundBlasterForegroundKind::runtimeOplWrite) {continue;}
        const auto write=std::find_if(next,rendered.audio.writes.end(),[&](const auto& item){return item.cycleOffset==effect.cycleOffset;});
        assert(write!=rendered.audio.writes.end());
        assert(write->write.registerNumber==effect.value>>8&&write->write.value==(effect.value&0xff));next=write+1;
    }
    const auto adjacent=clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon);
    assert(!adjacent.timing.callbacks.empty());phase=adjacent.timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    assert(phase.callback->program->enemy20CallerWindow==window);
    assert(phase.callback->program->enemy20McbByte==byte);
    const auto native = Enemy20McbTestAccess::nativeProcess(*byte, "STELLAR7.EXE",ownedSsm0(),recentMcbFrame());
    const auto foreign = *Enemy20McbTestAccess::fromCache(mcbCache(byte->value()));
    const auto foreignNative = Enemy20McbTestAccess::nativeProcess(foreign, "STELLAR7.EXE",ownedSsm0(),recentMcbFrame());
    rejects([&] { synth.configurePlayableEnemy20Process(*phase.callback,foreignNative); });
    auto wrong = *phase.callback; ++wrong.position;
    rejects([&] { synth.configurePlayableEnemy20Process(wrong,native); });
    rejects([&] { synth.configurePlayableEnemy20Process(*phase.callback,*byte); });
    assert(synth.callbackWorkState().program->enemy20McbByte == byte);
    synth.configurePlayableEnemy20Process(*phase.callback,native);
    phase.callback->program->enemy20McbByte = native;
    rejects([&] { synth.configurePlayableEnemy20Process(*phase.callback,native); });
    phase = clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
    assert(phase.callback->program->enemy20McbByte == native
        && synth.callbackWorkState().program->enemy20McbByte == native);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    sourceControl();controls20();mcbChecks();nativeProcessChecks();synthReplay();return 0;
}
