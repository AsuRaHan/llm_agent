cmake_minimum_required(VERSION 3.15)
project(Agent)

# Указываем CMake складывать готовый .exe прямо в корень проекта
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}")


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Dependencies ---

# nlohmann/json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG master
)
FetchContent_MakeAvailable(json)

# yhirose/cpp-httplib
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG master
)
FetchContent_MakeAvailable(httplib)


# --- Target ---

add_executable(Agent src/main.cpp src/ContextIndexer.cpp src/EmbeddingClient.cpp src/AssistantRole.cpp)

target_include_directories(Agent PUBLIC
  $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${httplib_SOURCE_DIR}>
)

# --- Platform Specifics ---

if(WIN32)
  target_link_libraries(Agent PRIVATE ws2_32)
  # httplib requires these as well
  target_link_libraries(Agent PRIVATE wsock32)
  target_link_libraries(Agent PRIVATE crypt32)
  # Add defines to avoid conflicts with Windows.h
  target_compile_definitions(Agent PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()