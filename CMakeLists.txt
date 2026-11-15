cmake_minimum_required(VERSION 3.16)
project(shattered_realities LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Makes M_PI available on MinGW and MSVC (harmless on Linux/macOS)
add_compile_definitions(_USE_MATH_DEFINES)

# Picks up every .cpp in src/ and every header in include/ automatically
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/include/*.h)

add_executable(shattered_realities ${SOURCES} ${HEADERS})
target_include_directories(shattered_realities PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Lets the program write images to <project>/output regardless of working directory
target_compile_definitions(shattered_realities PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

if (MSVC)
    target_compile_options(shattered_realities PRIVATE /W3)
else()
    target_compile_options(shattered_realities PRIVATE -Wall -Wextra)
endif()

# Statically link the MinGW runtime so the .exe doesn't pick up a mismatched
# libstdc++/libwinpthread DLL from elsewhere on PATH (e.g. PostgreSQL's bin).
if (MINGW)
    target_link_options(shattered_realities PRIVATE -static)
endif()