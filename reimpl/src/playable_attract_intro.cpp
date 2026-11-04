#include "playable_attract.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "stellar7/ui_text_work.hpp"
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::string_view opening="SOMEWHERE IN THE HEART OF THE$ARCTURAN EMPIRE ....";
constexpr std::uint8_t typewriterColor=148;
constexpr std::int64_t horizon=120000;
std::vector<std::uint8_t> introReturnPalette(const NativeStartupAssets& assets) {
    std::vector<std::uint8_t> result;result.reserve(vgaPaletteBytes);
    for(const auto color:assets.menuScreen.palette) {
        for(const auto value:{color.red,color.green,color.blue}) {result.push_back(value>>2);}
    }
    return result;
}
void releaseCurrent(AttractPresentation& p) {
    if(p.o.synth.driverState().sequenceSelected) {
        p.o.phase.callback=p.o.synth.stopPlayableSequence();
        p.o.phase.callback=p.o.synth.deselectPlayableSequence();
    } else if(p.o.synth.driverState().sequenceAllocated) {
        p.o.phase.callback=p.o.synth.releaseSuspendedPlayableSequence();
    }
}
enum class Overlay { transparent, opaque };
void overlay(AttractPresentation& p,const IndexedBitmap& bitmap,const ProjectedPoint point,const Overlay mode) {
    if(mode==Overlay::transparent) {p.normal(planarBitmapOperations(bitmap,point.x,PlanarBlitMode::generalTransparent));}
    //2C1F -> DS36BE=3568:1F18 is opaque, not a mirrored2C1B. Generic
    //1F18 CPU work is unqualified here; the native provider writes real pixels.
    for(std::uint16_t y=0;y<bitmap.height;++y) {
        for(std::uint16_t x=0;x<bitmap.width;++x) {
            const auto color=bitmap.pixels[y*bitmap.width+x];
            if(color!=0 || mode==Overlay::opaque) {p.o.pixels.write(p.o.pages.frontPage(),{static_cast<std::int16_t>(point.x+x),static_cast<std::int16_t>(point.y+y)},color);}
        }
    }
}
}
void AttractPresentation::titleScreen() {
    const auto titleCopy=[&](const std::uint16_t from,const std::uint16_t to) {
        //6367/6373 CALL8484, its eight-dispatch prefix and resident22A1 JMPF.
        normal({{NormalWorkKind::instructions,1+8+1}});
        normal(planarCopyOperations(0,320,200));
        copy(from,to);
        normal({{NormalWorkKind::instructions,2}}); //8484 cleanup/return.
    };
    o.pages.selectCockpitCopy();
    screen(o.startup.menuScreen,o.pages.cachePage());
    titleCopy(o.pages.cachePage(),o.pages.backPage());
    blackout();
    titleCopy(o.pages.cachePage(),o.pages.frontPage());
    o.pages.beginFrame();
    fadeIn();
}
void AttractPresentation::titleOverlays() {
    const auto& frames=o.assets.artGroups.at(2);
    if (frames.size()!=2) { throw std::invalid_argument("intro title requires its actual two-frame BMP group"); }
    overlay(*this,frames.at(0),{70,10},Overlay::transparent);
    overlay(*this,frames.at(1),{56,193},Overlay::opaque);
    publish();
}
bool PlayableAttractIntro::acceptsKey() const noexcept {
    return stage_==Stage::logoRelease || stage_==Stage::logoHold || stage_==Stage::typewriter
        || stage_==Stage::bridge || stage_==Stage::earth || stage_==Stage::menuSong;
}
void PlayableAttractIntro::blink(AttractPresentation& p,const std::size_t cycles) {
    for(std::size_t cycle=0;cycle<cycles;++cycle) {
        for(const auto color:{typewriterColor,std::uint8_t{0}}) {
            p.o.pixels.apply(uiFillOperations({x_,y_,4,8},color,p.o.pages.frontPage()).effects);p.publish();
            p.store(12);p.normal({{NormalWorkKind::instructions,3}});
            while(!p.wait()) {}
        }
    }
}
void PlayableAttractIntro::finishStory(AttractPresentation& p) {
    story_.reset();p.palette(p.o.startup.menuScreen.palette);stage_=Stage::menuSong;
}
void PlayableAttractIntro::advance(AttractPresentation& p,const PlayableAttractInput& input) {
    auto& o=p.o;
    const auto publication=o.configuration.options[2]!=0 ? PlayableMusicPublication::current : PlayableMusicPublication::suspended;
    const bool escaped=(input.sourceKey>>8)==1;
    const bool skipIntro=escaped || input.buttonHeld;
    if(stage_==Stage::entry) {
        //Menu4140/4170 closed both streams.6070 must open anew (BP-8=1).
        //Cached logo/fonts/songs survive; there is no fresh driver/reset/install.
        p.blackout();p.palette(o.startup.logoScreen.palette);p.upload(0);
        p.screen(o.startup.logoScreen,o.pages.backPage());
        p.copy(o.pages.backPage(),o.pages.frontPage());p.copy(o.pages.backPage(),o.pages.cachePage());
        p.fadeIn();p.palette(o.startup.menuScreen.palette); //6184's second PAL.
        o.phase.callback=o.synth.selectPlayableAttractLogo(publication);
        p.store(240);p.normal({{NormalWorkKind::instructions,3}});stage_=Stage::logoDelay;return;
    }
    if(stage_==Stage::logoDelay) {
        if(!p.wait()) {return;}
        if(o.configuration.options[3]!=0) {o.phase.callback=o.synth.triggerPlayableAttractLogoEvent();}
        logo_=startupLogoOperations(cacheBitmapGroup(o.startup.logoFrames,0));
        stage_=Stage::logoFrames;return;
    }
    if(stage_==Stage::logoFrames) {
        while(segment_<logo_->segments.size()) {
            const auto& segment=logo_->segments[segment_];
            if(segment.kind==StartupLogoKind::wait) {
                if(!logoWaitEntered_) {p.normal(segment.operations);logoWaitEntered_=true;}
                if(!p.wait()) {return;}
                logoWaitEntered_=false;++segment_;continue;
            }
            p.normal(segment.operations);
            switch(segment.kind) {
            case StartupLogoKind::countdownStore:p.store(segment.value);break;
            case StartupLogoKind::copyToCache:
                p.copy(o.pages.backPage(),o.pages.cachePage(),{segment.x,segment.y},segment.width,segment.height);break;
            case StartupLogoKind::copyToFront:
                p.copy(o.pages.cachePage(),o.pages.frontPage(),{segment.x,segment.y},segment.width,segment.height);break;
            case StartupLogoKind::bitmap: {
                const auto& bitmap=logo_->bitmaps.at(segment.frameIndex);
                for(std::uint16_t y=0;y<bitmap.height;++y) {for(std::uint16_t x=0;x<bitmap.width;++x) {
                    const auto color=bitmap.pixels[y*bitmap.width+x];
                    if(color!=0) {o.pixels.write(o.pages.cachePage(),{static_cast<std::int16_t>(segment.x+x),static_cast<std::int16_t>(segment.y+y)},color);}
                }}
                break;
            }
            case StartupLogoKind::frameReturn:
                p.normal({{NormalWorkKind::instructions,1}});p.publish();++segment_;return;
            default:break;
            }
            ++segment_;
        }
        logo_.reset();stage_=Stage::logoEnd;return;
    }
    if(stage_==Stage::logoEnd) {
        if(o.configuration.options[2]!=0) {
            if(!logoOnce_) {
                p.normal({{NormalWorkKind::instructions,7},{NormalWorkKind::playableAttractLogoOnce,1,1}});logoOnce_=true;
            }
            auto work=o.clock.renderWork(o.phase,{{NormalWorkKind::playableAttractLogoEndPoll,p.budget}},horizon);
            const auto done=work.timing.sequenceEndReturned;p.append(std::move(work));
            if(!done) {return;}
        } else {
            if(!logoOnce_) {p.store(480);p.normal({{NormalWorkKind::instructions,3}});logoOnce_=true;}
            if(!p.wait()) {return;}
        }
        stage_=Stage::logoRelease;return;
    }
    if(stage_==Stage::logoRelease) {
        releaseCurrent(p); //625D. Cached immutable SNG0 remains in the synth owner.
        if(skipIntro) {introSkipped_=true;p.blackout();p.palette(o.startup.menuScreen.palette);stage_=Stage::menuSong;return;}
        stage_=Stage::logoHold;return;
    }
    if(stage_==Stage::logoHold) {
        if(skipIntro) {introSkipped_=true;stage_=Stage::menuSong;return;}
        if(p.read()!=0) {return;}
        if(o.configuration.options[5]==0) {stage_=Stage::menuSong;return;}
        if(o.configuration.options[5]!=255 || !o.assets.storyScenes) {throw std::invalid_argument("attract Story requires source0/255 and actual scene/voice resources");}
        stage_=Stage::typewriter;return;
    }
    if(stage_==Stage::typewriter) {
        if(skipIntro) {introSkipped_=true;p.blackout();stage_=Stage::menuSong;return;}
        if(!storyStarted_) {p.blackout();p.palette(o.startup.menuScreen.palette);blink(p,5);storyStarted_=true;}
        if(character_<opening.size()) {
            const auto character=opening[character_++];blink(p,character=='$'?5:1);
            if(character=='$') {x_=40;y_=92;}
            else {
                o.pixels.apply(uiTextOperations(o.assets.titleFont,UiFontLayout::fixed,std::string_view(&character,1),
                    {x_,y_},typewriterColor,o.pages.frontPage(),{0,319,0,199}).effects);
                x_=static_cast<std::int16_t>(x_+8);p.publish();
            }
            return;
        }
        blink(p,5);p.blackout();stage_=Stage::bridge;
        story_=std::make_shared<PlayableStoryPresenter>(*o.assets.storyScenes,StoryScene::bridgeOpening,
            RadarPages{o.pages.cachePage(),o.pages.backPage(),o.pages.frontPage(),o.pages.drawPage(),o.pages.copyPage()});return;
    }
    if(stage_==Stage::bridge || stage_==Stage::earth) {
        const auto returnPalette=introReturnPalette(o.startup); //Actual retained12C6 is PAL1 until643B.
        p.append(story_->advance({o.pixels,o.pages,o.palette,o.synth,o.clock,o.phase,o.assets.scoreFont,
            returnPalette,o.configuration.options[2]!=0,o.configuration.options[3]!=0,PlayableStoryCaller::menuAttract},
            {static_cast<std::uint16_t>(input.sourceKey>>8),input.buttonHeld},p.budget));
        if(story_->result()==StorySceneResult::playing) {return;}
        if(stage_==Stage::bridge && story_->result()==StorySceneResult::complete) {
            stage_=Stage::earth;story_=std::make_shared<PlayableStoryPresenter>(*o.assets.storyScenes,StoryScene::earthOpening,
                RadarPages{o.pages.cachePage(),o.pages.backPage(),o.pages.frontPage(),o.pages.drawPage(),o.pages.copyPage()});return;
        }
        introSkipped_=story_->result()==StorySceneResult::skipped;finishStory(p);return;
    }
    if(stage_==Stage::menuSong) {
        if(!introSkipped_ && !skipIntro) { p.titleScreen(); }
        p.store(2000); //637F precedes639A stop/63A6 cached4C12 activation.
        o.phase.callback=o.synth.selectPlayableAttractMenu(publication);
        if(!introSkipped_ && !skipIntro) {
            p.titleOverlays();
        }
        p.normal({{NormalWorkKind::instructions,3}});stage_=Stage::menuHold;return;
    }
    if(stage_==Stage::menuHold) {
        if(!p.wait()) {return;}
        //641D BP-8=1: mandatory wait, palette release/readPAL2, primaryfont,
        //stream close. This branch does NOT execute6474's DS19CA store.
        p.blackout();p.palette(o.startup.reentryPalette);stage_=Stage::complete;return;
    }
    throw std::logic_error("completed attract intro must return to B12A caller");
}
}
