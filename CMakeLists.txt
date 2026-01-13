cmake_minimum_required(VERSION 3.10)
project(LunaInterpreter C ASM_NASM)

# 1. Set Compiler Options
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add -O2 for Release builds
if(MSVC)
    add_compile_options(/O2)
else()
    add_compile_options(-O2)
endif()

# 2. Include Directories
include_directories(include)

# 3. Source Files (C)
set(SRCS
    src/lexer.c src/token.c src/util.c src/ast.c src/parser.c
    src/interpreter.c src/value.c src/main.c src/math_lib.c
    src/string_lib.c src/error.c src/time_lib.c src/vec_lib.c
    src/env.c src/library.c src/file_lib.c src/list_lib.c
)

# 4. Assembly Files
# Note: CMake automatically uses 'nasm -f win64' (or elf64) depending on the environment
set(ASM_SRCS
    asm/time.asm
    asm/vec_math.asm
)

# 5. Define the Executable
add_executable(luna ${SRCS} ${ASM_SRCS})

# 6. Link Math Library (only needed for Linux/macOS, but safe to include)
if(NOT MSVC)
    target_link_libraries(luna m)
endif()

# 7. Output Directory Configuration (Mirroring your bin/ layout)
set_target_properties(luna PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# --- Utilities / Commands ---

# Run Command (Equivalent to 'make run')
add_custom_target(run
    COMMAND luna main.lu
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS luna
)

# REPL Command (Equivalent to 'make repl')
add_custom_target(repl
    COMMAND luna
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS luna
)
