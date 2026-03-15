file(GLOB_RECURSE EXTERNAL ./  *.cpp)

add_library(externalLib  ${EXTERNAL})