
// UserFaceSignUpDlg.h : header file
//

#pragma once

#include "CCustomImageView.h"
#include "LuxandFaceSDK.h"
// CUserFaceSignUpDlg dialog
class CUserFaceSignUpDlg : public CDialogEx
{
// Construction
public:
	CUserFaceSignUpDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_USERFACESIGNUP_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CCustomImageView m_cameraPreview;
	CString m_confirmPassword;
	CString m_curPassword;
	CCustomImageView m_firstImage;
	CIPAddressCtrl m_serverIP;
	CString m_recPassword;
	CCustomImageView m_secondImage;
    CString m_userName;
	CCustomImageView m_thirdImage;
	afx_msg void OnBnClickedCapture();
	afx_msg void OnBnClickedOk();
public:public:
	void receivedFrame(Mat image,Mat oldImage,int count,Rect faceRect);
	Mat m_img;
    bool m_isRunning;
	bool m_shouldExit;
	bool m_requireCapture;
	int cur_image_index;
	Mat *m_images;
	vector<FSDK_FaceTemplate> m_patternTemplates;
	vector<Mat> pattern_images;
	vector<int> labels;
	bool initComponents();
	void trainModel();
	bool m_saved;
	afx_msg void OnBnClickedCancel();
};
