cmake_minimum_required(VERSION 3.10.0)
project(ECS VERSION 0.1.0 LANGUAGES C CXX)

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(glm CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} glm::glm) 
