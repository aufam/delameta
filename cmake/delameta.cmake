## CPM.cmake
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2019-2023 Lars Melchior and contributors

set(CPM_DOWNLOAD_VERSION 0.40.5)
set(CPM_HASH_SUM "c46b876ae3b9f994b4f05a4c15553e0485636862064f1fcc9d8b4f832086bc5d")

if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

file(DOWNLOAD
     https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
     ${CPM_DOWNLOAD_LOCATION} EXPECTED_HASH SHA256=${CPM_HASH_SUM}
)

include(${CPM_DOWNLOAD_LOCATION})
## end of CPM.cmake

# add package from github.com
function(delameta_github_package url)
  # Parse input URL of the form "[user:password@]package:repo.git#tag"
  string(REGEX MATCH "([^:]+):([^#]+)#(.+)" _ "${url}")

  # Extract other variables
  set(PACKAGE ${CMAKE_MATCH_1})
  set(REPO_PATH ${CMAKE_MATCH_2})
  set(TAG ${CMAKE_MATCH_3})

  # Construct the full GitLab URL
  set(GIT_REPOSITORY "https://github.com/${REPO_PATH}.git")

  # Capture additional options if provided
  set(options ${ARGN})

  # Call CPMAddPackage with the parsed and constructed values
  CPMAddPackage(
    NAME           ${PACKAGE}
    GIT_REPOSITORY ${GIT_REPOSITORY}
    GIT_TAG        ${TAG}
    ${options}  # Any additional options passed to the function
  )

  set(${PACKAGE}_SOURCE_DIR ${${PACKAGE}_SOURCE_DIR} PARENT_SCOPE)
endfunction()
