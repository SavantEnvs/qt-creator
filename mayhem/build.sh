#!/usr/bin/env bash
#
# mayhem/build.sh — build the Qt Creator fuzz harnesses (sanitized) and the oracle binaries (normal flags).
#
# Qt Creator itself is a huge Qt/CMake application; the fuzz surface here is the self-contained parsers that
# only need QtCore (FuzzyMatcher, the QML/JS + qmldir parser, the GLSL parser) plus the C CmdBridge CBOR
# decoder. Their sources are compiled directly (no CMake/Qt Creator build), against the system Qt6 from apt,
# so nothing is fetched at build time and the script is re-runnable offline.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
LIBS="$SRC/src/libs"
CBR="$LIBS/gocmdbridge/server/c"
MAY="$SRC/mayhem"
OBJ="$MAY/obj"
BIN="$MAY/bin"
mkdir -p "$OBJ/san" "$OBJ/norm" "$BIN"

QT_CFLAGS="$(pkg-config --cflags Qt6Core)"
QT_LIBS="$(pkg-config --libs Qt6Core)"
QT_TEST_CFLAGS="$(pkg-config --cflags Qt6Test)"
QT_TEST_LIBS="$(pkg-config --libs Qt6Test)"
MOC="$(qmake6 -query QT_HOST_LIBEXECS 2>/dev/null)/moc"
[ -x "$MOC" ] || { echo "moc not found at $MOC" >&2; exit 1; }

# libFuzzer edge coverage inside the fuzzed code too (asan runtime provides the no-op callbacks for the
# -standalone reproducers); only meaningful when a sanitizer runtime is linked.
COV_INSTR=""
[ -z "$SANITIZER_FLAGS" ] || COV_INSTR="-fsanitize=fuzzer-no-link"

PROJ_DEFS="-DQT_CREATOR -DQMLJS_STATIC_LIBRARY=1 -DUTILS_STATIC_LIBRARY"
INCS="-I$LIBS -I$LIBS/utils"
CXXSTD="-std=c++20 -fPIC"

# Project sources shared by the harnesses (compiled once per flavor).
FUZZY_SRCS="utils/fuzzymatcher.cpp"
QMLJS_SRCS="qmljs/parser/qmljsast.cpp qmljs/parser/qmljsastvisitor.cpp qmljs/parser/qmljsgrammar.cpp qmljs/parser/qmljslexer.cpp qmljs/parser/qmljsparser.cpp"
QMLDIR_SRCS="qmljs/parser/qmldirparser.cpp languageutils/componentversion.cpp utils/qtcassert.cpp"
GLSL_SRCS="$(cd "$LIBS" && ls glsl/*.cpp | tr '\n' ' ')"
ALL_SRCS="$FUZZY_SRCS $QMLJS_SRCS $QMLDIR_SRCS $GLSL_SRCS"

objname() { echo "$1" | tr '/' '_'; }

# 1) sanitized objects of the project (ASan+UBSan halting, DWARF<=3) — the fuzzed code is instrumented.
jobs=()
for s in $ALL_SRCS; do
  jobs+=("$CXX $SANITIZER_FLAGS $COV_INSTR $DEBUG_FLAGS -O1 $CXXSTD $PROJ_DEFS $INCS $QT_CFLAGS -c $LIBS/$s -o $OBJ/san/$(objname "$s").o")
done
for h in fuzz_fuzzymatcher fuzz_qmljs_parser fuzz_qmldir_parser fuzz_glsl_parser; do
  jobs+=("$CXX $SANITIZER_FLAGS $COV_INSTR $DEBUG_FLAGS -O1 $CXXSTD $PROJ_DEFS $INCS $QT_CFLAGS -c $MAY/harnesses/$h.cc -o $OBJ/san/$h.o")
done
jobs+=("$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c $MAY/lsan_off.cc -o $OBJ/san/lsan_off.o")
jobs+=("$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c $STANDALONE_FUZZ_MAIN -o $OBJ/san/standalone_main.o")
printf '%s\n' "${jobs[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c '{}'

san_objs() { for s in "$@"; do printf '%s ' "$OBJ/san/$(objname "$s").o"; done; }

# link_harness <name> <project sources...>: libFuzzer binary + `<name>-standalone` run-once reproducer.
link_harness() {
  local name="$1"; shift
  local objs; objs="$(san_objs "$@")"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OBJ/san/fuzz_$name.o" $objs "$OBJ/san/lsan_off.o" $QT_LIBS -o "$SRC/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/san/fuzz_$name.o" "$OBJ/san/standalone_main.o" $objs "$OBJ/san/lsan_off.o" $QT_LIBS -o "$SRC/$name-standalone"
}
link_harness fuzzymatcher $FUZZY_SRCS
link_harness qmljs_parser $QMLJS_SRCS
link_harness qmldir_parser $QMLDIR_SRCS
link_harness glsl_parser $GLSL_SRCS

# CBOR decoder of the C CmdBridge: the harness ships in tree and #includes cbor.c itself. With -DLIBFUZZER
# it exposes only LLVMFuzzerTestOneInput (its own main() reads stdin and is compiled out).
CBOR_FLAGS="-std=c11 -D_GNU_SOURCE -pthread -O1 -DLIBFUZZER"
$CC $SANITIZER_FLAGS $COV_INSTR $DEBUG_FLAGS $CBOR_FLAGS -c "$CBR/cmdbridge_fuzz.c" -o "$OBJ/san/cmdbridge_fuzz.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OBJ/san/cmdbridge_fuzz.o" "$OBJ/san/lsan_off.o" -pthread -o "$SRC/cmdbridge_cbor"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/san/cmdbridge_fuzz.o" "$OBJ/san/standalone_main.o" "$OBJ/san/lsan_off.o" -pthread -o "$SRC/cmdbridge_cbor-standalone"

# 2) Oracle binaries, NORMAL flags (no sanitizers): the upstream FuzzyMatcher QtTest, the upstream C
#    CmdBridge server (driven by upstream's cmdbridge_py_test.py) and known-answer probes.
NORM_CXX="$CXX -O2 $COVERAGE_FLAGS $CXXSTD $PROJ_DEFS $INCS $QT_CFLAGS"
jobs=()
for s in $ALL_SRCS; do
  jobs+=("$NORM_CXX -c $LIBS/$s -o $OBJ/norm/$(objname "$s").o")
done
for k in kat_qmljs kat_qmldir kat_glsl; do
  jobs+=("$NORM_CXX -c $MAY/kat/$k.cc -o $OBJ/norm/$k.o")
done
printf '%s\n' "${jobs[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c '{}'
norm_objs() { for s in "$@"; do printf '%s ' "$OBJ/norm/$(objname "$s").o"; done; }

$CXX -O2 $COVERAGE_FLAGS "$OBJ/norm/kat_qmljs.o" $(norm_objs $QMLJS_SRCS) $QT_LIBS -o "$BIN/kat_qmljs"
$CXX -O2 $COVERAGE_FLAGS "$OBJ/norm/kat_qmldir.o" $(norm_objs $QMLDIR_SRCS) $QT_LIBS -o "$BIN/kat_qmldir"
$CXX -O2 $COVERAGE_FLAGS "$OBJ/norm/kat_glsl.o" $(norm_objs $GLSL_SRCS) $QT_LIBS -o "$BIN/kat_glsl"
$CC -O2 $COVERAGE_FLAGS -std=c11 -D_GNU_SOURCE -pthread -I"$CBR" "$MAY/kat/kat_cbor.c" -o "$BIN/kat_cbor"

# Upstream's own FuzzyMatcher test, built against the library source directly (no Qt Creator build).
"$MOC" "$SRC/tests/auto/utils/fuzzymatcher/tst_fuzzymatcher.cpp" -o "$OBJ/norm/tst_fuzzymatcher.moc"
$NORM_CXX $QT_TEST_CFLAGS -I"$OBJ/norm" "$SRC/tests/auto/utils/fuzzymatcher/tst_fuzzymatcher.cpp" \
  "$OBJ/norm/$(objname utils/fuzzymatcher.cpp).o" $QT_TEST_LIBS $QT_LIBS -o "$BIN/tst_fuzzymatcher"

# Upstream's C CmdBridge server, exactly as src/libs/gocmdbridge/server/c/CMakeLists.txt builds it.
$CC -D_GNU_SOURCE -std=c11 -O2 -pthread $COVERAGE_FLAGS -DGOBRIDGE_MAGIC_PACKET_MARKER='"PkgMarkerGoBridgeMagicPacket"' \
  "$CBR/cmdbridge.c" -o "$BIN/cmdbridge-linux-amd64"

for b in fuzzymatcher qmljs_parser qmldir_parser glsl_parser cmdbridge_cbor; do
  [ -x "$SRC/$b" ] && [ -x "$SRC/$b-standalone" ] || { echo "missing target binary $b" >&2; exit 1; }
done
echo "build.sh: done"
