#pragma once
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#include <windows.h>
#include <intrin.h>
#include <d3d9.h>
#include <d3dx9.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#include <D3dumddi.h>
#include "NT.h"
#include "hde.h"
#pragma comment(lib,"hde.lib")

#pragma region Memory

/***/#pragma function(memcmp)
/***/#pragma function(memset)
/***/#pragma function(memcpy)
/***/#pragma function(memmove)

#pragma endregion



#pragma region String

/***/#pragma function(strlen)
/***/#pragma function(strcmp)
/***/#pragma function(strcat)
/***/#pragma function(strcpy)

#pragma endregion



#pragma region WString

/***/#pragma function(wcslen)
/***/#pragma function(wcscmp)
/***/#pragma function(wcscat)
/***/#pragma function(wcscpy)

#pragma endregion