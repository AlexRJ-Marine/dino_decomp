#!/bin/bash
set -e
cd /tmp/REWise
python3 - <<'PY'
p = 'CMakeLists.txt'
s = open(p).read()
if 'src/input.c' not in s:
    s = s.replace('    src/exefile.c', '    src/exefile.c\n    src/input.c')
    open(p, 'w').write(s)
PY
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ls -l build/rewise
