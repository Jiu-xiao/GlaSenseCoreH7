cmake_minimum_required(VERSION 3.22)

include("cmake/gcc-arm-none-eabi.cmake")

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(PROJECT_NAME GlaSenseCoreH7)

# Include toolchain file

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Add LibXR
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LibXR.CMake)

file(
  GLOB ${PROJECT_NAME}_CAMERA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Camera/*.c")

target_sources(xr PRIVATE ${${PROJECT_NAME}_CAMERA_SOURCES})

target_include_directories(
  xr
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Camera)

include(${CMAKE_CURRENT_LIST_DIR}/Modules/CMakeLists.txt)

# Prints the section sizes
function(print_section_sizes TARGET)
  # Build Events command
  add_custom_command(
    TARGET ${TARGET}
    POST_BUILD
    COMMAND ${CMAKE_SIZE} ${TARGET})
endfunction()

# Creates output in hex format
function(create_hex_output TARGET)
  add_custom_target(
    ${TARGET}.hex ALL
    DEPENDS ${TARGET}
    COMMAND ${CMAKE_OBJCOPY} --output-target ihex ${TARGET} ${TARGET}.hex)
endfunction()

# Creates output in binary format
function(create_bin_output TARGET)
  add_custom_target(
    ${TARGET}.bin ALL
    DEPENDS ${TARGET}
    COMMAND ${CMAKE_OBJCOPY} --output-target binary --strip-all ${TARGET}
            ${TARGET}.bin)
endfunction()

create_hex_output(${PROJECT_NAME})
create_bin_output(${PROJECT_NAME})
print_section_sizes(${PROJECT_NAME})
