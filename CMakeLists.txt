# CMakeList.txt : CMake project for libsyscall, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project (libsyscall C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")

# Add source to this project's executable.
add_executable(libsyscall example.cpp)

add_subdirectory(wl_fd_allocator)

target_include_directories(libsyscall PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_property(TARGET libsyscall PROPERTY C_STANDARD 11)
set_property(TARGET libsyscall PROPERTY CXX_STANDARD 17)

target_link_libraries(libsyscall PUBLIC libsyscall_wl_fd_allocator)

# TODO: Add tests and install targets if needed.
