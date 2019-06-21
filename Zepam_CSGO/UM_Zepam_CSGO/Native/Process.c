#include "Native.h"
#include "../CRT/CRT.h"

PLDR_DATA_TABLE_ENTRY LdrpFindDataTableW(const wchar_t * BaseName) {

	const PLIST_ENTRY Head = &__GetPeb()->Ldr->InMemoryOrderModuleList;
	PLIST_ENTRY Next = Head->Flink;
	PLDR_DATA_TABLE_ENTRY LdrEntry = NULL;
	wchar_t SrcName[MAX_PATH];

	wchar_t* ComperandName = alloca(RtlStringSizeW(BaseName));

	if (!ComperandName) {

		return NULL;
	}

	wcscpy(ComperandName, BaseName);

	if (!AsciiAsciiToLowerCaseW(ComperandName, ComperandName)) {

		return NULL;
	}

	while (Next != Head) {

		LdrEntry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

		if (AsciiAsciiToLowerCaseW(SrcName, LdrEntry->BaseDllName.Buffer)) {

			if (wcscmp(SrcName, ComperandName) == 0) {

				return LdrEntry;
			}
		}
		Next = Next->Flink;
	}

	return NULL;
}

PLDR_DATA_TABLE_ENTRY LdrpFindDataTableA(const char * BaseName) {

	const wchar_t* ComperandName = alloca(RtlStringSizeA(BaseName) * sizeof(wchar_t));

	if (!ComperandName) {

		return NULL;
	}

	AsciiCharToWide(ComperandName, BaseName);

	return LdrpFindDataTableW(ComperandName);
}

PVOID LdrFindModuleW(const wchar_t * BaseName) {

	PLDR_DATA_TABLE_ENTRY LdrData = LdrpFindDataTableW(BaseName);

	if (LdrData != NULL) {

		return LdrData->DllBase;
	}

	return NULL;
}

PVOID LdrFindModuleA(const char * BaseName) {

	PLDR_DATA_TABLE_ENTRY LdrData = LdrpFindDataTableA(BaseName);

	if (LdrData != NULL) {

		return LdrData->DllBase;
	}

	return NULL;
}