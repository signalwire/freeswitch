/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Nikolay Popok mailto: <chaser@soft-industry.com>
 */

#include "stdafx.h"
#include "libzrtp_test_GUI.h"
#include <windows.h>
#include <commctrl.h>

//#include "resourcesp.h"

#include <Afx.h>


extern "C" 
{
	#include "zrtp.h"
	#include "zrtp_test_core.h"
}

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE			g_hInst;			// current instance
HWND				g_hWndMenuBar;		// menu bar handle
HWND				hWndList;
HWND				g_hWnd;

HFONT				font;

int action_id = 0;

static void do_action();
static void print_log_ce(int level, const char *data, int len, int offset);
static DWORD WINAPI destroy_func(void *param);

static FILE *log_file = NULL;

typedef struct zrtp_test_command
{		
	int32_t					code;
	int						first_conn;
	int						last_conn;
	uint32_t				extra, extra2;
} zrtp_test_command_t;

typedef enum zrtp_test_code
{
	ZRTP_TEST_CREATE = 0,
	ZRTP_TEST_DESTROY,
	ZRTP_TEST_ZSTART,
	ZRTP_TEST_ZSECURE,
	ZRTP_TEST_QUIT,
	ZRTP_TEST_INC,
	ZRTP_TEST_DEC,
	ZRTP_TEST_CLEAR,	
	ZRTP_TEST_SLEEP,
	ZRTP_TEST_LOGS,
	ZRTP_TEST_INFO,
	ZRTP_TEST_HELP,
	ZRTP_TEST_CMD_SIZE
} zrtp_test_code_t;

extern "C" {
	void do_quit();
	int zrtp_test_zrtp_init();
	void zrtp_test_crypto(zrtp_global_t* zrtp);
	int zrtp_add_system_state(zrtp_global_t* zrtp, MD_CTX *ctx);
}

// Forward declarations of functions included in this code module:
ATOM			MyRegisterClass(HINSTANCE, LPTSTR);
BOOL			InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
#ifndef WIN32_PLATFORM_WFSP
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
#endif // !WIN32_PLATFORM_WFSP

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
	MSG msg;

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) 
	{
		return FALSE;
	}

#ifndef WIN32_PLATFORM_WFSP
	HACCEL hAccelTable;
	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LIBZRTP_TEST_GUI));
#endif // !WIN32_PLATFORM_WFSP

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if ( msg.message == WM_KEYDOWN )
		{
			switch ( msg.wParam )  
			{
				case VK_LEFT:
				{
					msg.wParam = 0;
					break;
				}
				case VK_RIGHT:
				{
					msg.wParam = 0;
					break;
				}
			}
		}

#ifndef WIN32_PLATFORM_WFSP
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
#endif // !WIN32_PLATFORM_WFSP
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
	WNDCLASS wc;

	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LIBZRTP_TEST_GUI));
	wc.hCursor       = 0;
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = szWindowClass;

	return RegisterClass(&wc);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;
    TCHAR szTitle[MAX_LOADSTRING];		// title bar text
    TCHAR szWindowClass[MAX_LOADSTRING];	// main window class name

    g_hInst = hInstance; // Store instance handle in our global variable

#if defined(WIN32_PLATFORM_PSPC) || defined(WIN32_PLATFORM_WFSP)
    // SHInitExtraControls should be called once during your application's initialization to initialize any
    // of the device specific controls such as CAPEDIT and SIPPREF.
    SHInitExtraControls();
#endif // WIN32_PLATFORM_PSPC || WIN32_PLATFORM_WFSP

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING); 
    LoadString(hInstance, IDC_LIBZRTP_TEST_GUI, szWindowClass, MAX_LOADSTRING);

#if defined(WIN32_PLATFORM_PSPC) || defined(WIN32_PLATFORM_WFSP)
    //If it is already running, then focus on the window, and exit
    hWnd = FindWindow(szWindowClass, szTitle);	
    if (hWnd) 
    {
        // set focus to foremost child window
        // The "| 0x00000001" is used to bring any owned windows to the foreground and
        // activate them.
        SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
        return 0;
    } 
#endif // WIN32_PLATFORM_PSPC || WIN32_PLATFORM_WFSP

    if (!MyRegisterClass(hInstance, szWindowClass))
    {
    	return FALSE;
    }

    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

	g_hWnd = hWnd;
#ifdef WIN32_PLATFORM_PSPC
    // When the main window is created using CW_USEDEFAULT the height of the menubar (if one
    // is created is not taken into account). So we resize the window after creating it
    // if a menubar is present
    if (g_hWndMenuBar)
    {
        RECT rc;
        RECT rcMenuBar;

        GetWindowRect(hWnd, &rc);
        GetWindowRect(g_hWndMenuBar, &rcMenuBar);
        rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
		
        MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
    }
#endif // WIN32_PLATFORM_PSPC

    ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

#if defined(SHELL_AYGSHELL) && !defined(WIN32_PLATFORM_WFSP)
    static SHACTIVATEINFO s_sai;
#endif // SHELL_AYGSHELL && !WIN32_PLATFORM_WFSP
	
    switch (message) 
    {
		case WM_COMMAND:
            wmId    = LOWORD(wParam); 
            wmEvent = HIWORD(wParam); 
            // Parse the menu selections:
            switch (wmId)
            {
#ifndef WIN32_PLATFORM_WFSP
                case IDM_HELP_ABOUT:
                    DialogBox(g_hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
                    break;
#endif // !WIN32_PLATFORM_WFSP
#ifdef WIN32_PLATFORM_WFSP
                case IDM_OK:
					do_action();

                    break;
#endif // WIN32_PLATFORM_WFSP
#ifdef WIN32_PLATFORM_PSPC
                case IDM_OK:
					do_action();
                    break;
#endif // WIN32_PLATFORM_PSPC
				default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_CREATE:
#ifdef SHELL_AYGSHELL
            SHMENUBARINFO mbi;

            memset(&mbi, 0, sizeof(SHMENUBARINFO));
            mbi.cbSize     = sizeof(SHMENUBARINFO);
            mbi.hwndParent = hWnd;
            mbi.nToolBarId = IDR_MENU;
            mbi.hInstRes   = g_hInst;

            if (!SHCreateMenuBar(&mbi)) 
            {
                g_hWndMenuBar = NULL;
            }
            else
            {
                g_hWndMenuBar = mbi.hwndMB;
            }

#ifndef WIN32_PLATFORM_WFSP
            // Initialize the shell activate info structure
            memset(&s_sai, 0, sizeof (s_sai));
            s_sai.cbSize = sizeof (s_sai);
#endif // !WIN32_PLATFORM_WFSP
#endif // SHELL_AYGSHELL

#ifdef WIN32_PLATFORM_WFSP
			hWndList = CreateWindow(TEXT("listbox"),NULL, WS_CHILD|
				WS_VISIBLE|WS_HSCROLL|WS_VSCROLL|WS_TABSTOP, 0,0, 300, 200, hWnd, 
				(HMENU)"", g_hInst, NULL);			
#else
			hWndList = CreateWindow(TEXT("listbox"),NULL, WS_CHILD|
				WS_VISIBLE|WS_HSCROLL|WS_VSCROLL|WS_TABSTOP, 0,0, 250, 200, hWnd, 
				(HMENU)"", g_hInst, NULL);			
#endif // !WIN32_PLATFORM_WFSP
			
			font = CreateFont(10,           // height of font
                       0,            // average character width
                       0,               // angle of escapement
                       0,               // base-line orientation angle
                       400,     // font weight
                       0,               // italic attribute option
                       FALSE,                // underline attribute option
                       FALSE,                // strikeout attribute option
                       ANSI_CHARSET,         // character set identifier
                       OUT_DEFAULT_PRECIS,   // output precision
                       CLIP_DEFAULT_PRECIS,  // clipping precision
                       ANTIALIASED_QUALITY,  // output quality
                       FF_DONTCARE,          // pitch and family
					   TEXT("Times New Roman"));       

			SendMessage(hWndList, WM_SETFONT, (WPARAM)font, (LPARAM)TRUE);

			SetFocus(hWndList);
			break;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY:
#ifdef SHELL_AYGSHELL
            CommandBar_Destroy(g_hWndMenuBar);
#endif // SHELL_AYGSHELL
            PostQuitMessage(0);
            break;

#if defined(SHELL_AYGSHELL) && !defined(WIN32_PLATFORM_WFSP)
        case WM_ACTIVATE:
            // Notify shell of our activate message
            SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);
            break;
        case WM_SETTINGCHANGE:
            SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
            break;
#endif // SHELL_AYGSHELL && !WIN32_PLATFORM_WFSP

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HWND hwndAbout = NULL;

#ifndef WIN32_PLATFORM_WFSP
// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
#ifdef SHELL_AYGSHELL
            {
                // Create a Done button and size it.  
                SHINITDLGINFO shidi;
                shidi.dwMask = SHIDIM_FLAGS;
                shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN | SHIDIF_EMPTYMENU;
                shidi.hDlg = hDlg;
                SHInitDialog(&shidi);
            }
#endif // SHELL_AYGSHELL

            return (INT_PTR)TRUE;

        case WM_COMMAND:
#ifdef SHELL_AYGSHELL
            if (LOWORD(wParam) == IDOK)
#endif
            {
                return (INT_PTR)TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, message);
            return (INT_PTR)TRUE;

#ifdef _DEVICE_RESOLUTION_AWARE
        case WM_SIZE:
            {
		DRA::RelayoutDialog(
			g_hInst, 
			hDlg, 
			DRA::GetDisplayMode() != DRA::Portrait ? MAKEINTRESOURCE(IDD_ABOUTBOX_WIDE) : MAKEINTRESOURCE(IDD_ABOUTBOX));
            }
            break;
#endif
    }
    return (INT_PTR)FALSE;
}
#endif // !WIN32_PLATFORM_WFSP

static void do_action()
{
	switch (action_id)
	{
		case 0:
		{
			int status;

			zrtp_log_set_log_engine((zrtp_log_engine*)print_log_ce); 
			status = zrtp_test_zrtp_init();
			if (0 != status) {
				return;
			}

			zrtp_test_crypto(zrtp_global);

			{
				zrtp_test_channel_id_t id;
				zrtp_test_channel_config_t sconfig;
				
				sconfig.is_autosecure = 0;
				sconfig.is_preshared  = 0;
				sconfig.streams_count = 1;
				
				status = zrtp_test_channel_create(&sconfig, &id);
				
				if (0 == status) {
					zrtp_test_channel_start(id);
				}
			}
			break;
		}
		case 1:
		{
			zrtp_thread_create(destroy_func, NULL);
			break;
		}
		case 2:
		{
			DeleteObject(font);
			if (log_file) fclose(log_file);
#ifdef WIN32_PLATFORM_WFSP
            DestroyWindow(g_hWnd);
#endif
#ifdef WIN32_PLATFORM_PSPC
			SendMessage(g_hWnd, WM_CLOSE, 0, 0);				
#endif // WIN32_PLATFORM_PSPC
			break;
		}
	}

	action_id++;
}

static DWORD WINAPI destroy_func(void *param)
{
	do_quit();
	return 0;
}
                        
static void print_log_ce(int level, const char *data, int len, int offset)
{
	if ( !log_file )
		log_file = fopen("zrtp_test.log", "a");

	fprintf(log_file, "%s", data);
	
	if (level < 3 || 1)
	{
		CString strUnicode = data;
		SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)strUnicode.GetBuffer(100));
		SendMessage(hWndList, WM_VSCROLL, SB_BOTTOM, 0L);
	}
	
}

