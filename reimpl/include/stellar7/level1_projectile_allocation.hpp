#pragma once
#include <cstddef>
#include <vector>

namespace stellar7 {

enum class Level1ProjectileAllocationDomain { freshOnly, retained, retainedAirborne, retainedRoster };

//Source+61 names an existing FF pool record, independently of its current occupant.
struct Level1Entity;
bool level1RetainedEmitterIdentity(const std::vector<Level1Entity>& entities,std::size_t index);

} // namespace stellar7
