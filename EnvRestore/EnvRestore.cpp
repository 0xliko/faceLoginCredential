// EnvRestore.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <lmaccess.h>
#include <Wincrypt.h>
#include<tchar.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "Crypt32.lib")
#define s_RegistryKey_face  _T("SOFTWARE\\FaceLogin\\FaceCredentialProvider")
int main()
{
    std::cout << "restore already password";
	std::wstring password;
	HKEY key;
	LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, s_RegistryKey_face, NULL, KEY_QUERY_VALUE, &key);
	DWORD dwType, dwSize;
	BYTE* encrypted = new BYTE[1024];
	memset(encrypted, 0, 1024);
	dwSize = 1024;
	if (result == ERROR_SUCCESS) {
		
		LONG ret = RegQueryValueEx(key, L"token", NULL, &dwType, (BYTE*)encrypted, &dwSize);
		if (ret == ERROR_SUCCESS) {
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
				std::wstring old = password.substr(4, password.size() - 8);
				LPTSTR  pUserName = new WCHAR[128];
				memset((void*)pUserName, 0, 256);
				DWORD bufferSize = 128;
				GetUserName(pUserName, &bufferSize);
				NET_API_STATUS status = NetUserChangePassword(NULL, pUserName, password.c_str(),old.c_str());
				if (status == ERROR_INVALID_PASSWORD) {
				  	
			    }
			}
			else{
				
			}
		}
	
	}
}

