# Called by CMake POST_BUILD to remove stale Qt DLLs before copying fresh ones.
# DIR is passed via -DDIR=...
file(GLOB _stale_dlls "${DIR}/Qt6*.dll")
foreach(_f IN LISTS _stale_dlls)
    file(REMOVE "${_f}")
    message(STATUS "Removed stale: ${_f}")
endforeach()

