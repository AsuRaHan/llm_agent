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
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(json)

# yhirose/cpp-httplib
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(httplib)

# Fetch spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.x
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(spdlog)

# hnswlib
FetchContent_Declare(
  hnswlib
  GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
  GIT_TAG        v0.8.0
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(hnswlib)
# --- Target ---

add_executable(Agent 
      src/main.cpp 
      src/ContextIndexer.cpp 
      src/EmbeddingClient.cpp 
      src/AssistantRole.cpp
      src/Logger.cpp
      src/Config.cpp
      )

target_include_directories(Agent PUBLIC
  $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${httplib_SOURCE_DIR}>
  $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${hnswlib_SOURCE_DIR}>
)

# Link against dependencies
target_link_libraries(Agent PRIVATE spdlog::spdlog hnswlib)

# --- Platform Specifics ---

if(WIN32)
  # httplib requires ws2_32, wsock32, and crypt32 on Windows
  target_link_libraries(Agent PRIVATE ws2_32 wsock32 crypt32)
  # Add defines to avoid conflicts with Windows.h
  target_compile_definitions(Agent PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()