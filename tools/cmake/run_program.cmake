# Builds one program of tests/ against a built library with ceresc alone, runs it, and compares what it prints with
# tests/expected/<name>.expected - a ctest test (CMakeLists.txt).
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

execute_process(
    COMMAND "${CERESC}" "${work}/${NAME}.c" "${LIB}/libceres.car" --decls "${LIB}/libceres.decls.casm"
            -I "${ROOT}/include" -I "${ROOT}/tests" ${FLAGS} -o "${work}/${NAME}.cres" --run --clean --ceres-path "${CERES_DIR}"
    WORKING_DIRECTORY "${ROOT}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "${NAME}: the build or the run failed (${code})\n${err}")
endif()

# What the program printed: without the driver's "Wrote x" lines, with \n for a line end.
string(REPLACE "\r\n" "\n" out "${out}")
string(REGEX REPLACE "^(Wrote [^\n]*\n)+" "" out "${out}")
file(READ "${ROOT}/tests/expected/${NAME}.expected" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT out STREQUAL expected)
    message(FATAL_ERROR "${NAME} printed something other than tests/expected/${NAME}.expected:\n--- printed\n${out}\n--- expected\n${expected}")
endif()
message(STATUS "${NAME}: ok")
