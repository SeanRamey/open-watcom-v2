/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <wwindows.h>
#include <ddeml.h>
#include "wedit.h"
#include "dllmain.h"


/* Local Window callback functions prototypes */
WINEXPORT HDDEDATA CALLBACK VIW_DdeCallback( UINT wType, UINT wFmt, HCONV hConv, HSZ hszTopic, HSZ hszItem,
                                            HDDEDATA hData, ULONG_PTR dwData1, ULONG_PTR dwData2 );

static  HCONV       dde_hConv;
static  DWORD       idInstance;
static  HINSTANCE   hInstance;
static  BOOL        bConnected = FALSE;
static  FARPROC     VIW_DdeCallback_inst;

static BOOL doReset( void )
{
    // reset for another connect
    dde_hConv = 0;
    bConnected = FALSE;
    return( TRUE );
}

static char doRequest( char *szCommand )
{
    HDDEDATA    hddeData;
    HSZ         hszCommand;
    BYTE        result;

    hszCommand = DdeCreateStringHandle( idInstance, szCommand, CP_WINANSI );
    hddeData = DdeClientTransaction( NULL, 0, dde_hConv, hszCommand, CF_TEXT, XTYP_REQUEST, 500000, NULL );
    DdeFreeStringHandle( idInstance, hszCommand );

    if( hddeData != (HDDEDATA)NULL ) {
        DdeGetData( hddeData, &result, 1, 0 );
        DdeFreeDataHandle( hddeData );
        return( result );
    }
    return( 0 );

}

HDDEDATA CALLBACK VIW_DdeCallback( UINT wType, UINT wFmt, HCONV hConv, HSZ hszTopic, HSZ hszItem,
                                        HDDEDATA hData, ULONG_PTR dwData1, ULONG_PTR dwData2 )
{
    wFmt = wFmt;
    hConv = hConv;
    hszTopic = hszTopic;
    hszItem = hszItem;
    hData = hData;
    dwData1 = dwData1;
    dwData2 = dwData2;

    switch( wType ) {
        case XTYP_DISCONNECT:
            // user may have killed vi, or maybe this is a response to
            // our ddeuninitialize.  either way, reset for next connect
            if( bConnected ) {
                doReset();
            }
            return( NULL );
    }
    return( NULL );
}

int EDITAPI EDITConnect( void )
{
    char    *szProg = "VIW.EXE";
    char    szCommandLine[ 80 ];
    char    *szService = "VIW";     // arbitrary - we pass these to
    char    *szTopic = "Edit";      // viw on command line & it registers
    HSZ     hszService, hszTopic;   // under their names
    int     rc;

    if( bConnected ) {
        return( TRUE );
    }


    // initialize our idInstance in ddeml
    if( idInstance == 0 ) {
        VIW_DdeCallback_inst = MakeProcInstance( (FARPROC)VIW_DdeCallback, hInstance );
        if( DdeInitialize( &idInstance, (PFNCALLBACK)VIW_DdeCallback_inst, APPCMD_CLIENTONLY, 0L ) != DMLERR_NO_ERROR ) {
            FreeProcInstance( VIW_DdeCallback_inst );
            return( FALSE );
        }
    }

    // get handles to access strings
    hszService = DdeCreateStringHandle( idInstance, (LPSTR)szService, CP_WINANSI );
    hszTopic= DdeCreateStringHandle( idInstance, (LPSTR)szTopic, CP_WINANSI );

    // attempt connection
    dde_hConv = DdeConnect( idInstance, hszService, hszTopic, (PCONVCONTEXT)NULL );
    if( dde_hConv == 0 ) {
        // run editor (magically grabs focus)
        sprintf( szCommandLine, "%s -s ddesinit.vi -p \"%s %s\"", szProg, szService, szTopic );
        // ddesinit.vi will now add an ide-activate button to the toolbar
        // this button will NOT be saved by saveconfig
#ifdef __NT__
        {
            STARTUPINFO         si;
            PROCESS_INFORMATION pi;
            int                 n;

            // editor is up - try again (otherwise give up)
            memset( &si, 0, sizeof( si ) );
            si.cb = sizeof( si );
            rc = CreateProcess( NULL, szCommandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi );
            if( rc ) {
                rc = WaitForInputIdle( pi.hProcess, INFINITE );
                if( rc == 0 ) {
                    // Now this is starting to get scary
                    // if some problem to get connection then try in loop 100 times (maximum about 30 seconds)
                    for( n = 0; n < 100; ++n ) {
                        DWORD   status;

                        dde_hConv = DdeConnect( idInstance, hszService, hszTopic, (PCONVCONTEXT)NULL );
                        if( dde_hConv != 0 ) {
                            break;
                        }
                        GetExitCodeProcess( pi.hProcess, &status );
                        if( status != STILL_ACTIVE ) {
                            break;
                        }
                        Sleep( 250 );   // sleep 0.25 second
                    }
                }
                /*
                //  Carl Young 2004-01-27
                //  Close handles as they are no longer needed and nobody has been passed them anyway
                */
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
#else
        rc = WinExec( (LPSTR)szCommandLine, SW_RESTORE );
        if( rc >= 32 ) {
            dde_hConv = DdeConnect( idInstance, hszService, hszTopic, (PCONVCONTEXT) NULL );
        }
#endif
    }

    DdeFreeStringHandle( idInstance, hszService );
    DdeFreeStringHandle( idInstance, hszTopic );

    if( dde_hConv != 0 ) {
        bConnected = TRUE;
    }

    return( bConnected );
}

int EDITAPI EDITFile( LPSTR szFile, LPSTR szHelpFile )
{
    char        szCommand[ _MAX_PATH + 15 ];

    // don't handle help files yet
    szHelpFile = szHelpFile;
    if( !bConnected ) {
        return( FALSE );
    }

    if( strlen( szFile ) > _MAX_PATH ) {
        return( FALSE );
    }
    sprintf( szCommand, "EditFile %s", szFile );

    return( doRequest( szCommand ) != 0 );
}

int EDITAPI EDITLocateError( long lRow, int iCol,
                                int iLen, int idResource , LPSTR szErrmsg )
{
    char        szCommand[ 100 ];
    BOOL        rc;

    if( !bConnected ) {
        return( FALSE );
    }

    sprintf( szCommand, "Locate %ld %d %d", lRow, iCol, iLen );
    rc = doRequest( szCommand );

    // can't lookup info in help file yet
    idResource = idResource;

    if( szErrmsg != NULL ) {
        rc |= doRequest( "echo on" );
        sprintf( szCommand, "echo 1 \"%.*s\"", (int)( sizeof( szCommand ) - 10 ),  szErrmsg );
        rc |= doRequest( szCommand );
    }
    return( rc );
}

int EDITAPI EDITLocate( long iRow, int iCol, int iLen )
{
    return( EDITLocateError( iRow, iCol, iLen, 0, NULL ) );
}

int EDITAPI EDITShowWindow( int iCmdShow )
{
    if( !bConnected ) {
        return( FALSE );
    }

    // ensure iCmdShow is SW_RESTORE or SW_MINIMIZE only
    if( iCmdShow == EDIT_RESTORE || iCmdShow == EDIT_SHOWNORMAL ) {
        return( doRequest( "Restore" ) != 0 );
    } else if( iCmdShow == EDIT_MINIMIZE ) {
        return( doRequest( "Minimize" ) != 0 );
    }

    // bad argument
    return( FALSE );
}

int EDITAPI EDITDisconnect( void )
{
    if( idInstance != 0 ) {
        DdeUninitialize( idInstance );
        FreeProcInstance( VIW_DdeCallback_inst );
        idInstance = 0;
    }
    doReset();
    return( TRUE );
}

int EDITAPI EDITSaveFiles( void )
{
    if( !bConnected ) {
        return( FALSE );
    }
    return( doRequest( "PromptForSave" ) != 0 );
}

int EDITAPI EDITSaveThisFile( const char *filename )
{
    char szCommand[ _MAX_PATH + 25 ];
    if( !bConnected ) {
        return( TRUE );
    }
    sprintf( szCommand, "PromptThisFileForSave %s", filename );
    return( doRequest( szCommand ) != 0 );
}

int EDITAPI EDITQueryThisFile( const char *filename )
{
    char szCommand[ _MAX_PATH + 15 ];
    if( !bConnected ) {
        return( FALSE );
    }
    sprintf( szCommand, "QueryFile %s", filename );
    return( doRequest( szCommand ) );
}

#ifdef __NT__

BOOL WINAPI DllMain( HINSTANCE hDll, DWORD reason, LPVOID res )
{
    res = res;
    reason = reason;

    hInstance = hDll;

    return( 1 );
}

#else

int WINAPI LibMain( HINSTANCE hInst, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCmdLine )
{
    wDataSeg = wDataSeg;
    wHeapSize = wHeapSize;
    lpszCmdLine = lpszCmdLine;

    hInstance = hInst;

    return( 1 );
}

int WINAPI WEP( int q )
{
    q = q;
    return( 1);
}

#endif
