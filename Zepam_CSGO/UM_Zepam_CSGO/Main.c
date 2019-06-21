#include "stdafx.h"
#include "CRT/CRT.h"
#include "Native/Native.h"

#include "DX9/RenderHandler.h"

void StrToUTF8(const char * s, char* s_out)
{
	wchar_t wbuffer[256];
	impl_MultiByteToWideChar(CP_ACP, 0, s, 256, wbuffer, 256);
	impl_WideCharToMultiByte(CP_UTF7, 0, wbuffer, 256, s_out, 256, 0, 0);
}
static BOOLEAN InitializeDone = FALSE;
BOOL APIENTRY DllMain(HINSTANCE hDll, DWORD Reason, LPVOID Reserved) {

	UNREFERENCED_PARAMETER(hDll);
	UNREFERENCED_PARAMETER(Reason);
	UNREFERENCED_PARAMETER(Reserved);
	if (!InitializeDone) {
		InitCRT();

		AttachToDx9();

		InitializeDone = TRUE;
	}
	
	
	return TRUE;
}