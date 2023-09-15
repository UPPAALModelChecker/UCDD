find_package(UDBM 2.0.14 QUIET)

if (UDBM_FOUND)
  message(STATUS "Found UDBM: ${UDBM_DIR}")
else(UDBM_FOUND)
    message(STATUS "Failed to find UDBM, going to compile from source.")
    if (FIND_FATAL)
      message(FATAL_ERROR "Failed to find UDBM with CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
    endif(FIND_FATAL)
    include(FetchContent)
    set(UDBM_WITH_TESTS OFF CACHE BOOL "UDBM tests")
    FetchContent_Declare(
            UDBM
            GIT_REPOSITORY https://github.com/UPPAALModelChecker/UDBM.git
            GIT_TAG v2.0.14
            GIT_SHALLOW TRUE # get only the last commit version
            GIT_PROGRESS TRUE # show progress of download
            # FIND_PACKAGE_ARGS NAMES doctest
            USES_TERMINAL_DOWNLOAD TRUE # show progress in ninja generator
            USES_TERMINAL_CONFIGURE ON
            USES_TERMINAL_BUILD ON
            USES_TERMINAL_INSTALL ON
    )
    FetchContent_GetProperties(UDBM)
    if (udbm_POPULATED)
        message(STATUS "Found populated UUtils: ${uutils_SOURCE_DIR}")
    else (udbm_POPULATED)
        FetchContent_Populate(UDBM)
        add_subdirectory(${udbm_SOURCE_DIR} ${udbm_BINARY_DIR} EXCLUDE_FROM_ALL)
        message(STATUS "Got UDBM: ${udbm_SOURCE_DIR}")
    endif (udbm_POPULATED)
endif(UDBM_FOUND)
