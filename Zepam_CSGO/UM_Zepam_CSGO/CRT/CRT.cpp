#include "CRT.h"
#include "MultiByte.h"
#include "Alloc.h"

VOID InitCRT() {

	InitAllocations();
	InitMultiByteTranslation();
}