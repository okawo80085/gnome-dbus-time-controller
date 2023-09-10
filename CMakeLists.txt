cmake_minimum_required(VERSION 3.13)

project(gnomeDbusTimeController)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

option(CODE_FORMAT "CODE_FORMAT" OFF)

if (CODE_FORMAT)
    add_custom_target(codeformat
      find
        ${CMAKE_SOURCE_DIR}/src
        -type f
        (
        -name "*.cpp"
        -o
        -name "*.h"
        -o
        -name "*.hpp"
        )
        -not -name "gnome-dbus-time-service-glue.hpp"
        -print0
      | xargs -0 clang-format -i
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Checking source code formatting"
      VERBATIM
    )
endif()

set(WARNING_OPTIONS
    " -Wall \
          -Wno-long-long \
          -Wno-unused-variable \
          -Werror \
          -Wno-error=deprecated-declarations \
          -fno-sanitize-recover=all \
          -fsanitize=bounds \
          -fsanitize=signed-integer-overflow")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_OPTIONS}")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(THREADS_PTHREAD_ARG
    "2"
    CACHE STRING "Forcibly set by CMakeLists.txt." FORCE)

find_package(PkgConfig REQUIRED) # Include functions provided by PkgConfig
                                 # module.

pkg_check_modules(DBUS REQUIRED dbus-c++-1) # This calls pkgconfig with
                                            # appropriate arguments
# Use results of pkg_check_modules() call.
include_directories(${DBUS_INCLUDE_DIRS})
link_directories(${DBUS_LIBRARY_DIRS})

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
