#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
/tmp/REWise/build/rewise -l -e "original/SETUP.EXE" > "notes/wise-file-list.txt" 2>&1 || true
wc -l "notes/wise-file-list.txt"
sed -n '1,120p' "notes/wise-file-list.txt"
