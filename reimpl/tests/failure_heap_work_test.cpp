#include "stellar7/failure_heap_work.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
void sourceWrappers() {
    constexpr std::array<std::uint16_t,5> radii{171,107,88,107,157};
    for (std::uint16_t slot = 0; slot < 30; ++slot) {
        const FailureHeapIdentity identity{7,slot};
        Model3d model{}; model.collisionRadius = radii[slot%radii.size()];
        const auto type = static_cast<std::int16_t>(36+slot%radii.size());
        const auto create = failureDebrisCreateOperations(identity,type,44,model);
        assert(create.beforeProvider.size() == 1 && create.beforeProvider[0].count == 14);
        assert(create.provider.kind == FailureHeapProviderKind::allocate && create.provider.requestedBytes == 99);
        assert(create.provider.identity.generation == 7 && create.provider.identity.slot == slot);
        assert(create.provider.sourceEntry == 0xdcc6 && !create.provider.guestElapsedQualified);
        assert(create.afterProvider.size() == 3 && create.afterProvider[0].count == 9);
        assert(create.afterProvider[1].kind == NormalWorkKind::repeatString && create.afterProvider[1].count == 99);
        assert(create.afterProvider[2].kind == NormalWorkKind::instructions && create.afterProvider[2].count == 68);
        assert(create.collisionRadius == model.collisionRadius);
        const auto release = failureDebrisReleaseOperations(identity);
        assert(release.beforeProvider.size() == 1 && release.beforeProvider[0].count == 4);
        assert(release.provider.kind == FailureHeapProviderKind::release && release.provider.requestedBytes == 99);
        assert(release.provider.identity.generation == 7 && release.provider.identity.slot == slot);
        assert(release.provider.sourceEntry == 0xdcca && !release.provider.guestElapsedQualified);
        assert(release.afterProvider.size() == 1 && release.afterProvider[0].count == 22);
        assert(!release.collisionRadius);
    }
}
void guards() {
    Model3d model{}; model.collisionRadius = 171;
    rejects([&] { failureDebrisCreateOperations({0,30},36,44,model); });
    rejects([&] { failureDebrisReleaseOperations({0,30}); });
    rejects([&] { failureDebrisCreateOperations({0,0},35,44,model); });
    rejects([&] { failureDebrisCreateOperations({0,0},41,44,model); });
    rejects([&] { failureDebrisCreateOperations({0,0},36,36,model); });
    model.scaleShift = 1;
    rejects([&] { failureDebrisCreateOperations({0,0},36,44,model); });
    model.scaleShift = 0; model.collisionRadius = 0;
    rejects([&] { failureDebrisCreateOperations({0,0},36,44,model); });
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    sourceWrappers();
    guards();
}
