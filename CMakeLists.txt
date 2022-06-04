project(K-ProgVerifier)
cmake_minimum_required(VERSION 3.10.0)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_FLAGS "-Wall -fno-rtti")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(LLVM_LIBS support core irreader)

# set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(${PROJECT_SOURCE_DIR}/src/Unroller)

add_subdirectory(${PROJECT_SOURCE_DIR}/src/Encoder)

include_directories(${PROJECT_SOURCE_DIR}/include)

aux_source_directory(${PROJECT_SOURCE_DIR}/src src)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(k_verifier ${src})

target_link_libraries(k_verifier unroller encoder)

target_link_libraries(k_verifier ${LLVM_LIBS} z3)
