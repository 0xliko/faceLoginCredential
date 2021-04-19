#pragma once

class CCustomImageView :public CStatic
{
public:
	CCustomImageView() {};
	~CCustomImageView() {};
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
	
public:
	
	Mat m_bmp;

};

