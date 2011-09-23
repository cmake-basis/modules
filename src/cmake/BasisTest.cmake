##############################################################################
# @file  BasisTest.cmake
# @brief CTest configuration. Include this module instead of CTest.
#
# @note This module is included by basis_project_initialize().
#
# Copyright (c) 2011 University of Pennsylvania. All rights reserved.
# See https://www.rad.upenn.edu/sbia/software/license.html or COPYING file.
#
# Contact: SBIA Group <sbia-software at uphs.upenn.edu>
#
# @ingroup CMakeAPI
##############################################################################

# ============================================================================
# configuration
# ============================================================================

# include CTest module which enables testing, but prevent it from generating
# any configuration file or adding targets yet as we want to adjust the
# default CTest settings--in particular the site name--before
set (RUN_FROM_DART 1)
include (CTest)

# mark timeout option as advanced
mark_as_advanced (DART_TESTING_TIMEOUT)

# remove ".local" or ".uphs.upenn.edu" suffix from site name
string (REGEX REPLACE "\\.local$|\\.uphs\\.upenn\\.edu$" "" SITE "${SITE}")

set (RUN_FROM_CTEST_OR_DART 1)
include (CTestTargets)
set (RUN_FROM_CTEST_OR_DART)

# disable testing if no testing sources found
if (NOT EXISTS "${PROJECT_TESTING_DIR}")
  set (BUILD_TESTING "OFF" CACHE INTERNAL "No testing tree to build." FORCE)
else ()
  set (BUILD_TESTING "ON" CACHE BOOL "Build the testing tree.")
endif ()

# configure custom CTest settings and/or copy them to binary tree
# TODO How does this go well with the super-build?
if ("${PROJECT_BINARY_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  set (CTEST_CUSTOM_FILE "CTestCustom.cmake")
else ()
  set (CTEST_CUSTOM_FILE "CTestCustom-${PROJECT_NAME}.cmake")
endif ()

if (EXISTS "${PROJECT_CONFIG_DIR}/CTestCustom.cmake.in")
  configure_file (
    "${PROJECT_CONFIG_DIR}/CTestCustom.cmake.in"
    "${CMAKE_BINARY_DIR}/${CTEST_CUSTOM_FILE}"
    @ONLY
  )
elseif (EXISTS "${PROJECT_CONFIG_DIR}/CTestCustom.cmake")
  configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake"
    "${CMAKE_BINARY_DIR}/${CTEST_CUSTOM_FILE}"
    COPYONLY
  )
endif ()

if (
      NOT "${CTEST_CUSTOM_FILE}" STREQUAL "CTestCustom.cmake"
  AND EXISTS "${CMAKE_BINARY_DIR}/${CTEST_CUSTOM_FILE}"
)
  file (
    APPEND "${CMAKE_BINARY_DIR}/CTestCustom.cmake"
      "\ninclude (\"${CMAKE_BINARY_DIR}/${CTEST_CUSTOM_FILE}\")\n")
endif ()

# ============================================================================
# utilities
# ============================================================================

## @addtogroup CMakeAPI
#  @{

##############################################################################
# @brief Replaces CMake's set_tests_properties() command.
#
# @sa http://www.cmake.org/cmake/help/cmake-2-8-docs.html#command:set_tests_property
#
# @param [in] ARGN Arguments for set_tests_property().
#
# @returns Sets the given properties of the specified test.

function (basis_set_tests_properties)
  set (UIDS)
  list (GET ARGN 0 ARG)
  while (ARG AND NOT ARG STREQUAL "PROPERTIES")
    basis_test_uid (UID "${ARG}")
    list (APPEND UIDS "${UID}")
    list (REMOVE_AT ARGN 0)
    list (GET ARGN 0 ARG)
  endwhile ()
  set_tests_properties (${UIDS} ${ARGN})
endfunction ()

##############################################################################
# @brief Replaces CMake's get_test_property() command.
#
# @sa http://www.cmake.org/cmake/help/cmake-2-8-docs.html#command:get_test_property
#
# @param [out] VAR       Property value.
# @param [in]  TEST_NAME Name of test.
# @param [in]  ARGN      Remaining arguments of get_test_property().
#
# @returns Sets @p VAR to the value of the requested property.

function (basis_get_test_property VAR TEST_NAME)
  basis_test_uid (TEST_UID "${TEST_NAME}")
  get_test_property (VALUE "${TEST_UID}" ${ARGN})
  set (${VAR} "${VALUE}" PARENT_SCOPE)
endfunction ()

##############################################################################
# @brief Add test.
#
# @todo Make use of ExternalData module to fetch remote test data.
#
# @param [in] TEST_NAME Name of the test.
# @param [in] ARGN      The following parameters are parsed:
# @par
# <table border="0">
#   <tr>
#     @tp @b LANGUAGE lang @endtp
#     <td>The programming language in which the test is implemented.
#         It is determined automatically from the given input source code
#         files if not specified explicitly. Otherwise, if the programming
#         language could not be determined, it is assumed that the given
#         input file is already the test executable itself.</td>
#   </tr>
#   <tr>
#     @tp @b SOURCES file1 [file2 ...] @endtp
#     <td>The source files of the unit test. If this list contains a
#         file named either "*-main.*" or "*_main.*", the default
#         implementation of the main() function is not used.
#         Otherwise, the executable is linked to the default implementation of
#         the main() function, i.e., the static library basis_test_main.
#         If this option is omitted, the @p TARGET_NAME argument is assumed
#         to be the name of the C++ file with the test implementation.</td>
#   </tr>
#   <tr>
#     @tp @b LINK_DEPENDS file1|target1 [file2|target2 ...] @endtp
#     <td>Link dependencies.</td>
#   </tr>
#   <tr>
#     @tp @b ARGS arg1 [arg2 ...] @endtp
#     <td>Arguments passed on to CMake's add_test().</td>
#   </tr>
# </table>
#
# @returns Adds CTest test.

function (basis_add_test TEST_NAME)
  # --------------------------------------------------------------------------
  # parse arguments
  CMAKE_PARSE_ARGUMENTS (
    ARGN
    "UNITTEST;NO_DEFAULT_MAIN"
    "LANGUAGE;WORKING_DIRECTORY"
    "CONFIGURATIONS;SOURCES;LINK_DEPENDS;COMMAND;ARGS"
    ${ARGN}
  )

  # --------------------------------------------------------------------------
  # test name
  if (NOT ARGN_COMMAND AND NOT ARGN_SOURCES)
    get_filename_component (ARGN_SOURCES "${TEST_NAME}" ABSOLUTE)
    string (REGEX REPLACE "\\.in$" "" TEST_NAME "${TEST_NAME}")
    get_filename_component (TEST_NAME "${TEST_NAME}" NAME_WE)
  endif ()

  basis_check_test_name ("${TEST_NAME}")
  basis_test_uid (TEST_UID "${TEST_NAME}")

  # --------------------------------------------------------------------------
  # build test executable
  if (ARGN_SOURCES)
    if (ARGN_UNITTEST)
      list (APPEND ARGN_LINK_DEPENDS "basis${BASIS_NAMESPACE_SEPARATOR}test-all")
      if (NOT ARGN_NO_DEFAULT_MAIN)
        foreach (SOURCE ${ARGN_SOURCES})
          if (SOURCE MATCHES "-main\\.|_main\\.")
            set (ARGN_NO_DEFAULT_MAIN 1)
            break ()
          endif ()
        endforeach ()
      endif ()
      if (NOT ARGN_NO_DEFAULT_MAIN)
        list (APPEND ARGN_LINK_DEPENDS "basis${BASIS_NAMESPACE_SEPARATOR}test-main")
      endif ()
    endif ()

    basis_add_executable (${TEST_NAME} TEST ${ARGN_SOURCES})
    basis_target_link_libraries (${TEST_NAME} ${ARGN_LINK_DEPENDS})

    if (ARGN_COMMAND)
      basis_set_target_properties (${TEST_NAME} PROPERTIES OUTPUT_NAME ${ARGN_COMMAND})
    endif ()
    basis_get_target_location (ARGN_COMMAND "${TEST_NAME}" ABSOLUTE)
  endif ()

  # --------------------------------------------------------------------------
  # add test
  message (STATUS "Adding test ${TEST_UID}...")

  set (OPTS)
  if (ARGN_WORKING_DIRECTORY)
    list (APPEND OPTS "WORKING_DIRECTORY" "${ARGN_WORKING_DIRECTORY}")
  endif ()
  if (ARGN_CONFIGURATIONS)
    list (APPEND OPTS "CONFIGURATIONS")
    foreach (CONFIG ${ARGN_CONFIGURATIONS})
      list (APPEND OPTS "${CONFIG}")
    endforeach ()
  endif ()

  add_test (NAME ${TEST_UID} COMMAND ${ARGN_COMMAND} ${ARGN_ARGS} ${OPTS})

  message (STATUS "Adding test ${TEST_UID}... - done")
endfunction ()

#############################################################################
# @brief Add tests of default options for given executable.
#
# @par Default options:
# <table border="0">
#   <tr>
#     @tp <b>-u [ --usage ]</b> @endtp
#     <td>Usage information. The output has to match the regular expression
#         "[Uu]sage:\n\s*\<executable name\>", where \<executable name\>
#         is the name of the tested executable.</td>
#   </tr>
#   <tr>
#     @tp <b>-h [ --help ]</b> @endtp
#     <td>Help screen. Simply tests if the option is accepted.</td>
#   </tr>
#   <tr>
#     @tp <b>-V [ --version ]</b> @endtp
#     <td>Version information. Output has to include the project version string.</td>
#   </tr>
#   <tr>
#     @tp <b>-v [ --verbose ]</b> @endtp
#     <td>Increase verbosity of output messages. Simply tests if the option is accepted.</td>
#   </tr>
# </table>
#
# @param [in] TARGET_NAME Name of executable or script target.
#
# @returns Adds tests for the default options of the specified executable.

function (basis_add_tests_of_default_options TARGET_NAME)
  basis_target_uid (TARGET_UID "${TARGET_NAME}")

  if (BASIS_VERBOSE)
    message (STATUS "Adding tests of default options for ${TARGET_UID}...")
  endif ()

  if (NOT TARGET "${TARGET_UID}")
    message (FATAL_ERROR "Unknown target ${TARGET_UID}.")
  endif ()

  # get executable name
  get_target_property (PREFIX      ${TARGET_UID} "PREFIX")
  get_target_property (OUTPUT_NAME ${TARGET_UID} "OUTPUT_NAME")
  get_target_property (SUFFIX      ${TARGET_UID} "SUFFIX")

  if (NOT OUTPUT_NAME)
    set (EXEC_NAME "${TARGET_UID}")
  endif ()
  if (PREFIX)
    set (EXEC_NAME "${PREFIX}${EXEC_NAME}")
  endif ()
  if (SUFFIX)
    set (EXEC_NAME "${EXEC_NAME}${SUFFIX}")
  endif ()

  # get absolute path to executable
  get_target_property (EXEC_DIR ${TARGET_UID} "RUNTIME_OUTPUT_DIRECTORY")

  # executable command
  set (EXEC_CMD "${EXEC_DIR}/${EXEC_NAME}")

  # test option: -v
  basis_add_test (${EXEC}VersionS "${EXEC_CMD}" "-v")

  set_tests_properties (
    ${EXEC}VersionS
    PROPERTIES
      PASS_REGULAR_EXPRESSION "${EXEC} ${PROJECT_VERSION}"
  )

  # test option: --version
  basis_add_test (${EXEC}VersionL "${EXEC_CMD}" "--version")

  set_tests_properties (
    ${EXEC}VersionL
    PROPERTIES
      PASS_REGULAR_EXPRESSION "${EXEC} ${PROJECT_VERSION}"
  )

  # test option: -h
  basis_add_test (${EXEC}HelpS "${EXEC_CMD}" "-h")

  # test option: --help
  basis_add_test (${EXEC}HelpL "${EXEC_CMD}" "--help")

  # test option: -u
  basis_add_test (${EXEC}UsageS "${EXEC_CMD}" "-u")

  set_tests_properties (
    ${EXEC}UsageS
    PROPERTIES
      PASS_REGULAR_EXPRESSION "[Uu]sage:(\n)( )*${EXEC_NAME}"
  )

  # test option: --usage
  basis_add_test (${EXEC}UsageL "${EXEC_CMD}" "--usage")

  set_tests_properties (
    ${EXEC}UsageL
    PROPERTIES
      PASS_REGULAR_EXPRESSION "[Uu]sage:(\n)( )*${EXEC_NAME}"
  )

  if (BASIS_VERBOSE)
    message (STATUS "Adding tests of default options for ${EXEC}... - done")
  endif ()
endfunction ()

## @}
