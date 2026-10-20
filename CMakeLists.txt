cmake_minimum_required(VERSION 3.20)
project(TexturePreservingAbstraction LANGUAGES CXX)

# OpenCV 4 needs C++11. The implementation uses ordinary loops and STL containers.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)
if(OpenCV_VERSION VERSION_LESS "3.0")
    message(FATAL_ERROR "OpenCV 3 or later is required.")
endif()

add_library(crgf src/texture_abstraction.cpp)
target_include_directories(crgf PUBLIC include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(crgf PUBLIC ${OpenCV_LIBS})

add_executable(texture_abstraction src/main.cpp)
target_link_libraries(texture_abstraction PRIVATE crgf)

if(MSVC)
    target_compile_options(crgf PRIVATE /W4)
    target_compile_options(texture_abstraction PRIVATE /W4)
else()
    target_compile_options(crgf PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(texture_abstraction PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_crgf tests/test_crgf.cpp)
    target_link_libraries(test_crgf PRIVATE crgf)
    add_test(NAME crgf_correctness COMMAND test_crgf)
    add_test(NAME cli_help COMMAND texture_abstraction --help)
    add_test(NAME cli_invalid_mask COMMAND texture_abstraction in.png out.png 0)
    set_tests_properties(cli_invalid_mask PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_invalid_gamma COMMAND texture_abstraction in.png out.png 160 nan)
    set_tests_properties(cli_invalid_gamma PROPERTIES WILL_FAIL TRUE)
endif()
