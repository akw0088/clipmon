#define _CRT_SECURE_NO_DEPRECATE
#define _UNICODE
#define UNICODE
#define WIN32_EXTRA_LEAN

#include <windows.h>
#include <stdlib.h>
#include <TCHAR.h>
#include <Commctrl.h>
#include "lglcd.h"
#include "resource.h"
#pragma comment(lib, "lgLcd.lib")

#ifdef _UNICODE
	#define CF_TEXT_T CF_UNICODETEXT
#else
	#define CF_TEXT_T CF_TEXT
#endif

#define NAME_APP		TEXT("clipMon")
#define NAME_LCDMON		TEXT("Logitech G-series Clipboard Monitor")
#define WMU_UPDATE		(WM_APP + 1)
#define WMU_CONFIGURE		(WM_APP + 2)
#define WMU_UPDATEDATA		(WM_APP + 3)
#define TIMER_DELAY		1

typedef struct
{
	BOOL		negateLcd, wordWrap, alert;
	LOGFONT		logfont;
} appData;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI buttonCallback(int, DWORD, VOID *);
DWORD WINAPI configureCallback(INT, VOID *);
BOOL CALLBACK configureProc(HWND, UINT, WPARAM, LPARAM);

BOOL initLcd(HWND, lgLcdConnectContext *, lgLcdDeviceDesc *, lgLcdOpenContext *);
BOOL pickFont(HWND hwnd, CHOOSEFONT *, LOGFONT *);
VOID initData(appData *);
VOID initFont(LOGFONT *);
VOID postError(HWND, INT, TCHAR *);
VOID processString(HDC, BOOL, const lgLcdDeviceDesc *, TCHAR *, INT);
VOID processDrop(HDC, BOOL, const lgLcdDeviceDesc *, HDROP);
INT printLcdString(HDC, BOOL, INT, INT, INT, TCHAR *, INT);
VOID updateLcd(HWND, HBITMAP, lgLcdOpenContext *, BOOL);
VOID convertBitmap(BYTE *, BYTE *, INT);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, INT iCmdShow)
{
	HWND		hwnd;
	MSG		msg;
	WNDCLASS	wndclass;
	const TCHAR	szAppName[] = NAME_APP;

	wndclass.style		= CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc	= WndProc;
	wndclass.cbClsExtra	= 0;
	wndclass.cbWndExtra	= 0;
	wndclass.hInstance	= hInstance;
	wndclass.hIcon		= LoadIcon(hInstance, szAppName);
	wndclass.hCursor	= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName	= NULL;
	wndclass.lpszClassName	= szAppName;

	if (!RegisterClass(&wndclass))
	{
		MessageBox(NULL, TEXT("Unable to register window class."), szAppName, MB_ICONERROR);
		return 0;
	}

	hwnd = CreateWindow(	szAppName,			// window class name
				NAME_APP,			// window caption
				WS_OVERLAPPEDWINDOW,		// window style
				CW_USEDEFAULT,			// initial x position
				CW_USEDEFAULT,			// initial y position
				CW_USEDEFAULT,			// initial x size
				CW_USEDEFAULT,			// initial y size
				NULL,				// parent window handle
				NULL,				// window menu handle
				hInstance,			// program instance handle
				NULL);				// create struct pointer

//	ShowWindow(hwnd, iCmdShow);
	UpdateWindow(hwnd);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static lgLcdConnectContext	connectContext;
	static lgLcdDeviceDesc		deviceDescription;
	static lgLcdOpenContext		openContext;
	static HFONT			hFont, hStockFont;
	static HBITMAP			hLcdBitmap;
	static HDC			hdcMem;
	static HWND			hwndNextViewer;
	static BOOL			lcdOpen = FALSE;
	static appData			data;


	switch (message)
	{
	case WM_CREATE:
		InitCommonControls();		// Xp style buttons/checkboxes etc.
		lcdOpen = initLcd(hwnd, &connectContext, &deviceDescription, &openContext);
		if (lcdOpen)
		{
			hLcdBitmap = CreateBitmap(deviceDescription.Width, deviceDescription.Height, deviceDescription.Bpp, 0, NULL);
			hdcMem = CreateCompatibleDC(NULL);
			SelectObject(hdcMem, hLcdBitmap);
			SetTextColor(hdcMem, RGB(255,255,255));
			SetBkMode(hdcMem, TRANSPARENT);
			initData(&data);
			hFont = CreateFontIndirect(&(data.logfont));
			hStockFont = SelectObject(hdcMem, hFont);
			hwndNextViewer = SetClipboardViewer(hwnd);
		}
		return 0;

	case WM_DRAWCLIPBOARD:
		if (hwndNextViewer)
			SendMessage(hwndNextViewer, message, wParam, lParam);
		PostMessage(hwnd, WMU_UPDATE, 0, 0);
		return 0;

	case WM_CHANGECBCHAIN:
		if ((HWND) wParam == hwndNextViewer)
			hwndNextViewer = (HWND) lParam;
		else if (hwndNextViewer)
			SendMessage(hwndNextViewer, message, wParam, lParam);
		return 0;

	case WMU_UPDATEDATA:
		initData(&data);
		hFont = SelectObject(hdcMem, hStockFont);
		DeleteObject(hFont);
		hFont = CreateFontIndirect(&(data.logfont));
		hStockFont = SelectObject(hdcMem, hFont);
		PostMessage(hwnd, WMU_UPDATE, 0, 0);
		return 0;

	case WMU_UPDATE:
		{
			GLOBALHANDLE	hGlobal;
			HDROP		hDrop;

			OpenClipboard(hwnd);
			hGlobal = GetClipboardData(CF_TEXT_T);
			if (hGlobal != NULL)
			{
				TCHAR		*pGlobal, *str;
				INT		strLength;

				// pGlobal belongs to clipboard, modifying it would supposedly modify it in clipboard
				pGlobal = (PTSTR) GlobalLock(hGlobal);
				strLength = lstrlen(pGlobal);
				str = (TCHAR *)malloc(strLength * sizeof(TCHAR) + sizeof(TCHAR));
				if (str == NULL)
				{
					GlobalUnlock(hGlobal);
					CloseClipboard();
					postError(hwnd, 0, TEXT("malloc()"));
					return 0;
				}
				lstrcpy(str, pGlobal);
				GlobalUnlock(hGlobal);
				CloseClipboard();
				processString(hdcMem, data.wordWrap, &deviceDescription, str, strLength);
				free((VOID *)str);
			}

			hDrop = GetClipboardData(CF_HDROP);
			if (hDrop != NULL)
				processDrop(hdcMem, data.wordWrap, &deviceDescription, hDrop);
			CloseClipboard();
			if (hDrop == NULL && hGlobal == NULL)
				printLcdString(hdcMem, data.wordWrap, deviceDescription.Width, 0, 0, TEXT("Clipboard Empty"), 15);
			else if (data.alert)
			{
				lgLcdSetAsLCDForegroundApp(openContext.device, LGLCD_LCD_FOREGROUND_APP_YES);
				SetTimer(hwnd, TIMER_DELAY, 3000, NULL);
			}
			updateLcd(hwnd, hLcdBitmap, &openContext, data.negateLcd);
		}
		return 0;

	case WMU_CONFIGURE:
		DialogBox((HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), TEXT("config"), hwnd, configureProc);
		return 0;

	case WM_TIMER:
		lgLcdSetAsLCDForegroundApp(openContext.device, LGLCD_LCD_FOREGROUND_APP_NO);
		KillTimer(hwnd, TIMER_DELAY);
		return 0;

	case WM_DESTROY:
		if (lcdOpen)
		{
			// Errors dont matter, closing anyhow.
			lgLcdClose(openContext.device);
			lgLcdDisconnect(connectContext.connection);
			lgLcdDeInit();
		}
		hFont = SelectObject(hdcMem, hStockFont);
		DeleteObject(hFont);
		DeleteObject(hLcdBitmap);
		DeleteDC(hdcMem);
		ChangeClipboardChain(hwnd, hwndNextViewer);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

BOOL initLcd(HWND hwnd, lgLcdConnectContext *connectContext, lgLcdDeviceDesc *deviceDescription, lgLcdOpenContext *openContext)
{
	static HWND			hwndstat;	// memory must exist when referenced by callback after this function terminates
	DWORD				ret;

	hwndstat = hwnd;

	// setup connect structure
	ZeroMemory(connectContext, sizeof(lgLcdConnectContext));
	connectContext->appFriendlyName			= NAME_LCDMON;
	connectContext->isAutostartable			= TRUE;
	connectContext->isPersistent			= TRUE;
	connectContext->connection			= LGLCD_INVALID_CONNECTION;
	connectContext->onConfigure.configCallback	= configureCallback;
	connectContext->onConfigure.configContext	= &hwndstat;

	ret = lgLcdInit();
	if (ret != ERROR_SUCCESS)
	{
		postError(hwnd, ret, TEXT("lgLcdInit()") );
		return FALSE;
	}
	ret = lgLcdConnect(connectContext);
	if (ret != ERROR_SUCCESS)
	{
		lgLcdDeInit();
		postError(hwnd, ret, TEXT("lgLcdConnect()") );
		return FALSE;
	}

	ret = lgLcdEnumerate(connectContext->connection, 0, deviceDescription);
	if (ret != ERROR_SUCCESS)
	{
		lgLcdDisconnect(connectContext->connection);
		lgLcdDeInit();
		postError(hwnd, ret, TEXT("lgLcdEnumerate()") );
		return FALSE;
	}

	// Init openContext with some data from connectContext
	ZeroMemory(openContext, sizeof(lgLcdOpenContext));
	openContext->connection		= connectContext->connection;
	openContext->index		= 0;
	openContext->device		= 0;
	openContext->onSoftbuttonsChanged.softbuttonsChangedCallback	= buttonCallback;
	openContext->onSoftbuttonsChanged.softbuttonsChangedContext	= &hwndstat;

	ret = lgLcdOpen(openContext);
	if (ret != ERROR_SUCCESS)
	{
		lgLcdDisconnect(connectContext->connection);
		lgLcdDeInit();
		postError(hwnd, ret, TEXT("lgLcdOpen()") );
		return FALSE;
	}
	return TRUE;
}

// My callbacks act goofy sometimes, if you know what I'm doing wrong I'd love to hear it ;o)
DWORD WINAPI buttonCallback(INT device, DWORD dwButtons, VOID *hwnd)
{
	HWND	hwndClipMon = *((HWND *)hwnd);

	switch (dwButtons)
	{
	case LGLCDBUTTON_BUTTON0:
		PostMessage(hwndClipMon, WM_CLOSE, 0, 0);
		break;
	case LGLCDBUTTON_BUTTON1:
//		PostMessage(hwndClipMon, WMU_CONFIGURE, 0, 0);
		break;
	}
	return 0;
}

DWORD WINAPI configureCallback(INT device, VOID *hwnd)
{
	PostMessage(*((HWND *)hwnd), WMU_CONFIGURE, 0, 0);
	return 0;
}

VOID updateLcd(HWND hwnd, HBITMAP hBitmap, lgLcdOpenContext *openContext, BOOL negate)
{
	lgLcdBitmap160x43x1	bitmap;
	lgLcdBitmapHeader	header;
	BYTE			winbitmap[860];
	DWORD			ret;

	// set up structures
	header.Format = LGLCD_BMP_FORMAT_160x43x1;
	bitmap.hdr = header;

	GetBitmapBits(hBitmap, 860, &winbitmap);	// 860 = (lcdWidth * lcdHeigh) / 8
	if (negate)
	{
		int i;

		for (i = 0; i < 860; i++)
			winbitmap[i] = ~winbitmap[i];
	}
	convertBitmap(winbitmap, bitmap.pixels, 860);

	//update lcd display
	ret = lgLcdUpdateBitmap(openContext->device, &(bitmap.hdr), LGLCD_ASYNC_UPDATE(LGLCD_PRIORITY_NORMAL));
	postError(hwnd, ret, TEXT("lgLcdUpdateBitmap()") );
	ZeroMemory(&winbitmap, 860);
	SetBitmapBits(hBitmap, 860, &winbitmap);
}

VOID processString(HDC hdc, BOOL wordwrap, const lgLcdDeviceDesc *deviceDescription, TCHAR *str, INT strLength)
{
	TCHAR		*subString = NULL;
	INT		yPos = -1;

	//_tcstok == strtok for TCHAR's
	subString = _tcstok(str, TEXT("\r\n"));
	if (subString == NULL)
		printLcdString(hdc, wordwrap, deviceDescription->Width, 0, ++yPos, str, strLength);
	while (subString)
	{
		yPos += printLcdString(hdc, wordwrap, deviceDescription->Width, 0, ++yPos, subString, lstrlen(subString));
		subString = _tcstok(NULL, TEXT("\r\n"));
	}
}

VOID processDrop(HDC hdc, BOOL wordwrap, const lgLcdDeviceDesc *deviceDescription, HDROP hDrop)
{
	TCHAR	*buffer, file[MAX_PATH] = TEXT("");
	INT	i, cFiles, size = 0;

	cFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	for(i = 0; i < cFiles; i++)
	{
		size += DragQueryFile(hDrop, i, NULL, 0);
		size += 3;	// for quotes and space
	}
	buffer = (TCHAR *)malloc(size * sizeof(TCHAR) + sizeof(TCHAR));
	if (buffer == NULL)
	{
		postError(0, 0, TEXT("malloc()"));
		return;
	}
	buffer[0] = '\0';
	for(i = 0; i < cFiles; i++)
	{
		lstrcat(buffer, TEXT("\"") );
		DragQueryFile(hDrop, i, file, MAX_PATH);
		lstrcat(buffer, file);
		lstrcat(buffer, TEXT("\" ") );
	}
	printLcdString(hdc, wordwrap, deviceDescription->Width, 0, 0, buffer, size);
	free((VOID *)buffer);
}

// This is ugly and should be rewritten. I couldnt get DrawText to display on lcd for some reason a long while ago and should probably try again.
// (If there is a crash its probably in here, turning off wordwrapping should help)
INT printLcdString(HDC hdc, BOOL wordwrap, INT lcdWidth, INT xPos, INT yPos, TCHAR *str, INT strlength)
{
	TEXTMETRIC	tm;
	INT		i, lastSpace = -1, start = 0, extraLines = 0, iTabs[1], extents;


	GetTextMetrics(hdc, &tm);
	iTabs[0] = 2 * tm.tmAveCharWidth;

	if (wordwrap == FALSE)
	{
		TabbedTextOut(hdc, xPos, tm.tmAscent * yPos, str, strlength, 1, iTabs, 0);
		return 0;
	}

	extents = GetTabbedTextExtent(hdc, str, strlength, 1, iTabs);	// Gets dimensions of string in pixels

	if (LOWORD(extents) < lcdWidth)
	{
		TabbedTextOut(hdc, xPos, tm.tmAscent * yPos, str, strlength, 1, iTabs, 0);
	} else {
		// word wrap code
		for(i = 0; i < strlength; i++)
		{
			extents = GetTabbedTextExtent(hdc, &str[start], i - start, 1, iTabs);
			if (LOWORD(extents) > lcdWidth)	// too big, print last
			{
				if (lastSpace != -1)		// space was found, break there
				{
					TabbedTextOut(hdc, xPos, tm.tmAscent * yPos++, &str[start], lastSpace - start, 1, iTabs, 0);
					start = lastSpace + 1;		// move up position in string
					lastSpace = -1;			// reset space flag
					extraLines++;
					continue;
				} else {
					// large single word
					TabbedTextOut(hdc, xPos, tm.tmAscent * yPos++, &str[start], i - start, 1, iTabs, 0);
					start = i;
					extraLines++;
					continue;
				}
			}
			if (str[i] == ' ')
				lastSpace = i;
		}
		TabbedTextOut(hdc, xPos, tm.tmAscent * yPos, &str[start], strlength - start, 1, iTabs, 0);
	}
	return	extraLines;
}

BOOL CALLBACK configureProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HWND	hwndParent;
	static TCHAR	path[MAX_PATH];
	TCHAR		buffer[80];
	BOOL		checked;

	switch(message)
	{
	case WM_INITDIALOG:
		hwndParent = (HWND)GetWindowLongPtr(hDlg, GWLP_HWNDPARENT);	// GetWindowLong is not x64 compatible, this is. (HANDLE defined as a pointer)
		GetCurrentDirectory(MAX_PATH, path);
		lstrcat(path, TEXT("\\clipMon.ini"));

		checked = GetPrivateProfileInt(TEXT("clipMon"), TEXT("negate"), FALSE, path);
		if (checked)
			SendMessage(GetDlgItem(hDlg, IDC_NEG), BM_SETCHECK, BST_CHECKED, 0);
		else
			SendMessage(GetDlgItem(hDlg, IDC_NEG), BM_SETCHECK, BST_UNCHECKED, 0);	

		checked = GetPrivateProfileInt(TEXT("clipMon"), TEXT("wordwrap"), TRUE, path);
		if (checked)
			SendMessage(GetDlgItem(hDlg, IDC_WRAP), BM_SETCHECK, BST_CHECKED, 0);
		else
			SendMessage(GetDlgItem(hDlg, IDC_WRAP), BM_SETCHECK, BST_UNCHECKED, 0);	

		checked = GetPrivateProfileInt(TEXT("clipMon"), TEXT("alert"), TRUE, path);
		if (checked)
			SendMessage(GetDlgItem(hDlg, IDC_ALERT), BM_SETCHECK, BST_CHECKED, 0);
		else
			SendMessage(GetDlgItem(hDlg, IDC_ALERT), BM_SETCHECK, BST_UNCHECKED, 0);			

	case WM_COMMAND:
		{
			static CHOOSEFONT	cf;
			static LOGFONT		lf;
			static BOOL		fontFlag = FALSE;

			switch( LOWORD(wParam) )
			{
			case IDC_FONT:
				if (pickFont(hDlg, &cf, &lf))
					fontFlag = TRUE;
				else
					fontFlag = FALSE;
				return TRUE;
			case IDOK:
			case IDC_APPLY:
				checked = (int)SendMessage(GetDlgItem(hDlg, IDC_NEG), BM_GETCHECK, 0, 0);
				WritePrivateProfileString(TEXT("clipMon"), TEXT("negate"), _itot(checked, buffer, 10), path);

				checked = (int)SendMessage(GetDlgItem(hDlg, IDC_WRAP), BM_GETCHECK, 0, 0);
				WritePrivateProfileString(TEXT("clipMon"), TEXT("wordwrap"), _itot(checked, buffer, 10), path);

				checked = (int)SendMessage(GetDlgItem(hDlg, IDC_ALERT), BM_GETCHECK, 0, 0);
				WritePrivateProfileString(TEXT("clipMon"), TEXT("alert"), _itot(checked, buffer, 10), path);

				if (fontFlag)
				{
					//WritePrivateProfileStruct(TEXT("clipMon"), TEXT("logfont"), cf.lpLogFont, sizeof(LOGFONT), path);

					WritePrivateProfileString(TEXT("font"), TEXT("lfHeight"), _itot(lf.lfHeight, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfWidth"), _itot(lf.lfWidth, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfEscapement"), _itot(lf.lfEscapement, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfOrientation"), _itot(lf.lfOrientation, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfItalic"), _itot(lf.lfItalic, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfUnderline"), _itot(lf.lfUnderline, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfStrikeOut"), _itot(lf.lfStrikeOut, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfCharSet"), _itot(lf.lfCharSet, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfOutPrecision"), _itot(lf.lfOutPrecision, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfClipPrecision"), _itot(lf.lfClipPrecision, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfQuality"), _itot(lf.lfQuality, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfPitchAndFamily"), _itot(lf.lfPitchAndFamily, buffer, 10), path);
					WritePrivateProfileString(TEXT("font"), TEXT("lfFaceName"), lf.lfFaceName, path);
				}

				PostMessage(hwndParent, WMU_UPDATEDATA, 0, 0);
				if (wParam == IDOK)
					EndDialog(hDlg, 0);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, 0);
				return TRUE;
			}
		}
	}
	return FALSE;
}

VOID postError(HWND hwnd, INT ret, TCHAR *func)
{
	TCHAR	errorMsg[80];

	if (ret == ERROR_SUCCESS)
		return;
	else if ( !lstrcmp(func, TEXT("malloc()") ) )
	{
		wsprintf(errorMsg, TEXT("%s: Memory allocation failure."), func);
		MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
	}
	else if ( !lstrcmp(func, TEXT("lgLcdInit()") ) )
	{
		switch (ret)
		{
			case RPC_S_SERVER_UNAVAILABLE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem is not available."), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_OLD_WIN_VERSION:
				wsprintf(errorMsg, TEXT("%s: Unable to initialize on Win9x."), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_NO_SYSTEM_RESOURCES:
				wsprintf(errorMsg, TEXT("%s: Not enough system resources."), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_ALREADY_INITIALIZED:
				wsprintf(errorMsg, TEXT("%s: Device already initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if( !lstrcmp(func, TEXT("lgLcdConnect()")) )
	{
		switch (ret)
		{
			case ERROR_SERVICE_NOT_ACTIVE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem has not been initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_INVALID_PARAMETER:
				wsprintf(errorMsg, TEXT("%s: Invalid parameter!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_FILE_NOT_FOUND:
				wsprintf(errorMsg, TEXT("%s: LCDMon is not running."), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_ALREADY_EXISTS:
				wsprintf(errorMsg, TEXT("%s: Client connection has already been established!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case RPC_X_WRONG_PIPE_VERSION:
				wsprintf(errorMsg, TEXT("%s: Protocol version mismatch."), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if ( !lstrcmp(func, TEXT("lgLcdEnumerate()")) )
	{
		switch (ret)
		{
			case ERROR_SERVICE_NOT_ACTIVE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem has not been initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_NO_MORE_ITEMS:
				wsprintf(errorMsg, TEXT("%s: Unable to enumerate hardware."), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_INVALID_PARAMETER:
				wsprintf(errorMsg, TEXT("%s: Invalid parameter!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if ( !lstrcmp(func, TEXT("lgLcdOpen()")) )
	{
		switch (ret)
		{
			case ERROR_SERVICE_NOT_ACTIVE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem has not been initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_INVALID_PARAMETER:
				wsprintf(errorMsg, TEXT("%s: Invalid parameter!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_ALREADY_EXISTS:
				wsprintf(errorMsg, TEXT("%s: Client connection has already been established!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if ( !lstrcmp(func, TEXT("lgLcdUpdateBitmap()")) )
	{
		switch (ret)
		{
			case ERROR_SERVICE_NOT_ACTIVE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem has not been initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_INVALID_PARAMETER:
				wsprintf(errorMsg, TEXT("%s: Invalid parameter!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if ( !lstrcmp(func, TEXT("lgLcdClose()")) )
	{
		switch (ret)
		{
			case ERROR_SERVICE_NOT_ACTIVE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem has not been initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_INVALID_PARAMETER:
				wsprintf(errorMsg, TEXT("%s: Invalid parameter!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if ( !lstrcmp(func, TEXT("lgLcdDisconnect()")) )
	{
		switch (ret)
		{
			case ERROR_SERVICE_NOT_ACTIVE:
				wsprintf(errorMsg, TEXT("%s: Logitech LCD subsystem has not been initialized!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			case ERROR_INVALID_PARAMETER:
				wsprintf(errorMsg, TEXT("%s: Invalid parameter!"), func);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
				break;
			default:
				wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
				MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
		}
	}
	else if ( !lstrcmp(func, TEXT("lgLcdSetAsLCDForegroundApp()")) )
	{
		wsprintf(errorMsg, TEXT("%s: Unknown Error #%d"), func, ret);
		MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
	}
	else
	{
		wsprintf(errorMsg, TEXT("%s: Unknown Function Error #%d"), func, ret);
		MessageBox(hwnd, errorMsg, NAME_APP, MB_ICONERROR);
	}
	
	PostMessage(hwnd, WM_CLOSE, 0, 0);
}

// bitmap is using one bit per pixel (on/off), lcd is using entire byte for pixel.
// (logitech was probably thinking ahead with lcdsdk to expand to color lcds and wanted uniform data)
VOID convertBitmap(BYTE *bitAligned, BYTE *byteAligned, INT size)
{
	int i, j;

	for (i = 0, j = 0; i < size; i++)
	{
		byteAligned[j++] = (bitAligned[i] & 0x80) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x40) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x20) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x10) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x08) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x04) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x02) ? 0xFF : 0x00;
		byteAligned[j++] = (bitAligned[i] & 0x01) ? 0xFF : 0x00;
	}
}

VOID initData(appData *data)
{
	TCHAR	buffer[80], path[MAX_PATH];

	GetCurrentDirectory(MAX_PATH, path);
	lstrcat(path, TEXT("\\clipMon.ini"));

	GetPrivateProfileString(TEXT("clipMon"), TEXT("negate"), TEXT("0"), buffer, 80, path);
	data->negateLcd = _ttoi(buffer);

	GetPrivateProfileString(TEXT("clipMon"), TEXT("wordwrap"), TEXT("1"), buffer, 80, path);
	data->wordWrap = _ttoi(buffer);

	GetPrivateProfileString(TEXT("clipMon"), TEXT("alert"), TEXT("1"), buffer, 80, path);
	data->alert = _ttoi(buffer);

//	this does not even work
//	GetPrivateProfileStruct(TEXT("clipMon"), TEXT("logfont"), data->logfont, sizeof(LOGFONT), path);

// font
	GetPrivateProfileString(TEXT("font"), TEXT("lfHeight"), TEXT("12"), buffer, 80, path);
	data->logfont.lfHeight = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfWidth"), TEXT("5"), buffer, 80, path);
	data->logfont.lfWidth = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfEscapement"), TEXT("0"), buffer, 80, path);
	data->logfont.lfEscapement = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfOrientation"), TEXT("0"), buffer, 80, path);
	data->logfont.lfOrientation = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfWeight"), TEXT("400"), buffer, 80, path);
	data->logfont.lfWeight = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfItalic"), TEXT("0"), buffer, 80, path);
	data->logfont.lfItalic = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfUnderline"), TEXT("0"), buffer, 80, path);
	data->logfont.lfUnderline = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfStrikeOut"), TEXT("0"), buffer, 80, path);
	data->logfont.lfStrikeOut = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfCharSet"), TEXT("0"), buffer, 80, path);
	data->logfont.lfCharSet = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfOutPrecision"), TEXT("0"), buffer, 80, path);
	data->logfont.lfOutPrecision = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfClipPrecision"), TEXT("0"), buffer, 80, path);
	data->logfont.lfClipPrecision = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfQuality"), TEXT("3"), buffer, 80, path);
	data->logfont.lfQuality = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfPitchAndFamily"), TEXT("0"), buffer, 80, path);
	data->logfont.lfPitchAndFamily = _ttoi(buffer);
	GetPrivateProfileString(TEXT("font"), TEXT("lfFaceName"), TEXT("Arial"), buffer, 31, path);
	lstrcpy(data->logfont.lfFaceName, buffer);

}

// I dont use this anymore, but this was my development font
VOID initFont(LOGFONT *lf)
{
	lf->lfHeight			= 12;
	lf->lfWidth			= 5;
	lf->lfEscapement		= 0;
	lf->lfOrientation		= 0;
	lf->lfWeight			= 400;
	lf->lfItalic			= 0;
	lf->lfUnderline			= 0;
	lf->lfStrikeOut			= 0;
	lf->lfCharSet			= ANSI_CHARSET;
	lf->lfOutPrecision		= OUT_DEFAULT_PRECIS;
	lf->lfClipPrecision		= CLIP_DEFAULT_PRECIS;
	lf->lfQuality			= NONANTIALIASED_QUALITY;
	lf->lfPitchAndFamily		= DEFAULT_PITCH | FF_DONTCARE;
}

BOOL pickFont(HWND hwnd, CHOOSEFONT *cf, LOGFONT *lf)
{
	cf->lStructSize		= sizeof(CHOOSEFONT);
	cf->hwndOwner		= hwnd;
	cf->hDC			= NULL;
	cf->lpLogFont		= lf;
	cf->iPointSize		= 0;
	cf->Flags		= CF_SCREENFONTS | CF_FORCEFONTEXIST;
	cf->rgbColors		= 0;
	cf->lCustData		= 0;
	cf->lpfnHook		= NULL;
	cf->lpTemplateName	= NULL;
	cf->hInstance		= NULL;
	cf->lpszStyle		= NULL;
	cf->nFontType		= 0;
	cf->nSizeMin		= 0;
	cf->nSizeMax		= 0;

	return ChooseFont(cf);
}