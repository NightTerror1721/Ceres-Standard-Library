# The Ceres standard library, built with CMake: any generator (Ninja, Unix Makefiles, Visual Studio...), on any
# system CMake runs on. One build directory is one configuration of the library - an optimization level, soft
# double or not, the settings of ceres/config.h - and holds what tools/mklib.ps1 writes for a level:
#
#   <build>/libceres.car          the archive
#   <build>/libceres.decls.casm   the declarations of everything in it (what ceresc's --decls takes)
#   <build>/obj/<unit>.cobj       the objects, one per unit (the optional modules are linked by naming these)
#   <build>/bin/shell.cres        the Ceres shell (bin/shell/shell.c), linked against the archive
#
# The build is incremental and parallel: every C file is compiled on its own (the code ceresc generates for a
# unit does not depend on the rest of the library - only the declarations it imports do), its declarations are
# merged into the library's, and each unit is assembled against those. Changing one source rebuilds that unit;
# changing a header rebuilds the units that include it.
#
#   cmake --preset O2 && cmake --build --preset O2          (CMakePresets.json: O0, O1, O2, O3, Os, Og, O2-sd, ...)
#   cmake -S . -B build/cmake/mine -G Ninja -DCERES_OPT_LEVEL=s -DFS_MAX_OPEN=4
#   cmake --build build/cmake/mine -j
#   ctest --test-dir build/cmake/mine                     (the library linked into programs that are run)
#   cmake --install build/cmake/mine --prefix <sysroot>    (ceresc prog.c --sysroot <sysroot> -lceres --run)
#
# The Makefile drives all of this: `make help`.

cmake_minimum_required(VERSION 3.21)
project(CeresStdlib VERSION 1.0 LANGUAGES NONE)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCRIPTS "${ROOT}/tools/cmake")
if(CMAKE_HOST_WIN32)
    set(EXE ".exe")
else()
    set(EXE "")
endif()

# ---- the tools ----------------------------------------------------------------------------------------------------
# Found the way ceresc finds ceres (its --ceres-path, then CERES_PATH, then PATH): -DCERES=<file> on the command line,
# else the CERES_PATH environment variable - the executable, or the directory that holds it - else PATH. ceresc the
# same way, through -DCERESC and CERESC. A variable that is set decides: when it names no such tool the configure
# stops, rather than fall back to another copy. Nothing is looked for next to this checkout.

set(CERESC "" CACHE FILEPATH "ceresc, the C compiler; empty: the CERESC environment variable, then PATH")
set(CERES "" CACHE FILEPATH "ceres, the assembler, linker and virtual machine; empty: the CERES_PATH environment variable, then PATH")

# find_tool(<variable> <name> <environment variable>): the tool, in <variable>, and where it was found in <variable>_FROM.
function(find_tool var name env)
    if(${var})
        if(NOT EXISTS "${${var}}" OR IS_DIRECTORY "${${var}}")
            message(FATAL_ERROR "-D${var}=${${var}} is not the ${name} executable")
        endif()
        set(_found "${${var}}")
        set(_from "-D${var}")
    elseif(NOT "$ENV{${env}}" STREQUAL "")
        set(_given "$ENV{${env}}")
        if(IS_DIRECTORY "${_given}" AND EXISTS "${_given}/${name}${EXE}" AND NOT IS_DIRECTORY "${_given}/${name}${EXE}")
            set(_found "${_given}/${name}${EXE}")
        elseif(EXISTS "${_given}" AND NOT IS_DIRECTORY "${_given}")
            set(_found "${_given}")
        else()
            message(FATAL_ERROR "${env} names '${_given}', which is neither the ${name} executable nor a directory that holds it (unset it to look on PATH)")
        endif()
        set(_from "${env}")
    else()
        find_program(_found NAMES ${name} NO_CACHE NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH NO_CMAKE_INSTALL_PREFIX)
        if(NOT _found)
            message(FATAL_ERROR "cannot find ${name}: pass -D${var}=<file>, set ${env} to it or to its directory, or put its directory on PATH")
        endif()
        set(_from "PATH")
    endif()
    file(TO_CMAKE_PATH "${_found}" _found)
    set(${var} "${_found}" PARENT_SCOPE)
    set(${var}_FROM "${_from}" PARENT_SCOPE)
endfunction()
find_tool(CERESC ceresc CERESC)
find_tool(CERES ceres CERES_PATH)
get_filename_component(CERES_DIR "${CERES}" DIRECTORY)

find_program(NODE NAMES node nodejs DOC "Node.js, for the test runner and the documentation (optional)")

# ---- the configuration --------------------------------------------------------------------------------------------

set(CERES_OPT_LEVEL "2" CACHE STRING "ceresc's optimization level: 0, 1, 2, 3 (as 2), s (size) or g (debugging)")
set_property(CACHE CERES_OPT_LEVEL PROPERTY STRINGS 0 1 2 3 s g)
if(NOT CERES_OPT_LEVEL MATCHES "^[0123sg]$")
    message(FATAL_ERROR "CERES_OPT_LEVEL is '${CERES_OPT_LEVEL}': it must be 0, 1, 2, 3, s or g")
endif()

option(CERES_WERROR "treat compiler warnings as errors (-Werror)" ON)

set(CERES_OPT_FLAGS "" CACHE STRING
    "optimizations turned on or off one by one, on top of the level: -f<opt>/-fno-<opt>, ;-separated (ceresc --help lists them)")
foreach(_flag IN LISTS CERES_OPT_FLAGS)
    if(NOT _flag MATCHES "^-f(no-)?[a-z][a-z-]*$")
        message(FATAL_ERROR "CERES_OPT_FLAGS: '${_flag}' is not an -f<opt> or -fno-<opt> option")
    endif()
endforeach()

set(CERES_DEFINES "" CACHE STRING "more macros for every unit, NAME or NAME=VALUE, ;-separated (ceres/config.h's have their own settings)")
set(CERES_EXTRA_FLAGS "" CACHE STRING "anything else for ceresc, ;-separated")

set(CERES_OPTIONAL_MODULES "irq;fault;mmu" CACHE STRING
    "the optional modules to install as objects of their own: irq, fault, mmu (they bind interrupt vectors, so a program links them by name)")

# ceres/config.h: every `#ifndef NAME / #define NAME value` is a setting, a cache variable of the same name - empty
# keeps the header's default. Read from the header itself, so a setting added there is one here too.
set(CONFIG_HEADER "${ROOT}/include/ceres/config.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CONFIG_HEADER}")
file(STRINGS "${CONFIG_HEADER}" _config_lines)
set(CERES_CONFIG_SETTINGS "")
set(_comment "")
set(_pending "")
foreach(_line IN LISTS _config_lines)
    if(_line MATCHES "^// ?(.*)$")
        string(APPEND _comment " ${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^#ifndef ([A-Z_][A-Z0-9_]*)$")
        set(_pending "${CMAKE_MATCH_1}")
    elseif(_pending AND _line MATCHES "^#define ${_pending} (.+)$")
        string(STRIP "${_comment}" _doc)
        set(${_pending} "" CACHE STRING "${_doc} Default: ${CMAKE_MATCH_1}; empty keeps it.")
        set(CERES_CONFIG_DEFAULT_${_pending} "${CMAKE_MATCH_1}")
        list(APPEND CERES_CONFIG_SETTINGS "${_pending}")
        set(_pending "")
    elseif(_line STREQUAL "")
        set(_comment "")
    endif()
endforeach()

# ceresc runs in the checkout with relative paths: __FILE__ (what assert prints) is src/..., wherever the build is.
set(CERESC_FLAGS -I include -O${CERES_OPT_LEVEL})
if(CERES_WERROR)
    list(APPEND CERESC_FLAGS -Werror)
endif()
list(APPEND CERESC_FLAGS ${CERES_OPT_FLAGS})
set(_changed "")
foreach(_setting IN LISTS CERES_CONFIG_SETTINGS)
    if(NOT "${${_setting}}" STREQUAL "")
        # a number, not an expression: some become array sizes (ceres/config.h)
        if(NOT "${${_setting}}" MATCHES "^(0[xX][0-9a-fA-F]+|[0-9]+)$")
            message(FATAL_ERROR "${_setting} is '${${_setting}}': it must be a number")
        endif()
        list(APPEND CERESC_FLAGS "-D${_setting}=${${_setting}}")
        list(APPEND _changed "${_setting}=${${_setting}}")
    endif()
endforeach()
foreach(_define IN LISTS CERES_DEFINES)
    list(APPEND CERESC_FLAGS "-D${_define}")
endforeach()
list(APPEND CERESC_FLAGS ${CERES_EXTRA_FLAGS})

set(CERES_LIBDIR "" CACHE STRING
    "where the library goes under the install prefix; empty: lib (where ceresc -l looks)")
if(CERES_LIBDIR)
    set(LIBDIR "${CERES_LIBDIR}")
else()
    set(LIBDIR "lib")
endif()

# ---- the sources --------------------------------------------------------------------------------------------------

# The modules that bind interrupt vectors: in the archive like everything else (a member is only linked when it
# answers a name), and installed as objects of their own for a program to name (-lceres_irq, ...).
set(MODULE_irq src/ceres/irq.c)
set(MODULE_fault src/ceres/fault.c asm/optional/fault.casm)
set(MODULE_mmu src/ceres/mmu_fault.c asm/optional/mmu_fault.casm)
foreach(_module IN LISTS CERES_OPTIONAL_MODULES)
    if(NOT DEFINED MODULE_${_module})
        message(FATAL_ERROR "CERES_OPTIONAL_MODULES: there is no module '${_module}' (irq, fault, mmu)")
    endif()
endforeach()

file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS RELATIVE "${ROOT}" "${ROOT}/src/*.c")
file(GLOB ASM_SOURCES CONFIGURE_DEPENDS RELATIVE "${ROOT}" "${ROOT}/asm/*.casm")
list(SORT C_SOURCES)
list(SORT ASM_SOURCES)
list(APPEND ASM_SOURCES asm/optional/fault.casm asm/optional/mmu_fault.casm)

# The name a unit's object has: its path without the extension, flattened (src/ceres/irq.c -> src_ceres_irq), the
# same as tools/mklib.ps1 and tools/runtests.js give it.
function(flat_name out path)
    string(REGEX REPLACE "\\.[^./]+$" "" _name "${path}")
    string(REGEX REPLACE "[/.]" "_" _name "${_name}")
    set(${out} "${_name}" PARENT_SCOPE)
endfunction()

set(B "${CMAKE_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY "${B}/casm" "${B}/decls" "${B}/units" "${B}/obj" "${B}/deps")

# ---- 1. C -> CASM, each unit on its own -------------------------------------------------------------------------
# ceresc writes the unit's code and the declarations of what it defines and uses. Its header dependencies come from
# scanning its #include lines (tools/cmake/scan_includes.cmake): ceresc does not write them itself.

set(UNIT_DECLS "")
set(OBJECTS "")
foreach(_source IN LISTS C_SOURCES)
    flat_name(_flat "${_source}")
    add_custom_command(
        OUTPUT "casm/${_flat}.casm" "decls/${_flat}.decls.casm"
        COMMAND "${CERESC}" "${_source}" ${CERESC_FLAGS} -S -o "${B}/casm/${_flat}.casm" --emit-decls "${B}/decls/${_flat}.decls.casm"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE=${ROOT}/${_source}" "-DTARGET=casm/${_flat}.casm" "-DDEPFILE=${B}/deps/${_flat}.d"
                "-DINCLUDE_DIRS=${ROOT}/include" -P "${SCRIPTS}/scan_includes.cmake"
        DEPENDS "${ROOT}/${_source}" "${SCRIPTS}/scan_includes.cmake"
        DEPFILE "deps/${_flat}.d"
        WORKING_DIRECTORY "${ROOT}"
        COMMENT "ceresc -O${CERES_OPT_LEVEL} ${_source}"
        VERBATIM)
    list(APPEND UNIT_DECLS "${B}/decls/${_flat}.decls.casm")
endforeach()

# ---- 2. the library's declarations --------------------------------------------------------------------------------
# What every unit declares, merged into one file each generated unit imports (and a program takes with --decls). It
# is only rewritten when it changes, so a unit whose interface stayed the same does not reassemble the others.

string(REPLACE ";" "\n" _decls_list "${UNIT_DECLS}")
file(GENERATE OUTPUT "${B}/decls/units.txt" CONTENT "${_decls_list}\n")
add_custom_command(
    OUTPUT libceres.decls.casm
    COMMAND "${CMAKE_COMMAND}" "-DLIST=${B}/decls/units.txt" "-DOUT=${B}/libceres.decls.casm" -P "${SCRIPTS}/merge_decls.cmake"
    DEPENDS ${UNIT_DECLS} "${SCRIPTS}/merge_decls.cmake" "${B}/decls/units.txt"
    COMMENT "merging the declarations of the library"
    VERBATIM)

# ---- 3. CASM -> objects -------------------------------------------------------------------------------------------
# An object records the path it was assembled from, so the paths are relative - to the checkout for the hand-written
# units (asm/bits.casm, as tools/mklib.ps1 has it), to the build directory for the generated ones: the same library
# comes out wherever it is built.

foreach(_source IN LISTS C_SOURCES)
    flat_name(_flat "${_source}")
    add_custom_command(
        OUTPUT "obj/${_flat}.cobj"
        COMMAND "${CMAKE_COMMAND}" "-DBODY=${B}/casm/${_flat}.casm" "-DOUT=${B}/units/${_flat}.casm"
                "-DDECLS=../libceres.decls.casm" -P "${SCRIPTS}/wrap_unit.cmake"
        COMMAND "${CERES}" asm -c "units/${_flat}.casm" -o "obj/${_flat}.cobj"
        DEPENDS "casm/${_flat}.casm" libceres.decls.casm "${SCRIPTS}/wrap_unit.cmake"
        WORKING_DIRECTORY "${B}"
        COMMENT "ceres asm ${_flat}"
        VERBATIM)
    list(APPEND OBJECTS "obj/${_flat}.cobj")
endforeach()
foreach(_source IN LISTS ASM_SOURCES)
    flat_name(_flat "${_source}")
    add_custom_command(
        OUTPUT "obj/${_flat}.cobj"
        COMMAND "${CERES}" asm -c "${_source}" -o "${B}/obj/${_flat}.cobj"
        DEPENDS "${ROOT}/${_source}"
        WORKING_DIRECTORY "${ROOT}"
        COMMENT "ceres asm ${_source}"
        VERBATIM)
    list(APPEND OBJECTS "obj/${_flat}.cobj")
endforeach()
list(SORT OBJECTS)

# ---- 4. the archive -----------------------------------------------------------------------------------------------

add_custom_command(
    OUTPUT libceres.car
    COMMAND "${CMAKE_COMMAND}" -E rm -f libceres.car
    COMMAND "${CERES}" ar libceres.car ${OBJECTS}
    DEPENDS ${OBJECTS}
    WORKING_DIRECTORY "${B}"
    COMMENT "ceres ar libceres.car"
    VERBATIM)
add_custom_target(ceres_stdlib ALL DEPENDS libceres.car libceres.decls.casm)

# ---- 5. the shell -------------------------------------------------------------------------------------------------
# What `ceres run` starts without a program (CeresASM plan/v2 F7): a program like any other, compiled against the
# archive's declarations, assembled, and linked with the archive. The archive changes with any header the shell
# could see, so depending on it is enough.

file(MAKE_DIRECTORY "${B}/bin" "${B}/shell")
add_custom_command(
    OUTPUT bin/shell.cres
    COMMAND "${CERESC}" bin/shell/shell.c ${CERESC_FLAGS} --decls "${B}/libceres.decls.casm" -S -o "${B}/shell/shell.casm"
    COMMAND "${CERES}" asm -c "${B}/shell/shell.casm" -o "${B}/shell/shell.cobj"
    COMMAND "${CERES}" link "${B}/shell/shell.cobj" "${B}/libceres.car" -o "${B}/bin/shell.cres"
    DEPENDS "${ROOT}/bin/shell/shell.c" libceres.car libceres.decls.casm
    WORKING_DIRECTORY "${ROOT}"
    COMMENT "ceresc -O${CERES_OPT_LEVEL} bin/shell/shell.c"
    VERBATIM)
add_custom_target(ceres_shell ALL DEPENDS bin/shell.cres)

# ---- the documentation (node tools/gendocs.js) ---------------------------------------------------------------------

if(NODE)
    add_custom_target(docs COMMAND "${NODE}" tools/gendocs.js WORKING_DIRECTORY "${ROOT}" COMMENT "docs/reference from the headers" VERBATIM)
endif()

# ---- tests (ctest) ------------------------------------------------------------------------------------------------
# verify.*: programs built against this archive with ceresc alone, run, and compared with what they must print - the
# proof that the archive links. suite: every test of tests/ against this archive (node tools/runtests.js --library).

enable_testing()
set(_verify hello test_user_irq19 test_double)
foreach(_program IN LISTS _verify)
    add_test(NAME verify.${_program}
        COMMAND "${CMAKE_COMMAND}" "-DCERESC=${CERESC}" "-DCERES_DIR=${CERES_DIR}" "-DROOT=${ROOT}" "-DLIB=${B}"
                "-DNAME=${_program}" "-DFLAGS=-O${CERES_OPT_LEVEL}"
                -P "${SCRIPTS}/run_program.cmake")
    set_tests_properties(verify.${_program} PROPERTIES ENVIRONMENT "CERES_HEADLESS=1" LABELS verify TIMEOUT 180)
endforeach()
if(NODE)
    add_test(NAME suite COMMAND "${NODE}" tools/runtests.js --library "${B}" --levels ${CERES_OPT_LEVEL}
        WORKING_DIRECTORY "${ROOT}")
    set_tests_properties(suite PROPERTIES ENVIRONMENT "CERESC=${CERESC};CERES_PATH=${CERES_DIR}" LABELS suite
        TIMEOUT 3600 RUN_SERIAL TRUE)
endif()

# ---- install: a sysroot, the layout ceresc --sysroot and -l expect ------------------------------------------------
#   <prefix>/include/                           every header
#   <prefix>/<lib>/libceres.car                 the archive, and libceres.decls.casm beside it
#   <prefix>/<lib>/libceres_<m>.cobj            an optional module (-lceres_irq; -lceres_fault -lceres_fault_asm; ...)
#   <prefix>/bin/shell.cres                     the shell: `ceres run --sysroot <prefix>` starts it
# where <lib> is lib (CERES_LIBDIR says otherwise).

install(DIRECTORY include/ DESTINATION include)
install(FILES "${B}/bin/shell.cres" DESTINATION bin)
install(FILES "${B}/libceres.car" "${B}/libceres.decls.casm" DESTINATION "${LIBDIR}")
foreach(_module IN LISTS CERES_OPTIONAL_MODULES)
    foreach(_file IN LISTS MODULE_${_module})
        flat_name(_flat "${_file}")
        if(_file MATCHES "\\.casm$")
            set(_suffix "_asm")
        else()
            set(_suffix "")
        endif()
        install(FILES "${B}/obj/${_flat}.cobj" DESTINATION "${LIBDIR}" RENAME "libceres_${_module}${_suffix}.cobj")
    endforeach()
endforeach()

# ---- what this configuration is ----------------------------------------------------------------------------------

string(REPLACE ";" " " _flags_text "${CERESC_FLAGS}")
file(GENERATE OUTPUT "${B}/libceres.flags" CONTENT "${_flags_text}\n")
message(STATUS "ceresc    ${CERESC} (${CERESC_FROM})")
message(STATUS "ceres     ${CERES} (${CERES_FROM})")
message(STATUS "flags     ${_flags_text}")
if(_changed)
    message(STATUS "settings  ${_changed}")
endif()
list(LENGTH C_SOURCES _nc)
list(LENGTH ASM_SOURCES _na)
message(STATUS "units     ${_nc} C, ${_na} assembly")
