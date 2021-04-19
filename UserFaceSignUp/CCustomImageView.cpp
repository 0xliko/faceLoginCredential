#include "pch.h"
#include "framework.h"
#include "CCustomImageView.h"

BEGIN_MESSAGE_MAP(CCustomImageView, CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CCustomImageView::OnPaint() {
	CPaintDC dc(this);
	if (!m_bmp.empty()) {
		HBITMAP bmp = ConvertCVMatToBMP(m_bmp);
		CRect rt;
		GetClientRect(&rt);
		CBitmap bitmap; // Sequence is important
		CDC dcMemory;
		bitmap.Attach(bmp);
		
		BITMAP bitmapInfo;
		GetObject(bmp, sizeof(BITMAP),&bitmapInfo);

		dcMemory.CreateCompatibleDC(&dc);
        dcMemory.SelectObject(&bitmap);
		dc.SetStretchBltMode(COLORONCOLOR);
		dc.StretchBlt(0, 0, rt.Width(), rt.Height(), &dcMemory, 0, 0, bitmapInfo.bmWidth,bitmapInfo.bmHeight,SRCCOPY);
	}
	CStatic::OnPaint();
}
