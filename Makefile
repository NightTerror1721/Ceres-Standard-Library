# Ceres standard library.
#
# The real work is in tools/*.ps1 (PowerShell 5.1, which every Windows has): they find ceresc and ceres
# next to this checkout (../../Ceres-C, ../../CeresASM) or through the CERESC and CERES_DIR environment
# variables, and they compare what a test PRINTS byte for byte, which make cannot do portably.
#
#   make              build build/libceres.car and prove it links (tools/mklib.ps1)
#   make test         every test at -O0, -O1 and -O2, plus each header compiled on its own
#   make test-NAME    one test: tests/NAME.c   (make test-test_malloc, make test-hello)
#   make headers      only "each header compiles on its own"
#   make update-NAME  rewrite tests/expected/NAME.expected from a -O0 run - REVIEW the result
#   make clean

POWERSHELL ?= powershell -NoProfile -ExecutionPolicy Bypass

.PHONY: all build lib test headers clean
all: lib
build: lib

lib:
	$(POWERSHELL) -File tools/mklib.ps1

test:
	$(POWERSHELL) -File tools/runtests.ps1

test-%:
	$(POWERSHELL) -File tools/runtests.ps1 -Test $*

update-%:
	$(POWERSHELL) -File tools/runtests.ps1 -Test $* -Levels 0 -Update

headers:
	$(POWERSHELL) -File tools/runtests.ps1 -Headers

clean:
	@cmd /c "if exist build rmdir /s /q build"
