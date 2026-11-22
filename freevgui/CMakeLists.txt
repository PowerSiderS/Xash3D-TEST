#
# Copyright (c) 2016 Alibek Omarov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 3.9)
project(FREEVGUI)

if(WIN32)
	set(FREEVGUI_PLATFORM win32)
else()
	set(FREEVGUI_PLATFORM posix)
endif()

# no CONFIGURE_DEPENDS: the Android SDK ships CMake 3.10, which predates it
file(GLOB FREEVGUI_SOURCES
	*.cpp
	controls/*.cpp
	platform/common/*.cpp
	platform/${FREEVGUI_PLATFORM}/*.cpp)

# A7 build 64: include the xash engine glue in SHARED builds too.
# The external "vgui.so" the engine loads (engine/client/vgui/
# vgui_draw.c resolves its InitAPI entry) is exactly freevgui built
# SHARED with this glue; static builds keep exporting
# InitVGUISupportAPI via INTERNAL_VGUI_SUPPORT as before.
file(GLOB FREEVGUI_XASH_SOURCES platform/xash3d-fwgs/*.cpp)
list(APPEND FREEVGUI_SOURCES ${FREEVGUI_XASH_SOURCES})
set(FREEVGUI_XASH_SUPPORT ON)

if(USE_STATIC_FREEVGUI)
	add_library(vgui STATIC ${FREEVGUI_SOURCES})
else()
	add_library(vgui SHARED ${FREEVGUI_SOURCES})
endif()

target_include_directories(vgui PRIVATE .)
target_compile_features(vgui PUBLIC cxx_std_11)

# the Win32 font backend rasterises through GDI
if(WIN32)
	target_link_libraries(vgui PRIVATE gdi32)
endif()

if(FREEVGUI_XASH_SUPPORT)
	# engine headers (vgui_api.h); freevgui may sit next to engine/ (upstream
	# single-tree layout) or under 3rdparty/ (fwgs tree layout) - probe both.
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../engine/vgui_api.h")
		target_include_directories(vgui PRIVATE ../engine)
	elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/vgui_api.h")
		target_include_directories(vgui PRIVATE ../../engine)
	else()
		message(FATAL_ERROR "freevgui: cannot locate engine/vgui_api.h")
	endif()

	# rename the entry point to what the engine looks for in the client library
	if(USE_STATIC_FREEVGUI)
		target_compile_definitions(vgui PRIVATE INTERNAL_VGUI_SUPPORT)
	endif()
endif()

set_target_properties(vgui PROPERTIES
	POSITION_INDEPENDENT_CODE 1
	BUILD_RPATH "$ORIGIN"
	INSTALL_RPATH "$ORIGIN")

# Xash3D-TEST: name the output after VGUI_SUPPORT_DLL, which is what
# engine/client/vgui/vgui_draw.c dlopens: libvgui_support.so on
# POSIX/Android and vgui_support.dll on Windows.
set_target_properties(vgui PROPERTIES OUTPUT_NAME vgui_support)
