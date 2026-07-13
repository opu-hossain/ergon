cmake_minimum_required(VERSION 4.3)

# project definition and language
project(Ergon
    VERSION 1.0.0 
    DESCRIPTION "Ergon - an energitic language"
    LANGUAGES C
)

# language standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# source files
set(SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chunk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/value.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scanner.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/object.c
)

# sources / executables
add_executable(ergon ${SOURCES})

# include header files
target_include_directories(ergon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# compiler warnings
# if(MSVC)
#     target_compile_options(ergon PRIVATE /W4 /WX)
# else()
#     target_compile_options(PRIVATE 
#         -Wall -Wextra  -Wpedantic -Werror
#    )
# endif()
