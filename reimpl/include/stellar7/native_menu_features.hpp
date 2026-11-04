#pragma once
#include <cstdint>
namespace stellar7 {
enum class NativeMenuFeatures : std::uint8_t { legacy=0, fullGraphicDetail=1, storyPanels=2, full=3, joystickControls=4, automaticAttract=8, releaseMenus=15, nativeTitlePacing=16, enhancedMenus=31 };
constexpr bool hasNativeMenuFeature(const NativeMenuFeatures features,const NativeMenuFeatures feature) {
    return (static_cast<std::uint8_t>(features)&static_cast<std::uint8_t>(feature))==static_cast<std::uint8_t>(feature);
}
constexpr bool validNativeMenuFeatures(const NativeMenuFeatures features) {return static_cast<std::uint8_t>(features)<=static_cast<std::uint8_t>(NativeMenuFeatures::enhancedMenus);}
}
