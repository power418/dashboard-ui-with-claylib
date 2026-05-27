cmake_minimum_required(VERSION 3.22)

project(dashboard_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)

add_executable(dashboard_clay
    src/main.cpp
)

target_include_directories(dashboard_clay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(dashboard_clay PRIVATE
    OpenGL::GL
    PkgConfig::SDL2
    PkgConfig::SDL2_TTF
)

target_compile_definitions(dashboard_clay PRIVATE
    DASHBOARD_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

if (MSVC)
    target_compile_options(dashboard_clay PRIVATE /W4)
else()
    target_compile_options(dashboard_clay PRIVATE
        -Wall
        -Wextra
        -Wno-missing-field-initializers
        -Wno-sign-compare
        -Wno-unused-parameter
        -Wno-unused-variable
    )
endif()

install(TARGETS dashboard_clay RUNTIME DESTINATION bin)
install(FILES lib/clay.h DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY shaders/ DESTINATION share/dashboard-test/shaders)
