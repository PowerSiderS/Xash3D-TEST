cmake_minimum_required(VERSION 3.10...3.28)

# Install custom module path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# Must be specified before project() to check if we are targeting 64 bit
option(XASH_64BIT "Enable experimental 64-bit build." OFF)

if(NOT XASH_64BIT)
	include(VSForceXPToolchain)
endif()

project(CS16Client C CXX)

set(CMAKE_CXX_STANDARD 11)

include(FWGSLib)
include(CMakeDependentOption)

set(XASH3D_VERSION 0.19.4)

# User Defines
option(XASH_STATIC_GAMELIB "Build static game library (for Android or single binary)" OFF)
option(XASH_DEDICATED      "Enable dedicated build. Servers must choose this." OFF)

if(XASH_STATIC_GAMELIB)
	add_definitions(-DXASH_STATIC_GAMELIB=1)
endif()

# Windows only
cmake_dependent_option(XASH_NONSTANDARD_LOAD "Enable own DLL loader in Windows." ON "WIN32" OFF)

# POSIX only
cmake_dependent_option(XASH_USE_SELECT "Enable reading console commands from stdin." ON "NOT WIN32" OFF)
cmake_dependent_option(XASH_LIBDL "Enable libdl use. POSIX only." ON "NOT WIN32" OFF)

option(XASH_GLES   "Enable if running over GL to GLES translator." OFF)
option(XASH_NANOGL "Use NanoGL(GLES1)." OFF)
option(XASH_WES    "Use gl-wes-v2(GLES2)." OFF)
set(XASH_NANOGL_SRC "${CMAKE_SOURCE_DIR}/nanogl" CACHE PATH "NanoGL source path")
set(XASH_WES_SRC    "${CMAKE_SOURCE_DIR}/gl-wes-v2" CACHE PATH "gl-wes-v2 source path")

option(XASH_VECTORIZE_SINCOS "Try to use vectorized versions of sin(), cos() and sincos()" ON)

# Desktop or dedicated options
if(XASH_DEDICATED)
	option(XASH_SDL "Enable SDL." OFF)
	option(XASH_SINGLE_BINARY "Build engine as executable" ON)
	set(XASH_MAINUI OFF)
else()
	option(XASH_SDL "Enable SDL." ON)
	option(XASH_SINGLE_BINARY "Build engine as executable" OFF)
	option(XASH_MAINUI "Build mainui library" ON)
endif()

set(XASH_VGUI OFF) # CS16Client uses its own custom UI
option(XASH_USE_STB_SPRINTF "Use stb_sprintf implementation for sprintf" ON)
option(XASH_DOWNLOAD_DEPENDENCIES "Download dependencies automatically" OFF)

set(SDL2_PATH "${CMAKE_SOURCE_DIR}/SDL2" CACHE PATH "SDL2 path")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(XASH_64BIT ON CACHE BOOL "Enable experimental 64-bit build." FORCE)
	add_definitions(-DXASH_64BIT=1)
	message(STATUS "Building for 64-bit target.")
else()
	set(XASH_64BIT OFF CACHE BOOL "Enable experimental 64-bit build." FORCE)
	message(STATUS "Building for 32-bit (x86)")
endif()

if(XASH_NANOGL OR XASH_WES)
	set(XASH_GLES ON)
endif()

# Subprojects
if(NOT XASH_DEDICATED)
	add_subdirectory(cl_dll)
	add_subdirectory(mainui)
endif()

add_subdirectory(dlls)
add_subdirectory(engine)

if(NOT XASH_SINGLE_BINARY AND NOT ANDROID)
	add_subdirectory(game_launch)
endif()

if(WIN32)
	if(XASH_SINGLE_BINARY)
		set_property(DIRECTORY . PROPERTY VS_STARTUP_PROJECT xash)
	else()
		set_property(DIRECTORY . PROPERTY VS_STARTUP_PROJECT xash3d)
	endif()
endif()
