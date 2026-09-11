if(NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "SOURCE_DIR and OUTPUT_FILE are required")
endif()

# Keep this scope deliberately independent of Git. Story Mode work commonly
# includes untracked source and commissioned art while it is being reviewed,
# and those bytes must be just as attestable as committed inputs. Build trees,
# captures, documents, design sources, and tooling are excluded by selecting
# only the client-owned build/runtime roots below.
set(snapshot_schema "bayou-client-input-snapshot-v1")

file(GLOB_RECURSE project_input_files LIST_DIRECTORIES false
    "${SOURCE_DIR}/cmake/*"
    "${SOURCE_DIR}/src/client/*"
    "${SOURCE_DIR}/src/shared/*")
list(APPEND project_input_files
    "${SOURCE_DIR}/CMakeLists.txt"
    "${SOURCE_DIR}/Directory.Build.props"
    "${SOURCE_DIR}/client_debug.cfg"
    "${SOURCE_DIR}/client_release.cfg"
    "${SOURCE_DIR}/deploy/ca/isrg-root-x1.pem"
    "${SOURCE_DIR}/src/gameserver/ai_player.cpp"
    "${SOURCE_DIR}/src/gameserver/ai_player.hpp"
    "${SOURCE_DIR}/src/gameserver/game_engine.hpp")

# The exhaustive UI registry renders more than scenario illustrations: fonts,
# card/piece art, animation frames, shared chrome, and board textures all affect
# its pixels. Hash every runtime asset rather than trying to maintain a fragile
# hand-authored subset.
file(GLOB_RECURSE runtime_asset_files LIST_DIRECTORIES false
    "${SOURCE_DIR}/assets/*")

function(compute_snapshot_group group_name output_prefix)
    set(files ${ARGN})
    list(REMOVE_DUPLICATES files)
    list(SORT files)

    set(canonical "")
    set(file_count 0)
    set(total_bytes 0)
    foreach(input_file IN LISTS files)
        if(NOT EXISTS "${input_file}" OR IS_DIRECTORY "${input_file}")
            message(FATAL_ERROR
                "Snapshot input is missing or not a regular file: ${input_file}")
        endif()
        if(IS_SYMLINK "${input_file}")
            message(FATAL_ERROR
                "Snapshot input may not be a symbolic link: ${input_file}")
        endif()

        file(RELATIVE_PATH relative_path "${SOURCE_DIR}" "${input_file}")
        string(REPLACE "\\" "/" relative_path "${relative_path}")
        file(SIZE "${input_file}" file_size)
        file(SHA256 "${input_file}" file_sha256)
        string(TOLOWER "${file_sha256}" file_sha256)
        string(LENGTH "${relative_path}" path_size)

        # Length-frame the path so even punctuation in a valid filename cannot
        # make two different inventories hash to the same record stream.
        string(APPEND canonical
            "P${path_size}:${relative_path}S${file_size}:H${file_sha256}\n")
        math(EXPR file_count "${file_count} + 1")
        math(EXPR total_bytes "${total_bytes} + ${file_size}")
    endforeach()

    string(SHA256 group_sha256 "${canonical}")
    string(TOLOWER "${group_sha256}" group_sha256)
    set(${output_prefix}_NAME "${group_name}" PARENT_SCOPE)
    set(${output_prefix}_SHA256 "${group_sha256}" PARENT_SCOPE)
    set(${output_prefix}_FILE_COUNT "${file_count}" PARENT_SCOPE)
    set(${output_prefix}_TOTAL_BYTES "${total_bytes}" PARENT_SCOPE)
endfunction()

compute_snapshot_group("projectInputs" project ${project_input_files})
compute_snapshot_group("runtimeAssets" assets ${runtime_asset_files})

set(overall_canonical "${snapshot_schema}\n")
string(APPEND overall_canonical
    "${project_NAME}:${project_FILE_COUNT}:${project_TOTAL_BYTES}:${project_SHA256}\n")
string(APPEND overall_canonical
    "${assets_NAME}:${assets_FILE_COUNT}:${assets_TOTAL_BYTES}:${assets_SHA256}\n")
string(SHA256 snapshot_sha256 "${overall_canonical}")
string(TOLOWER "${snapshot_sha256}" snapshot_sha256)
math(EXPR snapshot_file_count
    "${project_FILE_COUNT} + ${assets_FILE_COUNT}")
math(EXPR snapshot_total_bytes
    "${project_TOTAL_BYTES} + ${assets_TOTAL_BYTES}")

# Validate the hashing primitive itself while generating the identity. The
# capture runtime performs the same known-answer check before trusting a
# snapshot, and a successful capture additionally proves both canonicalizers
# agree on the complete inventory.
string(SHA256 sha256_self_test "abc")
if(NOT sha256_self_test STREQUAL
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    message(FATAL_ERROR "CMake SHA-256 known-answer test failed")
endif()

find_package(Git QUIET)

set(BAYOU_BUILD_SOURCE_COMMIT "")
set(BAYOU_BUILD_SOURCE_DIRTY true)
set(BAYOU_BUILD_SOURCE_AVAILABLE false)
set(BAYOU_BUILD_SOURCE_METHOD "sha256-content-snapshot")

if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --verify HEAD
        RESULT_VARIABLE commit_result
        OUTPUT_VARIABLE commit_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" status
            --porcelain=v1 --untracked-files=all --ignore-submodules=none
        RESULT_VARIABLE status_result
        OUTPUT_VARIABLE status_output
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(commit_result EQUAL 0 AND status_result EQUAL 0)
        set(BAYOU_BUILD_SOURCE_COMMIT "${commit_output}")
        set(BAYOU_BUILD_SOURCE_AVAILABLE true)
        set(BAYOU_BUILD_SOURCE_METHOD "git-plus-sha256-content-snapshot")
        if(status_output STREQUAL "")
            set(BAYOU_BUILD_SOURCE_DIRTY false)
        endif()
    endif()
endif()

string(TIMESTAMP BAYOU_BUILD_IDENTITY_GENERATED_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")

set(header "#pragma once\n\n")
string(APPEND header "#include <cstdint>\n\n")
string(APPEND header "namespace bayou::client::build_identity\n{\n")
string(APPEND header "struct SnapshotGroup\n{\n")
string(APPEND header "    const char* name;\n")
string(APPEND header "    const char* sha256;\n")
string(APPEND header "    std::uint64_t fileCount;\n")
string(APPEND header "    std::uint64_t totalBytes;\n")
string(APPEND header "};\n\n")
string(APPEND header "inline constexpr const char* SourceCommit = \"${BAYOU_BUILD_SOURCE_COMMIT}\";\n")
string(APPEND header "inline constexpr bool SourceDirty = ${BAYOU_BUILD_SOURCE_DIRTY};\n")
string(APPEND header "inline constexpr bool SourceAvailable = ${BAYOU_BUILD_SOURCE_AVAILABLE};\n")
string(APPEND header "inline constexpr const char* SourceMethod = \"${BAYOU_BUILD_SOURCE_METHOD}\";\n")
string(APPEND header "inline constexpr const char* GeneratedUtc = \"${BAYOU_BUILD_IDENTITY_GENERATED_UTC}\";\n")
string(APPEND header "inline constexpr const char* SnapshotSchema = \"${snapshot_schema}\";\n")
string(APPEND header "inline constexpr const char* SnapshotAlgorithm = \"SHA-256\";\n")
string(APPEND header "inline constexpr const char* SnapshotScope = \"project-client-inputs-and-all-runtime-assets\";\n")
string(APPEND header "inline constexpr const char* SnapshotSha256 = \"${snapshot_sha256}\";\n")
string(APPEND header "inline constexpr std::uint64_t SnapshotFileCount = ${snapshot_file_count}ull;\n")
string(APPEND header "inline constexpr std::uint64_t SnapshotTotalBytes = ${snapshot_total_bytes}ull;\n")
string(APPEND header "inline constexpr SnapshotGroup ProjectInputs = {\"${project_NAME}\", \"${project_SHA256}\", ${project_FILE_COUNT}ull, ${project_TOTAL_BYTES}ull};\n")
string(APPEND header "inline constexpr SnapshotGroup RuntimeAssets = {\"${assets_NAME}\", \"${assets_SHA256}\", ${assets_FILE_COUNT}ull, ${assets_TOTAL_BYTES}ull};\n")
string(APPEND header "} // namespace bayou::client::build_identity\n")

set(temporary_file "${OUTPUT_FILE}.tmp")
file(WRITE "${temporary_file}" "${header}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${temporary_file}" "${OUTPUT_FILE}"
    COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE "${temporary_file}")
