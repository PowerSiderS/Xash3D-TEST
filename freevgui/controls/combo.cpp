// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026
// combo.cpp - ComboBox with a dropdown list (A7 build 66)
//
// The original VGUI2 ComboBox is a field with an arrow button that pops
// a list underneath. Here the list and its click catcher are docked as
// the last children of the top-level dialog panel, which gives them the
// same stacking a popup surface had: painted and hit-tested on top of
// every page control without needing a separate popup surface.

#include <string.h>

#include "controls/combo.h"
#include "app.h"
#include "font.h"
#include "signals.h"

using namespace vgui;

// one dropdown entry: tells its ComboBox which item got picked
class ComboBox::ItemButton : public Button, public ActionSignal
{
public:
	ItemButton( ComboBox *cb, int index, int x, int y, int wide, int tall ) :
		Button( "", x, y, wide, tall ), cb( cb ), index( index )
	{
		setButtonBorderEnabled( false );
		addActionSignal( this );
	}

	virtual void actionPerformed( Panel * ) override
	{
		cb->setCurrentItem( index );
		cb->closeDropdown();
	}

private:
	ComboBox *cb;
	int index;
};

ComboBox::ComboBox( const char *text, int x, int y, int wide, int tall ) :
	Panel( x, y, wide, tall ), current( -1 ), itemCount( 0 ),
	dropdownOpen( false ), dropCatch( nullptr ), dropList( nullptr ),
	dropItemH( 22 )
{
	memset( currentText, 0, sizeof( currentText ));
	memset( items, 0, sizeof( items ));

	if( text )
		strncpy( currentText, text, sizeof( currentText ) - 1 );

	face = new Button( "", 0, 0, wide - 15, tall );
	face->setButtonBorderEnabled( false );
	face->addActionSignal( this );
	addChild( face );
}

void ComboBox::actionPerformed( Panel *panel )
{
	if( panel == face )
	{
		if( dropdownOpen )
			closeDropdown();
		else
			openDropdown();
	}
	else if( panel == dropCatch )
	{
		// click anywhere outside the list dismisses it
		closeDropdown();
	}
	// ItemButton handles its own pick inside the nested class
}

void ComboBox::addItem( const char *item )
{
	if( !item || itemCount >= COMBO_MAX_ITEMS )
		return;

	strncpy( items[itemCount], item, sizeof( items[0] ) - 1 );
	items[itemCount][sizeof( items[0] ) - 1] = 0;
	itemCount++;

	// first added item preselects, like the original combos
	if( current < 0 )
		setCurrentItem( 0 );
}

void ComboBox::setCurrentItem( int index )
{
	if( index < 0 || index >= itemCount )
		return;

	current = index;
	strncpy( currentText, items[index], sizeof( currentText ) - 1 );
	currentText[sizeof( currentText ) - 1] = 0;
	repaint();
}

int ComboBox::getCurrentItem( void )
{
	return current;
}

const char *ComboBox::getText( void )
{
	return currentText;
}

void ComboBox::openDropdown( void )
{
	if( itemCount <= 0 )
		return;

	Panel *top = this;

	// dock under the top-level dialog panel
	while( top->getParent())
		top = top->getParent();

	if( top == this )
		return;

	int tw, th;

	top->getSize( tw, th );

	Font *font = getApp()->getScheme()->getFont( Scheme::SF_PRIMARY1 );
	int fontTall = font ? font->getTall() : 12;

	dropItemH = fontTall + 8;
	if( dropItemH < 20 )
		dropItemH = 20;

	if( !dropCatch )
	{
		// full-dialog catcher: any click outside the list closes it
		dropCatch = new Button( "", 0, 0, tw, th );
		dropCatch->setButtonBorderEnabled( false );
		dropCatch->setPaintBackgroundEnabled( false );
		dropCatch->addActionSignal( this );
		top->addChild( dropCatch );
	}

	if( !dropList )
	{
		int cw, ch;

		getSize( cw, ch );

		dropList = new Panel( 0, 0, cw - 15, itemCount * dropItemH );
		top->addChild( dropList );

		for( int i = 0; i < itemCount; i++ )
		{
			ItemButton *item = new ItemButton( this, i, 0, i * dropItemH,
				cw - 15, dropItemH );

			item->setText( items[i] );
			item->setFgColor( Scheme::SC_BLACK );
			dropList->addChild( item );
		}
	}

	// translate the combo's screen position into the top panel
	int sx, sy, tx, ty;
	int cw, ch;

	localToScreen( sx, sy );
	top->localToScreen( tx, ty );
	getSize( cw, ch );

	int listH = itemCount * dropItemH;
	int y = sy - ty + ch;

	// flip above the field when the list would overflow the dialog
	if( y + listH > th )
		y = sy - ty - listH;
	if( y < 0 )
		y = 0;

	dropList->setBounds( sx - tx, y, cw - 15, listH );
	dropList->setVisible( true );
	dropCatch->setVisible( true );
	dropdownOpen = true;
}

void ComboBox::closeDropdown( void )
{
	if( dropList )
		dropList->setVisible( false );

	if( dropCatch )
		dropCatch->setVisible( false );

	dropdownOpen = false;
}

void ComboBox::paintBackground( void )
{
	int wide, tall;

	getPaintSize( wide, tall );

	int faceWide = wide - 15;

	// the field: white, sunken edge
	drawSetColor( Scheme::SC_WHITE );
	drawFilledRect( 0, 0, faceWide, tall );

	drawSetColor( Scheme::SC_BLACK );
	drawOutlinedRect( 0, 0, faceWide, tall );

	drawSetColor( Scheme::SC_WHITE );
	drawFilledRect( 1, 1, faceWide - 1, 2 );
	drawFilledRect( 1, 2, 2, tall - 1 );

	drawSetColor( Scheme::SC_BLACK );
	drawFilledRect( 1, tall - 2, faceWide - 1, tall - 1 );
	drawFilledRect( faceWide - 2, 1, faceWide - 1, tall - 1 );

	// the arrow button: classic grey face, raised edge
	drawSetColor( 192, 192, 192, 255 );
	drawFilledRect( faceWide, 0, wide, tall );

	drawSetColor( Scheme::SC_BLACK );
	drawOutlinedRect( faceWide, 0, wide, tall );

	drawSetColor( Scheme::SC_WHITE );
	drawFilledRect( faceWide + 1, 1, wide - 1, 2 );
	drawFilledRect( faceWide + 1, 2, faceWide + 2, tall - 1 );

	drawSetColor( Scheme::SC_BLACK );
	drawFilledRect( faceWide + 1, tall - 2, wide - 1, tall - 1 );
	drawFilledRect( wide - 2, 1, wide - 1, tall - 1 );

	// down arrow, the Marlett 't' look drawn as a three row pyramid
	int cx = faceWide + 7;
	int mid = tall / 2;

	drawSetColor( Scheme::SC_BLACK );
	drawFilledRect( cx - 2, mid - 2, cx + 3, mid - 1 );
	drawFilledRect( cx - 1, mid - 1, cx + 2, mid );
	drawFilledRect( cx, mid, cx + 1, mid + 1 );

	// field text
	Font *font = getApp()->getScheme()->getFont( Scheme::SF_PRIMARY1 );
	int fontTall = font ? font->getTall() : 12;

	drawSetTextFont( Scheme::SF_PRIMARY1 );
	drawSetTextColor( Scheme::SC_BLACK );
	drawSetTextPos( 4, ( tall - fontTall ) / 2 );
	drawPrintText( currentText, (int)strlen( currentText ));
}
