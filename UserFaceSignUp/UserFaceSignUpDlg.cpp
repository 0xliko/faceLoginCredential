
// UserFaceSignUpDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "UserFaceSignUp.h"
#include "UserFaceSignUpDlg.h"
#include "afxdialogex.h"
#include <lmaccess.h>
#include <Wincrypt.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "Crypt32.lib")


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CUserFaceSignUpDlg dialog



CUserFaceSignUpDlg::CUserFaceSignUpDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_USERFACESIGNUP_DIALOG, pParent)
	, m_confirmPassword(_T(""))
	, m_curPassword(_T(""))
	, m_recPassword(_T(""))
	, m_userName(_T(""))
{
	m_isRunning = false;
	m_shouldExit = false;
	m_requireCapture = false;
	cur_image_index = 0;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_images = new Mat[3];
	m_saved = false;
	for (int i = 0; i < 3; i++) {
		Mat m;
		m_images[i] = m;
	}
	//labels.push_back(0);
	
}

void CUserFaceSignUpDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CAMERA_PREVIEW, m_cameraPreview);
	DDX_Text(pDX, IDC_CONFIRM_PASSWORD, m_confirmPassword);
	DDX_Text(pDX, IDC_CUR_PASSWORD, m_curPassword);
	DDX_Control(pDX, IDC_FIRSTIMAGE, m_firstImage);
	DDX_Control(pDX, IDC_IPADDRESS, m_serverIP);
	DDX_Text(pDX, IDC_REC_PASSWORD, m_recPassword);
	DDX_Control(pDX, IDC_SECONDIMAGE, m_secondImage);
	//  DDX_Control(pDX, IDC_THIRDIMAGE, m_secondPassword);
	DDX_Text(pDX, IDC_USERNAME, m_userName);
	DDX_Control(pDX, IDC_THIRDIMAGE, m_thirdImage);
}

BEGIN_MESSAGE_MAP(CUserFaceSignUpDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_CAPTURE, &CUserFaceSignUpDlg::OnBnClickedCapture)
	ON_BN_CLICKED(IDOK, &CUserFaceSignUpDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CUserFaceSignUpDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CUserFaceSignUpDlg message handlers
void logNumber(int number) {
	FILE* file;
	fopen_s(&file, "log.txt", "a");
	if (file) {
		fwprintf(file, L"%d\n", number);
		fclose(file);
	}
	return;
}
void logFloat(float number) {
	FILE* file;
	fopen_s(&file, "log.txt", "a");
	if (file) {
		fwprintf(file, L"%f\n", number);
		fclose(file);
	}
	return;
}


// CCameraTestDlg message handlers



DWORD WINAPI CamThreadFunction(LPVOID lpParam) {
	
	CUserFaceSignUpDlg* pDlg = (CUserFaceSignUpDlg*)lpParam;
	
#ifdef CAFFE
	Net net = cv::dnn::readNetFromCaffe(caffeConfigFile, caffeWeightFile);
#else
	Net net = cv::dnn::readNetFromTensorflow(tensorflowWeightFile, tensorflowConfigFile);
#endif

	VideoCapture cap;
	//cap.open("test.mp4");  // case use video file
	cap.open(0);  // case use camera
	//Sleep(500);
	if (!cap.isOpened())
	{
	
		pDlg->m_isRunning = false;
		OutputDebugStringA("not found camera");
		return -1;
	}

	Mat frame;
	Mat image;
	Mat oldImg;
	while (!pDlg->m_shouldExit) {
		cap >> frame;
		if (frame.empty())
			break;
		const int width = frame.cols;
		const int height = frame.rows;
		const int offsetLeft = max((width - height) / 2, 0);
		const int offsetTop = max((height - width) / 2, 0);
		const int length = min(width, height);
		image = Mat(frame, Rect(offsetLeft, offsetTop, length, length));
		Rect faceRect;
		image.copyTo(oldImg);
		const int faceCount = detectFaceOpenCVDNN(net, image,faceRect);
	    pDlg->receivedFrame(image,oldImg,faceCount,faceRect);
		
        Sleep(100);
	}
	pDlg->m_isRunning = false;
	pDlg->m_shouldExit = false;
	return 0;
}



void CUserFaceSignUpDlg::receivedFrame(Mat image,Mat oldImg,int count,Rect faceRect)
{
   
	//Ptr<EigenFaceRecognizer> model = EigenFaceRecognizer::create();
	//model->train(pattern_images, labels);
	if (!m_cameraPreview.m_bmp.empty())
		m_cameraPreview.m_bmp.release();
	if(m_patternTemplates.size()){
		HImage img;
		int status = FSDK_LoadImageFromBuffer(&img,oldImg.data, oldImg.cols, oldImg.rows, 3 * oldImg.cols, FSDK_IMAGE_COLOR_24BIT);
		if (status != FSDKE_OK) {
			OutputDebugStringA("load image error!");
		}
		else {

			bool successed = false;

			FSDK_SetFaceDetectionParameters(false, true, oldImg.cols);
			FSDK_SetFaceDetectionThreshold(3);
			FSDK_FaceTemplate FaceTemplate;
			int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
			FSDK_FreeImage(img);
			float similarity;
			for (int i = 0; i < m_patternTemplates.size(); i++) {
				FSDK_MatchFaces(&FaceTemplate, &m_patternTemplates[i], &similarity);
				//logFloat(similarity);
				if (similarity > 0.9)
					successed = successed || true;
			}
			if (successed) 
				cv::rectangle(image, faceRect, Scalar(0, 0, 255), 3);
			
		}
	}
	m_cameraPreview.m_bmp = image.clone();
	m_cameraPreview.InvalidateRect(NULL, false);
	if (m_requireCapture) {
		if (count != 1)
		{
			MessageBoxA(this->m_hWnd, "select capture button when detect one face", "warning", 0);
			m_requireCapture = false;
			InvalidateRect(NULL, false);
			return;
		}
		if (cur_image_index == 0)
		{
			m_firstImage.m_bmp = oldImg.clone();
			m_firstImage.InvalidateRect(NULL, false);
		}
		if (cur_image_index == 1)
		{
			m_secondImage.m_bmp = oldImg.clone();
			m_secondImage.InvalidateRect(NULL, false);
		}
		if (cur_image_index == 2)
		{
			m_thirdImage.m_bmp = oldImg.clone();
			m_thirdImage.InvalidateRect(NULL, false);
		}

		
		Mat oldm = m_images[cur_image_index];
		oldm.release();
		Mat m;
		resize(oldImg(faceRect), m, Size(178, 218));
		m_images[cur_image_index] = m;
		

		cur_image_index = (cur_image_index + 1) % 3;
		m_requireCapture = false;
	}

	
}


BOOL CUserFaceSignUpDlg::OnInitDialog()
{
	
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	bool result =  this->initComponents();
	if (!result) {
		exit(0);
	}
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CUserFaceSignUpDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		m_firstImage.InvalidateRect(NULL,false);
		m_secondImage.InvalidateRect(NULL, false);
		m_thirdImage.InvalidateRect(NULL, false);
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CUserFaceSignUpDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CUserFaceSignUpDlg::OnBnClickedCapture()
{
	m_requireCapture = true;
	// TODO: Add your control notification handler code here
}


void CUserFaceSignUpDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here

	
	UpdateData(true);
	if (m_thirdImage.m_bmp.empty())
	{
		MessageBoxA(this->m_hWnd, "please capture your face three time", "warning", 0);
		return;
	}
	BYTE bit1,bit2, bit3, bit4;
	if (m_serverIP.GetAddress(bit1, bit2, bit3, bit4) != 4) {
		MessageBoxA(this->m_hWnd, "please input ip address correctly", "warning", 0);
		return;
	}
	CString serverIP; 
	serverIP.Format(_T("%d.%d.%d.%d"), bit1, bit2, bit3, bit4);
	if (m_curPassword.IsEmpty())
	{
		MessageBoxA(this->m_hWnd, "please input current password", "warning", 0);
		return;
	}
	
	if (m_recPassword.IsEmpty())
	{
		MessageBoxA(this->m_hWnd, "please input recovery password", "warning", 0);
		return;
	}
	if (m_recPassword != m_confirmPassword)
	{
		MessageBoxA(this->m_hWnd, "confirm password missmatch", "warning", 0);
		return;
	}
	/// <summary>
	/// check if inputed password is correct or not
	/// </summary>
	/// 	
	LPTSTR  pUserName = new WCHAR[128];
	memset((void*)pUserName, 0, 256);
	DWORD bufferSize = 128;
	GetUserName(pUserName, &bufferSize);
	CString newPass = _T("face") + m_curPassword + _T("face");
	NET_API_STATUS status = NetUserChangePassword(NULL, pUserName, m_curPassword, newPass);
	if (status == ERROR_INVALID_PASSWORD) {
		MessageBoxA(this->m_hWnd, "incorrect current system password", "warning", 0);
		return;
	}
	
	

	if (m_userName.IsEmpty()) {
		MessageBoxA(this->m_hWnd, "please input username", "warning", 0);
		return;
	}
	DWORD cbSubkeyLength = 0;
	cbSubkeyLength = m_userName.GetLength();
	HKEY key;
	LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, s_RegistryKey_face, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE , &key);
	if (result == ERROR_SUCCESS) {
		DWORD length = m_userName.GetLength();
		result = RegSetKeyValue(key, NULL, _T("username"), RRF_RT_REG_SZ, (LPCTSTR)m_userName, length*2+1);
	}
	if (result == ERROR_SUCCESS) {
		DWORD length = serverIP.GetLength();
		result = RegSetKeyValue(key, NULL, _T("serverIP"), RRF_RT_REG_SZ, (LPCTSTR)serverIP, length * 2+1);
	}

	/// <summary>
	/// save encrypted new password
	/// </summary>
	DATA_BLOB DataIn;
	DATA_BLOB DataOut;
	DATA_BLOB DataVerify;
	BYTE* pbDataInput = (BYTE*)(LPCTSTR)newPass;
	DWORD cbDataInput = wcslen((wchar_t*)pbDataInput) * 2 + 1;
	DataIn.pbData = pbDataInput;
	DataIn.cbData = cbDataInput;
	if (CryptProtectData(
		&DataIn,
		description_string, // A description string. 
		NULL,                               // Optional entropy
											// not used.
		NULL,                               // Reserved.
		NULL,
		CRYPTPROTECT_LOCAL_MACHINE,
		&DataOut))
	{
		if (result == ERROR_SUCCESS) {
			result = RegSetKeyValue(key, NULL, _T("token"), RRF_RT_REG_SZ, DataOut.pbData, DataOut.cbData);
		}
		else {
			status = NetUserChangePassword(NULL, pUserName, newPass, m_curPassword);
		}
	}
	else
	{
		status = NetUserChangePassword(NULL, pUserName, newPass, m_curPassword);
		result = GetLastError();
	}
	
	
	

	/// <summary>
	/// save recovery password
	/// </summary>
	pbDataInput = (BYTE*)(LPCTSTR)m_recPassword;
	cbDataInput = m_recPassword.GetLength() * 2 + 1;
	DataIn.pbData = pbDataInput;
	DataIn.cbData = cbDataInput;
	if (CryptProtectData(
		&DataIn,
		description_string, // A description string. 
		NULL,                               // Optional entropy
											// not used.
		NULL,                               // Reserved.
		NULL,
		CRYPTPROTECT_LOCAL_MACHINE,
		&DataOut))
	{
		if (result == ERROR_SUCCESS) {
			result = RegSetKeyValue(key, NULL, _T("recpass"), RRF_RT_REG_SZ, DataOut.pbData, DataOut.cbData);

		}
		else {
			result = GetLastError();
		}
	}
	
	std::vector<uchar> buf;
	buf.resize(3 * 1024 * 1024);
	if (result == ERROR_SUCCESS) {

		cv::imencode(".png", m_firstImage.m_bmp, buf);
		buf.push_back(0);
		result = RegSetKeyValue(key, NULL, _T("total_img1"), RRF_RT_REG_SZ, buf.data(), buf.size()+1);
	}
	
	if (result == ERROR_SUCCESS) {

		cv::imencode(".png", m_secondImage.m_bmp, buf);
		buf.push_back(0);
		result = RegSetKeyValue(key, NULL, _T("total_img2"), RRF_RT_REG_SZ, buf.data(), buf.size()+1);
	}
	
	if (result == ERROR_SUCCESS) {

		cv::imencode(".png", m_thirdImage.m_bmp, buf);
		buf.push_back(0);
		result = RegSetKeyValue(key, NULL, _T("total_img3"), RRF_RT_REG_SZ, buf.data(), buf.size()+1);
	}
	if (result != ERROR_SUCCESS)  {
		MessageBoxA(this->m_hWnd, "Setup error! Please contact with admin team", "error", 0);
		return;
	}	
	m_saved = true;
	MessageBoxA(this->m_hWnd, "Welcome!", "success", 0);
	//CDialogEx::OnOK();

}


bool CUserFaceSignUpDlg::initComponents()
{
	if (FSDK_ActivateLibrary("fVrFCzYC5wOtEVspKM/zfLWVcSIZA4RNqx74s+QngdvRiCC7z7MHlSf2w3+OUyAZkTFeD4kSpfVPcRVIqAKWUZzJG975b/P4HNNzpl11edXGIyGrTO/DImoZksDSRs6wktvgr8lnNCB5IukIPV5j/jBKlgL5aqiwSfyCR8UdC9s=") != FSDKE_OK)
	{
		MessageBox(_T("Failed to activate fsdk library"), _T("warning"), 0);
		return false;
	}
	FSDK_Initialize("");
	
    HKEY key;
	LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, s_RegistryKey_face, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
	DWORD dwType, dwSize;
    BYTE *encrypted = new BYTE[3 * 1024 * 1024];
	memset(encrypted, 0, 3 * 1024 * 1024);
	dwSize = 3 * 1024 * 1024;
	if (result == ERROR_SUCCESS) {

		LONG ret = RegQueryValueEx(key, _T("serverIP"), NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			CString serverIP;
			serverIP.Append((wchar_t*)encrypted, dwSize);
			BYTE bit1, bit2, bit3, bit4;
			swscanf_s(serverIP.GetBuffer(),_T("%d.%d.%d.%d"), &bit1, &bit2, &bit3, &bit4);
			m_serverIP.SetAddress(bit1,bit2,bit3,bit4);
		}
		//memset(encrypted, 0, 3 * 1024 * 1024);
		//dwSize = 1024;
		//
		//ret = RegQueryValueEx(key, L"token", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		//if (ret == ERROR_SUCCESS) {
		//	
		//	DATA_BLOB DataIn;
		//	DATA_BLOB DataOut;
		//	DATA_BLOB DataVerify;
		//	DataIn.pbData = (BYTE*)encrypted;
		//	DataIn.cbData = dwSize;
		//	LPWSTR pDescrOut = NULL;
		//	if (CryptUnprotectData(
		//		&DataIn,
		//		&pDescrOut, // A description string. 
		//		NULL,                               // Optional entropy
		//											// not used.
		//		NULL,                               // Reserved.
		//		NULL,
		//		CRYPTPROTECT_LOCAL_MACHINE,
		//		&DataOut))
		//	{
		//		
		//		std::wstring password;
		//		password.assign((wchar_t*)DataOut.pbData, DataOut.cbData/2);
		//		wcout << password;
		//	}
		//	else {
		//		DWORD err = GetLastError();
		//		
		//	}
		//}

	    memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 128;
		ret = RegQueryValueEx(key, _T("username"), NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			CString username;
			username.Append((wchar_t*)encrypted, dwSize);
			m_userName = username;
		}
		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;
		ret = RegQueryValueEx(key, _T("total_img1"), NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			std::vector<uchar> data = std::vector<uchar>(encrypted, encrypted + dwSize);
			imdecode(data, IMREAD_COLOR, &(m_firstImage.m_bmp));
			HImage img;
			int status = FSDK_LoadImageFromBuffer(&img, m_firstImage.m_bmp.data, m_firstImage.m_bmp.cols, m_firstImage.m_bmp.rows, 3 * m_firstImage.m_bmp.cols, FSDK_IMAGE_COLOR_24BIT);
			if (status != FSDKE_OK) {
				OutputDebugStringA("load image error!");
			}
			FSDK_SetFaceDetectionParameters(false, true, m_firstImage.m_bmp.cols);
			FSDK_SetFaceDetectionThreshold(3);
			FSDK_FaceTemplate FaceTemplate;
			int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
			FSDK_FreeImage(img);
			m_patternTemplates.push_back(FaceTemplate);

		}
		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;
		ret = RegQueryValueEx(key, _T("total_img2"), NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			std::vector<uchar> data = std::vector<uchar>(encrypted, encrypted + dwSize);
			imdecode(data, IMREAD_COLOR, &(m_secondImage.m_bmp));
			HImage img;
			int status = FSDK_LoadImageFromBuffer(&img, m_secondImage.m_bmp.data, m_secondImage.m_bmp.cols, m_secondImage.m_bmp.rows, 3 * m_secondImage.m_bmp.cols, FSDK_IMAGE_COLOR_24BIT);
			if (status != FSDKE_OK) {
				OutputDebugStringA("load image error!");
			}
			FSDK_SetFaceDetectionParameters(false, true, m_secondImage.m_bmp.cols);
			FSDK_SetFaceDetectionThreshold(3);
			FSDK_FaceTemplate FaceTemplate;
			int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
			FSDK_FreeImage(img);
			m_patternTemplates.push_back(FaceTemplate);
		}
		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;
		ret = RegQueryValueEx(key, _T("total_img3"), NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			std::vector<uchar> data = std::vector<uchar>(encrypted, encrypted + dwSize);
			imdecode(data, IMREAD_COLOR, &(m_thirdImage.m_bmp));
			HImage img;
			int status = FSDK_LoadImageFromBuffer(&img, m_thirdImage.m_bmp.data, m_thirdImage.m_bmp.cols, m_thirdImage.m_bmp.rows, 3 * m_thirdImage.m_bmp.cols, FSDK_IMAGE_COLOR_24BIT);
			if (status != FSDKE_OK) {
				OutputDebugStringA("load image error!");
			}
			FSDK_SetFaceDetectionParameters(false, true, m_thirdImage.m_bmp.cols);
			FSDK_SetFaceDetectionThreshold(3);
			FSDK_FaceTemplate FaceTemplate;
			int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
			FSDK_FreeImage(img);
			m_patternTemplates.push_back(FaceTemplate);
			m_saved = true;
		}
		

	}
	UpdateData(false);
	trainModel();
	// TODO: Add extra initialization here
	DWORD   dwThreadId;
	HANDLE  hThread;
	m_isRunning = true;
	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		CamThreadFunction,       // thread function name
		(void*)this,          // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);   // returns the thread identifier 
	return true;
	// TODO: Add your implementation code here.
}


void CUserFaceSignUpDlg::trainModel()
{
	
}


void CUserFaceSignUpDlg::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	if (!m_saved) {
		MessageBox(L"You can close this program after sign up your face", L"warning", 0);
		return;
	}
	CDialogEx::OnCancel();
}
