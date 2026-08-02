# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED OUTPUT OR NOT DEFINED URL OR NOT DEFINED SHA256)
    message(FATAL_ERROR "OUTPUT, URL, and SHA256 are required")
endif()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")

if(EXISTS "${OUTPUT}")
    file(SHA256 "${OUTPUT}" existing_sha256)
    if(existing_sha256 STREQUAL SHA256)
        message(STATUS "Whisper model already downloaded and verified")
        return()
    endif()
    message(STATUS "Existing Whisper model has the wrong checksum; downloading it again")
    file(REMOVE "${OUTPUT}")
endif()

set(partial "${OUTPUT}.part")
if(EXISTS "${partial}")
    file(SHA256 "${partial}" partial_sha256)
    if(partial_sha256 STREQUAL SHA256)
        file(RENAME "${partial}" "${OUTPUT}")
        message(STATUS "Verified the completed Whisper model download")
        return()
    endif()
endif()
file(REMOVE "${partial}")
file(DOWNLOAD "${URL}" "${partial}"
    EXPECTED_HASH "SHA256=${SHA256}"
    SHOW_PROGRESS
    STATUS download_status
    TLS_VERIFY ON)
list(GET download_status 0 status_code)
list(GET download_status 1 status_message)
if(NOT status_code EQUAL 0)
    file(REMOVE "${partial}")
    message(FATAL_ERROR "Model download failed: ${status_message}")
endif()
file(RENAME "${partial}" "${OUTPUT}")
