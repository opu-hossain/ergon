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

# source files
set(SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_chunks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_value.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_vm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_compiler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ergon_scanner.c
)

# header files
set(HEADERS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ergon/ergon_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ergon/ergon_debug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ergon/ergon_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ergon/ergon_chunks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ergon/ergon_value.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vm/ergon_vm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/compiler/ergon_compiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/scanner/ergon_scanner.h
)

# external packages
#find_package(OpenSSL REQUIRED)

# sources / executables
add_executable(ergon ${SOURCES} ${HEADERS})

# include header files
target_include_directories(ergon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# link libraries
#target_link_libraries(truthbyte PRIVATE OpenSSL::Crypto)

# compiler warnings
# if(MSVC)
#     target_compile_options(ergon PRIVATE /W4 /WX)
# else()
#     target_compile_options(PRIVATE 
#         -Wall -Wextra  -Wpedantic -Werror
#    )
# endif()
