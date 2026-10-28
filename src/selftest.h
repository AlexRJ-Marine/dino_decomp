// Self-test shared by dino_port and dino_tools.
//
// `run_selftest` exercises the format readers and the world/simulation
// libraries directly: no window, no GL, no SDL. It is the cheapest gate that
// proves a refactor did not change behaviour, so both binaries expose it:
//
//   dino_port  --selftest
//   dino_tools --selftest
//
// Returns 0 when every check passes, 1 otherwise.

#ifndef DINO_SELFTEST_H
#define DINO_SELFTEST_H

int run_selftest();

#endif  // DINO_SELFTEST_H
