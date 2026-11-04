#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace stellar7 {
inline constexpr std::uint16_t attractMenuCountdown=30000;
inline constexpr std::uint16_t attractBriefingInitialCountdown=7500;
inline constexpr std::uint16_t attractBriefingPageCountdown=6000;
inline constexpr std::uint16_t attractBriefingInputCountdown=15000;
inline constexpr std::size_t attractBriefingPages=19;

enum class AttractStage { menu, briefing, intro, credits, postCredits };
enum class AttractBriefingAction { none, exit, next, stop, faster, zoomIn, zoomOut, escapePending };
struct AttractBriefingDecision {
    std::optional<std::uint16_t> countdown;
    bool nextPage=false;
    bool closed=false;
};

//1578/1D9E control state only. DS1DC1 remains in the retained PIT owner.
class PlayableAttractController {
public:
    //The caller passes the actual1611 captured read, including an IRQ-crossing zero.
    bool menuDecision(std::uint16_t capturedCountdown,bool voiceResourceAvailable);
    void enterBriefing();
    AttractBriefingDecision briefingDecision(std::uint16_t capturedCountdown,AttractBriefingAction action);
    void introReturned();
    void creditsReturned();
    void postCreditsReturned();
    AttractStage stage() const noexcept {return stage_;}
    bool automatic() const noexcept {return automatic_;}
    std::size_t page() const noexcept {return page_;}
    std::size_t tours() const noexcept {return tours_;}
private:
    AttractStage stage_=AttractStage::menu;
    bool automatic_=false;
    std::size_t page_=0;
    std::size_t tours_=0;
};
}
