#pragma once

#include "agent/agent_loop.h"

namespace oop_agent::harness {

// Harness code records trajectories through this observer boundary instead of
// making AgentLoop depend on HarnessRunner or TrajectoryRecorder.
using StepHook = oop_agent::agent::StepHook;

} // namespace oop_agent::harness
