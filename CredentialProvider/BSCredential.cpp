// Code based on the SampleV2CredentialProvider project provided by Microsoft and released under the MS-LPL License
// See: https://code.msdn.microsoft.com/windowsapps/V2-Credential-Provider-7549a730#content 

#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif
#include <unknwn.h>
#include "BSCredential.h"
#include "guid.h"
#include <Windows.h>
#include <lmaccess.h>
#include <Wincrypt.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "Crypt32.lib")

#define verifyPercent 0.3
#define verifyRepeatCount   10
BSCredential::BSCredential(UINT_PTR upAdviseContext,ICredentialProviderEvents   *pcpe) :
	_cRef(1),
	_pCredProvCredentialEvents(nullptr),
	_pszUserSid(nullptr),
	_pszQualifiedUserName(nullptr),
	_fIsLocalUser(false),
	_fChecked(false),
	_fShowControls(false),
	_dwComboIndex(0),
	m_pcpe(pcpe),
    m_upAdviseContext(upAdviseContext)
{
	DllAddRef();
	
	ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
	ZeroMemory(_rgFieldStatePairs, sizeof(_rgFieldStatePairs));
	ZeroMemory(_rgFieldStrings, sizeof(_rgFieldStrings));
	isRunning = false;
	isFinish = false;
	m_autoLogon = false;
	m_pCameraPreview = nullptr;
	m_faild = false;
}

BSCredential::~BSCredential()
{
	if (_rgFieldStrings[SFI_PASSWORD])
	{
		size_t lenPassword = wcslen(_rgFieldStrings[SFI_PASSWORD]);
		SecureZeroMemory(_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*_rgFieldStrings[SFI_PASSWORD]));
	}
	for (int i = 0; i < ARRAYSIZE(_rgFieldStrings); i++)
	{
		CoTaskMemFree(_rgFieldStrings[i]);
		CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
	}
	CoTaskMemFree(_pszUserSid);
	CoTaskMemFree(_pszQualifiedUserName);
	DllRelease();
	isRunning = false;
	while (!isFinish) {
		Sleep(100);
	}
	if(m_pCameraPreview)
	  delete m_pCameraPreview;
}

auto ConvertCVMatToBMP(cv::Mat frame) -> HBITMAP
{
	Mat dest(frame.cols, frame.rows, CV_8UC3);
	frame.copyTo(dest(Rect(0, 0, frame.cols, frame.rows)));
	frame = dest;
	resize(frame, frame, Size(frame.cols / 4 * 4, frame.rows));
	auto convertOpenCVBitDepthToBits = [](const int32_t value)
	{
		auto regular = 0u;

		switch (value)
		{
		case CV_8U:
		case CV_8S:
			regular = 8u;
			break;

		case CV_16U:
		case CV_16S:
			regular = 16u;
			break;

		case CV_32S:
		case CV_32F:
			regular = 32u;
			break;

		case CV_64F:
			regular = 64u;
			break;

		default:
			regular = 0u;
			break;
		}

		return regular;
	};

	auto imageSize = frame.size();
	assert(imageSize.width && "invalid size provided by frame");
	assert(imageSize.height && "invalid size provided by frame");

	if (imageSize.width && imageSize.height)
	{
		auto headerInfo = BITMAPINFOHEADER{};
		ZeroMemory(&headerInfo, sizeof(headerInfo));

		headerInfo.biSize = sizeof(headerInfo);
		headerInfo.biWidth = imageSize.width;
		headerInfo.biHeight = -(imageSize.height); // negative otherwise it will be upsidedown
		headerInfo.biPlanes = 1;// must be set to 1 as per documentation frame.channels();

		const auto bits = convertOpenCVBitDepthToBits(frame.depth());
		headerInfo.biBitCount = frame.channels() * bits;

		auto bitmapInfo = BITMAPINFO{};
		ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));

		bitmapInfo.bmiHeader = headerInfo;
		bitmapInfo.bmiColors->rgbBlue = 0;
		bitmapInfo.bmiColors->rgbGreen = 0;
		bitmapInfo.bmiColors->rgbRed = 0;
		bitmapInfo.bmiColors->rgbReserved = 0;

		auto dc = GetDC(nullptr);
		assert(dc != nullptr && "Failure to get DC");
		auto bmp = CreateDIBitmap(dc,
			&headerInfo,
			CBM_INIT,
			frame.data,
			&bitmapInfo,
			DIB_RGB_COLORS);
		assert(bmp != nullptr && "Failure creating bitmap from captured frame");

		return bmp;
	}
	else
	{
		return nullptr;
	}
}

int detectFaceOpenCVDNN(Net net, Mat& frameOpenCVDNN, Rect& patternRect, Rect& faceRect)
{
	int frameHeight = frameOpenCVDNN.rows;
	int frameWidth = frameOpenCVDNN.cols;
#ifdef CAFFE
	cv::Mat inputBlob = cv::dnn::blobFromImage(frameOpenCVDNN, inScaleFactor, cv::Size(inWidth, inHeight), meanVal, false, false);
#else
	cv::Mat inputBlob = cv::dnn::blobFromImage(frameOpenCVDNN, inScaleFactor, cv::Size(inWidth, inHeight), meanVal, true, false);
#endif

	net.setInput(inputBlob, "data");
	cv::Mat detection = net.forward("detection_out");

	cv::Mat detectionMat(detection.size[2], detection.size[3], CV_32F, detection.ptr<float>());
	int count = 0;
	for (int i = 0; i < detectionMat.rows; i++)
	{
		float confidence = detectionMat.at<float>(i, 2);

		if (confidence > confidenceThreshold)
		{

			int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * frameWidth);
			int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * frameHeight);
			int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * frameWidth);
			int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * frameHeight);
			if (detectionMat.at<float>(i, 5) >= 1 || detectionMat.at<float>(i, 6) >= 1
				|| detectionMat.at<float>(i, 3) >= 1 || detectionMat.at<float>(i, 4) >= 1)
				continue;
			if (detectionMat.at<float>(i, 5) <= 0 || detectionMat.at<float>(i, 6) <= 0
				|| detectionMat.at<float>(i, 3) <= 0 || detectionMat.at<float>(i, 4) <= 0)
				continue;
			const int top = min(y1, y2);
			const int left = min(x1, x2);
			const int width = abs(x1 - x2);
			const int height = abs(y1 - y2);
			if (width == 0 || height == 0) continue;
			if (width > frameOpenCVDNN.rows * 178 / 218.0) continue;
			int patternWidth = frameOpenCVDNN.cols * 178 / 218.0;
			int patternLeft = left + width / 2 - patternWidth / 2;
			if (patternLeft < 0) patternLeft = 0;
			if (patternLeft + patternWidth > frameOpenCVDNN.cols) patternLeft = frameOpenCVDNN.cols - patternWidth;
			patternRect = Rect(patternLeft, 0, patternWidth, frameOpenCVDNN.rows);
			faceRect = Rect(left, top, width, height);
			count++;
			cv::rectangle(frameOpenCVDNN, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2, 4);
		}
	}
	return count;

}




DWORD WINAPI CamThreadFunction(LPVOID lpParam) {
	BSCredential* credential = (BSCredential*)lpParam;
	#ifdef CAFFE
	   Net net = cv::dnn::readNetFromCaffe(caffeConfigFile, caffeWeightFile);
    #else
	  Net net = cv::dnn::readNetFromTensorflow(tensorflowWeightFile, tensorflowConfigFile);
    #endif
	
	VideoCapture cap;
	//cap.open("c:\\1.mp4");  // case use video file
	cap.open(0);  // case use camera
	Sleep(500);
	if (!cap.isOpened())
	{
		logText("no have camera");
		credential->isRunning = false;
		credential->processCaseNoCamera();
	    return -1;
	}
	Mat frame;
	Mat image;
	while (credential->isRunning) {
		cap >> frame;
		if (frame.empty())
			break;
		const int width = frame.cols;
		const int height = frame.rows;
		const int offsetLeft = max((width - height) / 2, 0);
		const int offsetTop = max((height - width) / 2, 0);
		const int length = min(width, height);
		image = Mat(frame, Rect(offsetLeft, offsetTop, length, length));
		Mat oldImg;
		image.copyTo(oldImg);
		Rect patternRect,faceRect;
		const int faceCount = detectFaceOpenCVDNN(net, image,patternRect,faceRect);
		credential->receivedFrame(image, oldImg, patternRect, faceRect,faceCount);
        Sleep(100);
	}
	credential->isRunning = false;
	credential->isFinish = true;
	return 0;
}


void BSCredential::processCaseNoCamera() {
	CREDENTIAL_PROVIDER_FIELD_STATE cpShow = CPFS_DISPLAY_IN_BOTH;
	_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_PASSWORD, cpShow);
	_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_SUBMIT_BUTTON, cpShow);
	_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_TRYAGAIN_CMD, cpShow);
	m_selected = false;
	m_pCameraPreview->hide();
	m_faild = true;
}


void BSCredential::receivedFrame(Mat image,Mat oldImg,Rect patternRect,Rect faceRect,int count) {
	/// try login   
	HWND hwndOwner;
	_pCredProvCredentialEvents->OnCreatingWindow(&hwndOwner);
	WINDOWINFO info;
	GetWindowInfo(hwndOwner, &info);
	logText("screen status when received frame");
	logNumber(info.dwWindowStatus);
	logText("-----------------");
	if (count == 1 && info.dwWindowStatus  && m_selected) {
		HImage img;
		int status = FSDK_LoadImageFromBuffer(&img, oldImg.data, oldImg.cols, oldImg.rows, 3 * oldImg.cols, FSDK_IMAGE_COLOR_24BIT);
		if (status != FSDKE_OK) {
			logText("load image error!");
		}
		else {
			bool successed = false;
			
            FSDK_SetFaceDetectionParameters(false, true, oldImg.cols);
			FSDK_SetFaceDetectionThreshold(3);
			FSDK_FaceTemplate FaceTemplate;
			int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
			FSDK_FreeImage(img);
			float similarity;
			for (int i = 0; i < 3; i++) {
				FSDK_MatchFaces(&FaceTemplate, &m_patternTemplates[i], &similarity);
				if (similarity > 0.9)
					successed = successed || true;
			}
			if(successed){
				cv::rectangle(image, faceRect, Scalar(0, 0, 255), 3);
				m_recognizedCount++;
			}
		}
	     m_curReceivedCount++;
		if (m_curReceivedCount == verifyRepeatCount && m_loginTryCount < 3) {

			if (m_curReceivedCount == 0 || m_recognizedCount / (m_curReceivedCount * 1.0) < verifyPercent) {
				//// case verify faild
				logText("-----------received count -------------");
				logNumber(m_recognizedCount);
				logNumber(verifyRepeatCount);
				logText("-------------------------\n");

				m_loginTryCount++;
				m_recognizedCount = 0;
				m_curReceivedCount = 0;
				if (m_loginTryCount == 3)
				{
					isRunning = false;
					CREDENTIAL_PROVIDER_FIELD_STATE cpShow = CPFS_DISPLAY_IN_BOTH;
                    SHStrDupW(failMessages[m_loginTryCount], &_rgFieldStrings[SFI_TRYAGAIN_CMD]);
					_pCredProvCredentialEvents->BeginFieldUpdates();
					_pCredProvCredentialEvents->SetFieldString(this, SFI_TRYAGAIN_CMD, _rgFieldStrings[SFI_TRYAGAIN_CMD]);
					_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_TRYAGAIN_CMD, cpShow);
					_pCredProvCredentialEvents->EndFieldUpdates();
					m_selected = false;
					m_pCameraPreview->hide();
					m_faild = true;
				}
				else {
					CREDENTIAL_PROVIDER_FIELD_STATE cpShow = CPFS_DISPLAY_IN_BOTH;
					SHStrDupW(failMessages[m_loginTryCount], &_rgFieldStrings[SFI_TRYAGAIN_CMD]);
					_pCredProvCredentialEvents->BeginFieldUpdates();
					_pCredProvCredentialEvents->SetFieldString(this, SFI_TRYAGAIN_CMD, _rgFieldStrings[SFI_TRYAGAIN_CMD]);
					_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_TRYAGAIN_CMD, cpShow);
					_pCredProvCredentialEvents->EndFieldUpdates();
					m_selected = false;
					m_pCameraPreview->hide();
					m_faild = true;
				}
			}
			else {
				/// case success on login
				isRunning = false;
				m_autoLogon = true;
				m_faild = true;
				m_pCameraPreview->hide();
				SHStrDupW(password.c_str(), &_rgFieldStrings[SFI_PASSWORD]);
				m_pcpe->CredentialsChanged(m_upAdviseContext);
				
			}
			
		}

	}
	HBITMAP bbmp = ConvertCVMatToBMP(image);
	if (m_pCameraPreview) {
			m_pCameraPreview->m_cameraBitmap = bbmp;
			m_pCameraPreview->m_cameraWidth = image.cols;
			InvalidateRect(m_pCameraPreview->m_hWnd, NULL,FALSE);
	}		
	


}


// Initializes one credential with the field information passed in.
// Set the value of the SFI_LARGE_TEXT field to pwzUsername.
HRESULT BSCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
	_In_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR const *rgcpfd,
	_In_ FIELD_STATE_PAIR const *rgfsp,
	_In_ ICredentialProviderUser *pcpUser)
{
	logText("BSCredential initialize v1");
	HRESULT hr = S_OK;
	_cpus = cpus;

	GUID guidProvider;
	pcpUser->GetProviderID(&guidProvider);
	_fIsLocalUser = (guidProvider == Identity_LocalUserProvider);
	
	// Copy the field descriptors for each field. This is useful if you want to vary the field
	// descriptors based on what Usage scenario the credential was created for.
	for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(_rgCredProvFieldDescriptors); i++)
	{
		_rgFieldStatePairs[i] = rgfsp[i];
		hr = FieldDescriptorCopy(rgcpfd[i], &_rgCredProvFieldDescriptors[i]);
	}
	
	// Initialize the String value of all the fields.
	if (SUCCEEDED(hr))  hr = SHStrDupW(L"Face", &_rgFieldStrings[SFI_LABEL]); 
	if (SUCCEEDED(hr)) hr = SHStrDupW(L"Failed on face recognization", &_rgFieldStrings[SFI_TRYAGAIN_CMD]); 		
	if (SUCCEEDED(hr)) hr = SHStrDupW(L"", &_rgFieldStrings[SFI_PASSWORD]);
	if (SUCCEEDED(hr)) hr = pcpUser->GetStringValue(PKEY_Identity_QualifiedUserName, &_pszQualifiedUserName); 
    if (SUCCEEDED(hr)) hr = pcpUser->GetSid(&_pszUserSid); 
	logText("BSCredential initialize ended v1");
	initCamera();
    return hr;
}

// LogonUI calls this in order to give us a callback in case we need to notify it of anything.

HRESULT BSCredential::Advise(_In_ ICredentialProviderCredentialEvents *pcpce)
{
	logText("-----------\n Advise \n-----------\n ");
	if (_pCredProvCredentialEvents != nullptr) {
		_pCredProvCredentialEvents->Release();
	}
	HRESULT hr = pcpce->QueryInterface(IID_PPV_ARGS(&_pCredProvCredentialEvents));
	if (SUCCEEDED(hr)) {
		CREDENTIAL_PROVIDER_FIELD_STATE cpfsShow = CPFS_HIDDEN;
		_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_PASSWORD, cpfsShow);
		_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_SUBMIT_BUTTON, cpfsShow);
		_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_TRYAGAIN_CMD, cpfsShow);
		if (!m_pCameraPreview) {
			m_pCameraPreview = new CCameraPreview();
			HWND hwndOwner;
			_pCredProvCredentialEvents->OnCreatingWindow(&hwndOwner);
			m_pCameraPreview->Initialize(this, hwndOwner);
			m_pCameraPreview->hide();
		}
		else {
			m_pCameraPreview->hide();
		}
	}

	return hr;
}

// LogonUI calls this to tell us to release the callback.
HRESULT BSCredential::UnAdvise()
{
	
	logText("unadvise");
    if (_pCredProvCredentialEvents) {
		_pCredProvCredentialEvents->Release();
	}
	_pCredProvCredentialEvents = nullptr;
	if (m_pCameraPreview) {
		m_pCameraPreview->hide();
	}
	return S_OK;
}

// LogonUI calls this function when our tile is selected (zoomed)
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the
// field definitions. But if you want to do something
// more complicated, like change the contents of a field when the tile is
// selected, you would do it here.
HRESULT BSCredential::SetSelected(_Out_ BOOL *pbAutoLogon)
{
	logText("----------------\nselected\n-----------");
	*pbAutoLogon = m_autoLogon;
	m_autoLogon = false;
	if (!m_faild) {
		m_pCameraPreview->show();
		m_selected = true;
	}
	m_loginTryCount = m_curReceivedCount = m_recognizedCount = 0;
    return S_OK;
}

// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. The most common thing to do here (which we do below)
// is to clear out the password field.
HRESULT BSCredential::SetDeselected()
{
	m_pCameraPreview->hide();
	m_selected = false;
	HRESULT hr = S_OK;
	if (_rgFieldStrings[SFI_PASSWORD])
	{
		size_t lenPassword = wcslen(_rgFieldStrings[SFI_PASSWORD]);
		SecureZeroMemory(_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*_rgFieldStrings[SFI_PASSWORD]));

		CoTaskMemFree(_rgFieldStrings[SFI_PASSWORD]);
		hr = SHStrDupW(L"", &_rgFieldStrings[SFI_PASSWORD]);

		if (SUCCEEDED(hr) && _pCredProvCredentialEvents)
		{
			_pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, _rgFieldStrings[SFI_PASSWORD]);
		}
	}
	return hr;
}

// Get info for a particular field of a tile. Called by logonUI to get information
// to display the tile.
HRESULT BSCredential::GetFieldState(DWORD dwFieldID,
	_Out_ CREDENTIAL_PROVIDER_FIELD_STATE *pcpfs,
	_Out_ CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE *pcpfis)
{
	HRESULT hr;

	// Validate our parameters.
	if ((dwFieldID < ARRAYSIZE(_rgFieldStatePairs)))
	{
		*pcpfs = _rgFieldStatePairs[dwFieldID].cpfs;
		*pcpfis = _rgFieldStatePairs[dwFieldID].cpfis;
		hr = S_OK;
	}
	else
	{
		hr = E_INVALIDARG;
	}
	return hr;
}

// Sets ppwsz to the string value of the field at the index dwFieldID
HRESULT BSCredential::GetStringValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ PWSTR *ppwsz)
{
	HRESULT hr;
	*ppwsz = nullptr;

	// Check to make sure dwFieldID is a legitimate index
	if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors))
	{
		// Make a copy of the string and return that. The caller
		// is responsible for freeing it.
		hr = SHStrDupW(_rgFieldStrings[dwFieldID], ppwsz);
	}
	else
	{
		hr = E_INVALIDARG;
	}

	return hr;
}

// Get the image to show in the user tile
HRESULT BSCredential::GetBitmapValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ HBITMAP *phbmp)
{
	HRESULT hr;
	*phbmp = nullptr;
	logText("Get Bitmap value");
	if ((SFI_TILEIMAGE == dwFieldID))
	{
		HBITMAP hbmp = LoadBitmap(HINST_THISDLL, MAKEINTRESOURCE(IDB_TILE_IMAGE));
		if (hbmp != nullptr)
		{
			hr = S_OK;
			*phbmp = hbmp;
		}
		else
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}
	else
	{
		hr = E_INVALIDARG;
	}

	return hr;
}

// Sets pdwAdjacentTo to the index of the field the submit button should be
// adjacent to. We recommend that the submit button is placed next to the last
// field which the user is required to enter information in. Optional fields
// should be below the submit button.
HRESULT BSCredential::GetSubmitButtonValue(DWORD dwFieldID, _Out_ DWORD *pdwAdjacentTo)
{
	HRESULT hr;

	if (SFI_SUBMIT_BUTTON == dwFieldID)
	{
		// pdwAdjacentTo is a pointer to the fieldID you want the submit button to
		// appear next to.
		*pdwAdjacentTo = SFI_PASSWORD;
		hr = S_OK;
	}
	else
	{
		hr = E_INVALIDARG;
	}
	return hr;
}

// Sets the value of a field which can accept a string as a value.
// This is called on each keystroke when a user types into an edit field
HRESULT BSCredential::SetStringValue(DWORD dwFieldID, _In_ PCWSTR pwz)
{
	HRESULT hr;

	// Validate parameters.
	if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
		(CPFT_EDIT_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft ||
			CPFT_PASSWORD_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft))
	{
		PWSTR *ppwszStored = &_rgFieldStrings[dwFieldID];
		CoTaskMemFree(*ppwszStored);
		hr = SHStrDupW(pwz, ppwszStored);
	}
	else
	{
		hr = E_INVALIDARG;
	}

	return hr;
}

// Returns whether a checkbox is checked or not as well as its label.
#pragma warning (disable: 4100)
HRESULT BSCredential::GetCheckboxValue(DWORD dwFieldID, _Out_ BOOL *pbChecked, _Outptr_result_nullonfailure_ PWSTR *ppwszLabel)
{
	*ppwszLabel = nullptr;
	*pbChecked = FALSE;
	return E_INVALIDARG; //no checkbox
}

// Sets whether the specified checkbox is checked or not.
HRESULT BSCredential::SetCheckboxValue(DWORD dwFieldID, BOOL bChecked)
{
	return E_INVALIDARG; //no checkbox
}

// Returns the number of items to be included in the combobox (pcItems), as well as the
// currently selected item (pdwSelectedItem).
HRESULT BSCredential::GetComboBoxValueCount(DWORD dwFieldID, _Out_ DWORD *pcItems, _Deref_out_range_(< , *pcItems) _Out_ DWORD *pdwSelectedItem)
{
	*pcItems = 0;
	*pdwSelectedItem = 0;
	return S_OK;
}

// Called iteratively to fill the combobox with the string (ppwszItem) at index dwItem.
HRESULT BSCredential::GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, _Outptr_result_nullonfailure_ PWSTR *ppwszItem)
{
	return S_OK;
}

// Called when the user changes the selected item in the combobox.
HRESULT BSCredential::SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem)
{
	return S_OK;
}

// Called when the user clicks a command link. We have no command links.
HRESULT BSCredential::CommandLinkClicked(DWORD dwFieldID) { 
	
	HRESULT hr = S_OK;

	CREDENTIAL_PROVIDER_FIELD_STATE cpfsShow = CPFS_HIDDEN;

	// Validate parameter.
	if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
		(CPFT_COMMAND_LINK == _rgCredProvFieldDescriptors[dwFieldID].cpft))
	{
		HWND hwndOwner = nullptr;
		switch (dwFieldID)
		{
		case SFI_TRYAGAIN_CMD: {
			_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_TRYAGAIN_CMD, CPFS_HIDDEN);
			if (m_loginTryCount == 3) {
				CREDENTIAL_PROVIDER_FIELD_STATE cpShow = CPFS_DISPLAY_IN_BOTH;
				_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_PASSWORD, cpShow);
				_pCredProvCredentialEvents->SetFieldState(nullptr, SFI_SUBMIT_BUTTON, cpShow);
			}
			else {
				m_pCameraPreview->show();
				Sleep(1000);
				m_selected = true;
				
			}
		}
		default:
			hr = E_INVALIDARG;
		}

	}
	else
	{
		hr = E_INVALIDARG;
	}

	return hr;
}

// Collect the username and password into a serialized credential for the correct usage scenario
// (logon/unlock is what's demonstrated in this sample).  LogonUI then passes these credentials
// back to the system to log on.

HRESULT BSCredential::GetSerialization(_Out_ CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpgsr,
	_Out_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpcs,
	_Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
	_Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
	logText("Get Serialization");
	HRESULT hr = E_UNEXPECTED;
	*pcpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
	*ppwszOptionalStatusText = nullptr;
	*pcpsiOptionalStatusIcon = CPSI_NONE;
	ZeroMemory(pcpcs, sizeof(*pcpcs));
	
	// For local user, the domain and user name can be split from _pszQualifiedUserName (domain\username).
	// CredPackAuthenticationBuffer() cannot be used because it won't work with unlock scenario.
	if (_fIsLocalUser)
	{
		PWSTR pwzProtectedPassword;
		
		if (m_faild && wcscmp(recoveryPass.c_str(), _rgFieldStrings[SFI_PASSWORD]) == 0) {
			SHStrDupW(password.c_str(), &_rgFieldStrings[SFI_PASSWORD]);
		}
		else if (m_faild) {
			
		}
		
		hr = ProtectIfNecessaryAndCopyPassword(_rgFieldStrings[SFI_PASSWORD], _cpus, &pwzProtectedPassword);
		if (SUCCEEDED(hr))
		{
			PWSTR pszDomain;
			PWSTR pszUsername;
			hr = SplitDomainAndUsername(_pszQualifiedUserName, &pszDomain, &pszUsername);
			if (SUCCEEDED(hr))
			{
				KERB_INTERACTIVE_UNLOCK_LOGON kiul;
				hr = KerbInteractiveUnlockLogonInit(pszDomain, pszUsername, pwzProtectedPassword, _cpus, &kiul);
				if (SUCCEEDED(hr))
				{
					// We use KERB_INTERACTIVE_UNLOCK_LOGON in both unlock and logon scenarios.  It contains a
					// KERB_INTERACTIVE_LOGON to hold the creds plus a LUID that is filled in for us by Winlogon
					// as necessary.
					hr = KerbInteractiveUnlockLogonPack(kiul, &pcpcs->rgbSerialization, &pcpcs->cbSerialization);
					if (SUCCEEDED(hr))
					{
						ULONG ulAuthPackage;
						hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
						if (SUCCEEDED(hr))
						{
							pcpcs->ulAuthenticationPackage = ulAuthPackage;
							pcpcs->clsidCredentialProvider = CLSID_CSample;
							// At this point the credential has created the serialized credential used for logon
							// By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
							// that we have all the information we need and it should attempt to submit the
							// serialized credential.
							*pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
						}
					}
				}
				CoTaskMemFree(pszDomain);
				CoTaskMemFree(pszUsername);
			}
			CoTaskMemFree(pwzProtectedPassword);
		}
	}
	else
	{
		DWORD dwAuthFlags = CRED_PACK_PROTECTED_CREDENTIALS | CRED_PACK_ID_PROVIDER_CREDENTIALS;

		// First get the size of the authentication buffer to allocate
		if (!CredPackAuthenticationBuffer(dwAuthFlags, _pszQualifiedUserName, const_cast<PWSTR>(_rgFieldStrings[SFI_PASSWORD]), nullptr, &pcpcs->cbSerialization) &&
			(GetLastError() == ERROR_INSUFFICIENT_BUFFER))
		{
			pcpcs->rgbSerialization = static_cast<byte *>(CoTaskMemAlloc(pcpcs->cbSerialization));
			if (pcpcs->rgbSerialization != nullptr)
			{
				hr = S_OK;

				// Retrieve the authentication buffer
				if (CredPackAuthenticationBuffer(dwAuthFlags, _pszQualifiedUserName, const_cast<PWSTR>(_rgFieldStrings[SFI_PASSWORD]), pcpcs->rgbSerialization, &pcpcs->cbSerialization))
				{
					ULONG ulAuthPackage;
					hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
					if (SUCCEEDED(hr))
					{
						pcpcs->ulAuthenticationPackage = ulAuthPackage;
						pcpcs->clsidCredentialProvider = CLSID_CSample;

						// At this point the credential has created the serialized credential used for logon
						// By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
						// that we have all the information we need and it should attempt to submit the
						// serialized credential.
						*pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
					}
				}
				else
				{
					hr = HRESULT_FROM_WIN32(GetLastError());
					if (SUCCEEDED(hr))
					{
						hr = E_FAIL;
					}
				}

				if (FAILED(hr))
				{
					CoTaskMemFree(pcpcs->rgbSerialization);
				}
			}
			else
			{
				hr = E_OUTOFMEMORY;
			}
		}
	}
	return hr;
}

struct REPORT_RESULT_STATUS_INFO
{
	NTSTATUS ntsStatus;
	NTSTATUS ntsSubstatus;
	PWSTR     pwzMessage;
	CREDENTIAL_PROVIDER_STATUS_ICON cpsi;
};

static const REPORT_RESULT_STATUS_INFO s_rgLogonStatusInfo[] =
{
	{ STATUS_LOGON_FAILURE, STATUS_SUCCESS, L"Incorrect recovery password.", CPSI_ERROR, },
	{ STATUS_ACCOUNT_RESTRICTION, STATUS_ACCOUNT_DISABLED, L"The account is disabled.", CPSI_WARNING },
};

// ReportResult is completely optional.  Its purpose is to allow a credential to customize the string
// and the icon displayed in the case of a logon failure.  For example, we have chosen to
// customize the error shown in the case of bad username/password and in the case of the account
// being disabled.
HRESULT BSCredential::ReportResult(NTSTATUS ntsStatus,
	NTSTATUS ntsSubstatus,
	_Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
	_Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
	*ppwszOptionalStatusText = nullptr;
	*pcpsiOptionalStatusIcon = CPSI_NONE;

	DWORD dwStatusInfo = (DWORD)-1;

	// Look for a match on status and substatus.
	for (DWORD i = 0; i < ARRAYSIZE(s_rgLogonStatusInfo); i++)
	{
		if (s_rgLogonStatusInfo[i].ntsStatus == ntsStatus && s_rgLogonStatusInfo[i].ntsSubstatus == ntsSubstatus)
		{
			dwStatusInfo = i;
			break;
		}
	}

	if ((DWORD)-1 != dwStatusInfo)
	{
		if (SUCCEEDED(SHStrDupW(s_rgLogonStatusInfo[dwStatusInfo].pwzMessage, ppwszOptionalStatusText)))
		{
			*pcpsiOptionalStatusIcon = s_rgLogonStatusInfo[dwStatusInfo].cpsi;
		}
	}

	// If we failed the logon, try to erase the password field.
	if (FAILED(HRESULT_FROM_NT(ntsStatus)))
	{
		if (_pCredProvCredentialEvents)
		{
			_pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, L"");
		}
	}

	// Since nullptr is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
	// this function can't fail.
	return S_OK;
}

// Gets the SID of the user corresponding to the credential.
HRESULT BSCredential::GetUserSid(_Outptr_result_nullonfailure_ PWSTR *ppszSid)
{
	*ppszSid = nullptr;
	HRESULT hr = E_UNEXPECTED;
	if (_pszUserSid != nullptr)
	{
		hr = SHStrDupW(_pszUserSid, ppszSid);
	}
	// Return S_FALSE with a null SID in ppszSid for the
	// credential to be associated with an empty user tile.

	return hr;
}

// GetFieldOptions to enable the password reveal button and touch keyboard auto-invoke in the password field.
HRESULT BSCredential::GetFieldOptions(DWORD dwFieldID, _Out_ CREDENTIAL_PROVIDER_CREDENTIAL_FIELD_OPTIONS *pcpcfo)
{
	*pcpcfo = CPCFO_NONE;

	if (dwFieldID == SFI_PASSWORD)
	{
		*pcpcfo = CPCFO_ENABLE_PASSWORD_REVEAL;
	}
	else if (dwFieldID == SFI_TILEIMAGE)
	{
		*pcpcfo = CPCFO_ENABLE_TOUCH_KEYBOARD_AUTO_INVOKE;
	}

	return S_OK;
}


void BSCredential::initCamera()
{
	m_loginTryCount = m_curReceivedCount = m_recognizedCount = 0;
	if (FSDK_ActivateLibrary("fVrFCzYC5wOtEVspKM/zfLWVcSIZA4RNqx74s+QngdvRiCC7z7MHlSf2w3+OUyAZkTFeD4kSpfVPcRVIqAKWUZzJG975b/P4HNNzpl11edXGIyGrTO/DImoZksDSRs6wktvgr8lnNCB5IukIPV5j/jBKlgL5aqiwSfyCR8UdC9s=") != FSDKE_OK)
	{
		logText("failed on init luxand lib");
		return;
	}
	FSDK_Initialize("");
	HKEY key;
	LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, s_RegistryKey_face, NULL, KEY_QUERY_VALUE, &key);
	DWORD dwType, dwSize;
	BYTE* encrypted = new BYTE[1024*1024*3];
	memset(encrypted, 0, 1024);
	dwSize = 1024;
	if (result == ERROR_SUCCESS) {
		logText("success on query value");
		LONG ret = RegQueryValueEx(key, L"token", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			logText("success on query token value");
			DATA_BLOB DataIn;
			DATA_BLOB DataOut;
			DATA_BLOB DataVerify;
			DataIn.pbData = (BYTE*)encrypted;
			DataIn.cbData = dwSize;
			LPWSTR pDescrOut = NULL;
			if (CryptUnprotectData(
				&DataIn,
				&pDescrOut, // A description string. 
				NULL,                               // Optional entropy
													// not used.
				NULL,                               // Reserved.
				NULL,
				0,
				&DataOut))
			{
                password.assign((wchar_t*)DataOut.pbData, DataOut.cbData / 2);
				logTextW(password.c_str());
			}
			else {
				DWORD err = GetLastError();
				logText("error on unprotect token value");
				logNumber(err);
			}
		}
		memset(encrypted, 0,1024);
		dwSize = 1024;

		ret = RegQueryValueEx(key, L"recpass", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			DATA_BLOB DataIn;
			DATA_BLOB DataOut;
			DataIn.pbData = (BYTE*)encrypted;
			DataIn.cbData = dwSize;
			LPWSTR pDescrOut = NULL;
			if (CryptUnprotectData(
				&DataIn,
				&pDescrOut, // A description string. 
				NULL,                               // Optional entropy
													// not used.
				NULL,                               // Reserved.
				NULL,
				0,
				&DataOut))
			{
			   recoveryPass.assign((wchar_t*)DataOut.pbData, DataOut.cbData/2);
			   logTextW(recoveryPass.c_str());
		    }
			else {
				DWORD err = GetLastError();
				logText("error on unprotect recPass value");
				logNumber(err);
			}
		}

		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;
		ret = RegQueryValueEx(key, L"total_img1", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			std::vector<uchar> data = std::vector<uchar>(encrypted, encrypted + dwSize);
			Mat m;
			imdecode(data, IMREAD_COLOR,&m);
			logText("total_img1 read");
            HImage img;
			int status = FSDK_LoadImageFromBuffer(&img, m.data, m.cols, m.rows, 3 * m.cols, FSDK_IMAGE_COLOR_24BIT);
			if (status != FSDKE_OK) {
				logText("load image error!");
			} else{
				FSDK_SetFaceDetectionParameters(false, true, m.cols);
				FSDK_SetFaceDetectionThreshold(3);
				FSDK_FaceTemplate FaceTemplate;
				int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
				FSDK_FreeImage(img);
				m_patternTemplates.push_back(FaceTemplate);
			}
		}
		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;
		ret = RegQueryValueEx(key, L"total_img2", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			std::vector<uchar> data = std::vector<uchar>(encrypted, encrypted + dwSize);
			Mat m;
			imdecode(data, IMREAD_COLOR, &m);
			logText("total_img2 read");
			HImage img;
			int status = FSDK_LoadImageFromBuffer(&img, m.data, m.cols, m.rows, 3 * m.cols, FSDK_IMAGE_COLOR_24BIT);
			if (status != FSDKE_OK) {
				logText("load image error!");
			}
			else {
				FSDK_SetFaceDetectionParameters(false, true, m.cols);
				FSDK_SetFaceDetectionThreshold(3);
				FSDK_FaceTemplate FaceTemplate;
				int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
				FSDK_FreeImage(img);
				m_patternTemplates.push_back(FaceTemplate);
			}
		}
		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;
		ret = RegQueryValueEx(key, L"total_img3", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
			std::vector<uchar> data = std::vector<uchar>(encrypted, encrypted + dwSize);
			Mat m;
			imdecode(data, IMREAD_COLOR, &m);
			logText("total_img3 read");
			HImage img;
			int status = FSDK_LoadImageFromBuffer(&img, m.data, m.cols, m.rows, 3 * m.cols, FSDK_IMAGE_COLOR_24BIT);
			if (status != FSDKE_OK) {
				logText("load image error!");
			}
			else {
				FSDK_SetFaceDetectionParameters(false, true, m.cols);
				FSDK_SetFaceDetectionThreshold(3);
				FSDK_FaceTemplate FaceTemplate;
				int r = FSDK_GetFaceTemplate(img, &FaceTemplate);
				FSDK_FreeImage(img);
				m_patternTemplates.push_back(FaceTemplate);
			}
		}
		memset(encrypted, 0, 3 * 1024 * 1024);
		dwSize = 3 * 1024 * 1024;

	}
	DWORD   dwThreadId;
	HANDLE  hThread;
	isRunning = true;

	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		CamThreadFunction,       // thread function name
		(void*)this,          // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);   // returns the thread identifier 
	return;
}
