# Writes the header dependencies of one C unit as a depfile, for CMakeLists.txt: ceresc has no -MD of its own.
#
#   cmake -DSOURCE=<file.c> -DTARGET=<output the rule is for> -DDEPFILE=<file.d> -DINCLUDE_DIRS=<dir>[|<dir>...]
#         -P scan_includes.cmake
#
# Every #include line counts, whatever #if it sits under: a header named in a branch not taken is a dependency too
# many, never one too few. "name" is looked for beside the including file first, then in the include directories;
# <name> only in the include directories. A header that is not found (one the compiler would reject) is left out.

string(REPLACE "|" ";" INCLUDE_DIRS "${INCLUDE_DIRS}")
set(found "")
set(queue "${SOURCE}")
while(queue)
    list(POP_FRONT queue file)
    get_filename_component(here "${file}" DIRECTORY)
    file(STRINGS "${file}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]")
    foreach(line IN LISTS includes)
        if(NOT line MATCHES "#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
            continue()
        endif()
        set(name "${CMAKE_MATCH_2}")
        set(candidates "")
        if(CMAKE_MATCH_1 STREQUAL "\"")
            list(APPEND candidates "${here}/${name}")
        endif()
        foreach(dir IN LISTS INCLUDE_DIRS)
            list(APPEND candidates "${dir}/${name}")
        endforeach()
        foreach(candidate IN LISTS candidates)
            if(EXISTS "${candidate}" AND NOT IS_DIRECTORY "${candidate}")
                get_filename_component(candidate "${candidate}" REALPATH)
                if(NOT candidate IN_LIST found)
                    list(APPEND found "${candidate}")
                    list(APPEND queue "${candidate}")
                endif()
                break()
            endif()
        endforeach()
    endforeach()
endwhile()

# Make's syntax: a blank in a path is escaped with a backslash.
string(REPLACE " " "\\ " text "${TARGET}")
string(APPEND text ":")
list(SORT found)
foreach(dep IN LISTS found)
    string(REPLACE " " "\\ " dep "${dep}")
    string(APPEND text " \\\n  ${dep}")
endforeach()
file(WRITE "${DEPFILE}" "${text}\n")
