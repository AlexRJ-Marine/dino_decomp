// Headless report modes: every `--...-info` / `--...-dump` CLI option that
// prints a report and exits before the window is created.
//
// main() calls this once, right after argument parsing: a returned value is
// the process exit code, `std::nullopt` means "continue into interactive
// mode". Keeping these modes here is what keeps main.cpp a shell.

#pragma once

#include <optional>

#include "cli.h"

namespace dino::app {

std::optional<int> run_cli_reports(const Options& opt);

}  // namespace dino::app
