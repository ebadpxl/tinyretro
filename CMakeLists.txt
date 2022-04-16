cmake_minimum_required(VERSION 3.1)

project(TinyRetro)

find_package(SFML 2.5.1 COMPONENTS graphics REQUIRED)

add_executable(TinyRetro
    src/core.cpp
    src/main.cpp
)

target_compile_features(TinyRetro PRIVATE cxx_std_17)

target_link_libraries(TinyRetro
    ${CMAKE_DL_LIBS}
    sfml-graphics
)

target_include_directories(TinyRetro
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
