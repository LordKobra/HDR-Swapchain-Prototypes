cmake_minimum_required(VERSION 4.2)


###   USER SETTINGS   ###

set(SUPPORT_DX_BUILD ON)
set(WINDOWS_SDK_VERSION "10.0.19041.0")
set(WINDOWS_SDK_PATH "C:/Program Files (x86)/Windows Kits/10")

### USER SETTINGS END ###

project(VulkanTutorial LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS OFF)
 
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/game)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/game)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

add_executable(${PROJECT_NAME})

# Detect compiler and platform
if(MSVC)
    message(STATUS "Detected MSVC compiler")
    set(COMPILER_FLAGS /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(SEND_ERROR "Detected Clang compiler")
    set(COMPILER_FLAGS -Wall -Wextra -Werror)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Detected GCC compiler")
    set(COMPILER_FLAGS -Wall -Wextra -Werror)
else()
    message(SEND_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# Definitions

if(WIN32)
  add_compile_definitions(
      VK_USE_PLATFORM_WIN32_KHR
  )
  if(SUPPORT_DX_BUILD)
  add_compile_definitions(
      SUPPORT_DX_BUILD
  )
  endif()
elseif(LINUX)
  add_compile_definitions(
      VK_USE_PLATFORM_WAYLAND_KHR
  )
endif()
add_compile_definitions(
      UNICODE
      _UNICODE
      VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
      VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
      VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
  )

# Compiler flags
if(MSVC)
  add_compile_options(/permissive- /EHsc)
endif()

#
# Vulkan
#

find_package (Vulkan 1.4.341 REQUIRED) 

#
# Windows
#

if(WIN32)

  target_include_directories(${PROJECT_NAME} PRIVATE
      "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/shared"
      "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/um"
      "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/winrt"
      #"${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/cppwinrt"
  )

  target_link_libraries(${PROJECT_NAME} PRIVATE
      "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/dxgi.lib"
  )
endif()

#
# DX12
#

if(WIN32 AND SUPPORT_DX_BUILD)

  target_include_directories(${PROJECT_NAME} PRIVATE
      "${CMAKE_SOURCE_DIR}/DirectX-Headers/include/directx"
  )

  target_link_libraries(${PROJECT_NAME} PRIVATE
      # "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/d2d1.lib"
      # "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/d3d11.lib"
      "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/d3d12.lib"
      # "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/dwrite.lib"
      # "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/d3dcompiler.lib"
      # "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/dxcompiler.lib"
      # "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64/dxguid.lib"
  )

  # If using DXC via environment
  find_program(DXC_EXECUTABLE dxc)
  if(NOT DXC_EXECUTABLE)
      message(WARNING "DXC compiler not found")
  endif()

  # Copy Agility SDK DLLs
  # add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
  #    "C:/path/to/AgilitySDK/bin/x64/D3D12Core.dll"
  #    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/D3D12/D3D12Core.dll"
  #    COMMENT "Copying D3D12Core.dll"
  # )
endif()

#
# Wayland
#

if(LINUX)

file (GLOB WAYLAND_FILES wayland_protocols/xdg-shell.xml
                         wayland_protocols/color-management-v1.xml
                         wayland_protocols/xdg-decoration-unstable-v1.xml
                         wayland_protocols/viewporter.xml
                         wayland_protocols/fractional-scale-v1.xml)

function(scan_wayland_protocol TARGET)
  cmake_parse_arguments("WAYLAND" "" "" "SOURCES" ${ARGN})
  set(WAYLAND_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/wayland)
  set(WAYLAND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/include/wayland)
  set(WAYLAND_TARGET_FILES "")
  foreach(WAYLAND_XML ${WAYLAND_SOURCES})
    get_filename_component(WAYLAND_NAME ${WAYLAND_XML} NAME_WE)
    set(WAYLAND_HEADER "${WAYLAND_NAME}.h")
    set(WAYLAND_SOURCE "${WAYLAND_NAME}.c")
    add_custom_command(
      OUTPUT ${WAYLAND_HEADER_DIR}/${WAYLAND_HEADER}
      COMMAND wayland-scanner client-header ${WAYLAND_XML} ${WAYLAND_HEADER}
      WORKING_DIRECTORY ${WAYLAND_HEADER_DIR}
      DEPENDS ${WAYLAND_XML}
      COMMENT "Compiling Header for ${WAYLAND_XML}"
      VERBATIM
    )
    list(APPEND WAYLAND_TARGET_FILES ${WAYLAND_HEADER_DIR}/${WAYLAND_HEADER})
    add_custom_command(
      OUTPUT ${WAYLAND_SOURCE_DIR}/${WAYLAND_SOURCE}
      COMMAND wayland-scanner private-code ${WAYLAND_XML} ${WAYLAND_SOURCE}
      WORKING_DIRECTORY ${WAYLAND_SOURCE_DIR}
      DEPENDS ${WAYLAND_XML}
      COMMENT "Compiling private code for ${WAYLAND_XML}"
      VERBATIM
    )
    list(APPEND WAYLAND_TARGET_FILES ${WAYLAND_SOURCE_DIR}/${WAYLAND_SOURCE})
  endforeach()
  add_custom_target(${TARGET} DEPENDS ${WAYLAND_TARGET_FILES})
endfunction()

scan_wayland_protocol(WAYLAND_HDR_PROTOCOL SOURCES ${WAYLAND_FILES})
add_dependencies(${PROJECT_NAME} WAYLAND_HDR_PROTOCOL)

endif()

#
# Includes
#

include_directories (src/
                     include/
                     include/wayland/
                     imgui/
                     imgui/backends/
                     imgui/misc/cpp/)

#
# Add files
#

file (GLOB EXTERN_SOURCES   imgui/*.cpp
                            imgui/backends/imgui_impl_vulkan.cpp
                            imgui/misc/cpp/*.cpp)

set (GENERAL_SOURCES        src/utility.cpp
                            src/main.cpp
                            src/hdrManager.cpp)

file (GLOB SLANG_SHADERS    shaders/triangle.slang)

file (GLOB VK_SOURCES       src/vulkanImgui.cpp
                            src/vulkanApp.cpp
                            src/vulkanHDR.cpp
                            src/vulkanUtility.cpp)

if(WIN32 AND SUPPORT_DX_BUILD)
file ( GLOB OS_SOURCES      src/dxApp.cpp
                            src/dxImgui.cpp
                            src/dxHDR.cpp
                            imgui/backends/imgui_impl_dx12.cpp
                            imgui/backends/imgui_impl_win32.cpp
                            src/win32Window.cpp)
elseif(WIN32)
file ( GLOB OS_SOURCES      imgui/backends/imgui_impl_win32.cpp
                            src/win32Window.cpp)
elseif(LINUX)
# GLOB cant find them at configuration time, before the compile command is run
set ( OS_SOURCES            ${CMAKE_SOURCE_DIR}/src/linuxWayland.cpp
                            ${CMAKE_SOURCE_DIR}/src/linux_imgui_impl_wayland.cpp
                            ${CMAKE_SOURCE_DIR}/include/wayland/xdg-shell.c
                            ${CMAKE_SOURCE_DIR}/include/wayland/xdg-decoration-unstable-v1.c
                            ${CMAKE_SOURCE_DIR}/include/wayland/viewporter.c
                            ${CMAKE_SOURCE_DIR}/include/wayland/fractional-scale-v1.c
                            ${CMAKE_SOURCE_DIR}/include/wayland/color-management-v1.c)
endif()

#
# Organizing files
#

target_sources(${PROJECT_NAME} PRIVATE ${EXTERN_SOURCES} ${GENERAL_SOURCES}
                                       ${SLANG_SHADERS}
                                       ${VK_SOURCES} ${OS_SOURCES} )

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries (${PROJECT_NAME} PRIVATE
                       ${Vulkan_LIBRARIES})
if(WIN32)
target_link_libraries (${PROJECT_NAME} PRIVATE
                       dxgi)
elseif(LINUX)
# add_library(wayland-client STATIC) -> since build_shared_libs is off, we get static either way
target_link_libraries (${PROJECT_NAME} PRIVATE
                       wayland-client)                     
endif()

#
# Shader Compilation
#

function (add_slang_shader_target TARGET)
  cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
  set(SHADERS_DIR ${CMAKE_BINARY_DIR}/game/shaders)
  set(ENTRY_POINTS -entry vertMain -stage vertex -entry fragMain -stage pixel)
  file(MAKE_DIRECTORY ${SHADERS_DIR})
  #add_custom_command(
  #  OUTPUT ${SHADERS_DIR}
  #  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  #)
  add_custom_command(
    OUTPUT ${SHADERS_DIR}/slang.spv
    COMMAND slangc ${SLANG_SHADERS} -target spirv -profile spirv_1_4 -emit-spirv-directly -o slang.spv
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SLANG_SHADERS}
    COMMENT "Compiling Slang Shaders for SPIR-V"
    VERBATIM
  )
  if(WIN32)
  # Note
  # The slangc CLI currently cannot automatically deduce -entrypoint and -stage/-profile options from [shader(...)]
  # attributes when generating code for targets other than SPIRV, Metal, CUDA, or Optix. For targets such as HLSL, 
  # please continue to specify -entry and -stage options, even when compiling a file with the [shader(...)] attribute 
  # on its entry point.
  # Note 2: seems dxil cant handle multiple entry points either
  add_custom_command(
    OUTPUT ${SHADERS_DIR}/slang-vs.dxil
    COMMAND slangc ${SLANG_SHADERS} -target dxil -profile sm_6_0 -entry vertMain -stage vertex -o slang-vs.dxil
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SLANG_SHADERS}
    COMMENT "Compiling Slang Shaders for DXIL"
    VERBATIM
  )
  add_custom_command(
    OUTPUT ${SHADERS_DIR}/slang-ps.dxil
    COMMAND slangc ${SLANG_SHADERS} -target dxil -profile sm_6_0 -entry fragMain -stage pixel -o slang-ps.dxil
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SLANG_SHADERS}
    COMMENT "Compiling Slang Shaders for DXIL"
    VERBATIM
  )
  add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv ${SHADERS_DIR}/slang-vs.dxil ${SHADERS_DIR}/slang-ps.dxil)
  elseif(LINUX)
  add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
  endif()
endfunction()

add_slang_shader_target( SLANG_SHADER_SPIRV SOURCES ${SLANG_SHADERS})
add_dependencies(${PROJECT_NAME} SLANG_SHADER_SPIRV)