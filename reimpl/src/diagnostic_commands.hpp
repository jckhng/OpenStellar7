#pragma once

namespace stellar7::diagnostics {
// Unmatched commands/arity return false without consuming input.
bool runSoundBlasterWorkCommand(int argc, char** argv);
} // namespace stellar7::diagnostics
