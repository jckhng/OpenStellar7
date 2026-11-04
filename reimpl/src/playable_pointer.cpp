#include "playable_pointer.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t widgetBytes = 50;
constexpr std::size_t scratchBytes = 500;
}
ProjectedPoint sourcePointerPosition(const ProjectedPoint point) {
    //5B9E..5BE0; the painter independently clips the16x21 cursor at the page edge.
    return {std::clamp<std::int16_t>(point.x,0,319),std::clamp<std::int16_t>(point.y,0,196)};
}
std::optional<std::size_t> pointerWidgetHit(const std::vector<PointerHitBox>& boxes, const ProjectedPoint point) {
    for (std::size_t index=0;index<boxes.size();++index) {
        const auto& box=boxes[index];
        if (box.extentX<0 || box.extentY<0) {throw std::invalid_argument("negative source widget extent");}
        if (point.x>=box.x && point.y>=box.y && point.x<=int{box.x}+box.extentX
            && point.y<=int{box.y}+box.extentY) {
            return index;
        }
    }
    return std::nullopt;
}
PointerWidgetPoll pointerWidgetPoll(const std::optional<std::size_t> pressed,
    const std::vector<PointerHitBox>& boxes, const ProjectedPoint point, const PointerButton button) {
    if (pressed && *pressed>=boxes.size()) {throw std::invalid_argument("pressed widget outside source table");}
    if(button==PointerButton::cancel) {return {{},pressed,{}};}
    const auto hit=pointerWidgetHit(boxes,point);
    if (!pressed) {
        return {button==PointerButton::held ? hit : std::nullopt,{}, {}};
    }
    if (button==PointerButton::held && hit==pressed) {return {pressed,{}, {}};}
    //5B26..5B55: move-off while held cancels; release returns the retained old hit.
    return {{},pressed,button==PointerButton::released ? pressed : std::nullopt};
}
std::vector<PointerHitBox> sourceWidgetHitBoxes(const std::vector<std::uint8_t>& scratch,const std::size_t count) {
    if (scratch.size()!=scratchBytes || count>scratchBytes/widgetBytes) {
        throw std::invalid_argument("pointer requires the retained500-byte widget allocation");
    }
    const auto word=[&](const std::size_t offset) {
        return static_cast<std::int16_t>(scratch[offset] | (std::uint16_t{scratch[offset+1]}<<8));
    };
    std::vector<PointerHitBox> boxes;
    for (std::size_t index=0;index<count;++index) {
        const auto at=index*widgetBytes;
        boxes.push_back({word(at),word(at+2),word(at+4),word(at+6)});
    }
    return boxes;
}
const std::vector<PointerHitBox>& mainPointerHitBoxes() {
    //16B0/5720, templates17/18: (21,6,81,11)+(102,row).
    static const std::vector<PointerHitBox> boxes{{123,64,81,11},{123,96,81,11},{123,121,81,11},
        {123,146,81,11},{123,171,81,11}};
    return boxes;
}
const std::vector<PointerHitBox>& preferencePointerHitBoxes() {
    //180C: slider24=(0,0,84,6), toggle25=(10,5,17,14), EXIT18=(21,6,81,11).
    static const std::vector<PointerHitBox> boxes{{118,43,84,6},{52,77,17,14},{170,77,17,14},
        {52,106,17,14},{170,106,17,14},{52,135,17,14},{170,135,17,14},{123,165,81,11}};
    return boxes;
}
const std::vector<PointerHitBox>& briefingPointerHitBoxes() {
    //265F..273E templates9/10=(19/9,5,75,11);5..8=(0,0,16,14).
    static const std::vector<PointerHitBox> boxes{{30,167,75,11},{211,167,75,11},{24,39,16,14},
        {24,57,16,14},{24,75,16,14},{24,93,16,14}};
    return boxes;
}
const std::vector<PointerHitBox>& scoresPointerHitBoxes() {
    static const std::vector<PointerHitBox> boxes{{124,168,81,11}}; //2B51/18 at103,162.
    return boxes;
}
const std::vector<PointerHitBox>& continuePointerHitBoxes() {
    static const std::vector<PointerHitBox> boxes{{121,69,81,11},{121,113,81,11}}; //86E0/8707,17/19.
    return boxes;
}
const std::vector<PointerHitBox>& namePointerHitBoxes() {
    static const std::vector<PointerHitBox> boxes{{121,134,81,11}}; //32A4/18 at100,128.
    return boxes;
}
} // namespace stellar7
