/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "Framework.h"
#include "keydefs.h"
#include "Bitmap.h"
#include "Field.h"
#include "CheckBox.h"
#include "PicButton.h"
#include "Table.h"
#include "Action.h"
#include "YesNoMessageBox.h"
#include "Table.h"

#define ART_BANNER		"gfx/shell/head_creategame"

class CMenuMapListModel : public CMenuBaseModel
{
public:
	void Update() override;
	int GetColumns() const override { return 2; }
	int GetRows() const override { return m_iNumItems; }
	const char *GetCellText( int line, int column ) override
	{
		switch( column )
		{
		case 0: return mapName[line];
		case 1: return mapsDescription[line];
		}

		return NULL;
	}

	char		mapName[UI_MAXGAMES][64];
	char		mapsDescription[UI_MAXGAMES][64];
	int	m_iNumItems;
};

class CMenuCreateGame : public CMenuFramework
{
public:
	CMenuCreateGame() : CMenuFramework("CMenuCreateGame") { }
	static void Begin( CMenuBaseItem *pSelf, void *pExtra );

	void Reload( void ) override;

	char		*mapsDescriptionPtr[UI_MAXGAMES];

	CMenuField	maxClients;
	CMenuField	hostName;
	CMenuField	password;
	CMenuCheckBox   nat;
	CMenuCheckBox	hltv;
	CMenuCheckBox	dedicatedServer;

	// newgame prompt dialog
	CMenuYesNoMessageBox msgBox;

	CMenuTable        mapsList;
	CMenuMapListModel mapsListModel;

	CMenuPicButton *done;
private:
	void _Init() override;
	void _VidInit() override;
};

static CMenuCreateGame	uiCreateGame;

// OK while a game is already running: ask for confirmation before dropping it.
// Same thing MakeOpenEvent() did, only it now says so in the log, because a
// click that only opens a dialog used to look exactly like a click that did
// nothing at all
static void CreateGame_OpenExitPrompt( CMenuBaseItem *pSelf, void *pExtra )
{
	Con_Printf( "CreateGame: OK clicked in game, asking to leave the current game first\n" );
	((CMenuYesNoMessageBox *)pExtra)->ToggleVisibility();
}

/*
=================
CMenuCreateGame::Begin
=================
*/
void CMenuCreateGame::Begin( CMenuBaseItem *pSelf, void *pExtra )
{
	CMenuCreateGame *menu = (CMenuCreateGame*)pSelf->Parent();
	int rows = menu->mapsListModel.GetRows();
	int item = menu->mapsList.GetCurrentIndex();

	// every exit out of this function is written to the log now. clicking OK
	// used to be able to do nothing at all without leaving a single line
	// behind, so there was no way to tell which check gave up
	Con_Printf( "CreateGame: start requested, %d map rows, selected item %d\n", rows, item );

	// row 0 is the "Random map" entry, real maps are 1..rows-1
	if( item < 0 || item >= rows )
	{
		Con_Printf( "CreateGame: can't start, selection is out of range (rows %d, item %d)\n", rows, item );
		return;
	}

	const char *mapName;
	if( item == 0 )
	{
		if( rows < 2 )
		{
			Con_Printf( "CreateGame: can't start, the list only has the random entry\n" );
			return; // nothing but the random entry, no maps to pick from
		}

		// RandomLong() is inclusive on both ends, so the upper bound has to be
		// the last real map. GetRows() is one past the end of the list, which
		// gave us an empty name and made the IsMapValid check below bail out.
		int idx = EngFuncs::RandomLong( 1, rows - 1 );
		mapName = menu->mapsListModel.mapName[idx];
	}
	else
	{
		mapName = menu->mapsListModel.mapName[item];
	}

	Con_Printf( "CreateGame: picked map \"%s\"\n", mapName );

	if( EngFuncs::GetCvarFloat( "host_serverstate" ) )
	{
		if(	EngFuncs::GetCvarFloat( "maxplayers" ) == 1.0f )
			EngFuncs::HostEndGame( "end of the game" );
		else
			EngFuncs::HostEndGame( "starting new server" );
	}

	EngFuncs::CvarSetValue( "deathmatch", 1.0f );	// start deathmatch as default
	EngFuncs::CvarSetString( "defaultmap", mapName );
	EngFuncs::CvarSetValue( "sv_nat", EngFuncs::GetCvarFloat( "public" ) ? menu->nat.bChecked : 0 );
	menu->password.WriteCvar();
	menu->hostName.WriteCvar();
	menu->hltv.WriteCvar();
	menu->maxClients.WriteCvar();

	// IsMapValid() picks the spawn entity from maxplayers, so this has to run
	// after the values above are written. Doing it earlier saw maxplayers == 1
	// and looked for info_player_start, while every normal mp map only has
	// info_player_deathmatch.
	if( !EngFuncs::IsMapValid( mapName ))
	{
		Con_Printf( "CreateGame: can't start, bad map \"%s\"\n", mapName );
		return;
	}

	EngFuncs::PlayBackgroundTrack( NULL, NULL );

	// all done, start server
	if( menu->dedicatedServer.bChecked )
	{
		EngFuncs::WriteServerConfig( EngFuncs::GetCvarString( "servercfgfile" ));

		char cmd[128];
		sprintf( cmd, "#%s", gMenu.m_gameinfo.gamefolder );

		Con_Printf( "CreateGame: starting dedicated server \"%s\"\n", cmd );

		// NOTE: dedicated server will be executed "defaultmap"
		// from engine after restarting
		EngFuncs::ChangeInstance( cmd, "Starting dedicated server...\n" );
	}
	else
	{
		EngFuncs::WriteServerConfig( EngFuncs::GetCvarString( "lservercfgfile" ));

		char cmd[1024], cmd2[256];
		sprintf( cmd, "exec %s\n", EngFuncs::GetCvarString( "lservercfgfile" ) );
	
		Con_Printf( "CreateGame: exec listen server config\n" );
		EngFuncs::ClientCmd( TRUE, cmd );

		// dirty listenserver config form old xash may rewrite maxplayers
		EngFuncs::CvarSetValue( "maxplayers", atoi( menu->maxClients.GetBuffer() ));

		Com_EscapeCommand( cmd2, mapName, 256 );

		// hack: wait three frames allowing server to completely shutdown, reapply maxplayers and start new map
		sprintf( cmd, "endgame;menu_connectionprogress localserver;wait;wait;wait;maxplayers %i;latch;map %s\n", atoi( menu->maxClients.GetBuffer() ), cmd2 );

		Con_Printf( "CreateGame: running \"%s\"\n", cmd );
		EngFuncs::ClientCmd( FALSE, cmd );

	}
}

/*
=================
CMenuMapListModel::Update
=================
*/
void CMenuMapListModel::Update( void )
{
	char *afile;

	if( !uiStatic.needMapListUpdate )
		return;

	if( !EngFuncs::CreateMapsList( TRUE ) || (afile = (char *)EngFuncs::COM_LoadFile( "maps.lst", NULL )) == NULL )
	{
		uiCreateGame.done->SetGrayed( true );
		m_iNumItems = 0;
		Con_Printf( "Cmd_GetMapsList: can't open maps.lst\n" );
		Con_Printf( "CreateGame: map list unavailable, OK button disabled\n" );
		return;
	}

	char *pfile = afile;
	char token[1024];
	int numMaps = 1;

	strcpy( mapName[0], L( "GameUI_RandomMap" ) );
	mapsDescription[0][0] = 0;
	
	while(( pfile = EngFuncs::COM_ParseFile( pfile, token )) != NULL )
	{
		if( numMaps >= UI_MAXGAMES ) break;

		Q_strncpy( mapName[numMaps], token, 64 );
		if(( pfile = EngFuncs::COM_ParseFile( pfile, token )) == NULL )
		{
			Q_strncpy( mapsDescription[numMaps], mapName[numMaps], 64 );
			break; // unexpected end of file
		}
		Q_strncpy( mapsDescription[numMaps], token, 64 );
		numMaps++;
	}

	// grey out OK while there is no real map to start, but don't leave it
	// stuck greyed if maps show up on a later refresh
	uiCreateGame.done->SetGrayed( numMaps < 2 );
	m_iNumItems = numMaps;
	Con_Printf( "CreateGame: %d entries in map list, OK button %s\n", numMaps, numMaps < 2 ? "disabled" : "enabled" );
	EngFuncs::COM_FreeFile( afile );
	uiStatic.needMapListUpdate = false;
}

/*
=================
CMenuCreateGame::Init
=================
*/
void CMenuCreateGame::_Init( void )
{
	uiStatic.needMapListUpdate = true;
	banner.SetPicture( ART_BANNER );

	nat.SetNameAndStatus( L( "NAT" ), L( "Use NAT Bypass instead of direct mode" ) );
	nat.bChecked = true;

	dedicatedServer.SetNameAndStatus( L( "Dedicated server" ), L( "faster, but you can't join the server from this machine" ) );

	hltv.SetNameAndStatus( L( "HLTV" ), L( "Enable HLTV mode in Multiplayer" ) );
	hltv.LinkCvar( "hltv" );

	// add them here, so "done" button can be used by mapsListModel::Update
	AddItem( background );
	AddItem( banner );
	CMenuPicButton *advOpt = AddButton( L( "Adv. Options" ), L( "Open the game advanced options menu" ), PC_ADV_OPT, UI_AdvServerOptions_Menu );
	advOpt->SetGrayed( !UI_AdvServerOptions_IsAvailable() );

	done = AddButton( L( "GameUI_OK" ), L( "Start the multiplayer game" ), PC_OK, Begin );
	done->onReleasedClActive = CEventCallback( CreateGame_OpenExitPrompt, &msgBox );

	mapsList.SetCharSize( QM_SMALLFONT );
	mapsList.SetupColumn( 0, L( "GameUI_Map" ), 0.5f );
	mapsList.SetupColumn( 1, L( "Title" ), 0.5f );
	mapsList.SetModel( &mapsListModel );

	hostName.szName = L( "GameUI_ServerName" );
	hostName.iMaxLength = 28;
	hostName.LinkCvar( "hostname" );
	
	maxClients.iMaxLength = 3;
	maxClients.bNumbersOnly = true;
	maxClients.szName = L( "GameUI_MaxPlayers" );
	SET_EVENT_MULTI( maxClients.onChanged,
	{
		CMenuField *self = (CMenuField*)pSelf;
		const char *buf = self->GetBuffer();
		if( buf[0] == 0 ) return;
		
		int players = atoi( buf );
		if( players <= 1 )
			self->SetBuffer( "2" );
		else if( players > 32 )
			self->SetBuffer( "32" );
	});
	SET_EVENT_MULTI( maxClients.onCvarGet,
	{
		CMenuField *self = (CMenuField*)pSelf;
		const char *buf = self->GetBuffer();
		
		int players = atoi( buf );
		if( players <= 1 )
			self->SetBuffer( "16" );
		else if( players > 32 )
			self->SetBuffer( "32" );
	});
	maxClients.LinkCvar( "maxplayers" );

	password.szName = L( "GameUI_Password" );
	password.iMaxLength = 16;
	password.eTextAlignment = QM_CENTER;
	password.bHideInput = true;
	password.LinkCvar( "sv_password" );

	msgBox.onPositive = Begin;
	msgBox.SetMessage( L( "Starting a new game will exit any current game, OK to exit?" ) );
	msgBox.Link( this );

	AddButton( L( "GameUI_Cancel" ), L( "Return to the previous menu" ), PC_CANCEL, VoidCb( &CMenuCreateGame::Hide ) );
	AddItem( hostName );
	AddItem( maxClients );
	AddItem( password );
#if !(defined(__ANDROID__) || defined(__SAILFISH__))
	AddItem( dedicatedServer );
#endif
	// HLTV not yet supported
	//AddItem( hltv );
	AddItem( nat );
	AddItem( mapsList );
}

void CMenuCreateGame::_VidInit()
{
	nat.SetCoord( 72, 585 );
	if( !EngFuncs::GetCvarFloat("public") )
		nat.Hide();
	else nat.Show();

	hltv.SetCoord( 72, 635 );
	dedicatedServer.SetCoord( 72, 685 );

	mapsList.SetRect( 590, 230, -20, 465 );

	hostName.SetRect( 350, 260, 205, 32 );
	maxClients.SetRect( 350, 360, 205, 32 );
	password.SetRect( 350, 460, 205, 32 );
}

void CMenuCreateGame::Reload( void )
{
	mapsListModel.Update();
}

/*
=================
CMenuCreateGame::Precache
=================
*/
void UI_CreateGame_Precache( void )
{
	EngFuncs::PIC_Load( ART_BANNER );
}

/*
=================
CMenuCreateGame::Menu
=================
*/
void UI_CreateGame_Menu( void )
{
	if ( gMenu.m_gameinfo.gamemode == GAME_SINGLEPLAYER_ONLY )
		return;

	uiCreateGame.Show();
}
ADD_MENU( menu_creategame, UI_CreateGame_Precache, UI_CreateGame_Menu );
