cmake_minimum_required(VERSION 3.15)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()
# Разрешаем создание дублирующих целевых (для работы с несколькими tree-sitter грамматиками)
set(ALLOW_DUPLICATE_CUSTOM_TARGETS ON)

project(Agent)

# Указываем CMake складывать готовый .exe прямо в корень проекта
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}")

# Force all libraries (including fetched ones) to be built as static libraries (.lib)
set(BUILD_SHARED_LIBS OFF)

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
  GIT_TAG        v1.x # не менять!!!!
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(spdlog)

# hnswlib
FetchContent_Declare(
  hnswlib
  GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
  GIT_TAG        master
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(hnswlib)
# --- Target ---

# tree-sitter core library
FetchContent_Declare(
  tree_sitter_core
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
  GIT_TAG        master # As requested
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_MakeAvailable(tree_sitter_core)

# tree-sitter-cpp grammar
FetchContent_Declare(
  tree_s_cpp_gram
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
  GIT_TAG        master # As requested
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_Populate(tree_s_cpp_gram)
set(tree_sitter_cpp_grammar_SOURCE_DIR ${tree_s_cpp_gram_SOURCE_DIR})

# tree-sitter-markdown grammar
FetchContent_Declare(
  tree_s_md_gram
  GIT_REPOSITORY https://github.com/tree-sitter-grammars/tree-sitter-markdown.git
  GIT_TAG        split_parser
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_Populate(tree_s_md_gram)
set(tree_sitter_markdown_grammar_SOURCE_DIR ${tree_s_md_gram_SOURCE_DIR})

# tree-sitter-cmake grammar
# FetchContent_Declare(
#   tree_s_cmake_gram
#   GIT_REPOSITORY https://github.com/uyha/tree-sitter-cmake.git
#   GIT_TAG        master
#   GIT_SHALLOW TRUE
#   FETCHCONTENT_QUIET OFF
# )
# FetchContent_Populate(tree_s_cmake_gram)
# set(tree_sitter_cmake_grammar_SOURCE_DIR ${tree_s_cmake_gram_SOURCE_DIR})

# Create static library from tree-sitter-cpp
add_library(tree_sitter_cpp_lib STATIC
  ${tree_sitter_cpp_grammar_SOURCE_DIR}/src/parser.c
  ${tree_sitter_cpp_grammar_SOURCE_DIR}/src/scanner.c
)
target_include_directories(tree_sitter_cpp_lib PUBLIC
  ${tree_sitter_cpp_grammar_SOURCE_DIR}/src
)

# Create static library from tree-sitter-cmake (Только parser.c, сканер отсутствует)
# add_library(tree_sitter_cmake_lib STATIC
#   ${tree_sitter_cmake_grammar_SOURCE_DIR}/src/parser.c
#   ${tree_sitter_cmake_grammar_SOURCE_DIR}/src/scanner.c
# )
# target_include_directories(tree_sitter_cmake_lib PUBLIC
#   ${tree_sitter_cmake_grammar_SOURCE_DIR}/src
# )
# set_source_files_properties(
#   ${tree_sitter_cmake_grammar_SOURCE_DIR}/src/parser.c
#   ${tree_sitter_cmake_grammar_SOURCE_DIR}/src/scanner.c
#   PROPERTIES LANGUAGE C
# )

# Create static library from tree-sitter-markdown
add_library(tree_sitter_markdown_lib STATIC
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/parser.c
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/scanner.c
)
target_include_directories(tree_sitter_markdown_lib PUBLIC
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src
)

# Явно указываем, что это файлы на чистом Си
set_source_files_properties(
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/parser.c
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/scanner.c
  PROPERTIES LANGUAGE C
)

# Переопределяем видимость символов для статической библиотеки маркдауна, 
# чтобы предотвратить конфликты одинаковых имен функций с другими сканерами на этапе линковки
if(MSVC)
  target_compile_options(tree_sitter_markdown_lib PRIVATE "/wd4005" "/wd4142")
endif()

add_executable(Agent 
      src/main.cpp 
      src/ContextIndexer.cpp 
      src/EmbeddingClient.cpp 
      src/AssistantRole.cpp
      src/Logger.cpp
      src/CodeParser.cpp
      src/Config.cpp
      src/tools/ReadFileTool.cpp
      src/tools/ListDirectoryTool.cpp
      src/tools/ToolManager.cpp
      src/tools/CodeSearchTool.cpp
      src/tools/GrepSearchTool.cpp
      src/tools/FileGlobSearchTool.cpp
      src/tools/WriteFileTool.cpp
      src/tools/EditFileTool.cpp
      src/tools/ApplyDiffTool.cpp
      src/tools/ExecuteShellCommandTool.cpp
      src/tools/GetDateTimeTool.cpp
      src/tools/GetSystemInfoTool.cpp
      src/FileWatcher.cpp
      )

target_include_directories(Agent PUBLIC
  $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${httplib_SOURCE_DIR}>
  # Add src directory to includes to find headers in subdirectories like `tools`
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${hnswlib_SOURCE_DIR}>
  $<BUILD_INTERFACE:${tree_sitter_core_SOURCE_DIR}/lib/include> # Заголовочные файлы Tree-sitter core
)

# Link against dependencies
target_link_libraries(Agent PRIVATE spdlog::spdlog hnswlib tree-sitter tree_sitter_cpp_lib tree_sitter_markdown_lib)

# --- Platform Specifics ---

if(WIN32)
  # httplib requires ws2_32, wsock32, and crypt32 on Windows
  target_link_libraries(Agent PRIVATE ws2_32 wsock32 crypt32)
  # Add defines to avoid conflicts with Windows.h
  target_compile_definitions(Agent PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()