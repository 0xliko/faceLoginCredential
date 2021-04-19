
#ifndef CMD_EVENT_WND_H_
#define CMD_EVENT_WND_H_

#include <Windows.h>
#include "common.h"

class BSCredential;

class CCameraPreview
{
public:
	CCameraPreview();
    ~CCameraPreview();

    HRESULT Initialize(BSCredential *pProvider,HWND pParent);
    BOOL GetConnectedStatus();
	void show();
	void hide();
private:
    HRESULT _MyRegisterClass();
    HRESULT _InitInstance();
    BOOL _ProcessNextMessage();
    
    static DWORD WINAPI _ThreadProc(__in LPVOID lpParameter);
    static LRESULT CALLBACK    _WndProc(__in HWND hWnd, __in UINT message, __in WPARAM wParam, __in LPARAM lParam);
    
	BSCredential  * m_pCredential;        // Pointer to our owner.
    HWND  m_hWndButton;       // Handle to our window's button.
    HINSTANCE  m_hInst;            // Current instance
    BOOL m_fConnected;       // Whether or not we're connected.
	HWND                                    m_parent;
public:
	HWND  m_hWnd;             // Handle to our window.
	HBITMAP                                 m_cameraBitmap;
	int                                     m_cameraWidth;
};

#endif