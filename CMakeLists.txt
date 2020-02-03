# swd - Scripts with Dependencies
# Copyright (C) 2020 Pauli Saksa
#
# Licensed under The MIT License, see file LICENSE.txt in this source tree.

cmake_minimum_required(VERSION 3.0)
project(swd)

add_compile_options(-Wall -Wextra -Werror)
# -fdiagnostics-color=always)
set(CMAKE_CXX_STANDARD 14)

set(THIRD_PARTY ${CMAKE_SOURCE_DIR}/3rd-party)

add_subdirectory(src)
