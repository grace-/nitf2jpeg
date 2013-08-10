# CMakeLists.txt for NITF2JPEG
#
# Copyright 2013 by Grace Vesom <grace.vesom@gmail.com>

cmake_minimum_required(VERSION 2.8)

ADD_DEFINITIONS(-DTHIS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Find OpenCV
FIND_PACKAGE(OpenCV 2.4.3 EXACT REQUIRED)
INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})

# Project files
FILE(GLOB folder_source *.cpp)
FILE(GLOB folder_header *.h*)

ADD_EXECUTABLE(nitf2jpeg ${folder_source} ${folder_header})
SOURCE_GROUP("Source Files" FILES ${folder_source})
SOURCE_GROUP("Header Files" FILES ${folder_header})

TARGET_LINK_LIBRARIES(nitf2jpeg 
  ${OpenCV_LIBRARIES}
  )
