# Builds one program of tests/ against a built library with ceresc alone, runs it, and compares what it prints with
# tests/expected/<name>.expected - a ctest test (CMakeLists.txt). A program writes to the machine's terminal, never to
# the host's stdout (CeresASM plan/v2 F5): it runs headless, and what it printed is the output stream of
# `ceres run --transcript` (the error stream's bytes are between ESC[E and ESC[e). It must exit with 0, or with the
# number in tests/expected/<name>.status.
#
#   cmake -DCERESC=<ceresc> -DCERES_DIR=<where ceres is> -DROOT=<the checkout> -DLIB=<the build directory>
#         -DNAME=<test name> -DFLAGS=<more ceresc flags, |-separated> -P run_program.cmake
#
# The source is copied into the build directory first: ceresc writes what it generates beside the source, and two
# builds testing at once must not write the same files.

string(REPLACE "|" ";" FLAGS "${FLAGS}")
set(work "${LIB}/verify")
file(MAKE_DIRECTORY "${work}")
configure_file("${ROOT}/tests/${NAME}.c" "${work}/${NAME}.c" COPYONLY)

set(transcript "${work}/${NAME}.transcript")
file(REMOVE "${transcript}")

execute_process(
    COMMAND "${CERESC}" "${work}/${NAME}.c" "${LIB}/libceres.car" --decls "${LIB}/libceres.decls.casm"
            -I "${ROOT}/include" -I "${ROOT}/tests" ${FLAGS} -o "${work}/${NAME}.cres" --run --clean --ceres-path "${CERES_DIR}"
            --run-arg --headless --run-arg --speed --run-arg max --run-arg --gpu --run-arg software
            --run-arg --transcript --run-arg "${transcript}"
    WORKING_DIRECTORY "${ROOT}"
    OUTPUT_VARIABLE host_out
    ERROR_VARIABLE err
    RESULT_VARIABLE code)
set(want_status 0)
if(EXISTS "${ROOT}/tests/expected/${NAME}.status")
    file(READ "${ROOT}/tests/expected/${NAME}.status" want_status)
    string(STRIP "${want_status}" want_status)
endif()
if(NOT code EQUAL want_status)
    message(FATAL_ERROR "${NAME}: the build or the run failed (exit ${code}, expected ${want_status})\n${err}")
endif()

# What the program printed: the transcript's output stream (the error stream, between ESC[E and ESC[e, left out),
# with \n for a line end.
set(raw "")
if(EXISTS "${transcript}")
    file(READ "${transcript}" raw)
endif()
string(ASCII 27 esc)
set(out "")
while(TRUE)
    string(FIND "${raw}" "${esc}[E" start)
    if(start EQUAL -1)
        string(APPEND out "${raw}")
        break()
    endif()
    string(SUBSTRING "${raw}" 0 ${start} head)
    string(APPEND out "${head}")
    math(EXPR start "${start} + 3")
    string(SUBSTRING "${raw}" ${start} -1 raw)
    string(FIND "${raw}" "${esc}[e" stop)
    if(stop EQUAL -1)
        break()
    endif()
    math(EXPR stop "${stop} + 3")
    string(SUBSTRING "${raw}" ${stop} -1 raw)
endwhile()
string(REPLACE "${esc}[e" "" out "${out}")
string(REPLACE "\r\n" "\n" out "${out}")
file(READ "${ROOT}/tests/expected/${NAME}.expected" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT out STREQUAL expected)
    message(FATAL_ERROR "${NAME} printed something other than tests/expected/${NAME}.expected:\n--- printed\n${out}\n--- expected\n${expected}")
endif()
message(STATUS "${NAME}: ok")
