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

# Проверка архитектуры
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCHITECTURE "x64")
else()
    set(ARCHITECTURE "x86")
endif()

message(STATUS "Building for architecture: ${ARCHITECTURE}")

include(FetchContent)

# Find OpenSSL for HTTPS support in httplib
find_package(OpenSSL REQUIRED)


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
  GIT_TAG        master # Pin to the latest stable version to ensure ABI compatibility with grammars
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

# tree-sitter-css grammar
FetchContent_Declare(
  tree_s_css_gram
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-css.git
  GIT_TAG        master
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_Populate(tree_s_css_gram)
set(tree_sitter_css_grammar_SOURCE_DIR ${tree_s_css_gram_SOURCE_DIR})

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

# tree-sitter-html grammar
FetchContent_Declare(
  tree_s_html_gram
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-html.git
  GIT_TAG        master
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_Populate(tree_s_html_gram)
set(tree_sitter_html_grammar_SOURCE_DIR ${tree_s_html_gram_SOURCE_DIR})

# tree-sitter-javascript grammar
FetchContent_Declare(
  tree_s_js_gram
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-javascript.git
  GIT_TAG        master
  GIT_SHALLOW TRUE
  FETCHCONTENT_QUIET OFF
)
FetchContent_Populate(tree_s_js_gram)
set(tree_sitter_javascript_grammar_SOURCE_DIR ${tree_s_js_gram_SOURCE_DIR})

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

# Явно указываем, что все файлы грамматик - это чистый Си, чтобы избежать конфликтов при линковке в C++ проекте.
set_source_files_properties(
  ${tree_sitter_cpp_grammar_SOURCE_DIR}/src/parser.c
  ${tree_sitter_cpp_grammar_SOURCE_DIR}/src/scanner.c
  PROPERTIES LANGUAGE C
)
set_source_files_properties(
  ${tree_sitter_css_grammar_SOURCE_DIR}/src/parser.c
  ${tree_sitter_css_grammar_SOURCE_DIR}/src/scanner.c
  PROPERTIES LANGUAGE C
)
set_source_files_properties(
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/parser.c
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/scanner.c
  PROPERTIES LANGUAGE C
)
set_source_files_properties(
  ${tree_sitter_html_grammar_SOURCE_DIR}/src/parser.c
  ${tree_sitter_html_grammar_SOURCE_DIR}/src/scanner.c
  PROPERTIES LANGUAGE C
)
set_source_files_properties(
  ${tree_sitter_javascript_grammar_SOURCE_DIR}/src/parser.c
  ${tree_sitter_javascript_grammar_SOURCE_DIR}/src/scanner.c
  PROPERTIES LANGUAGE C
)

# Переопределяем видимость символов для статической библиотеки маркдауна, 
# чтобы предотвратить конфликты одинаковых имен функций с другими сканерами на этапе линковки
# if(MSVC)
#   target_compile_options(tree_sitter_markdown_lib PRIVATE "/wd4005" "/wd4142")
# endif()

add_executable(Agent
      src/main.cpp
      src/ApiHandlers.cpp
      src/SessionManager.cpp
      src/ContextIndexer.cpp
      src/ContextIndexerHelper/FileReader.cpp
      src/ContextIndexerHelper/ChunkerStrategy.cpp
      src/ContextIndexerHelper/IndexManager.cpp
      src/ContextIndexerHelper/Searcher.cpp
      src/ContextIndexerHelper/FileIndexer.cpp
      src/EmbeddingClient.cpp
      src/AssistantRole.cpp
      src/Logger.cpp
      src/CodeParser.cpp
      src/Config.cpp
      src/WebSocketServer.cpp
      src/ThreadPool.cpp
      src/tools/ReadFileTool.cpp
      src/SessionManager.cpp
      src/tools/ListDirectoryTool.cpp
      src/tools/ToolManager.cpp
      src/tools/CodeSearchTool.cpp
      src/tools/GrepSearchTool.cpp
      src/tools/FileGlobSearchTool.cpp
      src/tools/WriteFileTool.cpp
      src/tools/EditFileTool.cpp
      src/tools/ApplyDiffTool.cpp
      src/tools/ReadUrlTool.cpp
      src/tools/WebSearchTool.cpp
      src/tools/OpenUrlTool.cpp
      src/tools/GitHubSearchTool.cpp
      src/tools/ExecuteShellCommandTool.cpp
      src/tools/GetDateTimeTool.cpp
      src/tools/GetSystemInfoTool.cpp
      src/FileWatcher.cpp
      # Добавляем исходники грамматик напрямую в исполняемый файл
      ${tree_sitter_cpp_grammar_SOURCE_DIR}/src/parser.c
      ${tree_sitter_cpp_grammar_SOURCE_DIR}/src/scanner.c
      ${tree_sitter_css_grammar_SOURCE_DIR}/src/parser.c
      ${tree_sitter_css_grammar_SOURCE_DIR}/src/scanner.c
      ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/parser.c
      ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src/scanner.c
      ${tree_sitter_html_grammar_SOURCE_DIR}/src/parser.c
      ${tree_sitter_html_grammar_SOURCE_DIR}/src/scanner.c
      ${tree_sitter_javascript_grammar_SOURCE_DIR}/src/parser.c
      ${tree_sitter_javascript_grammar_SOURCE_DIR}/src/scanner.c
      )

target_include_directories(Agent PUBLIC
  $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${httplib_SOURCE_DIR}>
  # Add src directory to includes to find headers in subdirectories like `tools`
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tools
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ContextIndexerHelper
  $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${hnswlib_SOURCE_DIR}>
  $<BUILD_INTERFACE:${tree_sitter_core_SOURCE_DIR}/lib/include> # Заголовочные файлы Tree-sitter core
  # Добавляем пути к заголовочным файлам грамматик
  ${tree_sitter_cpp_grammar_SOURCE_DIR}/src
  ${tree_sitter_css_grammar_SOURCE_DIR}/src
  ${tree_sitter_markdown_grammar_SOURCE_DIR}/tree-sitter-markdown/src
  ${tree_sitter_html_grammar_SOURCE_DIR}/src
  ${tree_sitter_javascript_grammar_SOURCE_DIR}/src
)

# Устанавливаем минимальный уровень логирования spdlog на этапе компиляции.
# Это позволяет использовать SPDLOG_DEBUG и SPDLOG_TRACE в релизных сборках.
target_compile_definitions(Agent PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
# Включаем поддержку SSL в cpp-httplib
target_compile_definitions(Agent PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# Link against dependencies
target_link_libraries(Agent PRIVATE spdlog::spdlog hnswlib tree-sitter OpenSSL::SSL OpenSSL::Crypto)

# --- Platform Specifics ---

if(WIN32)
  # httplib requires ws2_32, wsock32, and crypt32 on Windows
  target_link_libraries(Agent PRIVATE ws2_32 wsock32 crypt32 Bcrypt)
  # Add defines to avoid conflicts with Windows.h
  target_compile_definitions(Agent PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()