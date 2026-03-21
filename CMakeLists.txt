cmake_minimum_required(VERSION 3.14)
project(OsgViewer VERSION 1.0 LANGUAGES CXX)

# -------------------------------------------------------
# C++ Standard
# -------------------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# -------------------------------------------------------
# OSG Root - user có thể override bằng -DOSG_ROOT=...
# Mặc định tìm trong thư mục cùng cấp với CMakeLists.txt
# -------------------------------------------------------
if(NOT DEFINED OSG_ROOT)
    set(OSG_ROOT "${CMAKE_SOURCE_DIR}/OSG")
endif()

if(NOT DEFINED THIRDPARTY_ROOT)
    set(THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/3rdParty")
endif()

if(NOT DEFINED OSG_DATA_DIR)
    set(OSG_DATA_DIR "${CMAKE_SOURCE_DIR}/OpenSceneGraph-Data")
endif()

message(STATUS "OSG_ROOT       = ${OSG_ROOT}")
message(STATUS "THIRDPARTY_ROOT= ${THIRDPARTY_ROOT}")
message(STATUS "OSG_DATA_DIR   = ${OSG_DATA_DIR}")

# -------------------------------------------------------
# Tìm OSG manually (không dùng find_package vì OSG
# không có sẵn trong system path trên Windows thường)
# -------------------------------------------------------
set(OSG_INCLUDE_DIR "${OSG_ROOT}/include")
set(OSG_LIB_DIR     "${OSG_ROOT}/lib")
set(THIRDPARTY_INCLUDE_DIR "${THIRDPARTY_ROOT}/include")
set(THIRDPARTY_LIB_DIR     "${THIRDPARTY_ROOT}/lib")

# Kiểm tra tồn tại
if(NOT EXISTS "${OSG_INCLUDE_DIR}/osg/Node")
    message(FATAL_ERROR 
        "Không tìm thấy OSG headers tại: ${OSG_INCLUDE_DIR}\n"
        "Hãy set -DOSG_ROOT=<path> hoặc đặt thư mục OSG cùng cấp với CMakeLists.txt"
    )
endif()

# -------------------------------------------------------
# Source files - tự động glob toàn bộ src/
# -------------------------------------------------------
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

if(NOT SOURCES)
    message(FATAL_ERROR "Không tìm thấy source files trong src/")
endif()

message(STATUS "Sources:")
foreach(SRC ${SOURCES})
    message(STATUS "  ${SRC}")
endforeach()

# Tự tổ chức file vào filters trong VS Solution Explorer
# src/Foo.cpp         → Source Files
# src/sub/Bar.cpp     → Source Files/sub
# shaders/a.vert      → Shaders
file(GLOB_RECURSE SHADER_FILES
    CONFIGURE_DEPENDS
    "shaders/*"
)

source_group(TREE "${CMAKE_SOURCE_DIR}/src"
    PREFIX "Source Files"
    FILES ${SOURCES}
)

source_group(TREE "${CMAKE_SOURCE_DIR}/shaders"
    PREFIX "Shaders"
    FILES ${SHADER_FILES}
)

# Them shader files vao project de hien thi trong VS
# (khong compile, chi de xem/edit tien)
add_executable(OsgViewer ${SOURCES} ${SHADER_FILES})

# -------------------------------------------------------
# Shaders: copy toàn bộ thư mục shaders/ vào output
# Dùng copy_directory để giữ nguyên cấu trúc folder
# -------------------------------------------------------
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_DST_DIR "$<TARGET_FILE_DIR:OsgViewer>/shaders")

if(EXISTS "${SHADER_SRC_DIR}")
    add_custom_command(TARGET OsgViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SHADER_SRC_DIR}"
            "${SHADER_DST_DIR}"
        COMMENT "Copying shaders/ -> output/shaders/"
    )
    message(STATUS "Shaders dir: ${SHADER_SRC_DIR}")
else()
    message(WARNING "Không tìm thấy thư mục shaders/ - shader files sẽ không được copy")
endif()

# -------------------------------------------------------
# Include directories
# -------------------------------------------------------
target_include_directories(OsgViewer PRIVATE
    ${OSG_INCLUDE_DIR}
    ${THIRDPARTY_INCLUDE_DIR}
    "${CMAKE_SOURCE_DIR}/src"   # de #include giua cac file trong src/
)

# -------------------------------------------------------
# Link libraries
# Tự động chọn Debug/Release suffix dựa trên build type
# -------------------------------------------------------
if(MSVC)
    # Windows MSVC: Debug dùng osgd.lib, Release dùng osg.lib
    target_link_directories(OsgViewer PRIVATE
        ${OSG_LIB_DIR}
        ${THIRDPARTY_LIB_DIR}
    )

    target_link_libraries(OsgViewer PRIVATE
        debug     osgd          optimized osg
        debug     osgDBd        optimized osgDB
        debug     osgGAd        optimized osgGA
        debug     osgViewerd    optimized osgViewer
        debug     osgUtild      optimized osgUtil
        debug     osgTextd      optimized osgText
        debug     OpenThreadsd  optimized OpenThreads
		debug     glew32d       optimized glew32
        opengl32
        glu32
    )
else()
    # Linux / Mac: dùng find_package chuẩn
    find_package(OpenSceneGraph REQUIRED COMPONENTS
        osg osgDB osgGA osgViewer osgUtil osgText OpenThreads
    )
    target_include_directories(OsgViewer PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(OsgViewer PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
endif()

# -------------------------------------------------------
# Copy DLLs vào output folder (Windows only)
# Tiện chạy từ build folder mà không cần set PATH
# -------------------------------------------------------
if(WIN32)
    # OSG DLLs
    file(GLOB OSG_DLLS "${OSG_ROOT}/bin/*.dll")
    file(GLOB THIRDPARTY_DLLS "${THIRDPARTY_ROOT}/bin/*.dll")

    add_custom_command(TARGET OsgViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OSG_DLLS}
            $<TARGET_FILE_DIR:OsgViewer>
        COMMENT "Copying OSG DLLs..."
    )

    add_custom_command(TARGET OsgViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${THIRDPARTY_DLLS}
            $<TARGET_FILE_DIR:OsgViewer>
        COMMENT "Copying 3rdParty DLLs..."
    )

    # Set OSG_FILE_PATH va working directory cho VS debugger
    set_target_properties(OsgViewer PROPERTIES
        VS_DEBUGGER_ENVIRONMENT
            "PATH=${OSG_ROOT}/bin;${THIRDPARTY_ROOT}/bin;$ENV{PATH}\nOSG_FILE_PATH=${OSG_DATA_DIR}"
        VS_DEBUGGER_WORKING_DIRECTORY
            "$<TARGET_FILE_DIR:OsgViewer>"
    )
endif()