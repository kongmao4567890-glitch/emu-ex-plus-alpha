# Enable experimental import std support for CMake 4.4.0
# UUID from Help/dev/experimental.rst v4.4.0
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d" CACHE STRING "")
set(CMAKE_CXX_STANDARD_LIBRARY "libstdc++" CACHE STRING "")

include("${CMAKE_CURRENT_LIST_DIR}/linux-x86_64.cmake")
