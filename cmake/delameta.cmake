## CPM.cmake
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2019-2023 Lars Melchior and contributors

set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_HASH_SUM "c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d")

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

# Check if GITLAB_TOKEN is specified and set defaults
if(GITLAB_TOKEN)
  set(GITLAB_USER "gitlab-ci-token")
  set(GITLAB_PASSWORD "${GITLAB_TOKEN}")
elseif(DEFINED ENV{GITLAB_TOKEN})
  set(GITLAB_USER "gitlab-ci-token")
  set(GITLAB_PASSWORD "$ENV{GITLAB_TOKEN}")
endif()

if (NOT DEFINED GITLAB_USER)
  message(FATAL_ERROR "GITLAB_USER must be specified")
endif()

if (NOT DEFINED GITLAB_PASSWORD)
  message(FATAL_ERROR "GITLAB_PASSWORD must be specified")
endif()

# add package from git.delameta.id
function(delameta_gitlab_add_package url)
  # Parse input URL of the form "[user:password@]package:repo.git#tag"
  string(REGEX MATCH "(([^:]+):([^@]+)@)?([^:]+):([^#]+)#(.+)" _ "${url}")

  # Set optional USER and PASSWORD, defaulting to GITLAB_USER and GITLAB_PASSWORD if not specified
  if (NOT DEFINED CMAKE_MATCH_2 OR CMAKE_MATCH_2 STREQUAL "")
    set(USER ${GITLAB_USER})
  else()
    set(USER ${CMAKE_MATCH_2})
  endif()

  if (NOT DEFINED CMAKE_MATCH_3 OR CMAKE_MATCH_3 STREQUAL "")
    set(PASSWORD ${GITLAB_PASSWORD})
  else()
    set(PASSWORD ${CMAKE_MATCH_3})
  endif()

  # Extract other variables
  set(PACKAGE ${CMAKE_MATCH_4})
  set(REPO_PATH ${CMAKE_MATCH_5})
  set(TAG ${CMAKE_MATCH_6})

  # Construct the full GitLab URL
  set(GIT_REPOSITORY "https://${USER}:${PASSWORD}@git.delameta.id/${REPO_PATH}.git")

  # Capture additional options if provided
  set(options ${ARGN})

  # Call CPMAddPackage with the parsed and constructed values
  CPMAddPackage(
    NAME           ${PACKAGE}
    GIT_REPOSITORY ${GIT_REPOSITORY}
    GIT_TAG        ${TAG}
    ${options}  # Any additional options passed to the function
  )
endfunction()

# add package from github.com
function(delameta_github_add_package url)
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
endfunction()
