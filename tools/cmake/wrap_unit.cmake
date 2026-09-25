# Makes a unit compiled on its own assemblable: the library's declarations imported on top of its code, the line
# ceresc writes itself when it compiles the whole library at once.
#
#   cmake -DBODY=<the unit's .casm> -DOUT=<the .casm to assemble> -DDECLS=<the declarations, relative to OUT>
#         -P wrap_unit.cmake

file(READ "${BODY}" body)
file(WRITE "${OUT}" "import \"${DECLS}\"\n\n${body}")
