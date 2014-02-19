IF (${CMAKE_CXX_COMPILER} MATCHES "clang")
    SET(COMPILER "clang")
ELSE ()
    SET(COMPILER "g++")
ENDIF ()

MESSAGE(STATUS "Compiler: ${COMPILER}")
