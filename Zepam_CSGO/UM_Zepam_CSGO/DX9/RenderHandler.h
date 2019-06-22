#pragma once
#include "../Native/Native.h"

typedef struct _Dx9Info {

	D3DDDI_DEVICEFUNCS * DeviceFuncs;
	DWORD_PTR * DevMethodsExtra;

	IDirect3DDevice9 * D3DDev;
	PFND3DDDI_DRAWINDEXEDPRIMITIVE OriginalDrawIdx;

}Dx9Info;
extern Dx9Info g_Dx9Info;

BOOLEAN AttachToDx9();
