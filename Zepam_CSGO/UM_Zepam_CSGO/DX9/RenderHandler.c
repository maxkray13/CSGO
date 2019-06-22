#include "RenderHandler.h"

Dx9Info g_Dx9Info;

static DWORD_PTR PtrDrawPoints = 0;

void* StackWalk(void** ebp) {

	if (ebp == NULL) {

		return NULL;
	}

	void** next = *(void***)(ebp);
	if (ebp[1] == (void*)PtrDrawPoints) {

		return next[4];
	}

	return StackWalk(next);
}

FORCEINLINE void* get_ent() {

	void* Stack = 0;
	__asm {
		mov Stack, ebp
	}
	return StackWalk((void**)(Stack));
}

HRESULT GenerateTexture(IDirect3DDevice9 *pD3Ddev, IDirect3DTexture9 **ppD3Dtex, DWORD colour32) {

	if (FAILED(pD3Ddev->lpVtbl->CreateTexture(pD3Ddev, 8, 8, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, ppD3Dtex, NULL))) {

		return E_FAIL;
	}
	D3DLOCKED_RECT d3dlr;
	(*ppD3Dtex)->lpVtbl->LockRect(*ppD3Dtex, 0, &d3dlr, 0, 0);
	DWORD *pDst = (DWORD*)d3dlr.pBits;

	for (int xy = 0; xy < 8 * 8 * 4; xy++) {

		*pDst++ = colour32;
	}

	(*ppD3Dtex)->lpVtbl->UnlockRect(*ppD3Dtex, 0);

	return S_OK;
}

LPDIRECT3DTEXTURE9  Red, Green;
BOOLEAN InitializeInternals = TRUE;
BOOLEAN Recursion = FALSE;

HRESULT hkDrawIndexedPrimitive(HANDLE hDevice, CONST D3DDDIARG_DRAWINDEXEDPRIMITIVE * Arg) {

	if (Recursion) {

		return g_Dx9Info.OriginalDrawIdx(hDevice, Arg);
	}
	if (InitializeInternals) {

		char Module[] = { 's','t','u','d','i','o','r','e','n','d','e','r','.','d','l','l','\0' };
		char Section[] = { '.','t','e','x','t','\0' };
		BYTE Pattern[] = { 0x8B,0x7D,0xFC,0x03,0xF8 };

		PtrDrawPoints = (DWORD_PTR)RtlFindPattern(Module, Section, Pattern, 0xCC);

		ZeroMemory(Module, sizeof(Module));
		ZeroMemory(Section, sizeof(Section));
		ZeroMemory(Pattern, sizeof(Pattern));

		GenerateTexture(g_Dx9Info.D3DDev, &Red, D3DCOLOR_RGBA(255, 0, 0, 255));
		GenerateTexture(g_Dx9Info.D3DDev, &Green, D3DCOLOR_RGBA(0, 255, 0, 255));

		InitializeInternals = FALSE;

		return g_Dx9Info.OriginalDrawIdx(hDevice, Arg);
	}
	PVOID Entity = get_ent();
	if (Entity) {

		UINT EntityTeam = *(UINT*)((DWORD_PTR)Entity + 0xF4);

		if (EntityTeam) {

			Recursion = TRUE;

			D3DDDIARG_RENDERSTATE RenderState;
			RenderState.State = D3DDDIRS_ZENABLE;
			RenderState.Value = FALSE;

			g_Dx9Info.D3DDev->lpVtbl->SetTexture(g_Dx9Info.D3DDev, 0, (IDirect3DBaseTexture9*)Green);
			g_Dx9Info.DeviceFuncs->pfnSetRenderState(hDevice, &RenderState);

			g_Dx9Info.D3DDev->lpVtbl->DrawIndexedPrimitive(
				g_Dx9Info.D3DDev,
				Arg->PrimitiveType,
				Arg->BaseVertexIndex,
				Arg->MinIndex,
				Arg->NumVertices,
				Arg->StartIndex,
				Arg->PrimitiveCount
			);
			RenderState.Value = TRUE;

			g_Dx9Info.D3DDev->lpVtbl->SetTexture(g_Dx9Info.D3DDev, 0, (IDirect3DBaseTexture9*)Red);
			g_Dx9Info.DeviceFuncs->pfnSetRenderState(hDevice, &RenderState);

			g_Dx9Info.D3DDev->lpVtbl->DrawIndexedPrimitive(
				g_Dx9Info.D3DDev,
				Arg->PrimitiveType,
				Arg->BaseVertexIndex,
				Arg->MinIndex,
				Arg->NumVertices,
				Arg->StartIndex,
				Arg->PrimitiveCount
			);

			Recursion = FALSE;
			return S_OK;
		}
	}

	return g_Dx9Info.OriginalDrawIdx(hDevice, Arg);
}

DWORD_PTR   OffsetAdapter = 0;
DWORD_PTR   OffsetBatch = 0;
DWORD_PTR   OffsetOriginalTable = 0;

BOOLEAN SolveAboveWin7(DWORD_PTR Temp) {

	HDE_STRUCT hde;
	ULONG Len;
	BOOLEAN FirstProbeChunk = TRUE;
	ULONG AdapterModRM = (ULONG)-1;

	do {
		Len = hde_disasm((void*)Temp, &hde);
		if (FirstProbeChunk) {// mov     reg1, [reg2+ptr_Adapter]

			if (hde.disp8)
				OffsetAdapter = hde.disp8;
			if (hde.disp16)
				OffsetAdapter = hde.disp16;
			if (hde.disp32)
				OffsetAdapter = hde.disp32;

			AdapterModRM = hde.modrm_reg;

			Temp += Len;
			Len = hde_disasm((void*)Temp, &hde);//push    reg_
			Temp += Len;
			Len = hde_disasm((void*)Temp, &hde);//mov     reg3, [reg1 + OffsetBatch]
			if (hde.opcode == 0x8B) {

				if (hde.disp8)
					OffsetBatch = hde.disp8;
				if (hde.disp16)
					OffsetBatch = hde.disp16;
				if (hde.disp32)
					OffsetBatch = hde.disp32;
			}
			else {

				return FALSE;
			}

			FirstProbeChunk = FALSE;
		}
		else {

			if (hde.opcode == 0x8D && hde.modrm_rm == AdapterModRM) {//lea     reg4, [reg1+OffsetOriginalTable]

				if (hde.disp8)
					OffsetOriginalTable = hde.disp8;
				if (hde.disp16)
					OffsetOriginalTable = hde.disp16;
				if (hde.disp32)
					OffsetOriginalTable = hde.disp32;
				break;
			}
			else if (hde.opcode == 0x81 && hde.modrm_rm == AdapterModRM) {//add     reg1, OffsetOriginalTable

				if (hde.imm8)
					OffsetOriginalTable = hde.imm8;
				if (hde.imm16)
					OffsetOriginalTable = hde.imm16;
				if (hde.imm32)
					OffsetOriginalTable = hde.imm32;
				break;
			}
		}

		Temp += Len;
	} while (
		hde.opcode != 0xC3 &&
		hde.opcode != 0xC2 &&
		hde.opcode != 0xCC
		);

	return OffsetAdapter != 0 && OffsetBatch != 0 && OffsetOriginalTable != 0;
}

BOOLEAN SolveWin7(DWORD_PTR Temp) {

	HDE_STRUCT hde;
	ULONG Len;
	BOOLEAN FirstMove = TRUE;

	do {
		Len = hde_disasm((void*)Temp, &hde);

		if (FirstMove) {

			if (hde.opcode == 0x8B) {

				if (hde.disp8)
					OffsetAdapter = hde.disp8;
				if (hde.disp16)
					OffsetAdapter = hde.disp16;
				if (hde.disp32)
					OffsetAdapter = hde.disp32;
				FirstMove = FALSE;
			}
		}
		else {

			if (hde.opcode == 0xE8) {

				Temp = Temp + hde.len + hde.rel32;
				continue;
			}
			if ((hde.opcode == 0x8B || hde.opcode == 0x8D) && hde.len == 0x6) {//mov||lea

				if (hde.opcode == 0x8B) {//mov
					if (hde.disp8)
						OffsetBatch = hde.disp8;
					if (hde.disp16)
						OffsetBatch = hde.disp16;
					if (hde.disp32)
						OffsetBatch = hde.disp32;
				}
				else {//0x8D lea

					if (hde.disp8)
						OffsetOriginalTable = hde.disp8;
					if (hde.disp16)
						OffsetOriginalTable = hde.disp16;
					if (hde.disp32)
						OffsetOriginalTable = hde.disp32;
				}
			}
		}

		if (OffsetAdapter && OffsetBatch && OffsetOriginalTable)
			break;

		Temp += Len;
	} while (
		hde.opcode != 0xC3 &&
		hde.opcode != 0xC2 &&
		hde.opcode != 0xCC
		);

	return OffsetAdapter != 0 && OffsetBatch != 0 && OffsetOriginalTable != 0;
}

BOOLEAN AttachToDx9() {

	ZeroMemory(&g_Dx9Info, sizeof(Dx9Info));
	HDE_STRUCT hde;
	char Section[] = { '.','t','e','x','t','\0' };
	char Module[] = { 'd','3','d','9','.','d','l','l','\0' };
	char Module1[] = { 's','h','a','d','e','r','a','p','i','d','x','9','.','d','l','l','\0' };
	BYTE Pattern[] = { 0xE8, 0xCC, 0xCC, 0xCC, 0xCC, 0x83, 0xCC, 0x2C, 0xDF };
	BYTE Pattern1[] = { 0xA1,0xCC,0xCC,0xCC,0xCC,0xCC,0x8B,0xCC,0xFF,0xCC,0xA8 };

	DWORD_PTR PtrEnableFullScreen = (DWORD_PTR)RtlFindPattern(Module, Section, Pattern, 0xCC);//E8 ? ? ? ? 83 ? 2C DF call CBaseDevice::StartDDIThreading
	if (!PtrEnableFullScreen) {

		return FALSE;
	}

	ULONG len = hde_disasm((void*)PtrEnableFullScreen, &hde);

	if (hde.opcode != 0xE8) {

		return FALSE;
	}

	DWORD_PTR Temp = PtrEnableFullScreen + hde.len + hde.rel32; //StartDDIThreading;
	BOOLEAN IsInChunk = FALSE;
	BOOLEAN SolveDx = FALSE;

	do {
		len = hde_disasm((void*)Temp, &hde);
		if (!IsInChunk) {

			if (hde.opcode == 0x0F && (hde.opcode2 >= 0x80 && hde.opcode2 <= 0x8F)) {//JNZ -> should jump to chunk

				if (hde.rel16) {

					Temp = Temp + hde.len + hde.rel16;
				}
				else if (hde.rel32) {

					Temp = Temp + hde.len + hde.rel32;
				}

				IsInChunk = TRUE;
				continue;
			}
			else if (hde.opcode == 0x74) {//JZ -> keep

				IsInChunk = TRUE;
			}
		}
		else {
			
			if (hde.opcode == 0x8B) {// mov     reg1, [reg2+OffsetAdapter]

				SolveDx = SolveAboveWin7(Temp);
			}
			else {//win 7  D3D8GetDriverFunctions

				SolveDx = SolveWin7(Temp);
			}
			break;
		}

		Temp += len;

	} while (
		hde.opcode != 0xC3 &&
		hde.opcode != 0xC2 &&
		hde.opcode != 0xCC
		);

	if (!SolveDx) {

		return FALSE;
	}

	DWORD_PTR DevicePtr = (DWORD_PTR)RtlFindPattern(Module1, Section, Pattern1, 0xCC);

	if (!DevicePtr) {

		return FALSE;
	}

	DevicePtr += 0x1;
	DevicePtr = **(DWORD_PTR**)DevicePtr;
	g_Dx9Info.D3DDev = (IDirect3DDevice9 *)DevicePtr;

	DWORD_PTR m_Adapter = *(DWORD_PTR *)(DevicePtr + OffsetAdapter );//0x4D4
	DWORD_PTR m_Batch = *(DWORD_PTR *)(m_Adapter + OffsetBatch);// 0x698;

	g_Dx9Info.DeviceFuncs = (D3DDDI_DEVICEFUNCS *)(m_Adapter + OffsetOriginalTable);//0x6A0;D3DDDI_DEVICEFUNCS *;
	g_Dx9Info.DevMethodsExtra = (DWORD_PTR *)(m_Batch + 0x60);
	g_Dx9Info.OriginalDrawIdx = g_Dx9Info.DeviceFuncs->pfnDrawIndexedPrimitive;
	g_Dx9Info.DeviceFuncs->pfnDrawIndexedPrimitive = (PFND3DDDI_DRAWINDEXEDPRIMITIVE)hkDrawIndexedPrimitive;

	ZeroMemory(Module1, sizeof(Module1));
	ZeroMemory(Pattern1, sizeof(Pattern1));
	ZeroMemory(Section, sizeof(Section));
	ZeroMemory(Pattern, sizeof(Pattern));
	ZeroMemory(Module, sizeof(Module));

	return TRUE;
}
