# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

set(IGDRCL_SRCS_tests_source_level_debugger
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_csr_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_preamble_test.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_preamble_test.inl
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_source_level_debugger})

if(SOURCE_LEVEL_DEBUGGER_HEADERS_DIR)
  target_sources(igdrcl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_tests.cpp)
endif()