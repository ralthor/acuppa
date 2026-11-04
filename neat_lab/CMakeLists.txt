cmake_minimum_required(VERSION 3.25)
project(acuppa_neat_lab LANGUAGES C CXX)

# The engine is consumed exactly as checked in; only targets this lab links are built.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." acuppa EXCLUDE_FROM_ALL)

enable_testing()

set(NEAT_LAB_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/../.tools/neat-venv/bin/python"
    CACHE FILEPATH "Python interpreter with neat-python installed")

add_library(neat_lander STATIC runner/lander.cpp runner/network.cpp runner/protocol.cpp)
target_include_directories(neat_lander PUBLIC runner)
target_link_libraries(neat_lander PUBLIC acuppa_core)
target_compile_features(neat_lander PUBLIC cxx_std_17)

add_executable(lander_runner runner/main.cpp)
target_link_libraries(lander_runner PRIVATE neat_lander)
find_package(Threads REQUIRED)
target_link_libraries(neat_lander PUBLIC Threads::Threads)

add_executable(neat_lander_test tests/lander_test.cpp)
target_link_libraries(neat_lander_test PRIVATE neat_lander)
add_test(NAME neat_lander_physics COMMAND neat_lander_test)

add_executable(neat_viewer_test tests/viewer_test.cpp)
target_link_libraries(neat_viewer_test PRIVATE acuppa_core)
add_test(NAME neat_viewer_headless COMMAND neat_viewer_test
         "${CMAKE_CURRENT_SOURCE_DIR}/viewer" "${CMAKE_CURRENT_BINARY_DIR}/viewer_fixture")

add_test(NAME neat_runner_protocol COMMAND "${NEAT_LAB_PYTHON}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runner_protocol.py" $<TARGET_FILE:lander_runner>)
add_test(NAME neat_trainer COMMAND "${NEAT_LAB_PYTHON}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_trainer.py" $<TARGET_FILE:lander_runner>
         "${CMAKE_CURRENT_BINARY_DIR}/trainer_fixture")

set_tests_properties(neat_lander_physics neat_viewer_headless neat_runner_protocol neat_trainer
                     PROPERTIES LABELS neat_lab)
