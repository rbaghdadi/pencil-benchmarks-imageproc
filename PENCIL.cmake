set(PENCIL_COMPILER     "ppcg"                 CACHE STRING "PENCIL compiler")
set(PENCIL_INCLUDE_DIRS "/usr/local/include"   CACHE PATH   "PENCIL library headers directory")
set(PENCIL_LIBRARIES    "libocl_pencil_opt.so" CACHE FILE   "PENCIL runtime library")

#device-specific settings
set(PENCIL_FLAGS_1D_BLOCKSIZE      "64"  CACHE STRING "Default block sizes for 1D kernels")
set(PENCIL_FLAGS_2D_BLOCKSIZE      "8,8" CACHE STRING "Default block sizes for 2D kernels")
set(PENCIL_FLAGS_LOCAL_MEMORY_SIZE "0"   CACHE STRING "Amount of __local memory to use. Set to 0 to disable __local memory usage (e.g. ARM Mali)")

#other important PPCG switches
set(PENCIL_FLAGS_MAXFUSE           true  CACHE BOOL "--isl-schedule-fuse=min|max")
set(PENCIL_FLAGS_NO_SEPARATE_COMP  true  CACHE BOOL "--no-isl-schedule-separate-components")
set(PENCIL_FLAGS_DISABLE_PRIVATE   false CACHE BOOL "Disable usage of private memory")

#PENCIL compilation flags. The overall number of threads cannot be more than the OpenCL device allows (ARM Mali: 64-256 (depends on kernel), AMD: 256, Intel HD: 512, nVidia Fermi: 4096
if(${PENCIL_FLAGS_MAXFUSE})
    set(PENCIL_FLAGS_DEFAULT "--isl-schedule-fuse=max")
else()
    set(PENCIL_FLAGS_DEFAULT "--isl-schedule-fuse=min")
endif()
if(${PENCIL_FLAGS_NO_SEPARATE_COMP})
    set(PENCIL_FLAGS_DEFAULT "${PENCIL_FLAGS_DEFAULT};--no-isl-schedule-separate-components")
endif()
if(${PENCIL_FLAGS_DISABLE_PRIVATE})
    set(PENCIL_FLAGS_DEFAULT "${PENCIL_FLAGS_DEFAULT};--no-private-memory")
endif()
if(${PENCIL_FLAGS_LOCAL_MEMORY_SIZE} GREATER 0)
    set(PENCIL_FLAGS_DEFAULT "${PENCIL_FLAGS_DEFAULT};--max-shared-memory=${PENCIL_FLAGS_LOCAL_MEMORY_SIZE}")
else()
    set(PENCIL_FLAGS_DEFAULT "${PENCIL_FLAGS_DEFAULT};--no-shared-memory")
endif()
set(PENCIL_FLAGS_DEFAULT_1D "${PENCIL_FLAGS_DEFAULT};--sizes=\"{kernel[i]->block[${PENCIL_FLAGS_1D_BLOCKSIZE}]}\"")
set(PENCIL_FLAGS_DEFAULT_2D "${PENCIL_FLAGS_DEFAULT};--sizes=\"{kernel[i]->block[${PENCIL_FLAGS_2D_BLOCKSIZE}]}\"")
set(PENCIL_REQUIRED_FLAGS "-D__PENCIL__;--target=opencl;--opencl-include-file=${PENCIL_INCLUDE_DIRS}/pencil_opencl.h;--isl-ast-always-print-block" CACHE STRING "Required PENCIL compilation flags")
mark_as_advanced(PENCIL_FLAGS_DEFAULT, PENCIL_FLAGS_DEFAULT_1D, PENCIL_FLAGS_DEFAULT_2D, PENCIL_REQUIRED_FLAGS)

function(pencil_wrap)
  cmake_parse_arguments(COMPILE_PENCIL "" "DEST" "DIM;FLAGS;FILES" ${ARGN})
  set(COMPILE_PENCIL_DEST_INCLUDE_DIRS "${COMPILE_PENCIL_DEST}_GEN_INCLUDE_DIRS")
  set(COMPILE_PENCIL_DEST_SOURCES "${COMPILE_PENCIL_DEST}_GEN_SOURCES")
  if("${COMPILE_PENCIL_FLAGS}" STREQUAL "")
    message(STATUS "Flags not set for ${COMPILE_PENCIL_DEST}, using default PENCIL flags")
    if("${COMPILE_PENCIL_DIM}" STREQUAL "1")
      set(COMPILE_PENCIL_FLAGS ${PENCIL_FLAGS_DEFAULT_1D})
    elseif("${COMPILE_PENCIL_DIM}" STREQUAL "2")
      set(COMPILE_PENCIL_FLAGS ${PENCIL_FLAGS_DEFAULT_2D})
    else()
      message(ERROR "Neither FLAGS nor DIM is not set for ${COMPILE_PENCIL_DEST}")
    endif()
  endif()
  foreach(pencil_file ${COMPILE_PENCIL_FILES})
    get_filename_component(PENCIL_FILE_NAME ${pencil_file} NAME_WE)
    get_filename_component(PENCIL_FILE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${pencil_file} DIRECTORY)
    list(APPEND ${COMPILE_PENCIL_DEST_INCLUDE_DIRS} ${PENCIL_FILE_DIRECTORY})
    add_custom_command(OUTPUT ${PENCIL_FILE_NAME}.ppcg.c ${PENCIL_FILE_NAME}.ppcg_kernel.cl
                       COMMAND ${PENCIL_COMPILER}
                       ARGS ${PENCIL_REQUIRED_FLAGS} ${COMPILE_PENCIL_FLAGS} -I${PENCIL_INCLUDE_DIRS} -o ${PENCIL_FILE_NAME}.ppcg.c ${CMAKE_CURRENT_SOURCE_DIR}/${pencil_file}
                       DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${pencil_file}
                      )
    list(APPEND ${COMPILE_PENCIL_DEST_SOURCES} ${PENCIL_FILE_NAME}.ppcg.c)
  endforeach()
  set(${COMPILE_PENCIL_DEST_SOURCES}      ${${COMPILE_PENCIL_DEST_SOURCES}}      PARENT_SCOPE)
  set(${COMPILE_PENCIL_DEST_INCLUDE_DIRS} ${${COMPILE_PENCIL_DEST_INCLUDE_DIRS}} PARENT_SCOPE)
endfunction()