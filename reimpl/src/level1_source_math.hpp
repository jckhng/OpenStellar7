#pragma once
#include "stellar7/simulation_prefix_work.hpp"
namespace stellar7::detail {
void steeringSourceOperations(Level1Entity&,std::int16_t,std::uint16_t,std::vector<NormalWorkOperation>&);
std::uint16_t headingSourceOperations(FixedPosition actor,FixedPosition target,
    std::vector<NormalWorkOperation>& operations);
}
