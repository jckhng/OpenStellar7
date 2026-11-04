#pragma once

#include "stellar7/failure_control_work.hpp"
#include "stellar7/failure_model_work.hpp"

namespace stellar7 {

enum class FailureGameplayStepKind { display, leave };

struct FailureGameplayStepWork {
    FailureGameplayStepKind kind;
    std::optional<FailureControlWork> entry;
    //expected remains the source snapshot before B996-owned observations.
    FailureControlWork control;
    std::optional<HorizonExecutionWork> horizon;
    std::optional<WorldViewSetupWork> view;
    std::optional<FailureSceneWork> scene;
    //Complete B996 child: eight wrapper instructions, B7AC and B9A5 scene.
    //The control owns7499..74A5, horizon, flip/clear and palette child positions.
    std::vector<NormalWorkOperation> sceneOperations;
};

}
