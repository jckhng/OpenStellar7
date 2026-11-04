#pragma once

#include <functional>
#include <iosfwd>

namespace stellar7 {

int runMenu(std::istream& input, std::ostream& output,
    const std::function<int()>& beginGameplay = {});

}  // namespace stellar7
