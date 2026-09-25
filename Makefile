# The Ceres standard library: `make help` says everything below.
#
# GNU Make 4.2 or later, on any system: the recipes run CMake, ctest and Node and nothing of a particular shell, so
# they work the same from sh, bash or cmd.exe. The build itself is CMakeLists.txt (CMake 3.21+, with Ninja when it
# is there): incremental and parallel - one unit per C file, a header change rebuilding only what includes it. The
# tests are tools/runtests.js (Node 16+).
#
#   make                                      the library at -O2 (build/cmake/O2/libceres.car)
#   make OPT=s                                at -Os; OPT is 0, 1, 2, 3, s or g
#   make levels LEVELS="0 1 2 s"              several levels, each in build/cmake/O<level>
#   make SOFT_DOUBLE=1                        for -fsoft-double programs (build/cmake/O2-sd)
#   make FS_MAX_OPEN=4 TASK_MAX=32            any setting of include/ceres/config.h
#   make FLAGS="-fno-inline -fno-cse"         optimizations one by one, on top of the level
#   make install PREFIX=/opt/ceres            a sysroot: ceresc prog.c --sysroot /opt/ceres -lceres --run
#   make test                                 every test, at every level in LEVELS, against what was built here
#   make check                                ctest in the build directory: link programs against it and run them

ifneq ($(filter 3.% 4.0% 4.1%,$(MAKE_VERSION)),)
$(error GNU Make 4.2 or later is needed (this is $(MAKE_VERSION)); on macOS: brew install make, then gmake)
endif

# ---- a configuration of your own ----------------------------------------------------------------------------------
# config.mk (or CONFIG_FILE=<file>), when there is one, sets any variable below once for every command - OPT = s,
# FS_MAX_OPEN = 4, PREFIX = /opt/ceres - so a build, its install and its tests agree without repeating them. What
# the command line says still wins. It is not part of the repository (.gitignore).
CONFIG_FILE ?= config.mk
-include $(CONFIG_FILE)

# ---- the tools ----------------------------------------------------------------------------------------------------
CMAKE ?= cmake
CTEST ?= ctest
NODE ?= node
CERESC ?=
CERES ?=
# The test runner finds the tools through the environment: CERESC, and CERES_PATH for ceres.
ifneq ($(strip $(CERESC)),)
export CERESC
endif
ifneq ($(strip $(CERES)),)
export CERES_PATH := $(CERES)
endif

# ---- what to build ------------------------------------------------------------------------------------------------
OPT ?= 2
LEVELS ?= 0 1 2
SOFT_DOUBLE ?= 0
WERROR ?= 1
FLAGS ?=
DEFINES ?=
CERESC_FLAGS ?=
MODULES ?= irq fault mmu

# ---- where, and how -----------------------------------------------------------------------------------------------
BUILD_ROOT ?= build/cmake
yes = $(filter 1 y Y yes YES Yes on ON On true TRUE True,$(strip $(1)))
SD_SUFFIX = $(if $(call yes,$(SOFT_DOUBLE)),-sd)
BUILD_DIR ?= $(BUILD_ROOT)/O$(OPT)$(SD_SUFFIX)
NINJA_VERSION := $(shell ninja --version 2>&1)
GENERATOR ?= $(if $(filter 1.%,$(NINJA_VERSION)),Ninja)
JOBS ?=
PREFIX ?=

# ---- tests --------------------------------------------------------------------------------------------------------
TEST ?=
GC ?= 0
TIMEOUT ?=

# ---- the settings of include/ceres/config.h: every `#ifndef NAME` in it is a variable of this Makefile -----------
# (A # inside a function call means different things to Make 4.2 and 4.3, so it comes from a variable.)
hash := \#
CONFIG_NAMES := $(patsubst $(hash)ifndef:%,%,$(filter $(hash)ifndef:%,$(subst $(hash)ifndef ,$(hash)ifndef:,$(file < include/ceres/config.h))))
# A setting counts when it is given to make, not when the environment happens to hold a variable of that name.
setting = $(if $(filter environment%,$(origin $(1))),,$($(1)))

empty :=
space := $(empty) $(empty)
comma := ,
list = $(subst $(space),;,$(strip $(1)))
bool = $(if $(call yes,$(1)),ON,OFF)

# Every setting on every configure - an empty one is the default - so the build directory is exactly what this
# command line says, whatever an earlier one said.
CMAKE_ARGS = \
	$(if $(GENERATOR),-G "$(GENERATOR)") \
	$(if $(CERESC),"-DCERESC=$(CERESC)") \
	$(if $(CERES),"-DCERES=$(CERES)") \
	-DCERES_OPT_LEVEL=$(OPT) \
	-DCERES_SOFT_DOUBLE=$(call bool,$(SOFT_DOUBLE)) \
	-DCERES_WERROR=$(call bool,$(WERROR)) \
	"-DCERES_OPT_FLAGS=$(call list,$(FLAGS))" \
	"-DCERES_DEFINES=$(call list,$(DEFINES))" \
	"-DCERES_EXTRA_FLAGS=$(call list,$(CERESC_FLAGS))" \
	"-DCERES_OPTIONAL_MODULES=$(call list,$(MODULES))" \
	$(foreach name,$(CONFIG_NAMES),"-D$(name)=$(call setting,$(name))")

JOBS_ARG = $(if $(JOBS),-j $(JOBS))
TEST_ARGS = \
	$(if $(TEST),--test $(subst $(space),$(comma),$(strip $(TEST)))) \
	$(if $(call yes,$(GC)),--gc-sections) \
	$(if $(TIMEOUT),--timeout $(TIMEOUT))

.PHONY: all lib configure levels soft-double sysroot install check test headers docs config clean distclean help
.DEFAULT_GOAL := all

all: lib

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_ARGS)

lib: configure
	$(CMAKE) --build "$(BUILD_DIR)" $(JOBS_ARG)

# One library per level, each in its own directory (make -j builds them at once).
levels: $(addprefix lib-O,$(LEVELS))
lib-O%:
	$(MAKE) --no-print-directory lib OPT=$* BUILD_DIR=$(BUILD_ROOT)/O$*$(SD_SUFFIX)

soft-double:
	$(MAKE) --no-print-directory lib SOFT_DOUBLE=1

install: lib
	$(if $(PREFIX),,$(error install needs PREFIX=<directory>: make install PREFIX=/opt/ceres))
	$(CMAKE) --install "$(BUILD_DIR)" --prefix "$(PREFIX)"

# The library both ways, in one prefix: lib/ for programs, lib/soft-double/ for -fsoft-double ones.
sysroot:
	$(if $(PREFIX),,$(error sysroot needs PREFIX=<directory>: make sysroot PREFIX=/opt/ceres))
	$(MAKE) --no-print-directory install SOFT_DOUBLE=0 BUILD_DIR=$(BUILD_ROOT)/O$(OPT)
	$(MAKE) --no-print-directory install SOFT_DOUBLE=1 BUILD_DIR=$(BUILD_ROOT)/O$(OPT)-sd

check: lib
	$(CTEST) --test-dir "$(BUILD_DIR)" --output-on-failure $(JOBS_ARG)

# The whole suite against the libraries built here, one per level (the examples need -O2's too). What the tests must
# print was written for a library without soft double.
ifneq ($(and $(call yes,$(SOFT_DOUBLE)),$(filter test test-% update-%,$(MAKECMDGOALS))),)
$(error the suite is for a library without soft double: make check SOFT_DOUBLE=1 runs the soft-double programs)
endif
test: $(addprefix lib-O,$(sort $(LEVELS) 2))
	$(NODE) tools/runtests.js --library "$(BUILD_ROOT)/O{level}" --levels $(subst $(space),$(comma),$(strip $(LEVELS))) $(TEST_ARGS)

test-%: $(addprefix lib-O,$(sort $(LEVELS) 2))
	$(NODE) tools/runtests.js --library "$(BUILD_ROOT)/O{level}" --levels $(subst $(space),$(comma),$(strip $(LEVELS))) --test $* $(TEST_ARGS)

# Rewrites tests/expected/NAME.expected from what the test prints at -O0 - REVIEW the result.
update-%: lib-O0 lib-O2
	$(NODE) tools/runtests.js --library "$(BUILD_ROOT)/O{level}" --levels 0 --test $* --update

headers:
	$(NODE) tools/runtests.js --headers

docs:
	$(NODE) tools/gendocs.js

clean:
	$(CMAKE) -E rm -rf "$(BUILD_DIR)"

distclean:
	$(CMAKE) -E rm -rf build

define CONFIG_TEXT
build directory   $(BUILD_DIR)
generator         $(if $(GENERATOR),$(GENERATOR),CMake's default)
level             -O$(OPT)
soft double       $(call bool,$(SOFT_DOUBLE))
-Werror           $(call bool,$(WERROR))
-f options        $(if $(strip $(FLAGS)),$(FLAGS),none)
defines           $(if $(strip $(DEFINES)),$(DEFINES),none)
more flags        $(if $(strip $(CERESC_FLAGS)),$(CERESC_FLAGS),none)
modules           $(MODULES)
config.h          $(foreach name,$(CONFIG_NAMES),$(name)=$(if $(call setting,$(name)),$(call setting,$(name)),default))
endef

config:
	$(info $(CONFIG_TEXT))
	@$(CMAKE) -E true

define HELP_TEXT
The Ceres standard library.

Targets
  make [lib]            build the library: $(BUILD_DIR)/libceres.car, libceres.decls.casm and obj/
  make levels           one library per level in LEVELS, each in $(BUILD_ROOT)/O<level>
  make soft-double      the library for -fsoft-double programs ($(BUILD_ROOT)/O<level>-sd)
  make install          install the library as a sysroot under PREFIX
  make sysroot          install it both ways under PREFIX (lib/ and lib/soft-double/)
  make check            ctest: link programs against this build and run them (and the suite, without soft double)
  make test             every test at every level in LEVELS, against the libraries built here
  make test-NAME        one test: tests/NAME.c
  make update-NAME      rewrite tests/expected/NAME.expected from a -O0 run - review the result
  make headers          compile each header on its own
  make docs             docs/reference from the headers
  make config           what the variables below come to
  make clean            remove the build directory; make distclean removes build/

What to build
  OPT=2                 the optimization level: 0, 1, 2, 3 (as 2), s (size) or g (debugging)
  LEVELS="0 1 2"        the levels of make levels and make test
  SOFT_DOUBLE=0         1: double is a real binary64 done in software (-fsoft-double)
  WERROR=1              0: warnings are not errors
  FLAGS=                optimizations one by one on top of the level: -fno-inline -fcse ... (ceresc --help)
  DEFINES=              more macros: NAME or NAME=VALUE ...
  CERESC_FLAGS=         anything else for ceresc
  MODULES="irq fault mmu"  the optional modules install puts beside the archive
  $(foreach name,$(CONFIG_NAMES),$(name)= )
                        the settings of include/ceres/config.h, empty for their default

Where, and with what
  BUILD_DIR=$(BUILD_ROOT)/O<level>[-sd]   BUILD_ROOT=build/cmake   PREFIX=   (install)
  GENERATOR=$(if $(GENERATOR),$(GENERATOR),)   (Ninja when found; any CMake generator)   JOBS=   (parallel jobs)
  CERESC=  CERES=  (else CERESC and CERES_PATH in the environment, the sibling checkouts, then PATH)
  CMAKE=cmake  CTEST=ctest  NODE=node

Tests
  TEST="a b"            only these tests   GC=1   link them with --gc-sections   TIMEOUT=180   seconds a program
endef

help:
	$(info $(HELP_TEXT))
	@$(CMAKE) -E true
