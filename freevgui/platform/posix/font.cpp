// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2024-2026 Alibek Omarov
// A7 build 66: real system-font backend for POSIX/Android. The original
// VGUI2 rendered text through Windows GDI; here glyphs come from the
// TTF files the engine already ships (gfx/fonts/tahoma.ttf and
// gfx/fonts/FiraSans-Regular.ttf from xash-extras) plus the game's own
// valve/resource/marlett.ttf - the VGUI2 glyph font - resolved by font
// name the same way mainui_cpp's FontManager resolves them, through the
// engine filesystem (game dir first).

#include <string.h>
#include <stdlib.h>

#include "platform/common/font.h"
#include "platform/xash3d-fwgs/support.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

using namespace vgui;

// font name -> game-relative ttf path (mirrors mainui FontManager, plus
// the Marlett glyph font CS 1.6 itself ships in valve/resource)
static const char *FontPlat_ResolvePath( const char *name )
{
	if( name && !strcmp( name, "Tahoma" ))
		return "gfx/fonts/tahoma.ttf";

	if( name && !strcmp( name, "Marlett" ))
		return "valve/resource/marlett.ttf";

	// the original scheme asks for Verdana; tahoma is the closest face
	// the engine ships, use it as the metric stand-in
	if( name && !strcmp( name, "Verdana" ))
		return "gfx/fonts/tahoma.ttf";

	// everything else ("Trebuchet MS", "Arial", "Default", ...) rides
	// the fwgs UI font
	return "gfx/fonts/FiraSans-Regular.ttf";
}

class FontPlat_STB : public BaseFontPlat
{
public:
	// the engine filesystem API is bound only after this shared object is
	// loaded, and the default Scheme is built from a static constructor, so
	// the face itself is resolved on first use instead of here
	FontPlat_STB( const char *name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol )
	{
		this->name = new char[strlen( name ) + 1];
		strcpy( this->name, name );
		this->tallPx = tall;
		this->weight = weight;
		this->italic = italic;
		this->underline = underline;
		this->strikeout = strikeout;
		this->symbol = symbol;
		this->unusedWide = wide;
		this->unusedRotation = rotation;
		this->scale = 0.0f;
		this->ascentPx = 0;
		this->usable = false;
		this->loadState = LOAD_PENDING;
		this->ttf = NULL;
		memset( &info, 0, sizeof( info ));

		// nothing else to do here: see ensureLoaded
	}

	// load states: pending = no attempt yet (engine FS not bound at
	// construction), done = face usable, failed = gave up for good
	enum
	{
		LOAD_PENDING,
		LOAD_DONE,
		LOAD_FAILED,
	};

	// resolve the TTF now, once the engine can serve files
	bool ensureLoaded( void )
	{
		if( loadState == LOAD_DONE )
			return true;
		if( loadState == LOAD_FAILED )
			return false;
		if( !g_engine || !g_engine->LoadFile )
			return false; // still not bound, keep waiting

		// a zero-height font (the default scheme placeholder) has no scale
		if( tallPx > 0 && ( tryFace( FontPlat_ResolvePath( name )) ||
			tryFace( "gfx/fonts/FiraSans-Regular.ttf" )))
		{
			loadState = LOAD_DONE;
			usable = true;
			return true;
		}

		loadState = LOAD_FAILED;
		usable = false;
		vgui_dprintf( "vgui: failed to load face \"%s\" %dpx weight %d\n",
			name, tallPx, weight );
		return false;
	}

	// true once the face has been read successfully
	bool usable;

	virtual ~FontPlat_STB()
	{
		if( ttf && g_engine && g_engine->FreeFile )
			g_engine->FreeFile( ttf );
		delete[] name;
	}

	virtual bool equals( const char *n, int t, int, float, int wt, bool it, bool ul,
		bool so, bool sy ) override
	{
		// fonts are matched by name + metrics, like any other platform backend
		return !strcmp( name, n ) && tallPx == t && weight == wt &&
			italic == it && underline == ul && strikeout == so && symbol == sy;
	}

	virtual void getCharRGBA( int ch, int x, int y, int wide, int tall, unsigned char *rgba ) override
	{
		if( !ensureLoaded( ))
			return;

		int ax, ay, aw, ah;

		stbtt_GetCodepointBitmapBox( &info, ch, scale, scale, &ax, &ay, &aw, &ah );

		int gw = aw - ax, gh = ah - ay;

		if( gw <= 0 || gh <= 0 )
			return;

		unsigned char *glyph = (unsigned char *)malloc( gw * gh );
		if( !glyph )
			return;

		stbtt_MakeCodepointBitmap( &info, glyph, gw, gh, gw, scale, scale, ch );

		// the atlas cell starts at the glyph's left edge (the 'a' offset
		// is applied by drawPrintText at draw time, not here); the cell
		// top is the ascent line, stb's bitmap box y grows down from the
		// baseline
		int ox = x;
		int oy = y + ascentPx + ay;

		for( int gy = 0; gy < gh; gy++ )
		{
			int outY = oy + gy;
			if( outY < 0 || outY >= tall )
				continue;

			for( int gx = 0; gx < gw; gx++ )
			{
				int outX = ox + gx;

				if( outX < 0 || outX >= wide )
					continue;

				// white glyph + coverage alpha, tinted by the draw color
				// at composite time, same convention as FontPlat_Bitmap
				unsigned char *dst = &rgba[( outY * wide + outX ) * 4 ];
				dst[0] = dst[1] = dst[2] = 255;
				dst[3] = glyph[gy * gw + gx];
			}
		}

		free( glyph );
	}

	virtual void getCharABCwide( int ch, int &a, int &b, int &c ) override
	{
		if( !ensureLoaded( ))
		{
			a = c = 0;
			b = tallPx / 2;
			return;
		}

		int advance, lsb;
		int ax, ay, aw, ah;

		stbtt_GetCodepointHMetrics( &info, ch, &advance, &lsb );
		stbtt_GetCodepointBitmapBox( &info, ch, scale, scale, &ax, &ay, &aw, &ah );

		a = (int)( lsb * scale + 0.5f );
		b = aw - ax;
		c = (int)( advance * scale + 0.5f ) - a - b;

		if( b < 0 ) b = 0;
		if( c < 0 ) c = 0;
	}

	virtual int getTall() override
	{
		return tallPx;
	}

	virtual int getWide() override
	{
		// no fixed cell width for a scalable font; drawPrintText advances
		// by ABC per glyph, so a nominal value is enough here
		return tallPx;
	}

	virtual void drawSetTextFont( SurfacePlat * ) override
	{
		// texturing goes through the surface font cache, nothing to bind
	}

private:
	// read one TTF through the engine filesystem and take the metrics from it
	bool tryFace( const char *path )
	{
		int len = 0;

		if( ttf )
		{
			g_engine->FreeFile( ttf );
			ttf = NULL;
		}

		unsigned char *buf = (unsigned char *)g_engine->LoadFile( path, &len );

		if( !buf || len <= 0 )
			return false;

		if( !stbtt_InitFont( &info, buf, stbtt_GetFontOffsetForIndex( buf, 0 )))
		{
			g_engine->FreeFile( buf );
			return false;
		}

		ttf = buf;

		int ascent, descent, lineGap;

		scale = stbtt_ScaleForPixelHeight( &info, (float)tallPx );
		stbtt_GetFontVMetrics( &info, &ascent, &descent, &lineGap );
		ascentPx = (int)( ascent * scale + 0.5f );

		return true;
	}

	char *name;
	unsigned char *ttf; // engine-owned buffer, freed in the dtor
	stbtt_fontinfo info;
	float scale;
	int ascentPx;
	int tallPx;
	int weight;
	int loadState;
	bool italic, underline, strikeout, symbol;
	int unusedWide;
	float unusedRotation;
};

BaseFontPlat *vgui::FontPlat_CreateSystem( const char *name, int tall, int wide,
	float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	// Always hand back the scalable backend, even when the face cannot be
	// read yet: the default Scheme fonts are built from a static constructor
	// before the engine filesystem exists, and returning NULL there used to
	// put a debug box font into the shared font cache for good.
	return new FontPlat_STB( name, tall, wide, rotation, weight,
		italic, underline, strikeout, symbol );
}
