// dino_tools: headless CLI for the Dino Island port.
//
// The self-test is the only tool today. It links dino_core (format readers and
// world simulation) without SDL or OpenGL, so it runs on a build machine with
// no display and is the cheapest gate for a refactor.

#include "selftest.h"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) {
            return run_selftest();
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf(
                "dino_tools - headless tools for the Dino Island port\n"
                "\n"
                "usage: dino_tools [--selftest]\n"
                "\n"
                "  --selftest   run the format/simulation self-test (no window)\n");
            return 0;
        }
        std::fprintf(stderr, "dino_tools: unknown option '%s'\n", argv[i]);
        std::fprintf(stderr, "try dino_tools --help\n");
        return 2;
    }
    std::printf("dino_tools 0.1 - environment OK\n");
    std::printf("try dino_tools --selftest\n");
    return 0;
}
