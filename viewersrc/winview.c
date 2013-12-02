/* winview.c */

/* Win32 GIF viewer using InterGif sources
 * (K) All Rites Reversed -- Copy What You Like
 *
 */

#include "windows.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "../src/animlib.h"
#include "resource.h"

HINSTANCE hInst; /* Instance handle (like a task handle) */

HBITMAP hbmTools = NULL, hbmTools2;

char szAppName[] = "InterGifViewer";

#define APPNAME "winview"

typedef struct view_ *view;


    /*================*
     *   Play tools   *
     *================*/

#define class_TOOLS "igviewertools"
#define tools_X 119
#define tools_Y 31

static const int buttons[6] = { 0, 19, 19+27, 19+27+27, 19+27+27+27, 19+27+27+27+19 };

typedef struct playtools_ *playtools;

playtools PlayTools_New( view v, HWND parent, int xsize, int ysize );
void PlayTools_Destroy( playtools* );


    /*===================*
     *   View handling   *
     *===================*/


/* Linked list of views */

typedef struct view_ {
    //struct view_ *pNext;
    HWND         hwnd;
    HWND         hwndTools;
    anim         a;
    char         *filename;
    char         *wintitle;
    BITMAPINFO   *pDib;
    void         *pDibBits;
    int          nFrame;
    HPALETTE     hPal;
    playtools    pt;
} viewstr;

int nViews = 0;

#if 0
view views = NULL;
view View_Find( HWND h )
{
    view v = views;

    while ( v )
    {
        if ( v->hwnd == h )
            return v;
        v = v->pNext;
    }
    return NULL;
}
#endif

// Foward declarations of functions included in this code module:

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ToolsWndProc(HWND, UINT, WPARAM, LPARAM);

void View_CreateDib( view v )
{
    unsigned int x = v->a->nWidth;
    unsigned int y = v->a->nHeight;
    unsigned int abw = (x+3) & (~3);
    BITMAPINFOHEADER bmih;
    unsigned int *pPal;
    palette p = v->a->pFrames[0].pal;
    unsigned int i;
    union {
        struct { WORD w; WORD w2; PALETTEENTRY p[256]; } padding;
        LOGPALETTE logpal;
    } lp;

    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = x;
    bmih.biHeight = -((int)y);
    bmih.biPlanes = 1;
    bmih.biBitCount = 8;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = y*abw;
    bmih.biXPelsPerMeter = bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 256;
    bmih.biClrImportant = 0;

    v->pDib = (BITMAPINFO*) malloc( sizeof(bmih) + 256*4 + bmih.biSizeImage );
    v->pDibBits = ((char*)v->pDib) + sizeof(bmih) + 256*4;

    memcpy( v->pDib, &bmih, sizeof(bmih) );

    pPal = (unsigned int*)(v->pDib->bmiColors);

    for ( i=0; i<p->nColours; i++ )
    {
        RGBQUAD *r = (RGBQUAD*)(pPal + i);
        unsigned int c = p->pColours[i];

        r->rgbBlue = c>>24;
        r->rgbGreen = c>>16;
        r->rgbRed = c>>8;
        r->rgbReserved = 0;
    }

    lp.logpal.palVersion = 0x300;
    lp.logpal.palNumEntries = p->nColours;
    for ( i=0; i < lp.logpal.palNumEntries; i++ )
    {
        unsigned int c = p->pColours[i];

        lp.logpal.palPalEntry[i].peRed = c>>8;
        lp.logpal.palPalEntry[i].peGreen = c>>16;
        lp.logpal.palPalEntry[i].peBlue = c>>24;
        lp.logpal.palPalEntry[i].peFlags = PC_NOCOLLAPSE;
    }
    v->hPal = CreatePalette( &lp.logpal );
}

void View_PlotFrame( view v, int f )
{
    anim a = v->a;
    framestr *pFrame;

    if ( !a || !v->pDib )
        return;

    pFrame = a->pFrames + f;

    Anim_DecompressAligned( pFrame->pImageData, pFrame->nImageSize,
                            a->nWidth, a->nHeight, v->pDibBits );
}

void View_Goto( view v, int frame )
{
    if ( v->nFrame != frame && v->a )
    {
        if ( !v->pDib )
            View_CreateDib(v);
        View_PlotFrame( v, frame );
        v->nFrame = frame;
        if ( v->hwnd )
            UpdateWindow( v->hwnd );
    }
}

BOOL View_New( const char *fname, int nCmdShow )
{
    view v = (view)calloc( sizeof(viewstr), 1 );
    unsigned int x,y;
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD dwExStyle = WS_EX_ACCEPTFILES;// | WS_EX_CLIENTEDGE;
    RECT r;
    const char *leaf, *leaf2;

    if ( !v )
        return FALSE;

    leaf = strrchr( fname, '\\' );
    leaf2 = strrchr( fname, '/' );
    if ( leaf2 > leaf )
        leaf = leaf2;

    if ( leaf )
        leaf++;
    else
        leaf = fname;

    nViews++;

    v->nFrame = -1;
    v->filename = strdup( fname );
    v->a = Anim_FromFile( fname, NULL );
    if ( v->a && Anim_CommonPalette(v->a) )
    {
        x = v->a->nWidth;
        y = v->a->nHeight;
    }
    else
    {
        MessageBox( NULL, Anim_Error, "InterGif Viewer", MB_ICONWARNING | MB_OK );
        x = y = 300;
    }

    View_Goto(v,0);

    if ( v->a && v->a->nFrames > 1 && x < tools_X )
        x = tools_X;

    v->wintitle = malloc( strlen(leaf)+50 );

    if ( v->a->nFrames > 1 )
        sprintf( v->wintitle, "%s: %dx%d, %d frames", leaf, v->a->nWidth, v->a->nHeight, v->a->nFrames );
    else
        sprintf( v->wintitle, "%s: %dx%d", leaf, v->a->nWidth, v->a->nHeight );

    v->hwnd = CreateWindowEx( dwExStyle, szAppName, v->wintitle, dwStyle,
		                      CW_USEDEFAULT, 0, x, y,
		                      NULL, NULL, hInst, NULL);
    if ( !v->hwnd )
        return FALSE;

    SetWindowLong( v->hwnd, GWL_USERDATA, (LONG)v );

    if ( v->a->nFrames > 1 )
        v->pt = PlayTools_New( v, v->hwnd, x, y );

    GetClientRect( v->hwnd, &r );
    x += (x - r.right);
    y += (y - r.bottom);

    if ( v->a->nFrames > 1 )
        y += tools_Y;

	SetWindowPos( v->hwnd, NULL, 0,0,x,y, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER );

	ShowWindow(v->hwnd, nCmdShow);
	UpdateWindow(v->hwnd);

    return TRUE;
}

void View_Destroy( view v )
{
    if ( v->hPal )
        DeleteObject( (HGDIOBJ) v->hPal );
    free( v->pDib );
    free( v->filename );
    free( v->wintitle );
    DestroyWindow( v->hwnd );
    PlayTools_Destroy( &v->pt );
    free( v );

    nViews--;
}

void View_RealizePalette( view v, BOOL bForceBackground )
{
    BOOL redraw = FALSE;
    HDC hdc = GetDC( v->hwnd );

    if ( GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE )
    {
        HPALETTE oldpal = SelectPalette( hdc, v->hPal, bForceBackground );

        redraw = ( RealizePalette( hdc ) > 0 );

        SelectPalette( hdc, oldpal, TRUE );
    }

    ReleaseDC( v->hwnd, hdc );

    if ( bForceBackground || redraw )
        InvalidateRect( v->hwnd, NULL, FALSE );
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
    view v = (view)GetWindowLong( hWnd, GWL_USERDATA );

	switch (message) {

		case WM_DISPLAYCHANGE:
		{
#if 0
            {
			    SIZE szScreen;
			    WORD cBitsPerPixel = (WORD)wParam;
                char buf[80];

			    szScreen.cx = LOWORD(lParam);
			    szScreen.cy = HIWORD(lParam);

                sprintf( buf, "Display changed to %dx%dx%dbpp", szScreen.cx, szScreen.cy, cBitsPerPixel );
			
		    	MessageBox (GetFocus(), buf, szAppName, 0);
            }
#endif

            View_RealizePalette( v, TRUE );
		}
		break;

		case WM_PAINT:
			hdc = BeginPaint (hWnd, &ps);
			if ( v && v->pDib )
            {
                int x = v->a->nWidth;
                int y = v->a->nHeight;
                HPALETTE hOldPal = SelectPalette( hdc, v->hPal, TRUE );

                SetDIBitsToDevice( hdc,
								   0,0,x,y,
                                   0,0,0,y,v->pDibBits,v->pDib, DIB_RGB_COLORS );

                if ( hOldPal )
                    SelectPalette( hdc, hOldPal, TRUE );
            }
			EndPaint (hWnd, &ps);
			break;     

        case WM_DROPFILES:
            {
                HDROP hDrop = (HDROP) wParam;
                char filename[MAX_PATH];
                UINT n = DragQueryFile( hDrop, (UINT)-1, NULL, MAX_PATH );
                UINT i;

                for ( i=0; i<n; i++ )
                {
                    DragQueryFile( hDrop, i, filename, MAX_PATH );
                    View_New( filename, SW_SHOW );
                }
                DragFinish( hDrop );
                return 0;
            }
            break;
            
        case WM_PALETTECHANGED:
            if ( v->hPal )
            {
                HWND hwndFocus = (HWND)wParam;
                if ( hwndFocus != v->hwnd )
                    View_RealizePalette( v, TRUE );
            }
            else
                return DefWindowProc( hWnd, message, wParam, lParam );
            break;

        case WM_QUERYNEWPALETTE:
            if ( v->hPal )
            {
                View_RealizePalette( v, FALSE );
            }
            else
                return DefWindowProc( hWnd, message, wParam, lParam );
            break;

        case WM_CLOSE:
            if ( v )
                View_Destroy(v);
            if ( !nViews )
			    PostQuitMessage(0);
            break;

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));
	}
	return (0);
}

typedef struct playtools_ {
    view v;
    int nButton;
} playtoolsstr;

LRESULT CALLBACK ToolsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
    BOOL ok = TRUE;
    playtools p = (playtools) GetWindowLong( hWnd, GWL_USERDATA );

	switch (message) {
		case WM_PAINT:
			hdc = BeginPaint (hWnd, &ps);
            /* Paint... */
            {
                HDC hdc2 = CreateCompatibleDC( hdc );
                HBITMAP hbmold = SelectObject( hdc2, hbmTools );
                int n = p->nButton;
                int left = (n>0) ? buttons[n] : 0;
                int right = (n>0) ? buttons[n+1] : 0;

                if ( n > 0 )
                    BitBlt( hdc, 0,0,left,tools_Y, hdc2, 0,0, SRCCOPY );

                if ( n > 0 )
                {
                    SelectObject( hdc2, hbmTools2 );
                    BitBlt( hdc, left, 0, right-left, tools_Y, hdc2,
                            left,0,SRCCOPY );
                }

                if ( n != 4 )
                {
                    SelectObject( hdc2, hbmTools );
                    BitBlt( hdc, right, 0, tools_X - right, tools_Y, hdc2,
                            right,0,SRCCOPY );
                }

                SelectObject( hdc2, hbmold );
                DeleteDC( hdc2 );
            }
			EndPaint (hWnd, &ps);
			break;

        case WM_MEASUREITEM:
            {
                UINT id = (UINT) wParam;
                LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT) lParam;
                lpmis->itemHeight = tools_Y;
                lpmis->itemWidth = buttons[id+1] - buttons[id];
                return TRUE;
            }
            break;

        case WM_DRAWITEM:
            {
                int id = (int) wParam;
                LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT) lParam;
                HDC hdc2 = CreateCompatibleDC( lpdis->hDC );
                HBITMAP hbmold;
                BOOL selected = (lpdis->itemState & ODS_SELECTED) != 0;

                if ( (id==1 || id==3) && !selected && p->nButton == id )
                    selected = TRUE;
                
                hbmold = SelectObject( hdc2,
                    selected ? hbmTools2 : hbmTools );

                ok = BitBlt( lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                                    lpdis->rcItem.right - lpdis->rcItem.left,
                                    lpdis->rcItem.bottom - lpdis->rcItem.top,
                                    hdc2,
                                    buttons[id], 0, SRCCOPY );
                SelectObject( hdc2, hbmold );
                DeleteDC( hdc2 );

                if ( selected )
                    p->nButton = id;
                else if ( p->nButton == id )
                    p->nButton = -1;
                return TRUE;
            }
            break;

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));
	}
	return (0);
}

playtools PlayTools_New( view v, HWND hParent, int x, int y )
{
    HWND hTools = CreateWindow( class_TOOLS, "", WS_CHILD | WS_VISIBLE,
                                     0, y, x, tools_Y, hParent, (HMENU)0, hInst, NULL );
    UINT i;
    playtools p = (playtools)calloc( sizeof(playtoolsstr), 1 );

    for ( i=0; i<5; i++ )
    {
        CreateWindow( "BUTTON", "", BS_OWNERDRAW | WS_CHILD | WS_VISIBLE,
                      buttons[i],0,buttons[i+1]-buttons[i],tools_Y, hTools,
                      (HMENU)i, hInst, NULL );
    }

    SetWindowLong( hTools, GWL_USERDATA, (LONG)v );
    p->v = v;
    p->nButton = -1;
    return p;
}

void PlayTools_Destroy( playtools *pp )
{
    /* Window will be destroyed when its parent is */
    if ( *pp )
    {
        free( *pp );
        *pp = NULL;
    }
}

/* Initialisation */

BOOL ViewerInit(HINSTANCE hInstance, int nCmdShow, const char *fname)
{
    WNDCLASS  wc;
    HWND      hwnd;

    // Win32 will always set hPrevInstance to NULL, so lets check
    // things a little closer. This is because we only want a single
    // version of this app to run at a time
    hwnd = FindWindow (szAppName, NULL);
    if (hwnd) {
        // We found another version of ourself. Lets defer to it:
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
        SetForegroundWindow (hwnd);

        // If this app actually had any functionality, we would
        // also want to communicate any action that our 'twin'
        // should now perform based on how the user tried to
        // execute us.
        return FALSE;
    }

    // Fill in window class structure with parameters that describe
    // the main window.
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon (hInstance, szAppName);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(1+COLOR_WINDOW);

    wc.lpszClassName = szAppName;

    RegisterClass( &wc );

    wc.lpfnWndProc = (WNDPROC)ToolsWndProc;
    wc.hbrBackground = (HBRUSH)(1+COLOR_BTNFACE);
    wc.lpszClassName = class_TOOLS;

    RegisterClass(&wc);

	hInst = hInstance; // Store instance handle in our global variable

    hbmTools = LoadImage( hInst, MAKEINTRESOURCE(IDB_TOOLS), IMAGE_BITMAP, tools_X, tools_Y,
                          /*LR_CREATEDIBSECTION |*/ LR_LOADMAP3DCOLORS );

    hbmTools2 = LoadImage( hInst, MAKEINTRESOURCE(IDB_TOOLS1), IMAGE_BITMAP, tools_X, tools_Y,
                           /*LR_CREATEDIBSECTION |*/ LR_LOADMAP3DCOLORS );

    if ( !View_New( fname, nCmdShow ) )
        return FALSE;

	return TRUE;
}

/* Entry point */

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	if ( !ViewerInit(hInstance, nCmdShow, lpCmdLine) )
		return (FALSE);

	while (GetMessage(&msg, NULL, 0, 0))
    {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (msg.wParam);
}

/* eof */
