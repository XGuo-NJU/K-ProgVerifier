include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_FLAGS "-Wall -fno-rtti")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(LLVM_LIBS support core irreader)

aux_source_directory(. ENCODER_SRC_DIR)

add_library(encoder ${ENCODER_SRC_DIR})

target_link_libraries(encoder ${LLVM_LIBS} z3)