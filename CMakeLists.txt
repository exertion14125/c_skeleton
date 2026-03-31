cmake_minimum_required(VERSION 3.5)
project(skeleton C)

# ============================================================
# C standard
# ============================================================
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_EXTENSIONS OFF)

# ============================================================
# Default build type (single-config)
# ============================================================
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ============================================================
# Options
# ============================================================
option(BUILD_EXAMPLES                              "Build example targets (example/)" ON)
option(BUILD_TESTS                                 "Build test targets (test/)"       OFF)

option(CTEST_AUTO_ADD                              "Auto register t_* targets into CTest" OFF)

set(CTEST_NAME_MODE "target" CACHE STRING "CTest test name mode: target|path")
set_property(CACHE CTEST_NAME_MODE PROPERTY STRINGS target path)

option(INSTALL_EXAMPLES                            "Install example binaries"         ON)
option(INSTALL_TESTS                               "Install test binaries"            OFF)
option(INSTALL_APPEND_BUILD_NUMBER                 "Append build number on install"   ON)

option(GEN_DEFAULT_LOG_INI                        "Generate default *_log.ini automatically" ON)
set(LOG_DEFAULTS_PROFILE "default" CACHE STRING "Log defaults profile: default|dev|prod")
set_property(CACHE LOG_DEFAULTS_PROFILE PROPERTY STRINGS default dev prod)

set(LOG_INI_INSTALL_LAYOUT "prefix_conf" CACHE STRING "Where to install *_log.ini: prefix_conf|data_conf")
set_property(CACHE LOG_INI_INSTALL_LAYOUT PROPERTY STRINGS prefix_conf data_conf)

# Load log default values from external cmake file(s)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/log_defaults_${LOG_DEFAULTS_PROFILE}.cmake")
        include("${CMAKE_SOURCE_DIR}/cmake/log_defaults_${LOG_DEFAULTS_PROFILE}.cmake")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/cmake/log_defaults.cmake")
        include("${CMAKE_SOURCE_DIR}/cmake/log_defaults.cmake")
else()
        message(STATUS "No log defaults cmake found (cmake/log_defaults*.cmake). Using template defaults only.")
endif()

# ============================================================
# Install layout (target-friendly)
#   - split tree under CMAKE_INSTALL_PREFIX
#     * app/bin/<cfg>
#     * examples/<cfg>
#     * tests/<cfg>/<suite>     <-- (A 방식으로 통일)
#   - generate install-tree CTestTestfile.cmake so tests can run on target
# ============================================================
option(INSTALL_SPLIT_TREE                          "Install into app/examples/tests separated trees" ON)

set(INSTALL_APP_ROOT "app" CACHE STRING "Install root dir for main app tree")
set(INSTALL_EXAMPLES_ROOT "examples" CACHE STRING "Install root dir for examples tree")
set(INSTALL_TESTS_ROOT "tests" CACHE STRING "Install root dir for tests tree")

option(INSTALL_CTEST_FILES                         "Generate and install target-friendly CTestTestfile.cmake" ON)

set(INSTALL_TESTS_RUNNER "run_ctest.sh" CACHE STRING "Installed test runner script name")

option(UNIQUE_TARGET_NAME_BY_PATH                  "Avoid name collision by path-hash suffix" OFF)

option(ENTRY_LINK_SRC_COMMON                       "Link src common sources into ex/t targets" ON)
option(ENTRY_EXCLUDE_SRC_COMMON_FOR_TEST_RA        "Do not link SRC_COMMON for test/ra/* entries" ON)
option(ENTRY_EXCLUDE_SRC_COMMON_FOR_EXAMPLES       "Do not link SRC_COMMON for example/* entries" OFF)

option(ENTRY_SIDECAR_CONFIG                        "Enable per-entry sidecar config (<entry>.cmake)" ON)

option(USE_ARM_TOOLCHAIN                           "Guide to use arm_none_linux_gnuabi.cmake" OFF)

option(INSTALL_STRIP_BINARIES                      "Strip binaries at install time" OFF)
option(OUTPUT_TO_BIN_DIR                           "Put runtime outputs under <build>/bin/(debug|release)" ON)

# ---- 난이도↓: group preset (sidecar에서 파일 나열 최소화) ----
option(ENTRY_GROUPS_ENABLE                         "Enable ENTRY_EXTRA_GROUPS preset mechanism" ON)
option(ENTRY_GROUPS_STRICT                         "Unknown group => FATAL_ERROR" ON)

# RA test suite auto-link (recommended)
option(TEST_RA_AUTO_LINK_COMMON                    "Auto link test/ra/common sources into all RA tests" ON)
option(TEST_RA_AUTO_LINK_PRODUCT_RA                "Auto link src/ra sources into all RA tests" ON)

# ============================================================
# Toolchain guard (guide only)
# ============================================================
if(USE_ARM_TOOLCHAIN AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        message(WARNING
                "USE_ARM_TOOLCHAIN=ON but CMAKE_TOOLCHAIN_FILE not set.\n"
                "Example:\n"
                "  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/arm-linux-gnueabi.cmake"
        )
endif()

# ============================================================
# Include paths & common libs
# ============================================================
set(PRJ_INC_DIRS
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/inc"
)

set(PRJ_COMMON_LIBS pthread rt m ncurses)

# ============================================================
# Build number (git commit count)
# ============================================================
if(NOT DEFINED BUILD_NUMBER)
        if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
                execute_process(
                        COMMAND git rev-list --count HEAD
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        OUTPUT_VARIABLE BUILD_NUMBER_GIT
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET
                )
                if(BUILD_NUMBER_GIT MATCHES "^[0-9]+$")
                        set(BUILD_NUMBER "${BUILD_NUMBER_GIT}")
                else()
                        set(BUILD_NUMBER "0")
                endif()
        else()
                set(BUILD_NUMBER "0")
        endif()
endif()
message(STATUS "Build number = ${BUILD_NUMBER}")

# ============================================================
# Output directory (optional)
# ============================================================
if(OUTPUT_TO_BIN_DIR)
        set(_cfg "debug")
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
                set(_cfg "release")
        endif()
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${_cfg}")
endif()

# ============================================================
# Install helpers
# ============================================================
include(GNUInstallDirs)

function(_get_cfg_subdir out)
        set(_d "debug")
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
                set(_d "release")
        endif()
        set(${out} "${_d}" PARENT_SCOPE)
endfunction()

function(_install_target_binary tgt name enable)
        # Backward compatible wrapper: install into default bindir
        _install_target_binary_ex(${tgt} "${name}" ${enable} "" _out_name _out_dir)
endfunction()

# Install binary into split-tree destination.
#   dest_subdir: relative to CMAKE_INSTALL_PREFIX, e.g. "app/bin" or "tests/unit"
#   out_name/out_dir: returned installed filename and installed directory (relative)
function(_install_target_binary_ex tgt name enable dest_subdir out_name out_dir)
        if(NOT enable)
                set(${out_name} "" PARENT_SCOPE)
                set(${out_dir} "" PARENT_SCOPE)
                return()
        endif()

        _get_cfg_subdir(_cfg)

        set(_out "${name}")
        if(INSTALL_APPEND_BUILD_NUMBER AND NOT "${BUILD_NUMBER}" STREQUAL "0")
                set(_out "${name}_${BUILD_NUMBER}")
        endif()

        # destination root
        if(INSTALL_SPLIT_TREE AND NOT "${dest_subdir}" STREQUAL "")
                set(_base "${dest_subdir}")
        else()
                set(_base "${CMAKE_INSTALL_BINDIR}")
        endif()

        set(_dir "${_base}/${_cfg}")

        install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${_dir}\")")
        install(FILES "$<TARGET_FILE:${tgt}>"
                DESTINATION "${_dir}"
                RENAME "${_out}"
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                            GROUP_EXECUTE GROUP_READ
                            WORLD_EXECUTE WORLD_READ
        )

        if(INSTALL_STRIP_BINARIES)
                install(CODE
                        "execute_process(COMMAND \"${CMAKE_STRIP}\" \"\${CMAKE_INSTALL_PREFIX}/${_dir}/${_out}\")"
                )
        endif()

        set(${out_name} "${_out}" PARENT_SCOPE)
        set(${out_dir} "${_dir}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------
# (A 방식) Install test binary into:
#   <prefix>/tests/<cfg>/<suite>/<name[_bn]>
# suite: ra | engine | integration | other ...
# ------------------------------------------------------------
function(_install_target_binary_tests tgt name enable suite out_name out_dir)
        if(NOT enable)
                set(${out_name} "" PARENT_SCOPE)
                set(${out_dir} "" PARENT_SCOPE)
                return()
        endif()

        _get_cfg_subdir(_cfg)

        set(_out "${name}")
        if(INSTALL_APPEND_BUILD_NUMBER AND NOT "${BUILD_NUMBER}" STREQUAL "0")
                set(_out "${name}_${BUILD_NUMBER}")
        endif()

        set(_dir "${INSTALL_TESTS_ROOT}/${_cfg}/${suite}")

        install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${_dir}\")")
        install(FILES "$<TARGET_FILE:${tgt}>"
                DESTINATION "${_dir}"
                RENAME "${_out}"
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                            GROUP_EXECUTE GROUP_READ
                            WORLD_EXECUTE WORLD_READ
        )

        if(INSTALL_STRIP_BINARIES)
                install(CODE
                        "execute_process(COMMAND \"${CMAKE_STRIP}\" \"\${CMAKE_INSTALL_PREFIX}/${_dir}/${_out}\")"
                )
        endif()

        set(${out_name} "${_out}" PARENT_SCOPE)
        set(${out_dir}  "${_dir}" PARENT_SCOPE)
endfunction()

# ============================================================
# Executable name macros
# ============================================================
function(_apply_exec_name_defines tgt)
        target_compile_definitions(${tgt} PRIVATE
                APP_EXEC_NAME="${tgt}"
                APP_EXEC_FILE="$<TARGET_FILE_NAME:${tgt}>"
        )
endfunction()

# ============================================================
# Unique target name
# ============================================================
function(_make_unique_target_name out prefix stem entry)
        set(_t "${prefix}_${stem}")
        if(UNIQUE_TARGET_NAME_BY_PATH)
                string(MD5 _md5 "${entry}")
                string(SUBSTRING "${_md5}" 0 8 _h)
                set(_t "${_t}_${_h}")
        endif()
        set(${out} "${_t}" PARENT_SCOPE)
endfunction()

# ============================================================
# src common collector (safe)
# ============================================================
# function(_collect_src_common out app_main)
#         file(GLOB SRC_TOP "${CMAKE_SOURCE_DIR}/src/*.c")
#         file(GLOB_RECURSE SRC_SUB "${CMAKE_SOURCE_DIR}/src/*/*.c")
#         set(SRC_ALL ${SRC_TOP} ${SRC_SUB})

#         set(_c "")
#         foreach(f ${SRC_ALL})
#                 if(f STREQUAL "${app_main}")
#                         continue()
#                 endif()
#                 if(f MATCHES "_main\\.c$")
#                         continue()
#                 endif()
#                 list(APPEND _c "${f}")
#         endforeach()

#         set(${out} "${_c}" PARENT_SCOPE)
# endfunction()
function(_collect_src_common out app_main)
        # src 이하 모든 .c를 깊이 제한 없이 수집
        file(GLOB_RECURSE SRC_ALL "${CMAKE_SOURCE_DIR}/src/*.c")

        set(_c "")
        foreach(f ${SRC_ALL})
                if(f STREQUAL "${app_main}")
                        continue()
                endif()
                if(f MATCHES "_main\\.c$")
                        continue()
                endif()
                list(APPEND _c "${f}")
        endforeach()

        set(${out} "${_c}" PARENT_SCOPE)
endfunction()

# ============================================================
# Collect RA product sources (src/ra/**/*.c excluding *_main.c)
# ============================================================
function(_collect_src_ra_product out)
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/ra")
                set(${out} "" PARENT_SCOPE)
                return()
        endif()

        file(GLOB_RECURSE _c "${CMAKE_SOURCE_DIR}/src/ra/*.c")
        list(FILTER _c EXCLUDE REGEX ".*_main\\.c$")
        set(${out} "${_c}" PARENT_SCOPE)
endfunction()

# ============================================================
# Collect RA test common sources (test/ra/common/**/*.c)
#   - excludes *_main.c by convention
# ============================================================
function(_collect_test_ra_common out)
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/test/ra/common")
                set(${out} "" PARENT_SCOPE)
                return()
        endif()

        file(GLOB_RECURSE _c "${CMAKE_SOURCE_DIR}/test/ra/common/*.c")
        list(FILTER _c EXCLUDE REGEX ".*_main\\.c$")
        set(${out} "${_c}" PARENT_SCOPE)
endfunction()

# ============================================================
# Entry rules (compile definitions by path)
# ============================================================
function(_apply_entry_rules tgt stem entry)
        target_link_libraries(${tgt} PRIVATE ${PRJ_COMMON_LIBS})

        if(entry MATCHES "/example/")
                target_compile_definitions(${tgt} PRIVATE APP_EXAMPLE=1)
        endif()

        if(entry MATCHES "/test/")
                target_compile_definitions(${tgt} PRIVATE APP_TEST=1)
        endif()

        if(entry MATCHES "/test/ra/")
                target_compile_definitions(${tgt} PRIVATE RA_MOCK=1)
        endif()

        if(entry MATCHES "/test/eng/")
                target_compile_definitions(${tgt} PRIVATE ENGINE_UT=1)
        endif()
endfunction()

# ============================================================
# CTest: name helpers
# ============================================================
function(_normalize_test_name_path out entry)
        file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${entry}")
        string(REPLACE "\\" "/" _rel "${_rel}")
        string(REGEX REPLACE "_main\\.c$" "" _rel "${_rel}")
        set(${out} "${_rel}" PARENT_SCOPE)
endfunction()

function(_get_ctest_name out tgt entry)
        if(CTEST_NAME_MODE STREQUAL "path")
                _normalize_test_name_path(_nm "${entry}")
                set(${out} "${_nm}" PARENT_SCOPE)
        else()
                set(${out} "${tgt}" PARENT_SCOPE)
        endif()
endfunction()

# ============================================================
# Sidecar loader (+ CTest sidecar fields)
# ============================================================
function(_load_entry_sidecar entry)
        set(ENTRY_EXTRA_SOURCES                      "" PARENT_SCOPE)
        set(ENTRY_EXTRA_INCLUDES                     "" PARENT_SCOPE)
        set(ENTRY_EXTRA_DEFINES                      "" PARENT_SCOPE)
        set(ENTRY_EXTRA_LIBS                         "" PARENT_SCOPE)
        set(ENTRY_EXTRA_CFLAGS                       "" PARENT_SCOPE)
        set(ENTRY_EXTRA_GROUPS                       "" PARENT_SCOPE)

        set(ENTRY_TEST_ARGS                          "" PARENT_SCOPE)
        set(ENTRY_TEST_ENV                           "" PARENT_SCOPE)
        set(ENTRY_TEST_LABELS                        "" PARENT_SCOPE)
        set(ENTRY_TEST_TIMEOUT                       "" PARENT_SCOPE)
        set(ENTRY_TEST_WORKING_DIRECTORY             "" PARENT_SCOPE)

        set(ENTRY_TEST_RESOURCE_LOCK                 "" PARENT_SCOPE)
        set(ENTRY_TEST_FIXTURES_SETUP                "" PARENT_SCOPE)
        set(ENTRY_TEST_FIXTURES_CLEANUP              "" PARENT_SCOPE)
        set(ENTRY_TEST_FIXTURES_REQUIRED             "" PARENT_SCOPE)

        set(ENTRY_TEST_DEPENDS                       "" PARENT_SCOPE)
        set(ENTRY_TEST_DEPENDS_TARGET                "" PARENT_SCOPE)
        set(ENTRY_TEST_COST                          "" PARENT_SCOPE)
        set(ENTRY_TEST_PASS_REGEX                    "" PARENT_SCOPE)
        set(ENTRY_TEST_FAIL_REGEX                    "" PARENT_SCOPE)

        if(NOT ENTRY_SIDECAR_CONFIG)
                return()
        endif()

        set(_sc "${entry}.cmake")
        if(EXISTS "${_sc}")
                include("${_sc}")

                set(ENTRY_EXTRA_SOURCES                      "${ENTRY_EXTRA_SOURCES}"                      PARENT_SCOPE)
                set(ENTRY_EXTRA_INCLUDES                     "${ENTRY_EXTRA_INCLUDES}"                     PARENT_SCOPE)
                set(ENTRY_EXTRA_DEFINES                      "${ENTRY_EXTRA_DEFINES}"                      PARENT_SCOPE)
                set(ENTRY_EXTRA_LIBS                         "${ENTRY_EXTRA_LIBS}"                         PARENT_SCOPE)
                set(ENTRY_EXTRA_CFLAGS                       "${ENTRY_EXTRA_CFLAGS}"                       PARENT_SCOPE)
                set(ENTRY_EXTRA_GROUPS                       "${ENTRY_EXTRA_GROUPS}"                       PARENT_SCOPE)

                set(ENTRY_TEST_ARGS                          "${ENTRY_TEST_ARGS}"                          PARENT_SCOPE)
                set(ENTRY_TEST_ENV                           "${ENTRY_TEST_ENV}"                           PARENT_SCOPE)
                set(ENTRY_TEST_LABELS                        "${ENTRY_TEST_LABELS}"                        PARENT_SCOPE)
                set(ENTRY_TEST_TIMEOUT                       "${ENTRY_TEST_TIMEOUT}"                       PARENT_SCOPE)
                set(ENTRY_TEST_WORKING_DIRECTORY             "${ENTRY_TEST_WORKING_DIRECTORY}"             PARENT_SCOPE)

                set(ENTRY_TEST_RESOURCE_LOCK                 "${ENTRY_TEST_RESOURCE_LOCK}"                 PARENT_SCOPE)
                set(ENTRY_TEST_FIXTURES_SETUP                "${ENTRY_TEST_FIXTURES_SETUP}"                PARENT_SCOPE)
                set(ENTRY_TEST_FIXTURES_CLEANUP              "${ENTRY_TEST_FIXTURES_CLEANUP}"              PARENT_SCOPE)
                set(ENTRY_TEST_FIXTURES_REQUIRED             "${ENTRY_TEST_FIXTURES_REQUIRED}"             PARENT_SCOPE)

                set(ENTRY_TEST_DEPENDS                       "${ENTRY_TEST_DEPENDS}"                       PARENT_SCOPE)
                set(ENTRY_TEST_DEPENDS_TARGET                "${ENTRY_TEST_DEPENDS_TARGET}"                PARENT_SCOPE)
                set(ENTRY_TEST_COST                          "${ENTRY_TEST_COST}"                          PARENT_SCOPE)
                set(ENTRY_TEST_PASS_REGEX                    "${ENTRY_TEST_PASS_REGEX}"                    PARENT_SCOPE)
                set(ENTRY_TEST_FAIL_REGEX                    "${ENTRY_TEST_FAIL_REGEX}"                    PARENT_SCOPE)
        endif()
endfunction()

# ============================================================
# Preset groups (ENTRY_EXTRA_GROUPS)
#   - 기본 제공 그룹: util_base, engine_min
#   - 실제 포함 파일은 디렉토리 기반으로 안전하게 수집(glob)
#   - RA 테스트에서 resource 구현이 섞이지 않도록 기본 필터 제공
# ============================================================
function(_grp_collect_util_base out)
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/util")
                set(${out} "" PARENT_SCOPE)
                return()
        endif()

        file(GLOB_RECURSE _c "${CMAKE_SOURCE_DIR}/src/util/*.c")
        list(FILTER _c EXCLUDE REGEX ".*_main\\.c$")
        set(${out} "${_c}" PARENT_SCOPE)
endfunction()

function(_grp_collect_engine_min out)
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/eng")
                set(${out} "" PARENT_SCOPE)
                return()
        endif()

        file(GLOB_RECURSE _c "${CMAKE_SOURCE_DIR}/src/eng/*.c")
        list(FILTER _c EXCLUDE REGEX ".*_main\\.c$")
        set(${out} "${_c}" PARENT_SCOPE)
endfunction()

function(_entry_groups_apply)
        if(NOT ENTRY_GROUPS_ENABLE)
                return()
        endif()

        if(NOT DEFINED ENTRY_EXTRA_GROUPS OR "${ENTRY_EXTRA_GROUPS}" STREQUAL "")
                return()
        endif()

        set(_g "${ENTRY_EXTRA_GROUPS}")
        string(REPLACE "," ";" _g "${_g}")

        foreach(grp ${_g})
                string(STRIP "${grp}" grp)
                if("${grp}" STREQUAL "")
                        continue()
                endif()

                if(grp STREQUAL "util_base")
                        _grp_collect_util_base(_s)
                        if(_s)
                                list(APPEND ENTRY_EXTRA_SOURCES ${_s})
                        endif()
                elseif(grp STREQUAL "engine_min")
                        _grp_collect_engine_min(_s)
                        if(_s)
                                list(APPEND ENTRY_EXTRA_SOURCES ${_s})
                        endif()
                else()
                        if(ENTRY_GROUPS_STRICT)
                                message(FATAL_ERROR "Unknown ENTRY_EXTRA_GROUPS item: ${grp}")
                        else()
                                message(WARNING "Unknown ENTRY_EXTRA_GROUPS item ignored: ${grp}")
                        endif()
                endif()
        endforeach()

        set(ENTRY_EXTRA_SOURCES          "${ENTRY_EXTRA_SOURCES}"          PARENT_SCOPE)
        set(ENTRY_EXTRA_INCLUDES         "${ENTRY_EXTRA_INCLUDES}"         PARENT_SCOPE)
        set(ENTRY_EXTRA_DEFINES          "${ENTRY_EXTRA_DEFINES}"          PARENT_SCOPE)
        set(ENTRY_EXTRA_LIBS             "${ENTRY_EXTRA_LIBS}"             PARENT_SCOPE)
        set(ENTRY_EXTRA_CFLAGS           "${ENTRY_EXTRA_CFLAGS}"           PARENT_SCOPE)
endfunction()

# ============================================================
# Optional: Generate default *_log.ini per executable target
#   - controlled by GEN_DEFAULT_LOG_INI option
#   - uses cmake/log.ini.in template
#   - install location controlled by LOG_INI_INSTALL_LAYOUT
# ============================================================
function(gen_log_ini_for_target tgt)
        if(NOT GEN_DEFAULT_LOG_INI)
                return()
        endif()

        # Decide install destination
        set(_ini_dest "conf/log")
        if(LOG_INI_INSTALL_LAYOUT STREQUAL "data_conf")
                # -> <prefix>/data/conf/log  (prefix=/이면 /data/conf/log)
                set(_ini_dest "data/conf/log")
        endif()

        # Resolve output name (handles OUTPUT_NAME)
        get_target_property(_out ${tgt} OUTPUT_NAME)
        if(NOT _out OR _out STREQUAL "NOTFOUND")
                set(_out "${tgt}")
        endif()

        set(APP "${_out}")
        set(_ini "${CMAKE_CURRENT_BINARY_DIR}/${_out}_log.ini")

        # Expand @APP@ token inside path templates if used
        set(_file_path "${LOG_FILE_PATH}")
        set(_uds_path  "${LOG_UI_UDS_PATH}")

        if(_file_path)
                string(REPLACE "@APP@" "${APP}" _file_path "${_file_path}")
        endif()
        if(_uds_path)
                string(REPLACE "@APP@" "${APP}" _uds_path "${_uds_path}")
        endif()

        # Provide expanded values to configure_file()
        set(LOG_FILE_PATH "${_file_path}")
        set(LOG_UI_UDS_PATH "${_uds_path}")

        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/log.ini.in")
                message(FATAL_ERROR "GEN_DEFAULT_LOG_INI=ON but template not found: ${CMAKE_SOURCE_DIR}/cmake/log.ini.in")
        endif()

        configure_file("${CMAKE_SOURCE_DIR}/cmake/log.ini.in" "${_ini}" @ONLY)

        # Make generation visible in default build
        add_custom_target(${tgt}_log_ini ALL DEPENDS "${_ini}")

        install(FILES "${_ini}" DESTINATION "${_ini_dest}")
endfunction()


# ============================================================
# CTest: label inference
# ============================================================
function(_infer_test_labels out entry stem)
        set(_labels "")

        if(entry MATCHES "/test/ra/")
                list(APPEND _labels "ra")
        endif()

        if(entry MATCHES "/test/mgr/")
                list(APPEND _labels "mgr")
        endif()

        if(entry MATCHES "/test/eng/")
                list(APPEND _labels "eng")
        endif()

        if(entry MATCHES "/test/integration/" OR stem MATCHES "^it_")
                list(APPEND _labels "integration")
        endif()

        if(entry MATCHES "/test/util/")
                list(APPEND _labels "util")
        endif()

        list(APPEND _labels "all")

        string(REPLACE ";" "," _label_str "${_labels}")
        set(${out} "${_label_str}" PARENT_SCOPE)
endfunction()

# ============================================================
# Pending depends(target) resolver
#   - store: "<testname>|<dep_tgt_csv>"
# ============================================================
function(_pending_depends_target_push testname dep_tgt_str)
        get_property(_lst GLOBAL PROPERTY CTEST_PENDING_DEPENDS_TARGET)
        if(NOT _lst)
                set(_lst "")
        endif()

        set(_s "${dep_tgt_str}")
        string(REPLACE ";" "," _s "${_s}")
        set(_item "${testname}|${_s}")

        if(_lst STREQUAL "")
                set(_lst "${_item}")
        else()
                list(APPEND _lst "${_item}")
        endif()

        set_property(GLOBAL PROPERTY CTEST_PENDING_DEPENDS_TARGET "${_lst}")
endfunction()

function(_resolve_pending_depends_target)
        get_property(_lst GLOBAL PROPERTY CTEST_PENDING_DEPENDS_TARGET)
        if(NOT _lst)
                return()
        endif()

        foreach(_item ${_lst})
                string(REPLACE "|" ";" _parts "${_item}")
                list(LENGTH _parts _n)
                if(NOT _n EQUAL 2)
                        continue()
                endif()

                list(GET _parts 0 _testname)
                list(GET _parts 1 _dep_csv)

                string(REPLACE "," ";" _dep_tgts "${_dep_csv}")

                set(_dep_names "")
                foreach(_dt ${_dep_tgts})
                        if(_dt STREQUAL "")
                                continue()
                        endif()

                        get_property(_mapped GLOBAL PROPERTY "CTEST_NAME_OF_${_dt}")
                        if(_mapped)
                                list(APPEND _dep_names "${_mapped}")
                        else()
                                list(APPEND _dep_names "${_dt}")
                        endif()
                endforeach()

                get_property(_old TEST "${_testname}" PROPERTY DEPENDS)
                if(_old)
                        set(_merged "${_old};${_dep_names}")
                else()
                        set(_merged "${_dep_names}")
                endif()

                set_tests_properties("${_testname}" PROPERTIES DEPENDS "${_merged}")
        endforeach()

        set_property(GLOBAL PROPERTY CTEST_PENDING_DEPENDS_TARGET "")
endfunction()

# ============================================================
# CTest: register test
# ============================================================
function(_register_ctest_if_needed tgt entry stem)
        if(NOT BUILD_TESTS)
                return()
        endif()

        if(NOT CTEST_AUTO_ADD)
                return()
        endif()

        if(NOT tgt MATCHES "^t_")
                return()
        endif()

        _get_ctest_name(_tname "${tgt}" "${entry}")

        # mapping: target -> testname (for depends_target conversion)
        set_property(GLOBAL PROPERTY "CTEST_NAME_OF_${tgt}" "${_tname}")

        if(ENTRY_TEST_ARGS)
                separate_arguments(_targs UNIX_COMMAND "${ENTRY_TEST_ARGS}")
                add_test(NAME "${_tname}" COMMAND ${tgt} ${_targs})
        else()
                add_test(NAME "${_tname}" COMMAND ${tgt})
        endif()

        if(ENTRY_TEST_ENV)
                set_tests_properties("${_tname}" PROPERTIES ENVIRONMENT "${ENTRY_TEST_ENV}")
        endif()

        if(ENTRY_TEST_LABELS)
                set(_lbl "${ENTRY_TEST_LABELS}")
                string(REPLACE "," ";" _lbl "${_lbl}")
                set_tests_properties("${_tname}" PROPERTIES LABELS "${_lbl}")
        else()
                _infer_test_labels(_auto "${entry}" "${stem}")
                string(REPLACE "," ";" _auto_sc "${_auto}")
                set_tests_properties("${_tname}" PROPERTIES LABELS "${_auto_sc}")
        endif()

        if(ENTRY_TEST_TIMEOUT)
                set_tests_properties("${_tname}" PROPERTIES TIMEOUT "${ENTRY_TEST_TIMEOUT}")
        endif()

        if(ENTRY_TEST_WORKING_DIRECTORY)
                set_tests_properties("${_tname}" PROPERTIES WORKING_DIRECTORY "${ENTRY_TEST_WORKING_DIRECTORY}")
        endif()

        if(ENTRY_TEST_RESOURCE_LOCK)
                set_tests_properties("${_tname}" PROPERTIES RESOURCE_LOCK "${ENTRY_TEST_RESOURCE_LOCK}")
        endif()

        if(ENTRY_TEST_FIXTURES_SETUP)
                set_tests_properties("${_tname}" PROPERTIES FIXTURES_SETUP "${ENTRY_TEST_FIXTURES_SETUP}")
        endif()

        if(ENTRY_TEST_FIXTURES_CLEANUP)
                set_tests_properties("${_tname}" PROPERTIES FIXTURES_CLEANUP "${ENTRY_TEST_FIXTURES_CLEANUP}")
        endif()

        if(ENTRY_TEST_FIXTURES_REQUIRED)
                set_tests_properties("${_tname}" PROPERTIES FIXTURES_REQUIRED "${ENTRY_TEST_FIXTURES_REQUIRED}")
        endif()

        # DEPENDS (direct test-name depends; user must write according to current name mode)
        if(ENTRY_TEST_DEPENDS)
                set(_dep "${ENTRY_TEST_DEPENDS}")
                string(REPLACE "," ";" _dep "${_dep}")
                set_tests_properties("${_tname}" PROPERTIES DEPENDS "${_dep}")
        endif()

        # DEPENDS_TARGET (always target-name depends; auto-convert at finalize)
        if(ENTRY_TEST_DEPENDS_TARGET)
                _pending_depends_target_push("${_tname}" "${ENTRY_TEST_DEPENDS_TARGET}")
        endif()

        if(ENTRY_TEST_COST)
                set_tests_properties("${_tname}" PROPERTIES COST "${ENTRY_TEST_COST}")
        endif()

        if(ENTRY_TEST_PASS_REGEX)
                set_tests_properties("${_tname}" PROPERTIES PASS_REGULAR_EXPRESSION "${ENTRY_TEST_PASS_REGEX}")
        endif()

        if(ENTRY_TEST_FAIL_REGEX)
                set_tests_properties("${_tname}" PROPERTIES FAIL_REGULAR_EXPRESSION "${ENTRY_TEST_FAIL_REGEX}")
        endif()
endfunction()

# ============================================================
# Install-tree CTest generator (target-friendly)
#   - Generate CTestTestfile.cmake for *installed* test binaries
#   - Layout (A 방식):
#       <prefix>/tests/<cfg>/
#         CTestTestfile.cmake
#         ra/CTestTestfile.cmake
#         engine/CTestTestfile.cmake
#         integration/CTestTestfile.cmake
#         other/CTestTestfile.cmake
#       <prefix>/tests/<cfg>/<suite>/t_...
# ============================================================

function(_infer_install_test_suite out entry)
        if(entry MATCHES "/test/ra/")
                set(${out} "ra" PARENT_SCOPE)
        elseif(entry MATCHES "/test/eng/")
                set(${out} "engine" PARENT_SCOPE)
        elseif(entry MATCHES "/test/integration/")
                set(${out} "integration" PARENT_SCOPE)
        elseif(entry MATCHES "/test/util/")
                set(${out} "util" PARENT_SCOPE)
        elseif(entry MATCHES "/test/mgr/")
                set(${out} "mgr" PARENT_SCOPE)
        else()
                set(${out} "other" PARENT_SCOPE)
        endif()
endfunction()

# Record: "<suite>|<testname>|<exe>|<args>|<env>|<timeout>|<labels>"
function(_install_ctest_record suite testname exe_rel args env timeout labels)
        get_property(_lst GLOBAL PROPERTY "INSTALL_CTEST_ITEMS_${suite}")
        if(NOT _lst)
                set(_lst "")
        endif()

        # IMPORTANT: global PROPERTY values are CMake "lists" (semicolon-separated).
        # Escape any ';' so one record remains one list item.
        set(_args "${args}")
        set(_env "${env}")
        set(_timeout "${timeout}")
        set(_labels "${labels}")

        string(REPLACE ";" "\\;" _args "${_args}")
        string(REPLACE ";" "\\;" _env "${_env}")
        string(REPLACE ";" "\\;" _timeout "${_timeout}")
        string(REPLACE ";" "\\;" _labels "${_labels}")

        # Avoid '|' inside fields
        string(REPLACE "|" "/" _args "${_args}")
        string(REPLACE "|" "/" _env "${_env}")
        string(REPLACE "|" "/" _timeout "${_timeout}")
        string(REPLACE "|" "/" _labels "${_labels}")

        set(_item "${suite}|${testname}|${exe_rel}|${_args}|${_env}|${_timeout}|${_labels}")
        if(_lst STREQUAL "")
                set(_lst "${_item}")
        else()
                list(APPEND _lst "${_item}")
        endif()
        set_property(GLOBAL PROPERTY "INSTALL_CTEST_ITEMS_${suite}" "${_lst}")
endfunction()

function(_write_install_ctest_files)
        if(NOT BUILD_TESTS OR NOT INSTALL_TESTS)
                return()
        endif()
        if(NOT INSTALL_CTEST_FILES)
                return()
        endif()
        if(NOT INSTALL_SPLIT_TREE)
                return()
        endif()

        _get_cfg_subdir(_cfg)
        set(_dst_root "${CMAKE_BINARY_DIR}/_install_ctest/${_cfg}")
        file(MAKE_DIRECTORY "${_dst_root}")

        # root file: include subdirs that exist
        set(_root_file "${_dst_root}/CTestTestfile.cmake")
        file(WRITE "${_root_file}" "# Auto-generated install-tree CTestTestfile.cmake\n")

        set(_suites "ra;eng;mgr;integration;util;other")
        foreach(suite ${_suites})
                get_property(_lst GLOBAL PROPERTY "INSTALL_CTEST_ITEMS_${suite}")
                if(NOT _lst)
                        continue()
                endif()

                file(MAKE_DIRECTORY "${_dst_root}/${suite}")
                file(APPEND "${_root_file}" "subdirs(\"${suite}\")\n")

                set(_sf "${_dst_root}/${suite}/CTestTestfile.cmake")
                file(WRITE "${_sf}" "# Auto-generated suite: ${suite}\n")

                foreach(_item ${_lst})
                        string(REPLACE "|" ";" _p "${_item}")
                        list(LENGTH _p _n)
                        if(_n LESS 3)
                                continue()
                        endif()
                        list(GET _p 1 _tname)
                        list(GET _p 2 _exe)
                        list(GET _p 3 _args)
                        list(GET _p 4 _env)
                        list(GET _p 5 _timeout)
                        list(GET _p 6 _labels)

                        # Make names unique across suites
                        set(_full_name "${suite}.${_tname}")

                        if(NOT "${_args}" STREQUAL "")
                                separate_arguments(_argv UNIX_COMMAND "${_args}")

                                # quote each arg safely
                                set(_argv_q "")
                                foreach(a IN LISTS _argv)
                                        string(REPLACE "\\" "\\\\" a_esc "${a}")
                                        string(REPLACE "\"" "\\\"" a_esc "${a_esc}")
                                        set(_argv_q "${_argv_q} \"${a_esc}\"")
                                endforeach()

                                file(APPEND "${_sf}" "add_test(${_full_name} \"${_exe}\"${_argv_q})\n")
                        else()
                                file(APPEND "${_sf}" "add_test(${_full_name} \"${_exe}\")\n")
                        endif()

                        if(NOT "${_env}" STREQUAL "")
                                file(APPEND "${_sf}" "set_tests_properties(${_full_name} PROPERTIES ENVIRONMENT \"${_env}\")\n")
                        endif()
                        if(NOT "${_timeout}" STREQUAL "")
                                file(APPEND "${_sf}" "set_tests_properties(${_full_name} PROPERTIES TIMEOUT ${_timeout})\n")
                        endif()
                        if(NOT "${_labels}" STREQUAL "")
                                # CTest expects ';' separated labels
                                string(REPLACE "," ";" _lbl_sc "${_labels}")
                                file(APPEND "${_sf}" "set_tests_properties(${_full_name} PROPERTIES LABELS \"${_lbl_sc}\")\n")
                        endif()
                endforeach()
        endforeach()

        # Install generated CTest files into: <prefix>/tests/<cfg>/**
        install(DIRECTORY "${_dst_root}/"
                DESTINATION "${INSTALL_TESTS_ROOT}/${_cfg}"
                FILES_MATCHING PATTERN "CTestTestfile.cmake")

        # Optional runner script (recommended for NFS/target)
        set(_runner_in "${CMAKE_SOURCE_DIR}/cmake/run_ctest.sh.in")
        if(EXISTS "${_runner_in}")
                set(_runner_out "${CMAKE_BINARY_DIR}/_install_ctest/${_cfg}/${INSTALL_TESTS_RUNNER}")
                configure_file("${_runner_in}" "${_runner_out}" @ONLY)
                install(PROGRAMS "${_runner_out}"
                        DESTINATION "${INSTALL_TESTS_ROOT}/${_cfg}")
        endif()
endfunction()

# ============================================================
# Main application
# ============================================================
set(PRJ_APP_MAIN "${CMAKE_SOURCE_DIR}/skeleton_main.c")
if(NOT EXISTS "${PRJ_APP_MAIN}")
        message(FATAL_ERROR "Main entry not found: ${PRJ_APP_MAIN}")
endif()

_collect_src_common(SRC_COMMON "${PRJ_APP_MAIN}")

add_executable(${PROJECT_NAME}
        ${PRJ_APP_MAIN}
        ${SRC_COMMON}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PRJ_INC_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PRJ_COMMON_LIBS})
_apply_exec_name_defines(${PROJECT_NAME})
gen_log_ini_for_target(${PROJECT_NAME})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

# main app install
if(INSTALL_SPLIT_TREE)
        _install_target_binary_ex(${PROJECT_NAME} "${PROJECT_NAME}" ON "${INSTALL_APP_ROOT}/bin" _a_out _a_dir)
else()
        _install_target_binary(${PROJECT_NAME} "${PROJECT_NAME}" ON)
endif()

# ============================================================
# examples / tests: entry target generator
# ============================================================
function(add_targets_from_entry_rule_recursive root prefix do_install)
        if(NOT IS_DIRECTORY "${root}")
                return()
        endif()

        file(GLOB_RECURSE ENTRY_FILES "${root}/*_main.c")
        foreach(entry ${ENTRY_FILES})
                get_filename_component(dir "${entry}" DIRECTORY)
                get_filename_component(base "${entry}" NAME_WE)
                string(REGEX REPLACE "_main$" "" stem "${base}")

                _make_unique_target_name(tgt "${prefix}" "${stem}" "${entry}")

                file(GLOB LOCAL_ALL "${dir}/*.c")
                set(local "")
                foreach(f ${LOCAL_ALL})
                        if(NOT f STREQUAL "${entry}" AND NOT f MATCHES "_main\\.c$")
                                list(APPEND local "${f}")
                        endif()
                endforeach()

                _load_entry_sidecar("${entry}")

                # ---- apply preset groups (reduce sidecar verbosity) ----
                _entry_groups_apply()

                set(srcs ${entry} ${local})

                # ----------------------------------------------------
                # RA tests: auto link (1) RA product sources and (2) RA test common
                #  - keep ENTRY_EXCLUDE_SRC_COMMON_FOR_TEST_RA policy (unit-test purity)
                # ----------------------------------------------------
                if(entry MATCHES "/test/ra/")
                        if(TEST_RA_AUTO_LINK_PRODUCT_RA AND SRC_RA_PRODUCT)
                                list(APPEND srcs ${SRC_RA_PRODUCT})
                        endif()
                        if(TEST_RA_AUTO_LINK_COMMON AND TEST_RA_COMMON_SRCS)
                                list(APPEND srcs ${TEST_RA_COMMON_SRCS})
                        endif()
                endif()

                if(ENTRY_LINK_SRC_COMMON)
                        if(NOT (ENTRY_EXCLUDE_SRC_COMMON_FOR_EXAMPLES AND entry MATCHES "/example/") AND
                           NOT (ENTRY_EXCLUDE_SRC_COMMON_FOR_TEST_RA AND entry MATCHES "/test/ra/"))
                                list(APPEND srcs ${SRC_COMMON})
                        endif()
                endif()

                if(ENTRY_EXTRA_SOURCES)
                        list(APPEND srcs ${ENTRY_EXTRA_SOURCES})
                endif()

                add_executable(${tgt} ${srcs})
                target_include_directories(${tgt} PRIVATE ${PRJ_INC_DIRS} ${dir})
                target_compile_options(${tgt} PRIVATE -Wall -Wextra)
                _apply_exec_name_defines(${tgt})
                _apply_entry_rules(${tgt} "${stem}" "${entry}")
                # gen_log_ini_for_target(${tgt})
                if(ENTRY_EXTRA_INCLUDES)
                        target_include_directories(${tgt} PRIVATE ${ENTRY_EXTRA_INCLUDES})
                endif()
                if(ENTRY_EXTRA_DEFINES)
                        target_compile_definitions(${tgt} PRIVATE ${ENTRY_EXTRA_DEFINES})
                endif()
                if(ENTRY_EXTRA_CFLAGS)
                        target_compile_options(${tgt} PRIVATE ${ENTRY_EXTRA_CFLAGS})
                endif()
                if(ENTRY_EXTRA_LIBS)
                        target_link_libraries(${tgt} PRIVATE ${ENTRY_EXTRA_LIBS})
                endif()

                _register_ctest_if_needed(${tgt} "${entry}" "${stem}")

                # ----------------------------------------------------
                # (A 방식) split install destination
                #   examples: <prefix>/examples/<cfg>/*
                #   tests   : <prefix>/tests/<cfg>/<suite>/*
                # ----------------------------------------------------
                set(_suite "")
                if(INSTALL_SPLIT_TREE)
                        if(prefix STREQUAL "ex")
                                _install_target_binary_ex(${tgt} "${tgt}" ${do_install} "${INSTALL_EXAMPLES_ROOT}" _inst_name _inst_dir)

                        elseif(prefix STREQUAL "t")
                                _infer_install_test_suite(_suite "${entry}")
                                _install_target_binary_tests(${tgt} "${tgt}" ${do_install} "${_suite}" _inst_name _inst_dir)

                        else()
                                _install_target_binary_ex(${tgt} "${tgt}" ${do_install} "" _inst_name _inst_dir)
                        endif()
                else()
                        _install_target_binary_ex(${tgt} "${tgt}" ${do_install} "" _inst_name _inst_dir)
                endif()

                # ---- install-tree CTest records (for target) ----
                if(prefix STREQUAL "t" AND do_install)
                        if(INSTALL_SPLIT_TREE AND INSTALL_CTEST_FILES)
                                _get_ctest_name(_ct_name "${tgt}" "${entry}")

                                if(ENTRY_TEST_LABELS)
                                        set(_ct_labels "${ENTRY_TEST_LABELS}")
                                else()
                                        _infer_test_labels(_ct_labels "${entry}" "${stem}")
                                endif()

                                # CTestTestfile.cmake는 <prefix>/tests/<cfg>/<suite>/ 에 설치되고,
                                # 바이너리도 같은 디렉토리에 설치되므로 "./<exe>"로 실행 가능.
                                _install_ctest_record(
                                        "${_suite}"
                                        "${_ct_name}"
                                        "./${_inst_name}"
                                        "${ENTRY_TEST_ARGS}"
                                        "${ENTRY_TEST_ENV}"
                                        "${ENTRY_TEST_TIMEOUT}"
                                        "${_ct_labels}"
                                )
                        endif()
                endif()
        endforeach()
endfunction()

# ============================================================
# examples / tests switch
# ============================================================
if(BUILD_EXAMPLES)
        add_targets_from_entry_rule_recursive("${CMAKE_SOURCE_DIR}/example" "ex" ${INSTALL_EXAMPLES})
endif()

if(BUILD_TESTS)
        enable_testing()

        # Pre-collect RA suite lists once
        if(TEST_RA_AUTO_LINK_PRODUCT_RA)
                _collect_src_ra_product(SRC_RA_PRODUCT)
        else()
                set(SRC_RA_PRODUCT "")
        endif()

        if(TEST_RA_AUTO_LINK_COMMON)
                _collect_test_ra_common(TEST_RA_COMMON_SRCS)
        else()
                set(TEST_RA_COMMON_SRCS "")
        endif()


        add_targets_from_entry_rule_recursive("${CMAKE_SOURCE_DIR}/test" "t" ${INSTALL_TESTS})
        _resolve_pending_depends_target()
        _write_install_ctest_files()
endif()

# ============================================================
# Summary
# ============================================================
# message(STATUS "============================================================")
# message(STATUS "Project               : ${PROJECT_NAME}")
# message(STATUS "Build type            : ${CMAKE_BUILD_TYPE}")
# message(STATUS "Install prefix        : ${CMAKE_INSTALL_PREFIX}")
# message(STATUS "Build examples        : ${BUILD_EXAMPLES}")
# message(STATUS "Build tests           : ${BUILD_TESTS}")
# message(STATUS "CTEST_AUTO_ADD        : ${CTEST_AUTO_ADD}")
# message(STATUS "CTEST_NAME_MODE       : ${CTEST_NAME_MODE}")
# message(STATUS "INSTALL_SPLIT_TREE    : ${INSTALL_SPLIT_TREE}")
# message(STATUS "INSTALL_CTEST_FILES   : ${INSTALL_CTEST_FILES}")
# message(STATUS "GEN_DEFAULT_LOG_INI   : ${GEN_DEFAULT_LOG_INI}")
# message(STATUS "LOG_DEFAULTS_PROFILE  : ${LOG_DEFAULTS_PROFILE}")
# message(STATUS "LOG_INI_INSTALL_LAYOUT: ${LOG_INI_INSTALL_LAYOUT}")
# message(STATUS "Build number          : ${PRJ_BUILD_NUMBER}")
# message(STATUS "============================================================")

# ============================================================
# distclean
# ============================================================
add_custom_target(distclean
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
)

