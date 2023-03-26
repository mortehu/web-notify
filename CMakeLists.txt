cmake_minimum_required(VERSION 3.14)

project(web-notify)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ASan" CACHE STRING "Build types" FORCE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer -O1 -g")
set(ASAN_LINKER_FLAGS "-fsanitize=address")

if(CMAKE_BUILD_TYPE STREQUAL "ASan")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ASAN_LINKER_FLAGS}")
endif()

include(FetchContent)

# Crow library
FetchContent_Declare(
  crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow
  GIT_TAG        v1.0+5
)
FetchContent_MakeAvailable(crow)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(PANGO REQUIRED pango)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)

# X11 and Xinerama
find_package(X11 REQUIRED)
if(NOT X11_Xinerama_FOUND)
  message(FATAL_ERROR "Xinerama not found")
endif()

add_executable(web-notify main.cc)

target_include_directories(web-notify PRIVATE ${Crow_SOURCE_DIR}/include ${CAIRO_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${PANGO_INCLUDE_DIRS} ${PANGOCAIRO_INCLUDE_DIRS})
target_link_libraries(web-notify PRIVATE ${X11_LIBRARIES} ${X11_Xinerama_LIB} ${CAIRO_LIBRARIES} ${GLIB2_LIBRARIES} ${PANGO_LIBRARIES} ${PANGOCAIRO_LIBRARIES})
