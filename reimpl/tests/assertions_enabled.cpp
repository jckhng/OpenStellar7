#include <cassert>
#include <cstdlib>

#ifdef NDEBUG
#error Test builds must enable assertions, including Release builds.
#endif

namespace {
// A runtime canary also catches a disabled/replaced assert macro.
const bool checked = [] {
    bool evaluated = false;
    assert((evaluated = true));
    if (!evaluated) {
        std::abort();
    }
    return true;
}();
}
