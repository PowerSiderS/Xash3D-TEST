// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026
// combo.h - ComboBox: a face field with a dropdown arrow and list, the
// look and touch interaction of the original VGUI2 ComboBox (A7 build 66)

#ifndef VGUI_COMBOBOX_H
#define VGUI_COMBOBOX_H

#include "controls/button.h"

namespace vgui
{
class CLASSEXPORT ComboBox : public Panel, public ActionSignal
{
public:
	ComboBox( const char *text, int x, int y, int wide, int tall );

	// face clicked: toggles the dropdown; the list catcher closes it
	virtual void actionPerformed( Panel * ) override;

	void addItem( const char *item );
	void setCurrentItem( int index );
	int getCurrentItem( void );
	const char *getText( void );

protected:
	virtual void paintBackground() override;

private:
	class ItemButton;

	void openDropdown( void );
	void closeDropdown( void );

	enum { COMBO_MAX_ITEMS = 256 };

	Button  *face;
	char     currentText[256];
	char     items[COMBO_MAX_ITEMS][128];
	int      current;
	int      itemCount;
	bool     dropdownOpen;

	// the list and its click catcher are children of the top-level
	// dialog panel: added after every page control they stack on top
	Button  *dropCatch;
	Panel   *dropList;
	int      dropItemH;
};
}

#endif // VGUI_COMBOBOX_H
