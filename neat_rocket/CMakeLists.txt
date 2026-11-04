cmake_minimum_required(VERSION 3.25)
project(acuppa_neat_rocket LANGUAGES C CXX)

# The engine is consumed exactly as checked in; only targets this lab links are built.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." acuppa EXCLUDE_FROM_ALL)

enable_testing()

set(NEAT_ROCKET_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/../.tools/neat-venv/bin/python"
    CACHE FILEPATH "Python interpreter with neat-python installed")

find_package(Threads REQUIRED)
add_library(neat_rocket STATIC runner/rocket.cpp runner/network.cpp runner/protocol.cpp)
target_include_directories(neat_rocket PUBLIC runner)
target_link_libraries(neat_rocket PUBLIC acuppa_core Threads::Threads)
target_compile_features(neat_rocket PUBLIC cxx_std_17)

add_executable(rocket_runner runner/main.cpp)
target_link_libraries(rocket_runner PRIVATE neat_rocket)

add_executable(neat_rocket_test tests/rocket_test.cpp)
target_link_libraries(neat_rocket_test PRIVATE neat_rocket)
add_test(NAME neat_rocket_physics COMMAND neat_rocket_test)

add_executable(neat_rocket_viewer_test tests/viewer_test.cpp)
target_link_libraries(neat_rocket_viewer_test PRIVATE acuppa_core)
add_test(NAME neat_rocket_viewer_headless COMMAND neat_rocket_viewer_test
         "${CMAKE_CURRENT_SOURCE_DIR}/viewer" "${CMAKE_CURRENT_BINARY_DIR}/viewer_fixture")

add_test(NAME neat_rocket_runner_protocol COMMAND "${NEAT_ROCKET_PYTHON}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runner_protocol.py" $<TARGET_FILE:rocket_runner>)
add_test(NAME neat_rocket_trainer COMMAND "${NEAT_ROCKET_PYTHON}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_trainer.py" $<TARGET_FILE:rocket_runner>
         "${CMAKE_CURRENT_BINARY_DIR}/trainer_fixture")

set_tests_properties(neat_rocket_physics neat_rocket_viewer_headless neat_rocket_runner_protocol
                     neat_rocket_trainer PROPERTIES LABELS neat_rocket)
