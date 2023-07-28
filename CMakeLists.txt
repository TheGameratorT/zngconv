cmake_minimum_required(VERSION 3.20)

# Set the project name and version
project(zngconv VERSION 1.0)

if (WIN32)
# Disable fopen deprecation warnings
add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
endif()

add_subdirectory("libs/lz4/build/cmake")

# Select all of the source files recursively
file(GLOB_RECURSE SOURCES "source/*.cpp" "source/args.c")

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES} $<IF:$<BOOL:${WIN32}>,win_resources.rc,>)

# Specify the C++ standard
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)

# Include
target_include_directories(${PROJECT_NAME} PRIVATE libs/stb)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE lz4_static)
