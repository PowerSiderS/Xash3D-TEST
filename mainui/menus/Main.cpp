/*
*
*	This file is part of CS16Client.
*
*	CS16Client is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	CS16Client is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with CS16Client.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "Action.h"
#include "BtnsBMPTable.h"

#define ART_MINIMIZE_N "gfx/shell/min_n.tga"
#define ART_MINIMIZE_F "gfx/shell/min_f.tga"
#define ART_MINIMIZE_D "gfx/shell/min_d.tga"

#define ART_CLOSEBTN_N "gfx/shell/cls_n.tga"
#define ART_CLOSEBTN_F "gfx/shell/cls_f.tga"
#define ART_CLOSEBTN_D "gfx/shell/cls_d.tga"

class CMenuMain: public CMenuFramework
{
public:
	CMenuMain() : CMenuFramework( "CMenuMain" ) { }

	bool KeyDown( int key ) override;

private:
	void _Init() override;
	void _VidInit( ) override;

	void QuitDialog( void *pExtra = NULL );
	void DisconnectDialogCb();
	void ViewReadmeCb();

	CMenuPicButton	console;
	class CMenuMainBanner : public CMenuBannerBitmap
	{
	public:
		virtual void Draw();
	} banner;

	CMenuPicButton	resumeGame;
	CMenuPicButton	disconnect;
	CMenuPicButton	createGame;
	CMenuPicButton	multiPlayer;
	CMenuPicButton	configuration;
	CMenuPicButton	viewReadme;
	CMenuPicButton	quit;

	// quit dialog
	CMenuYesNoMessageBox dialog;
};

static CMenuMain uiMain;

void CMenuMain::CMenuMainBanner::Draw()
{
	if( !uiMain.background.ShouldDrawLogoMovie() )
		return; // no logos for steam background

	if( EngFuncs::GetLogoLength() <= 0.05f || EngFuncs::GetLogoWidth() <= 32 )
		return;	// don't draw stub logo (GoldSrc rules)

	float	logoWidth, logoHeight, logoPosY;
	float	scaleX, scaleY;

	scaleX = ScreenWidth / 640.0f;
	scaleY = ScreenHeight / 480.0f;

	// a1ba: multiply by height scale to look better on widescreens
	logoWidth = EngFuncs::GetLogoWidth() * scaleX;
	logoHeight = EngFuncs::GetLogoHeight() * scaleY * uiStatic.scaleY;
	logoPosY = 70 * scaleY * uiStatic.scaleY;	// 70 it's empirically determined value (magic number)

	EngFuncs::DrawLogo( "logo.avi", 0, logoPosY, logoWidth, logoHeight );
}

void CMenuMain::QuitDialog(void *pExtra)
{
	if( CL_IsActive() && EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) == 1.0f )
		dialog.SetMessage( L( "StringsList_235" ) );
	else
		dialog.SetMessage( L( "StringsList_236" ) );

	dialog.onPositive.SetCommand( FALSE, "quit\n" );
	dialog.Show();
}

void CMenuMain::DisconnectDialogCb()
{
	dialog.onPositive.SetCommand( FALSE, "cmd disconnect;endgame disconnect;wait;wait;wait;menu_options;menu_main\n" );
	dialog.SetMessage( L( "Really disconnect?" ) );
	dialog.Show();
}

void CMenuMain::ViewReadmeCb()
{
	if( EngFuncs::FileExists( "readme.txt", 1 ) )
		EngFuncs::ShellExecute( "readme.txt", NULL, false );
	else if( EngFuncs::FileExists( "manual/manual.htm", 1 ) )
		EngFuncs::ShellExecute( "manual/manual.htm", NULL, false );
	else
		EngFuncs::ShellExecute( "cstrike/readme.txt", NULL, false );
}

/*
=================
CMenuMain::Key
=================
*/
bool CMenuMain::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
	{
		if ( CL_IsActive( ))
		{
			if( !dialog.IsVisible() )
				UI_CloseMenu();
		}
		else
		{
			QuitDialog( );
		}
		return true;
	}
	return CMenuFramework::KeyDown( key );
}

void CMenuMain::_Init( void )
{
	// console
	console.SetNameAndStatus( L( "GameUI_Console" ), L( "Show console" ) );
	console.iFlags |= QMF_NOTIFY;
	console.SetPicture( PC_CONSOLE );
	SET_EVENT_MULTI( console.onReleased,
	{
		UI_SetActiveMenu( FALSE );
		EngFuncs::KEY_SetDest( KEY_CONSOLE );
	});

	resumeGame.SetNameAndStatus( L( "GameUI_GameMenu_ResumeGame" ), L( "StringsList_188" ) );
	resumeGame.SetPicture( PC_RESUME_GAME );
	resumeGame.iFlags |= QMF_NOTIFY;
	resumeGame.onReleased = UI_CloseMenu;

	disconnect.SetNameAndStatus( L( "GameUI_GameMenu_Disconnect" ), L( "Disconnect from server" ) );
	disconnect.SetPicture( PC_DISCONNECT );
	disconnect.iFlags |= QMF_NOTIFY;
	disconnect.onReleased = VoidCb( &CMenuMain::DisconnectDialogCb );

	createGame.SetNameAndStatus( L( "GameUI_GameMenu_CreateServer" ), L( "Create new LAN game" ) );
	createGame.SetPicture( PC_CREATE_GAME );
	createGame.iFlags |= QMF_NOTIFY;
	createGame.onReleased = UI_CreateGame_Menu;
	if( !EngFuncs::CheckGameDll( ))
		createGame.SetGrayed( true );

	multiPlayer.SetNameAndStatus( L( "GameUI_Multiplayer" ), L( "StringsList_198" ) );
	multiPlayer.SetPicture( PC_MULTIPLAYER );
	multiPlayer.iFlags |= QMF_NOTIFY;
	multiPlayer.onReleased = UI_MultiPlayer_Menu;

	configuration.SetNameAndStatus( L( "GameUI_Options" ), L( "StringsList_193" ) );
	configuration.SetPicture( PC_CONFIG );
	configuration.iFlags |= QMF_NOTIFY;
	configuration.onReleased = UI_Options_Menu;

	viewReadme.SetNameAndStatus( L( "View readme" ), L( "View the game readme file" ) );
	viewReadme.SetPicture( PC_VIEW_README );
	viewReadme.iFlags |= QMF_NOTIFY;
	viewReadme.onReleased = VoidCb( &CMenuMain::ViewReadmeCb );

	quit.SetNameAndStatus( L( "GameUI_GameMenu_Quit" ), L( "StringsList_236" ) );
	quit.SetPicture( PC_QUIT );
	quit.iFlags |= QMF_NOTIFY;
	quit.onReleased = MenuCb( &CMenuMain::QuitDialog );

	dialog.Link( this );

	AddItem( background );
	AddItem( banner );

	AddItem( resumeGame );
	AddItem( disconnect );

	AddItem( console );
	AddItem( createGame );
	AddItem( multiPlayer );
	AddItem( configuration );
	AddItem( viewReadme );
	AddItem( quit );
}

/*
=================
UI_Main_Init
=================
*/
void CMenuMain::_VidInit( void )
{
	bool active = CL_IsActive( ) ? true : false;

	if ( active )
	{
		resumeGame.Show();
		disconnect.Show();
	}
	else
	{
		resumeGame.Hide();
		disconnect.Hide();
	}

	CMenuPicButton::ClearButtonStack();

	// In-game buttons (appear above the main layout when active)
	resumeGame.SetCoord( 24, 250 );
	disconnect.SetCoord( 24, 300 );

	// Fixed layout positions - each button STAYS IN ITS PLACE in the exact requested order:
	// 1: Console
	// 2: Create Game
	// 3: Multiplayer
	// 4: Configuration
	// 5: View readme
	// 6: Quit
	console.SetCoord( 24, 350 );
	createGame.SetCoord( 24, 400 );
	multiPlayer.SetCoord( 24, 450 );
	configuration.SetCoord( 24, 500 );
	viewReadme.SetCoord( 24, 550 );
	quit.SetCoord( 24, 600 );
}

/*
=================
UI_Main_Precache
=================
*/
void UI_Main_Precache( void )
{
#if !(defined(__ANDROID__) || defined(__SAILFISH__))
	EngFuncs::PIC_Load( ART_MINIMIZE_N );
	EngFuncs::PIC_Load( ART_MINIMIZE_F );
	EngFuncs::PIC_Load( ART_MINIMIZE_D );
#endif

	EngFuncs::PIC_Load( ART_CLOSEBTN_N );
	EngFuncs::PIC_Load( ART_CLOSEBTN_F );
	EngFuncs::PIC_Load( ART_CLOSEBTN_D );

	// precache .avi file and get logo width and height
	EngFuncs::PrecacheLogo( "logo.avi" );
}

/*
=================
UI_Main_Menu
=================
*/
void UI_Main_Menu( void )
{
	uiMain.Show();
}
ADD_MENU( menu_main, UI_Main_Precache, UI_Main_Menu );
