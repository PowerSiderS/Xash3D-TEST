// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026
// resfile.cpp - original GameUI resource (*.res) loader
//
// A7 build 65: parses the Valve VGUI2 resource format (KeyValues) and
// builds a live control tree under a parent panel so original CS 1.6
// dialogs - resource/CreateMultiplayerGame*.res and friends - can be
// brought up through this vgui2 runtime. File access goes through the
// engine (g_engine->LoadFile): game-dir relative paths, the same
// resolution rule the original GameUI used for its resources.
//
// Host API (EXPORT, see vgui.h):
//   void *FreeVGUILoadRes( const char *path, void *parent )
//     loads and builds the tree, returns the first top-level panel
//     (NULL parent docks the tree under the engine root panel)
//   void FreeVGUIUnloadRes( void *panel )
//     detaches and frees a tree built by the call above

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "vgui.h"
#include "panel.h"
#include "app.h"
#include "scheme.h"
#include "font.h"
#include "signals.h"
#include "controls/label.h"
#include "controls/button.h"
#include "controls/edit.h"
#include "controls/frame.h"
#include "controls/tab.h"
#include "controls/combo.h"
#include "fileimage.h"
#include "platform/xash3d-fwgs/support.h"

namespace vgui
{
namespace res
{

// case-insensitive compare, portable across the NDK/desktop builds
static int ricmp( const char *a, const char *b )
{
	while( *a && *b )
	{
		int ca = (unsigned char)*a++, cb = (unsigned char)*b++;

		if( ca >= 'A' && ca <= 'Z' ) ca += 'a' - 'A';
		if( cb >= 'A' && cb <= 'Z' ) cb += 'a' - 'A';
		if( ca != cb ) return ca - cb;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

struct KVNode
{
	std::string name;
	std::vector< std::pair<std::string, std::string> > values;
	std::vector<KVNode> children;

	const std::string *find( const char *key ) const
	{
		for( size_t i = 0; i < values.size(); i++ )
		{
			if( !ricmp( values[i].first.c_str(), key ))
				return &values[i].second;
		}
		return NULL;
	}
};

// minimal KeyValues reader: quoted or bare tokens, { } blocks, // comments
class KVReader
{
public:
	KVReader( const char *buf, int size ): p( buf ), end( buf + size ) {}

	// reads "name" { ... } into node
	bool parseRoot( KVNode &node )
	{
		std::string first, second;

		if( !nextToken( first ) || !nextToken( second ) || second != "{" )
			return false;

		node.name = first;
		return parseBlock( node );
	}

private:
	void skipWs( void )
	{
		while( p < end )
		{
			if( *p == '/' && p + 1 < end && p[1] == '/' )
			{
				while( p < end && *p != '\n' ) p++;
			}
			else if( *p == '/' && p + 1 < end && p[1] == '*' )
			{
				p += 2;
				while( p + 1 < end && !( p[0] == '*' && p[1] == '/' )) p++;
				if( p + 1 < end ) p += 2;
			}
			else if( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' )
				p++;
			else break;
		}
	}

	bool nextToken( std::string &tok )
	{
		skipWs();
		tok.clear();

		if( p >= end )
			return false;

		if( *p == '{' || *p == '}' )
		{
			tok.assign( 1, *p++ );
			return true;
		}

		if( *p == '"' )
		{
			p++;
			while( p < end && *p != '"' )
			{
				if( *p == '\\' && p + 1 < end )
					p++; // \" and friends - keep the literal char
				tok += *p++;
			}
			if( p < end ) p++; // closing quote
			return true;
		}

		while( p < end && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' &&
			*p != '{' && *p != '}' && *p != '"' )
			tok += *p++;

		return !tok.empty();
	}

	bool parseBlock( KVNode &node )
	{
		for( ;; )
		{
			std::string key;

			if( !nextToken( key ) || key == "}" )
				return key == "}";

			std::string val;

			if( !nextToken( val ))
				return false;

			if( val == "{" )
			{
				KVNode child;

				child.name = key;
				if( !parseBlock( child ))
					return false;
				node.children.push_back( child );
			}
			else
			{
				node.values.push_back( std::make_pair( key, val ));
			}
		}
	}

	const char *p;
	const char *end;
};

static int kvInt( const KVNode &n, const char *key, int def )
{
	const std::string *v = n.find( key );

	return v ? atoi( v->c_str() ) : def;
}

// ---------------------------------------------------------------------------
// #GameUI_* / #Cstrike_* token lookup (A7 build 68): the .res files label
// their controls with tokens, resolved by GameUI.dll through the game's
// english tables. The shipped files are UTF-16; flatten them for the KV
// reader and merge every level so both the "lang" wrapper style and flat
// pair files work.
// ---------------------------------------------------------------------------

static std::string flattenLanguageFile( const char *buf, int size )
{
	std::string out;

	if( size >= 2 && (unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE )
	{
		for( int i = 2; i + 1 < size; i += 2 )
		{
			unsigned char c = (unsigned char)buf[i], hi = (unsigned char)buf[i + 1];

			if( hi == 0 && c )
				out += (char)c;
			else if( c == '\n' && hi == 0 )
				out += '\n';
		}
	}
	else
	{
		out.assign( buf, size );
	}

	return out;
}

static std::map<std::string, std::string> &stringTable( void )
{
	static std::map<std::string, std::string> table;
	static bool loaded = false;

	// Language files use a small hierarchy (usually lang -> Tokens), while
	// some shipped files put pairs directly at the root. Walk the complete
	// tree so both layouts resolve the original #GameUI_* labels.
	static void (*collectValues)( const KVNode &, std::map<std::string, std::string> & ) =
		[]( const KVNode &node, std::map<std::string, std::string> &out )
		{
			for( size_t i = 0; i < node.values.size(); i++ )
				out[node.values[i].first] = node.values[i].second;

			for( size_t i = 0; i < node.children.size(); i++ )
				collectValues( node.children[i], out );
		};

	if( !loaded )
	{
		loaded = true;

		static const char *files[] =
		{
			"resource/gameui_english.txt",
			"resource/cstrike_english.txt",
			"resource/valve_english.txt",
		};

		for( int f = 0; f < 3; f++ )
		{
			if( !g_engine || !g_engine->LoadFile )
				break;

			int size = 0;
			char *buf = (char *)g_engine->LoadFile( files[f], &size );

			if( !buf )
				continue;

			std::string flat = flattenLanguageFile( buf, size );

			if( g_engine->FreeFile )
				g_engine->FreeFile( buf );

			KVNode root;
			KVReader reader( flat.c_str(), (int)flat.size( ));

			if( !reader.parseRoot( root ))
				continue;

			collectValues( root, table );
		}
	}

	return table;
}

// resolves a "#Token" label; plain text and unknown tokens pass through
// unchanged so a missing file stays visible instead of eating labels
static const char *resolveLabel( const char *text )
{
	if( !text || text[0] != '#' )
		return text;

	std::map<std::string, std::string> &table = stringTable();
	std::map<std::string, std::string>::iterator it = table.find( text + 1 );

	return it != table.end() ? it->second.c_str() : text;
}

// the original Divider: a one-pixel rule on the .res geometry
class DividerPanel : public Panel
{
public:
	DividerPanel( int x, int y, int w, int h ): Panel( x, y, w, h )
	{
		setPaintBorderEnabled( false );
		setPaintBackgroundEnabled( true );
		setPaintEnabled( false );
	}

	virtual void paintBackground() override
	{
		int wide, tall;

		getPaintSize( wide, tall );
		drawSetColor( Scheme::SC_SECONDARY1 );
		drawFilledRect( 0, 0, wide, tall > 1 ? 2 : 1 );
	}
};

// field-name registry: only .res-built controls carry names; the map
// lives here so Panel keeps its original class layout (A7 build 66)
static std::map<Panel *, std::string> &panelNames( void )
{
	static std::map<Panel *, std::string> names;

	return names;
}

// drop a panel's name as its tree gets freed
void forgetPanelName( Panel *p )
{
	panelNames().erase( p );
}

// walk a tree for a control by its .res fieldName
Panel *findNamed( Panel *root, const char *name )
{
	std::map<Panel *, std::string>::iterator it = panelNames().find( root );

	if( it != panelNames().end() && !ricmp( it->second.c_str(), name ))
		return root;

	for( int i = 0; i < root->getChildCount(); i++ )
	{
		Panel *found = findNamed( root->getChild( i ), name );

		if( found )
			return found;
	}

	return NULL;
}

// builds one control (and, recursively, everything nested in it).
// control classes unknown to us degrade to a plain Panel container so
// original layouts still come up with the right geometry; ComboBox
// collapses to a read-only edit box until the dropdown composite lands
// with the first real .res-driven dialog.
static Panel *createControl( const KVNode &n )
{
	const std::string *cls = n.find( "ControlName" );
	const char *cname = cls ? cls->c_str() : n.name.c_str();
	const std::string *text = n.find( "labelText" );

	if( !text ) text = n.find( "text" );

	// tokens resolve the way GameUI.dll resolved them
	const char *label = resolveLabel( text ? text->c_str() : "" );

	int x = kvInt( n, "xpos", 0 ), y = kvInt( n, "ypos", 0 );
	int w = kvInt( n, "wide", 0 ), h = kvInt( n, "tall", 0 );

	Panel *p = NULL;

	if( !ricmp( cname, "Label" ))
	{
		p = new Label( label, x, y, w, h );
	}
	else if( !ricmp( cname, "Button" ) || !ricmp( cname, "TextButton" ))
	{
		p = new Button( label, x, y, w, h );
	}
	else if( !ricmp( cname, "CheckButton" ))
	{
		p = new CheckButton( label, x, y, w, h );
	}
	else if( !ricmp( cname, "ToggleButton" ))
	{
		p = new ToggleButton( label, x, y, w, h );
	}
	else if( !ricmp( cname, "RadioButton" ))
	{
		p = new RadioButton( label, x, y, w, h );
	}
	else if( !ricmp( cname, "Frame" ))
	{
		Frame *f = new Frame( x, y, w, h );

		// GameUI frames carry a title and a close button only, are
		// movable by the caption but never resized by the user
		f->setTrayButtonVisible( false );
		f->setMinimizeButtonVisible( false );
		f->setMaximizeButtonVisible( false );
		f->setMenuButtonVisible( false );
		f->setSizeable( false );
		p = f;
	}
	else if( !ricmp( cname, "ComboBox" ))
	{
		// face + dropdown arrow + list (A7 build 66); items come from
		// the host through FreeVGUIComboAddItem - the .res has none
		p = new ComboBox( label, x, y, w, h );
	}
	else if( !ricmp( cname, "TextEntry" ) || !ricmp( cname, "RichText" ) ||
		!ricmp( cname, "TextBuffer" ))
	{
		EditPanel *e = new EditPanel( x, y, w, h );

		// "text"/"defaultText": initial content (A7 build 68) - the
		// modern bot page's "Player" default arrives this way
		const std::string *init = n.find( "text" );

		if( !init ) init = n.find( "defaultText" );

		if( init )
		{
			for( size_t ci = 0; ci < init->size( ); ci++ )
				e->doCursorInsertChar(( *init )[ci] );
		}

		p = e;
	}
	else if( !ricmp( cname, "PropertySheet" ))
	{
		// the tab container of the original dialogs; the host docks
		// the loaded page panels through FreeVGUITabAddPage
		p = new TabPanel( x, y, w, h );
	}
	else if( !ricmp( cname, "Divider" ))
	{
		p = new DividerPanel( x, y, w, h );
	}
	else if( !ricmp( cname, "CCvarToggleCheckButton" ))
	{
		// cvar-backed toggle renders exactly like a check button
		p = new CheckButton( label, x, y, w, h );
	}
	else
	{
		// PropertyPage, CPanelListPanel, EditablePanel, anything
		// unknown: container with the original geometry, children
		// still build
		p = new Panel( x, y, w, h );
	}

	if( !p )
		return NULL;

	p->setBounds( x, y, w, h );
	p->setVisible( kvInt( n, "visible", 1 ) != 0 );

	// keep the .res field name for FreeVGUIFindChild lookups
	const std::string *fieldName = n.find( "fieldName" );

	if( fieldName )
		panelNames()[p] = *fieldName;

	// GameUI frames lay their .res children out in the client area,
	// below the caption strip (A7 build 68)
	Panel *childHost = p;
	Frame *asFrame = dynamic_cast<Frame *>( p );

	if( asFrame )
		childHost = asFrame->getClient();

	for( size_t i = 0; i < n.children.size(); i++ )
	{
		Panel *child = createControl( n.children[i] );

		if( child )
			childHost->addChild( child );
	}

	return p;
}

// ---------------------------------------------------------------------------
// ClientScheme.res - the original VGUI2 scheme tables. The Colors section
// maps onto the slots every freevgui control draws from, so the dialogs
// come up with the authentic CS palette (BaseText 255 176 0 etc); the
// Fonts section builds real Fonts through the TTF backend, honoring the
// file's yres resolution ranges the way VGUI2 did.
// ---------------------------------------------------------------------------

struct SchemeColorMap { const char *name; Scheme::SchemeColor slot; };

static const SchemeColorMap schemeColorMap[] =
{
	{ "BaseText",       Scheme::SC_PRIMARY1   }, // CS orange body text
	{ "BrightBaseText", Scheme::SC_PRIMARY2   },
	{ "SelectedText",   Scheme::SC_PRIMARY3   },
	{ "LabelDimText",   Scheme::SC_SECONDARY1 }, // dimmed labels
	{ "DisabledText1",  Scheme::SC_SECONDARY2 },
	{ "DisabledText2",  Scheme::SC_SECONDARY3 },
	{ "SelectionBG",    Scheme::SC_USER       }, // orange highlight fill
};

struct SchemeFontMap { const char *name; Scheme::SchemeFont slot; };

static const SchemeFontMap schemeFontMap[] =
{
	{ "Default",          Scheme::SF_PRIMARY1 },
	{ "DefaultSmall",     Scheme::SF_PRIMARY2 },
	{ "DefaultVerySmall", Scheme::SF_PRIMARY3 },
	{ "Marlett",          Scheme::SF_SECONDARY },
};

// pick the font entry whose yres range brackets the screen height, like
// VGUI2 did; a rangeless entry suits any resolution, and if nothing
// matches the first listed entry is used
static const KVNode *pickFontEntry( const KVNode &font, int yres )
{
	const KVNode *first = NULL;

	for( size_t i = 0; i < font.children.size(); i++ )
	{
		const KVNode &e = font.children[i];
		const std::string *yr = e.find( "yres" );

		if( !first )
			first = &e;

		if( !yr )
			return &e;

		int lo = 0, hi = 0;

		if( sscanf( yr->c_str(), "%d %d", &lo, &hi ) == 2 && yres >= lo && yres <= hi )
			return &e;
	}

	return first;
}

// called by the platform startup after the root panel exists: loads
// resource/ClientScheme.res (game dir first) over the built-in scheme
void LoadClientScheme( void )
{
	if( !g_engine || !g_engine->LoadFile )
		return;

	int size = 0;
	char *buf = (char *)g_engine->LoadFile( "resource/ClientScheme.res", &size );

	if( !buf || size <= 0 )
	{
		// no scheme file: the built-in defaults stay in place
		if( buf && g_engine->FreeFile )
			g_engine->FreeFile( buf );
		return;
	}

	KVNode root;
	KVReader reader( buf, size );
	bool ok = reader.parseRoot( root );

	g_engine->FreeFile( buf );

	if( !ok )
		return;

	Scheme *scheme = App::getInstance()->getScheme();
	const KVNode *colors = NULL, *fonts = NULL;
	int wide = 0, tall = 0;

	if( g_rootPanel )
		g_rootPanel->getSize( wide, tall );

	for( size_t i = 0; i < root.children.size(); i++ )
	{
		if( !ricmp( root.children[i].name.c_str(), "Colors" ))
			colors = &root.children[i];
		else if( !ricmp( root.children[i].name.c_str(), "Fonts" ))
			fonts = &root.children[i];
	}

	if( colors )
	{
		const size_t mapCount = sizeof( schemeColorMap ) / sizeof( schemeColorMap[0] );

		for( size_t i = 0; i < colors->values.size(); i++ )
		{
			for( size_t m = 0; m < mapCount; m++ )
			{
				if( ricmp( colors->values[i].first.c_str(), schemeColorMap[m].name ))
					continue;

				int r = 0, g = 0, b = 0, a = 255;

				sscanf( colors->values[i].second.c_str(), "%d %d %d %d", &r, &g, &b, &a );

				// freevgui follows the VGUI surface convention used by the
				// engine: alpha 0 is opaque and 255 is transparent.  The
				// original ClientScheme.res stores ordinary compositing alpha,
				// so convert it while importing the scheme. Without this,
				// opaque scheme text is discarded and zero-alpha window colors
				// fill the whole dialog black.
				if( a < 0 ) a = 0;
				if( a > 255 ) a = 255;
				a = 255 - a;
				scheme->setColor( schemeColorMap[m].slot, r, g, b, a );
			}
		}
	}

	if( fonts )
	{
		const size_t mapCount = sizeof( schemeFontMap ) / sizeof( schemeFontMap[0] );

		for( size_t m = 0; m < mapCount; m++ )
		{
			const KVNode *font = NULL;

			for( size_t i = 0; i < fonts->children.size(); i++ )
			{
				if( !ricmp( fonts->children[i].name.c_str(), schemeFontMap[m].name ))
					font = &fonts->children[i];
			}

			if( !font )
				continue;

			const KVNode *entry = pickFontEntry( *font, tall ? tall : 480 );

			if( !entry )
				continue;

			const std::string *face = entry->find( "name" );
			int tallF = kvInt( *entry, "tall", 12 );
			int weight = kvInt( *entry, "weight", 400 );
			bool underline = kvInt( *entry, "underline", 0 ) != 0;

			scheme->setFont( schemeFontMap[m].slot,
				new Font( face ? face->c_str() : "Tahoma", tallF, 0, 0.0f,
					weight, false, underline, false, false ));
		}
	}
}

} // namespace res

// menu-row registry for FreeVGUIGameMenuLoad trees (build 69)
namespace res
{
	void forgetMenuRows( Panel *p );
}

// detach-and-free a built tree. no freevgui control defines a
// destructor yet, so this walks children explicitly; memory owned by
// individual controls is released when they grow destructors.
static void deleteTree( Panel *p )
{
	res::forgetPanelName( p );
	res::forgetMenuRows( p );

	while( p->getChildCount() > 0 )
	{
		Panel *child = p->getChild( 0 );

		p->removeChild( child );
		deleteTree( child );
	}

	delete p;
}

} // namespace vgui

// ---------------------------------------------------------------------------
// host-facing API: the support lib stays a plain library, the host (menu
// client) pulls original dialogs up by resource path through these exports.
// ---------------------------------------------------------------------------

extern "C" EXPORT void *FreeVGUILoadRes( const char *path, void *parent )
{
	if( !path || !vgui::g_engine || !vgui::g_engine->LoadFile )
		return NULL;

	int size = 0;
	char *buf = (char *)vgui::g_engine->LoadFile( path, &size );

	if( !buf || size <= 0 )
	{
		if( buf && vgui::g_engine->FreeFile )
			vgui::g_engine->FreeFile( buf );
		return NULL;
	}

	vgui::res::KVNode root;
	vgui::res::KVReader reader( buf, size );
	bool ok = reader.parseRoot( root );

	vgui::g_engine->FreeFile( buf );

	if( !ok )
		return NULL;

	vgui::Panel *host = parent ? (vgui::Panel *)parent : vgui::g_rootPanel;

	if( !host )
		return NULL;

	// A7 build 68: the original pages (CreateMultiplayerGame*Page.res)
	// list every control at the top level with no wrapping PropertyPage
	// panel - GameUI.dll built that panel around them in code. Group a
	// flat multi-block file under one auto-sized container so the page
	// travels as a single tree into the tab sheet; a single-block file
	// (the frame shell, a real PropertyPage) stays as-is.
	if( root.children.size() > 1 )
	{
		int maxx = 0, maxy = 0;

		for( size_t i = 0; i < root.children.size(); i++ )
		{
			const vgui::res::KVNode &b = root.children[i];
			int bx = vgui::res::kvInt( b, "xpos", 0 ), by = vgui::res::kvInt( b, "ypos", 0 );
			int bw = vgui::res::kvInt( b, "wide", 0 ), bh = vgui::res::kvInt( b, "tall", 0 );

			if( bx + bw > maxx ) maxx = bx + bw;
			if( by + bh > maxy ) maxy = by + bh;
		}

		vgui::Panel *wrap = new vgui::Panel( 0, 0, maxx > 0 ? maxx : 416, maxy > 0 ? maxy : 276 );

		wrap->setPaintBackgroundEnabled( false );
		wrap->setPaintEnabled( false );
		wrap->setPaintBorderEnabled( false );

		for( size_t i = 0; i < root.children.size(); i++ )
		{
			vgui::Panel *c = vgui::res::createControl( root.children[i] );

			if( c )
				wrap->addChild( c );
		}

		host->addChild( wrap );

		return wrap;
	}

	vgui::Panel *first = NULL;

	for( size_t i = 0; i < root.children.size(); i++ )
	{
		vgui::Panel *c = vgui::res::createControl( root.children[i] );

		if( c )
		{
			host->addChild( c );
			if( !first ) first = c;
		}
	}

	return first;
}

extern "C" EXPORT void FreeVGUIUnloadRes( void *panel )
{
	vgui::Panel *p = (vgui::Panel *)panel;

	if( !p )
		return;

	if( vgui::App::getInstance( ))
		vgui::App::getInstance()->panelDeleted( p );

	if( p->getParent() )
		p->getParent()->removeChild( p );

	vgui::deleteTree( p );
}

// ---------------------------------------------------------------------------
// build 66 additions: dialog assembly (tab pages), control lookup by field
// name, combo items, button callbacks and input forwarding for the host
// ---------------------------------------------------------------------------

extern "C" EXPORT void FreeVGUITabAddPage( void *sheet, void *page, const char *title )
{
	vgui::TabPanel *tp = (vgui::TabPanel *)sheet;

	if( !tp || !page || !title )
		return;

	// reparents the loaded PropertyPage under the sheet's client area
	tp->addTabPanel( title, (vgui::Panel *)page );
}

extern "C" EXPORT void *FreeVGUIFindChild( void *root, const char *fieldName )
{
	vgui::Panel *p = (vgui::Panel *)root;

	if( !p || !fieldName )
		return NULL;

	return vgui::res::findNamed( p, fieldName );
}

extern "C" EXPORT void FreeVGUIComboAddItem( void *combo, const char *item )
{
	vgui::ComboBox *cb = (vgui::ComboBox *)combo;

	if( !cb || !item )
		return;

	cb->addItem( item );
}

// the returned pointer stays valid until the combo is freed or items
// are added again - copy it out if it must outlive the dialog
extern "C" EXPORT const char *FreeVGUIComboGetText( void *combo )
{
	vgui::ComboBox *cb = (vgui::ComboBox *)combo;

	return cb ? cb->getText() : NULL;
}

extern "C" EXPORT void FreeVGUISetTitle( void *panel, const char *title )
{
	vgui::Frame *f = (vgui::Frame *)panel;

	if( !f || !title )
		return;

	f->setTitle( title );
}

// C callback bridge: host buttons (Start/Cancel) fire these
class ActionCB : public vgui::ActionSignal
{
public:
	ActionCB( void (*fn)( void * ), void *ud ): fn( fn ), ud( ud )
	{
	}

	virtual void actionPerformed( vgui::Panel * ) override
	{
		if( fn )
			fn( ud );
	}

private:
	void (*fn)( void * );
	void *ud;
};

extern "C" EXPORT void FreeVGUISetAction( void *panel, void (*fn)( void * ), void *userdata )
{
	vgui::Button *b = (vgui::Button *)panel;

	if( !b || !fn )
		return;

	b->addActionSignal( new ActionCB( fn, userdata ));
}

// the caption close button: GameUI frames close on it (A7 build 68)
class FrameCloseCB : public vgui::FrameSignal
{
public:
	FrameCloseCB( void (*fn)( void * ), void *ud ): fn( fn ), ud( ud )
	{
	}

	virtual void closing( vgui::Frame * ) override
	{
		if( fn )
			fn( ud );
	}

	virtual void minimizing( vgui::Frame *, bool ) override
	{
	}

private:
	void (*fn)( void * );
	void *ud;
};

extern "C" EXPORT void FreeVGUISetCloseAction( void *panel, void (*fn)( void * ), void *userdata )
{
	vgui::Frame *f = (vgui::Frame *)panel;

	if( !f || !fn )
		return;

	f->addFrameSignal( new FrameCloseCB( fn, userdata ));
}

// input forwarding: while a .res dialog is up the host (menu client)
// receives the raw engine input and relays it here (A7 build 66)
extern "C" EXPORT void FreeVGUIMouseMove( int x, int y )
{
	vgui::XashMouseMove( x, y );
}

extern "C" EXPORT void FreeVGUIMouse( int action, int code )
{
	vgui::XashMouse( (VGUI_MouseAction)action, code );
}

extern "C" EXPORT void FreeVGUIKey( int action, int code )
{
	vgui::XashKey( (VGUI_KeyAction)action, (VGUI_KeyCode)code );
}

// dialog layout helpers: the .res files are authored in the original
// ~640x480 design space, the host scales the whole tree to the real
// screen and centers the frame (A7 build 66)
static void scaleTree_r( vgui::Panel *p, float scale )
{
	for( int i = 0; i < p->getChildCount(); i++ )
	{
		vgui::Panel *c = p->getChild( i );
		int x, y, w, h;

		c->getPos( x, y );
		c->getSize( w, h );
		c->setPos(( int )( x * scale + 0.5f ), ( int )( y * scale + 0.5f ));
		c->setSize(( int )( w * scale + 0.5f ), ( int )( h * scale + 0.5f ));
		scaleTree_r( c, scale );
	}
}

extern "C" EXPORT void FreeVGUIScaleTree( void *root, float scale )
{
	vgui::Panel *p = (vgui::Panel *)root;

	if( !p || scale <= 0.0f )
		return;

	// A7 build 69: scale the tree root too - proportional scales the
	// whole tree; the host re-places the root afterwards and reads the
	// scaled size back through FreeVGUIGetBounds
	int x, y, w, h;

	p->getPos( x, y );
	p->getSize( w, h );
	p->setPos(( int )( x * scale + 0.5f ), ( int )( y * scale + 0.5f ));
	p->setSize(( int )( w * scale + 0.5f ), ( int )( h * scale + 0.5f ));

	scaleTree_r( p, scale );
}

extern "C" EXPORT void FreeVGUISetBounds( void *panel, int x, int y, int wide, int tall )
{
	vgui::Panel *p = (vgui::Panel *)panel;

	if( !p )
		return;

	p->setBounds( x, y, wide, tall );
}

extern "C" EXPORT void FreeVGUIGetBounds( void *panel, int *x, int *y, int *wide, int *tall )
{
	vgui::Panel *p = (vgui::Panel *)panel;

	if( !p )
		return;

	p->getPos( *x, *y );
	p->getSize( *wide, *tall );
}

// the physical screen the engine sized the root panel for - the space
// every dialog coordinate lives in
extern "C" EXPORT void FreeVGUIGetScreenSize( int *wide, int *tall )
{
	if( !vgui::g_rootPanel )
	{
		*wide = *tall = 0;
		return;
	}

	vgui::g_rootPanel->getSize( *wide, *tall );
}

// ---------------------------------------------------------------------------
// build 68 additions: page wiring for the original CreateMultiplayerGame
// pages - the runtime visibility GameUI applies over the shipped .res,
// combo defaults and token lookup for the host's own strings
// ---------------------------------------------------------------------------

extern "C" EXPORT void FreeVGUISetVisible( void *panel, int state )
{
	vgui::Panel *p = (vgui::Panel *)panel;

	if( !p )
		return;

	p->setVisible( state != 0 );
	p->repaint();
}

extern "C" EXPORT void FreeVGUIComboSetCurSel( void *combo, int index )
{
	vgui::ComboBox *cb = (vgui::ComboBox *)combo;

	if( cb )
		cb->setCurrentItem( index );
}

extern "C" EXPORT const char *FreeVGUIGetLocalizedString( const char *token )
{
	return vgui::res::resolveLabel( token );
}

// ---------------------------------------------------------------------------
// A7 build 69 additions: the original GameUI main menu. The shipped
// GameMenu.res carries only labels and commands - CGameMenu/CGameMenuItem
// built the rows in code, stacked them over the BackgroundLayout art and
// lit the hovered row with the CGameMenuItemHighlight overlay. Recreated
// here the same way: rows are Buttons whose background paints the
// ClientScheme SelectionBG highlight (255 176 0) at 0.8 alpha while the
// cursor is over them. Layout constants come from the reference recording
// (1920x1010 client area, measured per pixel) and scale by screen height.
// ---------------------------------------------------------------------------

namespace vgui
{
namespace res
{

// design-space geometry measured off the reference recording's frames
#define GM_DESIGN_W        1920
#define GM_DESIGN_H        1010
#define GM_RANDOM_Y          0
#define GM_RANDOM_H         44
#define GM_RANDOM_FONTSCALE 1.5f
#define GM_LIST_Y           61
#define GM_ITEM_H           34
#define GM_PITCH            40
#define GM_LOGO_X            6
#define GM_LOGO_Y          242
#define GM_LOGO_W          294
#define GM_LOGO_H           36
#define GM_FRIENDS_X      1735
#define GM_FRIENDS_Y       195
#define GM_FRIENDS_W       100
#define GM_FRIENDS_H        28

#define GM_FLAG_ONLYINGAME  1
#define GM_FLAG_NOTMULTI    2
#define GM_FLAG_NOTSINGLE   4

// one row of the main menu: white text, the orange SelectionBG bar on
// hover (the original CGameMenuItemHighlight owned by every item)
class GameMenuItem : public Button
{
public:
	GameMenuItem( const char *text, int x, int y, int wide, int tall ):
		Button( text, x, y, wide, tall ),
		m_Flags( 0 )
	{
		setButtonBorderEnabled( false );
		setPaintEnabled( true );
		setPaintBackgroundEnabled( true );
		setFgColor( 255, 255, 255, 0 );
		setTextAlignment( Label::LEFT );
		addInputSignal( new ItemHover( this ));
	}

	void setCommand( const char *cmd ) { m_Command = cmd ? cmd : ""; }
	void setFlags( int flags ) { m_Flags = flags; }

	virtual void paintBackground() override
	{
		if( !isArmed( ))
			return;

		int wide, tall;

		getPaintSize( wide, tall );

		// the original highlight: SelectionBG behind the label,
		// CGameMenuItem drew it through its owned overlay at 0.8 alpha
		drawSetColor( 255, 176, 0, 80 );
		drawFilledRect( 0, 0, wide, tall );
	}

private:
	// hover: the original armed the item whenever the cursor entered it
	class ItemHover : public InputSignalAdapter
	{
	public:
		ItemHover( GameMenuItem *item ): m_Item( item )
		{
		}

		virtual void cursorEntered( Panel * ) override
		{
			m_Item->setArmed( true );
			m_Item->repaint( );
		}

		virtual void cursorExited( Panel * ) override
		{
			m_Item->setArmed( false );
			m_Item->repaint( );
		}

	private:
		GameMenuItem *m_Item;
	};

	std::string m_Command;
	int m_Flags;
};

// one BackgroundLayout.txt art tile (and the game logo): uploaded once,
// stretched over its panel the way CBitmapImagePanel drew them
class ArtPanel : public Panel
{
public:
	ArtPanel( const unsigned char *rgba, int texWide, int texTall,
		int x, int y, int wide, int tall ):
		Panel( x, y, wide, tall ),
		m_pRGBA( new char[texWide * texTall * 4] ),
		m_iTexWide( texWide ),
		m_iTexTall( texTall ),
		m_bUploaded( false )
	{
		// Texture IDs belong to the engine-side VGUI texture table.  Do not
		// use private/arbitrary IDs here: VGUI_UploadTexture validates that
		// the ID was returned by GenerateTexture(), and an unregistered ID
		// can also make the following bind dereference a null texture table.
		m_iTexID = g_surface ? g_surface->createNewTextureID() : 0;
		if( m_iTexID <= 0 )
		{
			vgui_dprintf( "freevgui: failed to allocate art texture %dx%d\n",
				texWide, texTall );
			setPaintBackgroundEnabled( false );
		}

		memcpy( m_pRGBA, rgba, (size_t)texWide * texTall * 4 );

		setPaintEnabled( false );
		setPaintBorderEnabled( false );
		setPaintBackgroundEnabled( true );
	}

	virtual ~ArtPanel()
	{
		delete[] m_pRGBA;
	}

	virtual void paintBackground() override
	{
		if( m_iTexID <= 0 )
			return;

		int wide, tall;

		getPaintSize( wide, tall );

		// upload on first paint: the surface is only guaranteed then
		if( !m_bUploaded )
		{
			drawSetTextureRGBA( m_iTexID, m_pRGBA, m_iTexWide, m_iTexTall );
			m_bUploaded = true;
		}

		drawSetTexture( m_iTexID );
		// VGUI alpha is inverted: 0 is opaque and 255 is transparent.
		// ArtPanel does not go through Bitmap::paint(), so it must establish
		// the image colour itself before drawing the textured quad.
		drawSetColor( 255, 255, 255, 0 );
		drawTexturedRect( 0, 0, wide, tall );
	}

private:
	char *m_pRGBA;
	int m_iTexWide, m_iTexTall, m_iTexID;
	bool m_bUploaded;
};

// decode a 32-bit TGA the engine handed us (menu art, game logo)
static FileImage *LoadEngineTGA( const char *path )
{
	if( !g_engine || !g_engine->LoadFile )
		return NULL;

	int size = 0;
	char *buf = (char *)g_engine->LoadFile( path, &size );

	if( !buf || size <= 0 )
	{
		if( buf && g_engine->FreeFile )
			g_engine->FreeFile( buf );
		return NULL;
	}

	FileImageStream_Memory stream( buf, size );
	FileImage *image = new FileImage;

	bool ok = Load32BitTGA( &stream, image );

	if( g_engine->FreeFile )
		g_engine->FreeFile( buf );

	if( !ok )
	{
		delete image;
		return NULL;
	}

	return image;
}

// whitespace token reader for BackgroundLayout.txt: rows are bare
// token sets ("path flag x y"), not keyvalues pairs
static bool layoutNextToken( const char **p, char *out, int outSize )
{
	const char *s = *p;

	while( *s && (unsigned char)*s <= ' ' )
		s++;

	if( !*s )
	{
		*p = s;
		return false;
	}

	int i = 0;

	while( *s && (unsigned char)*s > ' ' )
	{
		if( i < outSize - 1 )
			out[i++] = *s;
		s++;
	}

	out[i] = 0;
	*p = s;

	return true;
}

// the fullscreen art behind the main menu: resource/BackgroundLayout.txt
// lists the shipped background tiles at design coordinates; every
// "scaled" tile stretches by screen width over the design resolution,
// exactly like the original CBasePanel laid the loading art out (the
// bottom row crops on non-16:9 screens, as it did on PC)
static Panel *buildBackground( void )
{
	int size = 0;
	char *buf = (char *)g_engine->LoadFile( "resource/BackgroundLayout.txt", &size );

	if( !buf || size <= 0 )
	{
		if( buf )
			g_engine->FreeFile( buf );
		return NULL;
	}

	std::string text( buf, size );

	g_engine->FreeFile( buf );

	int screenW = 0, screenH = 0;

	g_rootPanel->getSize( screenW, screenH );

	if( screenW <= 0 || screenH <= 0 )
		return NULL;

	Panel *root = new Panel( 0, 0, screenW, screenH );

	root->setPaintEnabled( false );
	root->setPaintBackgroundEnabled( false );
	root->setPaintBorderEnabled( false );

	const char *p = text.c_str( );
	char token[512];
	int designW = 1280;
	float scale = (float)screenW / (float)designW;

	while( layoutNextToken( &p, token, sizeof( token )))
	{
		if( !ricmp( token, "resolution" ))
		{
			char dim[32];

			if( layoutNextToken( &p, dim, sizeof( dim )))
				designW = atoi( dim );
			if( layoutNextToken( &p, dim, sizeof( dim )))
				(void)atoi( dim ); // design height: tiles carry it

			if( designW > 0 )
				scale = (float)screenW / (float)designW;

			continue;
		}

		// art row: path, "scaled" flag, design x, design y
		std::string path = token;
		char flag[64], sx[32], sy[32];

		if( !layoutNextToken( &p, flag, sizeof( flag )))
			break;
		if( !layoutNextToken( &p, sx, sizeof( sx )))
			break;
		if( !layoutNextToken( &p, sy, sizeof( sy )))
			break;

		FileImage *img = LoadEngineTGA( path.c_str( ));

		if( !img )
			continue;

		bool scaled = !ricmp( flag, "scaled" );

		int x = (int)( atoi( sx ) * scale + 0.5f );
		int y = (int)( atoi( sy ) * scale + 0.5f );
		int w = scaled ? (int)( img->m_Width * scale + 0.5f ) : img->m_Width;
		int h = scaled ? (int)( img->m_Height * scale + 0.5f ) : img->m_Height;

		ArtPanel *tile = new ArtPanel( img->m_pData, img->m_Width, img->m_Height,
			x, y, w, h );

		delete img;

		root->addChild( tile );
	}

	if( root->getChildCount( ) == 0 )
	{
		delete root;
		return NULL;
	}

	return root;
}

// rebuild the ClientScheme Default font at a scale - the "Random
// server" caption carries 1.5x size in the recording, which comes from
// the newer GameUI build's code, not from any shipped file
static Font *buildScaledDefaultFont( float scale )
{
	int size = 0;
	char *buf = (char *)g_engine->LoadFile( "resource/ClientScheme.res", &size );

	if( !buf || size <= 0 )
	{
		if( buf )
			g_engine->FreeFile( buf );
		return NULL;
	}

	KVNode root;
	KVReader reader( buf, size );
	bool ok = reader.parseRoot( root );

	g_engine->FreeFile( buf );

	if( !ok )
		return NULL;

	const KVNode *fonts = NULL;

	for( size_t i = 0; i < root.children.size( ); i++ )
	{
		if( !ricmp( root.children[i].name.c_str( ), "Fonts" ))
			fonts = &root.children[i];
	}

	if( !fonts )
		return NULL;

	const KVNode *def = NULL;

	for( size_t i = 0; i < fonts->children.size( ); i++ )
	{
		if( !ricmp( fonts->children[i].name.c_str( ), "Default" ))
			def = &fonts->children[i];
	}

	if( !def )
		return NULL;

	int wide = 0, tall = 0;

	g_rootPanel->getSize( wide, tall );

	const KVNode *entry = pickFontEntry( *def, tall ? tall : 480 );

	if( !entry )
		return NULL;

	const std::string *face = entry->find( "name" );
	int tallF = kvInt( *entry, "tall", 12 );
	int weight = kvInt( *entry, "weight", 400 );

	return new Font( face ? face->c_str( ) : "Verdana",
		(int)( tallF * scale + 0.5f ), 0, 0.0f, weight, false, false, false, false );
}

// per-menu row registry the host-facing exports read back
struct GameMenuRow
{
	Panel *panel;
	std::string command;
	int flags;
};

static std::map<Panel *, std::vector<GameMenuRow>> &menuRows( void )
{
	static std::map<Panel *, std::vector<GameMenuRow>> rows;

	return rows;
}

void forgetMenuRows( Panel *p )
{
	menuRows( ).erase( p );
}

// GameMenu.res -> the main menu: rows in numeric key order stacked at
// the recorded geometry, the quick-join caption on top, the game logo
// and the Friends caption placed like CBasePanel placed them
static Panel *buildGameMenu( void )
{
	int size = 0;
	char *buf = (char *)g_engine->LoadFile( "resource/GameMenu.res", &size );

	if( !buf || size <= 0 )
	{
		if( buf )
			g_engine->FreeFile( buf );
		return NULL;
	}

	KVNode root;
	KVReader reader( buf, size );
	bool ok = reader.parseRoot( root );

	g_engine->FreeFile( buf );

	if( !ok )
		return NULL;

	// parseRoot already consumed the outer "GameMenu" block. Its children
	// are numbered rows, not another GameMenu wrapper.
	const KVNode *menuBlock = !ricmp( root.name.c_str(), "GameMenu" ) ? &root : NULL;

	for( size_t i = 0; i < root.children.size( ); i++ )
	{
		if( !ricmp( root.children[i].name.c_str( ), "GameMenu" ))
			menuBlock = &root.children[i];
	}

	if( !menuBlock )
		return NULL;

	// numeric key order, the way the original menu carried them
	std::vector<const KVNode *> entries;

	for( size_t i = 0; i < menuBlock->children.size( ); i++ )
		entries.push_back( &menuBlock->children[i] );

	std::stable_sort( entries.begin( ), entries.end( ),
		[]( const KVNode *a, const KVNode *b )
		{
			return atoi( a->name.c_str( )) < atoi( b->name.c_str( ));
		});

	Panel *menu = new Panel( 0, 0, GM_DESIGN_W, GM_DESIGN_H );

	menu->setPaintEnabled( false );
	menu->setPaintBackgroundEnabled( false );
	menu->setPaintBorderEnabled( false );

	std::vector<GameMenuRow> rows;
	// GameMenuItem is created before any dialog controls and must use the same
	// scalable face as the original GameUI rows. Keep this explicit rather
	// than relying on a platform's default scheme slot (which is Marlett on
	// some VGUI2 builds and is only suitable for checkbox glyphs).
	// Use the shipped Tahoma face directly for these rows. The menu is
	// created before any dialog scheme is consumed, and the cstrike bundle
	// carries this face at gfx/fonts/tahoma.ttf.
	Font *menuFont = new Font( "Tahoma", 20, 0, 0.0f, 600,
		false, false, false, false );

	// the quick-join caption above the list: a newer-build code
	// addition, recreated here in code the same way
	{
		Font *bigFont = buildScaledDefaultFont( GM_RANDOM_FONTSCALE );

		GameMenuItem *random = new GameMenuItem( "Random server", 0,
			GM_RANDOM_Y, 1, GM_RANDOM_H );

		if( bigFont )
			random->setFont( bigFont );

		random->setCommand( "RandomServer" );

		menu->addChild( random );

		GameMenuRow row = { random, "RandomServer", 0 };
		rows.push_back( row );
	}

	int y = GM_LIST_Y;

	for( size_t i = 0; i < entries.size( ); i++ )
	{
		const KVNode &n = *entries[i];

		const std::string *labelKey = n.find( "label" );
		const std::string *cmdKey = n.find( "command" );

		const char *label = resolveLabel( labelKey ? labelKey->c_str( ) : "" );
		const char *command = cmdKey ? cmdKey->c_str( ) : "";

		int flags = 0;

		if( kvInt( n, "OnlyInGame", 0 )) flags |= GM_FLAG_ONLYINGAME;
		if( kvInt( n, "notmulti", 0 )) flags |= GM_FLAG_NOTMULTI;
		if( kvInt( n, "notsingle", 0 )) flags |= GM_FLAG_NOTSINGLE;

		Panel *rowPanel;

		if( !label[0] && !command[0] )
		{
			// the empty OnlyInGame entry is the group divider
			rowPanel = new DividerPanel( 0, y + GM_ITEM_H / 2, 1, 2 );
		}
		else
		{
			GameMenuItem *item = new GameMenuItem( label, 0, y, 1, GM_ITEM_H );

			if( menuFont )
				item->setFont( menuFont );

			item->setCommand( command );

			rowPanel = item;
		}

		menu->addChild( rowPanel );

		GameMenuRow row = { rowPanel, command, flags };
		rows.push_back( row );

		y += GM_PITCH;
	}

	// every row shares the width of the widest label: the highlight
	// bar spans the whole row like the CGameMenuItemHighlight did
	int maxTextWide = 0;

	for( size_t i = 0; i < rows.size( ); i++ )
	{
		Button *item = dynamic_cast<Button *>( rows[i].panel );

		if( !item )
			continue;

		int tw = 0, th = 0;

		item->getContentSize( tw, th );

		if( tw > maxTextWide )
			maxTextWide = tw;
	}

	for( size_t i = 0; i < rows.size( ); i++ )
	{
		int rx, ry, rw, rh;

		rows[i].panel->getPos( rx, ry );
		rows[i].panel->getSize( rw, rh );
		rows[i].panel->setBounds( rx, ry, maxTextWide + 2, rh );
	}

	// the game logo over the quit row
	{
		FileImage *logo = LoadEngineTGA( "resource/logo_game.tga" );

		if( logo )
		{
			// The shipped texture has transparent padding. Fit the visible
			// artwork to the logo rectangle without changing its pixels.
			int left = logo->m_Width, top = logo->m_Height, right = 0, bottom = 0;
			for( int y = 0; y < logo->m_Height; y++ )
				for( int x = 0; x < logo->m_Width; x++ )
					if( logo->m_pData[( y * logo->m_Width + x ) * 4 + 3] )
					{
						left = std::min( left, x );
						top = std::min( top, y );
						right = std::max( right, x + 1 );
						bottom = std::max( bottom, y + 1 );
					}

			if( right > left && bottom > top )
			{
				int w = right - left, h = bottom - top;
				std::vector<unsigned char> pixels( w * h * 4 );
				for( int y = 0; y < h; y++ )
					memcpy( &pixels[y * w * 4],
						&logo->m_pData[(( y + top ) * logo->m_Width + left ) * 4], w * 4 );
				ArtPanel *art = new ArtPanel( pixels.data(), w, h,
					GM_LOGO_X, GM_LOGO_Y, GM_LOGO_W, GM_LOGO_H );
				menu->addChild( art );
			}

			delete logo;
		}
	}

	// the Friends caption on the right edge: Steam friends on PC, a
	// caption here until its Android counterpart lands
	{
		Label *friends = new Label( "Friends", GM_FRIENDS_X, GM_FRIENDS_Y,
			GM_FRIENDS_W, GM_FRIENDS_H );

		friends->setFgColor( 255, 255, 255, 0 );
		friends->setPaintBackgroundEnabled( false );

		menu->addChild( friends );
	}

	menuRows( )[menu] = rows;

	return menu;
}

} // namespace res
} // namespace vgui

// the art behind the main menu, built from the shipped
// BackgroundLayout.txt + resource/background tiles (A7 build 69)
extern "C" EXPORT void *FreeVGUILoadBackground( void *parent )
{
	if( !vgui::g_engine || !vgui::g_engine->LoadFile || !vgui::g_rootPanel )
		return NULL;

	vgui::Panel *host = parent ? (vgui::Panel *)parent : vgui::g_rootPanel;
	vgui::Panel *bg = vgui::res::buildBackground( );

	if( !bg )
		return NULL;

	// first in z-order: the menu and dialogs draw over the art
	host->addChild( bg );

	return bg;
}

// the main menu list from resource/GameMenu.res (A7 build 69)
extern "C" EXPORT void *FreeVGUIGameMenuLoad( void *parent )
{
	if( !vgui::g_engine || !vgui::g_engine->LoadFile || !vgui::g_rootPanel )
		return NULL;

	vgui::Panel *host = parent ? (vgui::Panel *)parent : vgui::g_rootPanel;
	vgui::Panel *menu = vgui::res::buildGameMenu( );

	if( !menu )
		return NULL;

	host->addChild( menu );

	return menu;
}

extern "C" EXPORT int FreeVGUIGameMenuCount( void *menu )
{
	std::map<vgui::Panel *, std::vector<vgui::res::GameMenuRow>>::iterator it =
		vgui::res::menuRows( ).find(( vgui::Panel *)menu );

	return it != vgui::res::menuRows( ).end( ) ? (int)it->second.size( ) : 0;
}

extern "C" EXPORT void *FreeVGUIGameMenuItem( void *menu, int index )
{
	std::map<vgui::Panel *, std::vector<vgui::res::GameMenuRow>>::iterator it =
		vgui::res::menuRows( ).find(( vgui::Panel *)menu );

	if( it == vgui::res::menuRows( ).end( ) || index < 0 || index >= (int)it->second.size( ))
		return NULL;

	return it->second[index].panel;
}

extern "C" EXPORT const char *FreeVGUIGameMenuCommand( void *menu, int index )
{
	std::map<vgui::Panel *, std::vector<vgui::res::GameMenuRow>>::iterator it =
		vgui::res::menuRows( ).find(( vgui::Panel *)menu );

	if( it == vgui::res::menuRows( ).end( ) || index < 0 || index >= (int)it->second.size( ))
		return NULL;

	return it->second[index].command.c_str( );
}

extern "C" EXPORT int FreeVGUIGameMenuFlags( void *menu, int index )
{
	std::map<vgui::Panel *, std::vector<vgui::res::GameMenuRow>>::iterator it =
		vgui::res::menuRows( ).find(( vgui::Panel *)menu );

	if( it == vgui::res::menuRows( ).end( ) || index < 0 || index >= (int)it->second.size( ))
		return 0;

	return it->second[index].flags;
}
