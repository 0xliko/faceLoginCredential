
#include "stdafx.h"
#include <strsafe.h>

#include "CameraPreview.h"

// Custom messages for managing the behavior of the window thread.
#define WM_EXIT_THREAD              WM_USER + 1
#define WM_TOGGLE_CONNECTED_STATUS  WM_USER + 2

const WCHAR c_szClassName[] = L"CameraPreview";

CCameraPreview::CCameraPreview() : m_hWnd(NULL), m_hInst(NULL), m_fConnected(FALSE), m_pCredential(NULL)
{
	m_cameraBitmap = nullptr;
}

CCameraPreview::~CCameraPreview()
{
    // If we have an active window, we want to post it an exit message.
    if (m_hWnd != NULL)
    {
        PostMessage(m_hWnd, WM_EXIT_THREAD, 0, 0);
        m_hWnd = NULL;
    }   
}

// Performs the work required to spin off our message so we can listen for events.
HRESULT CCameraPreview::Initialize(BSCredential *pCredential,HWND pParent)
{
	logText("Initialize Camera Preview");
    HRESULT hr = S_OK;

	m_parent = pParent;
    m_pCredential = pCredential;
    
    
    // Create and launch the window thread.
    HANDLE hThread = CreateThread(NULL, 0, _ThreadProc, this, 0, NULL);
    if (hThread == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    return hr;
}

// Wraps our internal connected status so callers can easily access it.
BOOL CCameraPreview::GetConnectedStatus()
{
    return m_fConnected;
}


HRESULT CCameraPreview::_MyRegisterClass()
{
    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style            = CS_HREDRAW | CS_VREDRAW ;
    wcex.lpfnWndProc      = _WndProc;
    wcex.hInstance        = m_hInst;
    wcex.hIcon            = NULL;
    wcex.hCursor          = NULL;
    wcex.hbrBackground    = NULL;
    wcex.lpszClassName    = c_szClassName;

	if (RegisterClassEx(&wcex))
		return S_OK;
    const DWORD err = GetLastError();
	const HRESULT hr = HRESULT_FROM_WIN32(err);
    if (err == ERROR_CLASS_ALREADY_EXISTS)
        return S_OK;
	logText("while register window");
    logNumberLong(err - ERROR_ALREADY_EXISTS);
	return hr;
}


HRESULT CCameraPreview::_InitInstance()
{
	RECT rect;
	GetWindowRect(m_parent, &rect);
	const int width = rect.right - rect.left;
	const int height = rect.bottom - rect.top;
	const int top = height / 2 - 240;
	const int left = width / 2 - 640;
    HRESULT hr = S_OK;
    m_hWnd = CreateWindowEx(
        WS_EX_TOPMOST ,
        c_szClassName, 
        L"", 
		WS_POPUP,
        left,top,480,480, 
        m_parent,
        NULL, m_hInst, NULL);
    if (m_hWnd == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
		logText("error on create window");
		logNumber((int)hr);
    }

	

    return hr;
}
void CCameraPreview::show() {
	ShowWindow(m_hWnd, SW_NORMAL);
}
void CCameraPreview::hide() {
	ShowWindow(m_hWnd, SW_HIDE);
}
// Called from the separate thread to process the next message in the message queue. If
// there are no messages, it'll wait for one.
BOOL CCameraPreview::_ProcessNextMessage()
{
	
    // Grab, translate, and process the message.
    MSG msg;
    GetMessage(&(msg), m_hWnd, 0, 0);
    TranslateMessage(&(msg));
    DispatchMessage(&(msg));

    // This section performs some "post-processing" of the message. It's easier to do these
    // things here because we have the handles to the window, its button, and the provider
    // handy.
	
    switch (msg.message)
    {
    // Return to the thread loop and let it know to exit.
    case WM_EXIT_THREAD: return FALSE;
	
    // Toggle the connection status, which also involves updating the UI.
    case WM_TOGGLE_CONNECTED_STATUS:
	case WM_PAINT:
	{

		    
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(m_hWnd, &ps);
			if (m_cameraBitmap) {
				HDC hdcReel = CreateCompatibleDC(hdc);
				HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcReel, m_cameraBitmap);
				
				StretchBlt(hdc, 0, 0, 480,480, hdcReel, 0, 0, m_cameraWidth, m_cameraWidth, SRCCOPY);
				SelectObject(hdcReel, hbmpOld);
				m_cameraBitmap = nullptr;
				DeleteDC(hdcReel);
				DeleteObject(hbmpOld);
			}

		
			EndPaint(m_hWnd, &ps);
		
		/*	HDC hdcReel = CreateCompatibleDC(hdc);
			HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcReel, m_cameraBitmap);
			SelectObject(hdcReel, hbmpOld);
			StretchBlt(hdc, 0, 0, 320, 320, hdcReel, 0, 0, m_cameraWidth, m_cameraWidth, SRCCOPY);
			DeleteObject(hbmpOld);
			m_cameraBitmap = nullptr;
			DeleteDC(hdcReel);*/
		

		
	}
        break;
    }

    return TRUE;
}

// Manages window messages on the window thread.
LRESULT CALLBACK CCameraPreview::_WndProc(__in HWND hWnd, __in UINT message, __in WPARAM wParam, __in LPARAM lParam)
{
	
    switch (message)
    {
    // Originally we were going to work with USB keys being inserted and removed, but it
    // seems as though these events don't get to us on the secure desktop. However, you
    // might see some messageboxi in CredUI.
    // TODO: Remove if we can't use from LogonUI.
    case WM_DEVICECHANGE:
        //MessageBox(NULL, L"Device change", L"Device change", 0);
        break;

    // We assume this was the button being clicked.
    case WM_COMMAND:
        PostMessage(hWnd, WM_TOGGLE_CONNECTED_STATUS, 0, 0);
        break;

    // To play it safe, we hide the window when "closed" and post a message telling the 
    // thread to exit.
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        PostMessage(hWnd, WM_EXIT_THREAD, 0, 0);
        break;
	
	case WM_PAINT:
	   break;
	
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Our thread procedure. We actually do a lot of work here that could be put back on the 
// main thread, such as setting up the window, etc.
DWORD WINAPI CCameraPreview::_ThreadProc(__in LPVOID lpParameter)
{
    CCameraPreview *pCommandWindow = static_cast<CCameraPreview*>(lpParameter);
    if (pCommandWindow == NULL)
    {
        // TODO: What's the best way to raise this error?
        return 0;
    }

    HRESULT hr = S_OK;

    // Create the window.
    pCommandWindow->m_hInst = GetModuleHandle(NULL);
    if (pCommandWindow->m_hInst != NULL)
    {            
        hr = pCommandWindow->_MyRegisterClass();
        if (SUCCEEDED(hr))
        {
            hr = pCommandWindow->_InitInstance();
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    // ProcessNextMessage will pump our message pump and return false if it comes across
    // a message telling us to exit the thread.
    if (SUCCEEDED(hr))
    {        
        while (pCommandWindow->_ProcessNextMessage()) 
        {
        }
    }
    else
    {
        if (pCommandWindow->m_hWnd != NULL)
        {
            pCommandWindow->m_hWnd = NULL;
        }
    }

    return 0;
}
