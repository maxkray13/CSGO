#include "CRT.h"


#pragma section(".heap$1", read, write)
#pragma section(".heap$2", read, write)
#pragma section(".heap$3", read, write)

__declspec(allocate(".heap$1")) char HeapBuffA[0x8000];
/*__declspec(allocate(".heap$2")) char HeapBuffB[0x8000];
__declspec(allocate(".heap$2")) char HeapBuffC[0x8000];
__declspec(allocate(".heap$2")) char HeapBuffD[0x8000];
__declspec(allocate(".heap$2")) char HeapBuffE[0x8000];
__declspec(allocate(".heap$2")) char HeapBuffF[0x8000];*/
__declspec(allocate(".heap$3")) char HeapEnd;

typedef struct _HEAP_ENTRY
{
	BOOL		Used;
	SIZE_T		Size;
	DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) LIST_ENTRY	HeapLinks;
	DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) LIST_ENTRY	HeapFreeLinks;
	CHAR		Data;
} HEAP_ENTRY, *PHEAP_ENTRY;

DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) LIST_ENTRY	HeapEntries;
DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) LIST_ENTRY	HeapFreeEntries;

SEMAPHORE g_AllocatorSema;

VOID InitAllocations() {

	InitializeListHead(&HeapEntries);
	InitializeListHead(&HeapFreeEntries);
}

__declspec(allocator,restrict) void * __cdecl malloc(size_t _Size) {

	UNREFERENCED_PARAMETER(HeapBuffA);
	/*UNREFERENCED_PARAMETER(HeapBuffB);
	UNREFERENCED_PARAMETER(HeapBuffC);
	UNREFERENCED_PARAMETER(HeapBuffD);
	UNREFERENCED_PARAMETER(HeapBuffE);
	UNREFERENCED_PARAMETER(HeapBuffF);*/
	UNREFERENCED_PARAMETER(HeapEnd);

	LockSemaphore(&g_AllocatorSema);

	const PLIST_ENTRY	Head = &HeapFreeEntries;
	PLIST_ENTRY			Next = Head->Flink;
	PHEAP_ENTRY			NewEntry = NULL;
	PHEAP_ENTRY			Entry = NULL;

	while (Next != Head) {

		Entry = CONTAINING_RECORD(Next, HEAP_ENTRY, HeapFreeLinks);

		if (Entry->Size >= _Size) {

			RemoveEntryList(&Entry->HeapFreeLinks);

			const INT_PTR Spare = Entry->Size - _Size - FIELD_OFFSET(HEAP_ENTRY, Data);

			if (Spare > FIELD_OFFSET(HEAP_ENTRY, Data) * 0x8) {

				NewEntry = (PHEAP_ENTRY)(&Entry->Data + _Size);
				NewEntry->Size = Spare;
				NewEntry->Used = FALSE;
				InsertHeadList(&Entry->HeapLinks, &NewEntry->HeapLinks);
				InsertTailList(&HeapFreeEntries, &NewEntry->HeapFreeLinks);
				Entry->Size = _Size;
			}

			Entry->Used = TRUE;

			UnlockSemaphore(&g_AllocatorSema);
			return &Entry->Data;
		}

		Next = Next->Flink;
	}

	if (IsListEmpty(&HeapEntries)) {

		NewEntry = (PHEAP_ENTRY)ALIGN_UP(HeapBuffA, MEMORY_ALLOCATION_ALIGNMENT);
	}
	else {

		Entry = CONTAINING_RECORD(HeapEntries.Blink, HEAP_ENTRY, HeapLinks);
		NewEntry = (PHEAP_ENTRY)ALIGN_UP(&Entry->Data + Entry->Size, MEMORY_ALLOCATION_ALIGNMENT);
	}

	if (&NewEntry->Data + _Size >= &HeapEnd) {

		UnlockSemaphore(&g_AllocatorSema);
		return NULL;
	}

	NewEntry->Size = _Size;
	NewEntry->Used = TRUE;
	InsertTailList(&HeapEntries, &NewEntry->HeapLinks);

	UnlockSemaphore(&g_AllocatorSema);
	return &NewEntry->Data;
}

void __cdecl free(void * _Block) {

	LockSemaphore(&g_AllocatorSema);

	const PHEAP_ENTRY  Entry = CONTAINING_RECORD(_Block, HEAP_ENTRY, Data);

	const PHEAP_ENTRY  NextEntry = Entry->HeapLinks.Flink != &HeapEntries
		? CONTAINING_RECORD(Entry->HeapLinks.Flink, HEAP_ENTRY, HeapLinks)
		: NULL;

	const PHEAP_ENTRY  PrevEntry = Entry->HeapLinks.Blink != &HeapEntries
		? CONTAINING_RECORD(Entry->HeapLinks.Blink, HEAP_ENTRY, HeapLinks)
		: NULL;

	Entry->Used = FALSE;
	InsertTailList(&HeapFreeEntries, &Entry->HeapFreeLinks);

	if (NextEntry && !NextEntry->Used) {

		RemoveEntryList(&NextEntry->HeapLinks);
		RemoveEntryList(&NextEntry->HeapFreeLinks);
		Entry->Size = NextEntry->Size + Entry->Size + UFIELD_OFFSET(HEAP_ENTRY, Data);
		Entry->Used = FALSE;
	}

	if (PrevEntry && !PrevEntry->Used) {

		RemoveHeadList(&PrevEntry->HeapLinks);
		RemoveHeadList(&PrevEntry->HeapFreeLinks);
		PrevEntry->Size = PrevEntry->Size + Entry->Size + UFIELD_OFFSET(HEAP_ENTRY, Data);
		PrevEntry->Used = FALSE;
	}

	UnlockSemaphore(&g_AllocatorSema);
}