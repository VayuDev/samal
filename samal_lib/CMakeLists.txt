cmake_minimum_required(VERSION 3.0)

project(samal_lib)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-strict-aliasing -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -fno-strict-aliasing -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g -march=native -mtune=native -fno-strict-aliasing")


if(SAMAL_ENABLE_JIT)
    message(STATUS "Enable JIT")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dx86_64_BIT_MODE -DSAMAL_ENABLE_JIT")
elseif(SAMAL_ALIGNED_ACCESS)
    message(STATUS "Enable aligned access")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dx86_64_BIT_MODE")
endif()



file(GLOB_RECURSE HEADERS
        "${PROJECT_SOURCE_DIR}/include/*.hpp")

file(GLOB_RECURSE SOURCES
        "${PROJECT_SOURCE_DIR}/lib/*.cpp")

include_directories(include)
include_directories(../peg_parser/include)
if(SAMAL_ENABLE_JIT)
    include_directories(../third_party/xbyak/xbyak)
endif()
add_library(samal_lib STATIC ${SOURCES} ${HEADERS})

target_link_libraries(samal_lib peg_parser)
if(SAMAL_ENABLE_JIT)
    target_link_libraries(samal_lib xbyak m stdc++fs)
endif()