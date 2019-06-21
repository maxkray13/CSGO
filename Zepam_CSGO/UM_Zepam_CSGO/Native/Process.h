#pragma once
#include "../stdafx.h"

PLDR_DATA_TABLE_ENTRY
LdrpFindDataTableW(
	const wchar_t* BaseName
);

PLDR_DATA_TABLE_ENTRY
LdrpFindDataTableA(
	const char* BaseName
);

PVOID
LdrFindModuleW(
	const wchar_t* BaseName
);

PVOID
LdrFindModuleA(
	const char* BaseName
);