# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\MiniAccess_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\MiniAccess_autogen.dir\\ParseCache.txt"
  "MiniAccess_autogen"
  )
endif()
