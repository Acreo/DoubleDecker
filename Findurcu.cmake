################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################

if (NOT MSVC)
    include(FindPkgConfig)
    pkg_check_modules(PC_URCU "urcu")
    if (NOT PC_URCU_FOUND)
        pkg_check_modules(PC_URCU "urcu")
    endif (NOT PC_URCU_FOUND)
    if (PC_URCU_FOUND)
        # add CFLAGS from pkg-config file, e.g. draft api.
        add_definitions(${PC_URCU_CFLAGS} ${PC_URCU_CFLAGS_OTHER})
        # some libraries install the headers is a subdirectory of the include dir
        # returned by pkg-config, so use a wildcard match to improve chances of finding
        # headers and SOs.
        set(PC_URCU_INCLUDE_HINTS ${PC_URCU_INCLUDE_DIRS} ${PC_URCU_INCLUDE_DIRS}/*)
        set(PC_URCU_LIBRARY_HINTS ${PC_URCU_LIBRARY_DIRS} ${PC_URCU_LIBRARY_DIRS}/*)
    endif(PC_URCU_FOUND)
endif (NOT MSVC)

find_path (
    URCU_INCLUDE_DIRS
    NAMES urcu.h
    HINTS ${PC_URCU_INCLUDE_HINTS}
)

find_library (
    URCU_LIBRARIES
    NAMES urcu
    HINTS ${PC_URCU_LIBRARY_HINTS}
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    URCU
    REQUIRED_VARS URCU_LIBRARIES URCU_INCLUDE_DIRS
)
mark_as_advanced(
    URCU_FOUND
    URCU_LIBRARIES URCU_INCLUDE_DIRS
)

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
