#include "stellar7/sight_work.hpp"
#include <cassert>
#include <stdexcept>
namespace {
std::uint64_t count(const stellar7::SightWork& work,stellar7::NormalWorkKind kind) {
    std::uint64_t total=0;
    for (const auto& operation:work.operations) {
        if (operation.kind==kind) { total+=operation.count; }
    }
    return total;
}
}
int main() {
    const stellar7::Viewport viewport{24,294,24,145};
    const auto normal=stellar7::sightOperations(5,false,viewport,95);
    const auto alternate=stellar7::sightOperations(5,true,viewport,95);
    assert(!normal.alternatePhase && alternate.alternatePhase);
    assert(normal.color==95 && alternate.color==95);
    assert(normal.pixels.size()==139 && alternate.pixels.size()==157);
    assert(count(normal,stellar7::NormalWorkKind::port16Write)==11);
    assert(count(normal,stellar7::NormalWorkKind::port8Write)==12);
    assert(count(alternate,stellar7::NormalWorkKind::port16Write)==13);
    assert(count(alternate,stellar7::NormalWorkKind::port8Write)==16);
    assert(normal.pixels[66].point==stellar7::ProjectedPoint({159,85}));
    for (const auto& pixel:normal.pixels) { assert(pixel.color==95); }
    const auto wrappedColor=stellar7::sightOperations(7,true,viewport,255);
    for (const auto& pixel:wrappedColor.pixels) { assert(pixel.color==255); }
    bool rejected=false;
    try { stellar7::sightOperations(8,true,viewport,95); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    const stellar7::SightRefreshInput overflow{{{14,32768},{-1,0}},0};
    assert(stellar7::sightOperations(8,false,viewport,95,overflow).alternatePhase);
    const stellar7::SightRefreshInput excluded{{{14,32768}},0};
    assert(!stellar7::sightOperations(8,true,viewport,95,excluded).alternatePhase);
    const stellar7::SightRefreshInput threshold{{{13,33792}},0};
    assert(!stellar7::sightOperations(8,true,viewport,95,threshold).alternatePhase);
    const stellar7::SightRefreshInput wrapped{{{8,32767}},65535};
    assert(stellar7::sightOperations(8,false,viewport,95,wrapped).alternatePhase);
    assert(!stellar7::sightOperations(7,false,viewport,95,overflow).alternatePhase);
}
