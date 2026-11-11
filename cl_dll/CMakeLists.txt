cmake_minimum_required(VERSION 3.10)
project(CLDLL CXX)

set(CMAKE_CXX_STANDARD 11)

if(NOT MSVC)
	if(APPLE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	endif()
endif()

set(CLDLL_LIBRARY client)

set(WEAPONEVENT_SRCS
	./events/ev_cs16.cpp
	./events/event_ak47.cpp
	./events/event_aug.cpp
	./events/event_awp.cpp
	./events/event_createexplo.cpp
	./events/event_createsmoke.cpp
	./events/event_deagle.cpp
	./events/event_decal_reset.cpp
	./events/event_elite_left.cpp
	./events/event_elite_right.cpp
	./events/event_famas.cpp
	./events/event_fiveseven.cpp
	./events/event_g3sg1.cpp
	./events/event_galil.cpp
	./events/event_glock18.cpp
	./events/event_knife.cpp
	./events/event_m249.cpp
	./events/event_m3.cpp
	./events/event_m4a1.cpp
	./events/event_mac10.cpp
	./events/event_mp5n.cpp
	./events/event_p228.cpp
	./events/event_p90.cpp
	./events/event_scout.cpp
	./events/event_sg550.cpp
	./events/event_sg552.cpp
	./events/event_tmp.cpp
	./events/event_ump45.cpp
	./events/event_usp.cpp
	./events/event_vehicle.cpp
	./events/event_xm1014.cpp
	./events/hl_events.cpp
	./ev_common.cpp
)

set(CLWEAPON_SRCS
	./com_weapons.cpp
	./cs_wpn/cs_baseentity.cpp
	./cs_wpn/cs_weapons.cpp
	./wpn_shared/wpn_ak47.cpp
	./wpn_shared/wpn_aug.cpp
	./wpn_shared/wpn_awp.cpp
	./wpn_shared/wpn_c4.cpp
	./wpn_shared/wpn_deagle.cpp
	./wpn_shared/wpn_elite.cpp
	./wpn_shared/wpn_famas.cpp
	./wpn_shared/wpn_fiveseven.cpp
	./wpn_shared/wpn_flashbang.cpp
	./wpn_shared/wpn_g3sg1.cpp
	./wpn_shared/wpn_galil.cpp
	./wpn_shared/wpn_glock18.cpp
	./wpn_shared/wpn_hegrenade.cpp
	./wpn_shared/wpn_knife.cpp
	./wpn_shared/wpn_m249.cpp
	./wpn_shared/wpn_m3.cpp
	./wpn_shared/wpn_m4a1.cpp
	./wpn_shared/wpn_mac10.cpp
	./wpn_shared/wpn_mp5navy.cpp
	./wpn_shared/wpn_p228.cpp
	./wpn_shared/wpn_p90.cpp
	./wpn_shared/wpn_scout.cpp
	./wpn_shared/wpn_sg550.cpp
	./wpn_shared/wpn_sg552.cpp
	./wpn_shared/wpn_smokegrenade.cpp
	./wpn_shared/wpn_tmp.cpp
	./wpn_shared/wpn_ump45.cpp
	./wpn_shared/wpn_usp.cpp
	./wpn_shared/wpn_xm1014.cpp
)

set(HUD_SRCS
	./hud/ammo.cpp
	./hud/ammo_secondary.cpp
	./hud/ammohistory.cpp
	./hud/battery.cpp
	./hud/geiger.cpp
	./hud/health.cpp
	./hud/hud.cpp
	./hud/hud_msg.cpp
	./hud/hud_redraw.cpp
	./hud/hud_spectator.cpp
	./hud/hud_update.cpp
	./hud/sniperscope.cpp
	./hud/nvg.cpp
	./hud/menu.cpp
	./hud/message.cpp
	./hud/status_icons.cpp
	./hud/statusbar.cpp
	./hud/text_message.cpp
	./hud/train.cpp
	./hud/scoreboard.cpp
	./hud/MOTD.cpp
	./hud/radio.cpp
	./hud/radar.cpp
	./hud/timer.cpp
	./hud/money.cpp
	./hud/flashlight.cpp
	./hud/death.cpp
	./hud/saytext.cpp
	./hud/spectator_gui.cpp
	./include/hud/ammo.h
	./include/hud/ammohistory.h
	./include/hud/health.h
	./include/hud/hud.h
	./include/hud/hud_iface.h
	./include/hud/hud_spectator.h
	./include/hud/radar.h
)

set(STUDIORENDER_SRCS
	./studio/GameStudioModelRenderer.cpp
	./studio/StudioModelRenderer.cpp
	./studio/studio_util.cpp

	./include/studio/GameStudioModelRenderer.h
	./include/studio/StudioModelRenderer.h
	./include/studio/studio_util.h
)

set(PLAYERMOVE_SRCS
	../pm_shared/pm_debug.cpp
	../pm_shared/pm_math.cpp
	../pm_shared/pm_shared.cpp
)

set(CLDLL_SOURCES
	./cdll_int.cpp
	./demo.cpp
	./entity.cpp
	./in_camera.cpp
	./input.cpp
	./rain.cpp
	./tri.cpp
	./util.cpp
	./view.cpp
	./input_xash3d.cpp
	./vgui_parser.cpp
	./unicode_strtools.cpp
	./draw_util.cpp
	./include/camera.h
	./include/cl_dll.h
	./include/cl_util.h
	./include/com_weapons.h
	./include/csprite.h
	./include/demo.h
	./include/draw_util.h
	./include/eventscripts.h
	./include/events.h
	./include/ev_hldm.h
	./include/overview.h
	./include/parsemsg.h
	./include/rain.h
	./include/tf_defs.h
	./include/unicode_strtools.h
	./include/util_vector.h
	./include/vgui_parser.h
	./include/view.h
	./include/wrect.h
	./include/math/neon_mathfun.h
	./include/math/sse_mathfun.h

	${PLAYERMOVE_SRCS}
	${HUD_SRCS}
	${WEAPONEVENT_SRCS}
	${CLWEAPON_SRCS}
	${STUDIORENDER_SRCS}
)

include_directories(
	..
	stubs/
	include/
	include/hud/
	include/studio/
	include/math/
	../cl_dll/
	../common/
	../engine/
	../pm_shared/
	../game_shared/
	../public/
	${SDL2_INCLUDE_DIR}
)

if(XASH_STATIC_GAMELIB)
	add_library(${CLDLL_LIBRARY} STATIC ${CLDLL_SOURCES})
else()
	add_library(${CLDLL_LIBRARY} SHARED ${CLDLL_SOURCES})
endif()

if(MSVC)
	add_definitions(
		-DWIN32 -D_WINDOWS -D_WINDLL
		-DCLIENT_WEAPONS -DCLIENT_DLL
		-D_CS16CLIENT_ENABLE_GSRC_SUPPORT
		-D_CS16CLIENT_ALLOW_SPECIAL_SCRIPTING
		-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE
	)
else()
	add_definitions(
		-D_CS16CLIENT_ENABLE_GSRC_SUPPORT
		-DCLIENT_WEAPONS -DCLIENT_DLL
		-D_CS16CLIENT_ALLOW_SPECIAL_SCRIPTING
		-Dstricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -D_snprintf=snprintf
	)
	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		add_definitions(-DLINUX -D_LINUX)
	endif()
endif()

if(WIN32)
	set_target_properties(${CLDLL_LIBRARY} PROPERTIES
		OUTPUT_NAME "client"
		PREFIX ""
	)
else()
	set_target_properties(${CLDLL_LIBRARY} PROPERTIES
		OUTPUT_NAME "client"
	)
endif()

if(NOT XASH_STATIC_GAMELIB)
	target_link_libraries(${CLDLL_LIBRARY} ${CMAKE_DL_LIBS})
	if(NOT WIN32)
		target_link_libraries(${CLDLL_LIBRARY} -lm)
	endif()
	if(XASH_SDL)
		target_link_libraries(${CLDLL_LIBRARY} ${SDL2_LIBRARY})
	endif()
endif()
