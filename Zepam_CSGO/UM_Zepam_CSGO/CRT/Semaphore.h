#pragma once

typedef struct _SEMAPHORE
{
	volatile LONG_PTR BeingOwned;
}SEMAPHORE,*PSEMAPHORE;

static FORCEINLINE BOOLEAN TryLockSemaphore(const PSEMAPHORE Sema) {

	return _InterlockedCompareExchange(&Sema->BeingOwned, TRUE, FALSE) == FALSE;
}

static FORCEINLINE VOID UnlockSemaphore(const PSEMAPHORE Sema) {

	_InterlockedExchange(&Sema->BeingOwned, FALSE);
}

static FORCEINLINE VOID LockSemaphore(const PSEMAPHORE Sema) {

	while (!TryLockSemaphore(Sema))
		_mm_pause();
}