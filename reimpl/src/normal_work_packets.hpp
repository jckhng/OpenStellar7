#pragma once
#include "stellar7/normal_clock.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace stellar7 {
inline std::vector<std::vector<NormalWorkOperation>> normalWorkPackets(
    const std::vector<NormalWorkOperation>& operations,const std::size_t budget) {
    if(budget==0) {throw std::invalid_argument("normal work requires positive packet capacity");}
    std::vector<std::vector<NormalWorkOperation>> result(1);
    std::size_t used=0;
    const auto next=[&]() {if(!result.back().empty()) {result.emplace_back();} used=0;};
    for(auto operation:operations) {
        if(operation.kind==NormalWorkKind::instructions) {
            while(operation.count!=0) {
                if(used==budget) {next();}
                const auto count=std::min<std::uint64_t>(operation.count,budget-used);
                result.back().push_back({operation.kind,count,operation.value});
                operation.count-=count;used+=static_cast<std::size_t>(count);
            }
            continue;
        }
        std::size_t charge=0;
        switch(operation.kind) {
        case NormalWorkKind::string: charge=2;break;
        case NormalWorkKind::repeatString: charge=2;break;
        case NormalWorkKind::port8Write: charge=4;break;
        case NormalWorkKind::port16Write: charge=3;break;
        default: break;
        }
        //Guard units bound batching only. Source work, REP boundaries and IRQ
        //execution remain unchanged in normalWorkTiming.
        if(charge==0 || operation.count==0 || operation.count>budget/charge) {
            next();result.back().push_back(operation);next();continue;
        }
        const auto units=static_cast<std::size_t>(operation.count)*charge;
        if(units>budget-used) {next();}
        result.back().push_back(operation);used+=units;
    }
    if(result.back().empty()) {result.pop_back();}
    return result;
}
}
