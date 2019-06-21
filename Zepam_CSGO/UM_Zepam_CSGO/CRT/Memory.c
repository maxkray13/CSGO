#include "CRT.h"

int memcmp(const void *Buf1, const void *Buf2, size_t n) {

	if (!n) {

		return 0;
	}
	
	const unsigned char *Ptr1 = (const unsigned char*)Buf1;
	const unsigned char *Ptr2 = (const unsigned char*)Buf2;

	for (size_t i = 0; i < n; i++) {

		if (Ptr1[i] != Ptr2[i])
			return Ptr1[i] - Ptr2[i];
	}

	return 0;
}

void *memset(void *Dst, int Val, size_t Size) {

	__stosb((BYTE*)Dst, (BYTE)Val, Size);
	return Dst;
}

void *memcpy(void *Dst, const void *Src, size_t Size) {

	__movsb((BYTE*)Dst, (const BYTE*)Src, Size);
	return Dst;
}

void *memmove(void *Dst, const void *Src, size_t n)
{
	void *ret = Dst;
	if (Dst <= Src || (char *)Dst >= ((char *)Src + n)) {

		while (n--) {

			*(char*)Dst = *(char*)Src;
			Dst = (char*)Dst + 1;
			Src = (char*)Src + 1;
		}
	}
	else {

		Dst = (char*)Dst + n - 1;
		Src = (char*)Src + n - 1;

		while (n--) {

			*(char*)Dst = *(char*)Src;
			Dst = (char*)Dst - 1;
			Src = (char*)Src - 1;
		}
	}

	return ret;
}