project(K-ProgVerifier)
cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# set(CMAKE_BUILD_TYPE Debug)

# add_subdirectory(${PROJECT_SOURCE_DIR}/src/Math)

# add_subdirectory(${PROJECT_SOURCE_DIR}/src/Analysis)

include_directories(${PROJECT_SOURCE_DIR}/include)

aux_source_directory(${PROJECT_SOURCE_DIR}/src src)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(k_verifier ${src})

# target_link_libraries(sli analysis math)
