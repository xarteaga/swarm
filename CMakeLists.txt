#
# This file is part of the swarm project
#
# Copyright (c) 2021 by Xavier Arteaga
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
cmake_minimum_required(VERSION 3.20)
project(swarm)

set(CMAKE_CXX_STANDARD 14)

INCLUDE(FindPkgConfig)

# Find required headers and libraries
FIND_PATH(
        LIBSSH_INCLUDE_DIRS
        NAMES libssh/libssh.h
        PATHS /usr/local/include
        /usr/include
)
message(STATUS "LIBSSH INCLUDE DIRS " ${LIBSSH_INCLUDE_DIRS})


FIND_LIBRARY(
        LIBSSH_LIBRARY
        NAMES ssh
        PATHS /usr/local/lib
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib64
        /usr/local/lib32
)
message(STATUS "LIBSSH LIBRARIES " ${LIBSSH_LIBRARY})

FIND_LIBRARY(
        LIBSSL_LIBRARY
        NAMES ssl
        PATHS /usr/local/lib
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib64
        /usr/local/lib32
)
message(STATUS "LIBSSL LIBRARIES " ${LIBSSL_LIBRARY})

FIND_LIBRARY(
        LIBPTHREAD_LIBRARY
        NAMES pthread
        PATHS /usr/local/lib
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib64
        /usr/local/lib32
)
message(STATUS "LIBPTHREAD LIBRARIES " ${LIBPTHREAD_LIBRARY})

# Put required libraries together
set(SWARM_LIBRARIES ${LIBSSH_LIBRARY} ${LIBSSL_LIBRARY} ${LIBGSSAPI_LIBRARY} ${LIBPTHREAD_LIBRARY})

include_directories(${LIBSSH_INCLUDE_DIRS})
link_directories(${LIBSSH_LIBRARY_DIRS})

add_library(swarm-lib hostnames.cpp ssh_impl.cpp args.h)
target_link_libraries(swarm-lib ${SWARM_LIBRARIES})

add_executable(swarm-cc swarm_cc.cpp)
target_link_libraries(swarm-cc ${SWARM_LIBRARIES} swarm-lib)
install(TARGETS swarm-cc)
install(TARGETS swarm-lib)