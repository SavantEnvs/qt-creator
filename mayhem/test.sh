#!/usr/bin/env bash
#
# mayhem/test.sh — RUN the oracle that mayhem/build.sh produced (normal flags, no sanitizers).
#
#   1. tst_fuzzymatcher    upstream's own QtTest suite for FuzzyMatcher (tests/auto/utils/fuzzymatcher)
#   2. cmdbridge_py_test   upstream's protocol test for the C CmdBridge server (91 CBOR command round-trips
#                          against the real cmdbridge binary)
#   3. kat_*               known-answer probes: fixed QML / qmldir / GLSL / CBOR inputs parsed by the real
#                          library code, whose complete output is diffed against the committed golden file
#                          mayhem/kat/<probe>.expected
#
# A program that is neutered to exit(0) prints nothing / no "Totals:" line / no test summary, so every check
# below fails when its binary is a no-op.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN="$SRC/mayhem/bin"
passed=0; failed=0
for b in tst_fuzzymatcher cmdbridge-linux-amd64 kat_cbor kat_qmljs kat_qmldir kat_glsl; do
  [ -x "$BIN/$b" ] || { echo "missing $BIN/$b — mayhem/build.sh should have built it" >&2; exit 2; }
done

# 1. upstream QtTest suite
out="$(QT_QPA_PLATFORM=offscreen "$BIN/tst_fuzzymatcher" 2>&1)"
echo "$out" | tail -5
if [[ "$out" =~ Totals:\ ([0-9]+)\ passed,\ ([0-9]+)\ failed,\ ([0-9]+)\ skipped ]]; then
  p=${BASH_REMATCH[1]}; f=${BASH_REMATCH[2]}
  if [ "$p" -ge 70 ]; then passed=$((passed + p)); else failed=$((failed + 1)); echo "FAIL tst_fuzzymatcher: only $p passed" >&2; fi
  failed=$((failed + f))
else
  failed=$((failed + 1)); echo "FAIL tst_fuzzymatcher: no Totals line" >&2
fi

# 2. upstream CmdBridge protocol test
out="$(python3 "$SRC/src/libs/gocmdbridge/server/c/cmdbridge_py_test.py" "$BIN/cmdbridge-linux-amd64" 2>&1)"
echo "$out" | grep -E "FAIL|Error|^[0-9]+ tests" | tail -10
if [[ "$out" =~ ([0-9]+)\ tests,\ ([0-9]+)\ passed,\ ([0-9]+)\ failed ]]; then
  t=${BASH_REMATCH[1]}; p=${BASH_REMATCH[2]}; f=${BASH_REMATCH[3]}
  if [ "$t" -ge 80 ] && [ "$p" -ge 80 ]; then passed=$((passed + p)); else failed=$((failed + 1)); echo "FAIL cmdbridge_py_test: $p/$t" >&2; fi
  failed=$((failed + f))
else
  failed=$((failed + 1)); echo "FAIL cmdbridge_py_test: no summary line" >&2
fi

# 3. known-answer probes
for k in kat_cbor kat_qmljs kat_qmldir kat_glsl; do
  if "$BIN/$k" 2>/dev/null | diff -u "$SRC/mayhem/kat/$k.expected" - ; then
    passed=$((passed + 1)); echo "PASS $k"
  else
    failed=$((failed + 1)); echo "FAIL $k: output differs from mayhem/kat/$k.expected" >&2
  fi
done

emit_ctrf "qttest+cmdbridge-py+kat" "$passed" "$failed" 0
