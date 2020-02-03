# swd - Scripts with Dependencies
# Copyright (C) 2020 Pauli Saksa
#
# Licensed under The MIT License, see file LICENSE.txt in this source tree.

add_executable(swd
    config.cc
    hash-cache.cc
    hash-cache_impl.cc
    hash-tools.cc
    master.cc
    scan.cc
    script-tools.cc
    script.cc
    #
    utils/ansi.cc
    utils/exec.cc
    utils/path.cc
    utils/stream.cc
    utils/string.cc
    #
    main.cc
)
target_compile_options(swd PRIVATE -O2)
target_include_directories(swd PRIVATE ${THIRD_PARTY})
