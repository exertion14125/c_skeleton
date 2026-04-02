# 툴체인 파일: arm-linux-gnueabi.cmake

# Target system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Cross toolchain path
set(CROSS_DIR /opt/toolchain/s5pv210-linaro/gcc/bin)
set(CMAKE_C_COMPILER ${CROSS_DIR}/arm-linux-gnueabi-gcc)

# # FPU flags (Cortex-A8, VFPv3, softfp ABI)
set(FPU_FLAGS "-mfloat-abi=softfp -mfpu=vfpv3")
# set(FPU_FLAGS "")

# Compile/Link flags Set (-Wl for linker flags)
set(CMAKE_C_FLAGS "-Wall -O2 -g ${FPU_FLAGS}" CACHE STRING "C compile flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-L/opt/toolchain/s5pv210-linaro/gcc/arm-linux-gnueabi/libc/usr/lib" CACHE STRING "Executable linker flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/toolchain/s5pv210-linaro/sysroot/usr/lib" CACHE STRING "" FORCE)
 
# Include/Lib paths
include_directories(/opt/toolchain/s5pv210-linaro/sysroot/usr/include)
link_directories(/opt/toolchain/s5pv210-linaro/sysroot/usr/lib)

# [Optional] Default install prefix for cross environment.
# You can override this at configure time with -DCMAKE_INSTALL_PREFIX=/path
if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # Applies only if not explicitly specified by the user
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_INSTALL_PREFIX "/nfs/jgkim12/git/release" CACHE PATH "Install path prefix" FORCE)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_INSTALL_PREFIX "/nfs/jgkim12/git/debug" CACHE PATH "Install path prefix" FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "/nfs/jgkim12/git/other" CACHE PATH "Install path prefix" FORCE)
    endif()
endif()