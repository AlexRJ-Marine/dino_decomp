#!/bin/bash
set -e
ROOT="/mnt/c/Users/Alex/Documents/Dino decomp"
cd "$ROOT"
mkdir -p game
/tmp/REWise/build/rewise -x "game" "original/SETUP.EXE" > "notes/wise-extract.log" 2>&1 || true
tail -20 "notes/wise-extract.log"
echo "--- extracted size ---"
du -sh "game"
echo "--- top level ---"
find "game" -maxdepth 3 -type f | wc -l
