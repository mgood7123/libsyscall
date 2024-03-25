# CMakeList.txt : CMake project for libsyscall, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project(libsyscall_wl_fd_allocator CXX)

# Add source to this project's executable.
add_library(libsyscall_wl_fd_allocator STATIC wl_fd_allocator.cpp)

target_include_directories(libsyscall_wl_fd_allocator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_property(TARGET libsyscall_wl_fd_allocator PROPERTY C_STANDARD 11)
set_property(TARGET libsyscall_wl_fd_allocator PROPERTY CXX_STANDARD 17)

# TODO: Add tests and install targets if needed.
