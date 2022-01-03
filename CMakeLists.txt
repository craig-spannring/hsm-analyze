cmake_minimum_required(VERSION 3.4.3)
project(hsm-analyze)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if (NOT llvm_config_prog)
  #set(LLVM_CONFIG_PROG "llvm-config")
  set(llvm_config_prog "/Users/cts/tmp/installed/bin/llvm-config")
endif()

if (NOT LLVM_DIR)
  execute_process(
    COMMAND ${llvm_config_prog} --cmakedir
    OUTPUT_VARIABLE LLVM_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

execute_process(
  COMMAND ${llvm_config_prog} --has-rtti
  OUTPUT_VARIABLE llvm_config_has_rtti
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT Clang_DIR)
  file(REAL_PATH "${LLVM_DIR}/../clang" Clang_DIR)
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}")
list(APPEND CMAKE_MODULE_PATH "${Clang_DIR}")


option(USE_STATIC_CRT
	"Set to ON to link against static CRT (/MT)." OFF)

option(USE_RELEASE_LIBS_IN_DEBUG
	"Set to ON to link against release llvm/clang libs in Debug." OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# DLL CRT by default
	set(LLVM_USE_CRT_RELEASE "MD")
	set(LLVM_USE_CRT_DEBUG "MDd")

	if (USE_STATIC_CRT)
		set(LLVM_USE_CRT_RELEASE "MT")
		set(LLVM_USE_CRT_DEBUG "MTd")
	endif()

	if (USE_RELEASE_LIBS_IN_DEBUG)
		set(LLVM_USE_CRT_DEBUG ${LLVM_USE_CRT_RELEASE})
		add_definitions(/U_DEBUG)
	endif()

	set(LLVM_USE_CRT_MINSIZEREL ${LLVM_USE_CRT_RELEASE})
	set(LLVM_USE_CRT_RELWITHDEBINFO ${LLVM_USE_CRT_RELEASE})
endif()

# Clang libs disable exceptions so disable try/catch in MSVC headers
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_definitions(/D_HAS_EXCEPTIONS=0)
endif()

# Find ClangConfig.cmake which will find_package(LLVM)
# On Windows: -DCMAKE_PREFIX_PATH="C:\Program Files\LLVM\lib\cmake\clang"
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${CLANG_CMAKE_DIR}")

# HandleLLVMOptions sets up compilation flags (disable warnings, etc.)
list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
include(HandleLLVMOptions)

file(GLOB SRC "src/*.cpp" "src/*.h")
add_executable(hsm-analyze ${SRC})

include_directories(${LLVM_INCLUDE_DIRS})
target_link_libraries(hsm-analyze PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling)


message("rtti ${llvm_config_has_rtti}")

if (NOT ${llvm_config_has_rtti})
  message("disabling rtti")
  if(MSVC)
    target_compile_options(hsm-analyze PRIVATE "/GR-")
  else()
    target_compile_options(hsm-analyze PRIVATE "-fno-exceptions" "-fno-rtti")
  endif()
endif()
