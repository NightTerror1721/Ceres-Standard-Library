# Makes a unit compiled on its own assemblable: the library's declarations imported on top of its code, the line
# ceresc writes itself when it compiles the whole library at once.
#
#   cmake -DBODY=<the unit's .casm> -DOUT=<the .casm to assemble> -DDECLS=<the declarations, relative to OUT>
#         -P wrap_unit.cmake

# Run with -P, a script gets no policies from CMakeLists.txt: without these, an older CMake (3.28 on Ubuntu 24.04)
# does not know IN_LIST and the like.
cmake_policy(VERSION 3.21)

file(READ "${BODY}" body)
file(WRITE "${OUT}" "import \"${DECLS}\"\n\n${body}")
