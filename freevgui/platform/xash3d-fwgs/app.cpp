// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2024-2026 Alibek Omarov

#include "support.h"

using namespace vgui;

// resfile.cpp: the original ClientScheme.res loader (A7 build 66)
namespace vgui
{
namespace res
{
	void LoadClientScheme( void );
}
}

vguiapi_t *vgui::g_engine = nullptr;
Panel *vgui::g_rootPanel = nullptr;
XashSurface *vgui::g_surface = nullptr;

// declare as global so it installs itself as App::getInstance singleton
static XashApp staticApp;

static void XashStartup( int width, int height )
{
	if( g_rootPanel )
	{
		g_rootPanel->setSize( width, height );
		return;
	}

	g_rootPanel = new Panel( 0, 0, width, height );
	g_rootPanel->setPaintBorderEnabled( false );
	g_rootPanel->setPaintBackgroundEnabled( false );
	g_rootPanel->setVisible( true );
	g_rootPanel->setCursor( new Cursor( Cursor::DC_NONE ));

	staticApp.start();
	staticApp.setMinimumTickMillisInterval( 0 );

	g_surface = new XashSurface( g_rootPanel );
	g_rootPanel->setSurfaceBaseTraverse( g_surface );
	g_engine->DrawInit();
	// A7 build 66: the original ClientScheme.res - CS orange palette +
	// Verdana/Tahoma/Marlett fonts - goes over the built-in defaults
	vgui::res::LoadClientScheme();
}

static void XashShutdown( void )
{
	staticApp.stop();

	delete g_rootPanel;
	g_rootPanel = nullptr;

	delete g_surface;
	g_surface = nullptr;
}

static void *XashGetPanel( void )
{
	return (void *)g_rootPanel;
}

static void XashPaint( void )
{
	static int paintCalls;
	paintCalls++;

	// A7 build 66: the GameUI dialogs open from the main menu, so the
	// tree paints whenever it exists, not only in game
	// Paint whenever the support layer is initialized.  The engine invokes
	// this callback from both the active 2D pass and the menu pass; gating on
	// root child count can suppress the first frame while GameUI is attaching
	// its background/menu tree.
	if( !g_rootPanel || !g_surface )
	{
		if( paintCalls <= 10 )
			vgui_dprintf( "freevgui: paint %d skipped root=%p surface=%p\n",
				paintCalls, (void *)g_rootPanel, (void *)g_surface );
		return;
	}

	Panel *panel = g_surface->getPanel();

	if( !panel )
		return;

	if( paintCalls <= 10 )
		vgui_dprintf( "freevgui: paint %d root=%p children=%d size=%dx%d\n",
			paintCalls, (void *)g_rootPanel, g_rootPanel->getChildCount(),
			g_rootPanel->getWide(), g_rootPanel->getTall() );

	int width, height;

	g_rootPanel->getSize( width, height );

	g_scissor.enable();

	staticApp.externalTick();

	panel->setBounds( 0, 0, width, height );
	panel->repaint();
	panel->paintTraverse();

	g_scissor.disable();
}

#if defined( INTERNAL_VGUI_SUPPORT )
// what the engine probes in the client library when we're linked into it statically
#define VGUI_SUPPORT_INIT InitVGUISupportAPI
#else
#define VGUI_SUPPORT_INIT InitAPI
#endif

extern "C" void EXPORT VGUI_SUPPORT_INIT( vguiapi_t *api )
{
	// the structure is owned by the engine, keep the pointer, not a copy
	g_engine = api;

	api->Startup = XashStartup;
	api->Shutdown = XashShutdown;
	api->GetPanel = XashGetPanel;
	api->Paint = XashPaint;
	api->Mouse = XashMouse;
	api->Key = XashKey;
	api->MouseMove = XashMouseMove;
	api->TextInput = XashTextInput;
}
