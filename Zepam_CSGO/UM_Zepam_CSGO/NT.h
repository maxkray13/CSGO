#pragma once
#include <Windows.h>
//#include <winternl.h>


typedef PVOID * PPVOID;
typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID;

#define STATIC_UNICODE_BUFFER_LENGTH 261
#define WIN32_CLIENT_INFO_LENGTH 62
#define GDI_BATCH_BUFFER_SIZE 310


#if defined _M_IX86
#define TYPE32  ULONG
#define PEBTEB_BITS 32
#elif defined _M_X64
#define TYPE64  ULONGLONG
#define PEBTEB_BITS 64
#endif

typedef struct _GDI_TEB_BATCH {
	ULONG    Offset;
	ULONG_PTR HDC;
	ULONG    Buffer[GDI_BATCH_BUFFER_SIZE];
} GDI_TEB_BATCH, *PGDI_TEB_BATCH;


typedef struct _PEB {
	BOOLEAN InheritedAddressSpace;      // These four fields cannot change unless the
	BOOLEAN ReadImageFileExecOptions;   //
	BOOLEAN BeingDebugged;              //
	union {
		BOOLEAN BitField;                  //
		struct {
			BOOLEAN ImageUsesLargePages : 1;
			BOOLEAN SpareBits : 7;
		};
	};
	HANDLE Mutant;      // INITIAL_PEB structure is also updated.

	PVOID ImageBaseAddress;
	struct _PEB_LDR_DATA * Ldr;
	struct _RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	struct _RTL_CRITICAL_SECTION* FastPebLock;
	PVOID AtlThunkSListPtr;     // Used only for AMD64
	PVOID SparePtr2;
	ULONG EnvironmentUpdateCount;
	PVOID KernelCallbackTable;
	ULONG SystemReserved[1];
	ULONG SpareUlong;
	struct _PEB_FREE_BLOCK* FreeList;
	ULONG TlsExpansionCounter;
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[2];         // TLS_MINIMUM_AVAILABLE bits
	PVOID ReadOnlySharedMemoryBase;
	PVOID ReadOnlySharedMemoryHeap;
	PPVOID ReadOnlyStaticServerData;
	PVOID AnsiCodePageData;
	PVOID OemCodePageData;
	PVOID UnicodeCaseTableData;

	//
	// Useful information for LdrpInitialize
	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;

	//
	// Passed up from MmCreatePeb from Session Manager registry key
	//

	LARGE_INTEGER CriticalSectionTimeout;
	SIZE_T HeapSegmentReserve;
	SIZE_T HeapSegmentCommit;
	SIZE_T HeapDeCommitTotalFreeThreshold;
	SIZE_T HeapDeCommitFreeBlockThreshold;

	//
	// Where heap manager keeps track of all heaps created for a process
	// Fields initialized by MmCreatePeb.  ProcessHeaps is initialized
	// to point to the first free byte after the PEB and MaximumNumberOfHeaps
	// is computed from the page size used to hold the PEB, less the fixed
	// size of this data structure.
	//

	ULONG NumberOfHeaps;
	ULONG MaximumNumberOfHeaps;
	PPVOID ProcessHeaps;

	//
	//
	PVOID GdiSharedHandleTable;
	PVOID ProcessStarterHelper;
	ULONG GdiDCAttributeList;
	struct _RTL_CRITICAL_SECTION* LoaderLock;

	//
	// Following fields filled in by MmCreatePeb from system values and/or
	// image header.
	//

	ULONG OSMajorVersion;
	ULONG OSMinorVersion;
	USHORT OSBuildNumber;
	USHORT OSCSDVersion;
	ULONG OSPlatformId;
	ULONG ImageSubsystem;
	ULONG ImageSubsystemMajorVersion;
	ULONG ImageSubsystemMinorVersion;
	ULONG_PTR ImageProcessAffinityMask;
	struct _GDI_HANDLE_BUFFER* GdiHandleBuffer;
	struct _PS_POST_PROCESS_INIT_ROUTINE* PostProcessInitRoutine;

	PVOID TlsExpansionBitmap;
	ULONG TlsExpansionBitmapBits[32];   // TLS_EXPANSION_SLOTS bits

	//
	// Id of the Hydra session in which this process is running
	//
	ULONG SessionId;

	//
	// Filled in by LdrpInstallAppcompatBackend
	//
	ULARGE_INTEGER AppCompatFlags;

	//
	// ntuser appcompat flags
	//
	ULARGE_INTEGER AppCompatFlagsUser;

	//
	// Filled in by LdrpInstallAppcompatBackend
	//
	PVOID pShimData;

	//
	// Filled in by LdrQueryImageFileExecutionOptions
	//
	PVOID AppCompatInfo;

	//
	// Used by GetVersionExW as the szCSDVersion string
	//
	
	UNICODE_STRING CSDVersion;

	//
	// Fusion stuff
	//
	const struct _ACTIVATION_CONTEXT_DATA * ActivationContextData;
	struct _ASSEMBLY_STORAGE_MAP * ProcessAssemblyStorageMap;
	const struct _ACTIVATION_CONTEXT_DATA * SystemDefaultActivationContextData;
	struct _ASSEMBLY_STORAGE_MAP * SystemAssemblyStorageMap;

	//
	// Enforced minimum initial commit stack
	//
	SIZE_T MinimumStackCommit;

	//
	// Fiber local storage.
	//

	PPVOID FlsCallback;
	LIST_ENTRY FlsListHead;
	PVOID FlsBitmap;
	ULONG FlsBitmapBits[FLS_MAXIMUM_AVAILABLE / (sizeof(ULONG) * 8)];
	ULONG FlsHighIndex;
} PEB, *PPEB;



typedef struct _PEB_LDR_DATA {
	ULONG Length;
	BOOLEAN Initialized;
	HANDLE SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID EntryInProgress;
	BOOLEAN ShutdownInProgress;
	HANDLE ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;


//
//  Fusion/sxs thread state information
//

#define ACTIVATION_CONTEXT_STACK_FLAG_QUERIES_DISABLED (0x00000001)

typedef struct _ACTIVATION_CONTEXT_STACK {
	struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME * ActiveFrame;
	LIST_ENTRY FrameListCache;
	ULONG Flags;
	ULONG NextCookieSequenceNumber;
	ULONG StackId;
} ACTIVATION_CONTEXT_STACK, *PACTIVATION_CONTEXT_STACK;

typedef const ACTIVATION_CONTEXT_STACK * PCACTIVATION_CONTEXT_STACK;

#define TEB_ACTIVE_FRAME_CONTEXT_FLAG_EXTENDED (0x00000001)

typedef struct _TEB_ACTIVE_FRAME_CONTEXT {
	ULONG Flags;
	PCSTR FrameName;
} TEB_ACTIVE_FRAME_CONTEXT, *PTEB_ACTIVE_FRAME_CONTEXT;

typedef const TEB_ACTIVE_FRAME_CONTEXT *PCTEB_ACTIVE_FRAME_CONTEXT;

typedef struct _TEB_ACTIVE_FRAME_CONTEXT_EX {
	TEB_ACTIVE_FRAME_CONTEXT BasicContext;
	PCSTR SourceLocation; // e.g. "Z:\foo\bar\baz.c"
} TEB_ACTIVE_FRAME_CONTEXT_EX, *PTEB_ACTIVE_FRAME_CONTEXT_EX;

typedef const TEB_ACTIVE_FRAME_CONTEXT_EX *PCTEB_ACTIVE_FRAME_CONTEXT_EX;

#define TEB_ACTIVE_FRAME_FLAG_EXTENDED (0x00000001)

typedef struct _TEB_ACTIVE_FRAME {
	ULONG Flags;
	struct _TEB_ACTIVE_FRAME* Previous;
	PCTEB_ACTIVE_FRAME_CONTEXT Context;
} TEB_ACTIVE_FRAME, *PTEB_ACTIVE_FRAME;

typedef const TEB_ACTIVE_FRAME *PCTEB_ACTIVE_FRAME;

typedef struct _TEB_ACTIVE_FRAME_EX {
	TEB_ACTIVE_FRAME BasicFrame;
	PVOID ExtensionIdentifier; // use address of your DLL Main or something unique to your mapping in the address space
} TEB_ACTIVE_FRAME_EX, *PTEB_ACTIVE_FRAME_EX;

typedef const TEB_ACTIVE_FRAME_EX *PCTEB_ACTIVE_FRAME_EX;


typedef struct _TEB {
	NT_TIB NtTib;
	PVOID EnvironmentPointer;
	CLIENT_ID ClientId;
	PVOID ActiveRpcHandle;
	PVOID ThreadLocalStoragePointer;
	PPEB ProcessEnvironmentBlock;
	ULONG LastErrorValue;
	ULONG CountOfOwnedCriticalSections;
	PVOID CsrClientThread;
	PVOID Win32ThreadInfo;          // PtiCurrent
	ULONG User32Reserved[26];       // user32.dll items
	ULONG UserReserved[5];          // Winsrv SwitchStack
	PVOID WOW32Reserved;            // used by WOW
	LCID CurrentLocale;
	ULONG FpSoftwareStatusRegister; // offset known by outsiders!
	PVOID SystemReserved1[54];      // Used by FP emulator
	NTSTATUS ExceptionCode;         // for RaiseUserException
	// 4 bytes of padding here on native 64bit TEB and TEB64
	PACTIVATION_CONTEXT_STACK ActivationContextStackPointer; // Fusion activation stack
#if (defined(PEBTEB_BITS) && (PEBTEB_BITS == 64) || (!defined(PEBTEB_BITS) && defined(_WIN64)))
	UCHAR SpareBytes1[28]; // native 64bit TEB and TEB64
#else
	UCHAR SpareBytes1[40]; // native 32bit TEB and TEB32
#endif
	GDI_TEB_BATCH GdiTebBatch;      // Gdi batching
	CLIENT_ID RealClientId;
	HANDLE GdiCachedProcessHandle;
	ULONG GdiClientPID;
	ULONG GdiClientTID;
	PVOID GdiThreadLocalInfo;
	ULONG_PTR Win32ClientInfo[WIN32_CLIENT_INFO_LENGTH]; // User32 Client Info
	PVOID glDispatchTable[233];     // OpenGL
	ULONG_PTR glReserved1[29];      // OpenGL
	PVOID glReserved2;              // OpenGL
	PVOID glSectionInfo;            // OpenGL
	PVOID glSection;                // OpenGL
	PVOID glTable;                  // OpenGL
	PVOID glCurrentRC;              // OpenGL
	PVOID glContext;                // OpenGL
	ULONG LastStatusValue;
	UNICODE_STRING StaticUnicodeString;
	WCHAR StaticUnicodeBuffer[STATIC_UNICODE_BUFFER_LENGTH];
	PVOID DeallocationStack;
	PVOID TlsSlots[TLS_MINIMUM_AVAILABLE];
	LIST_ENTRY TlsLinks;
	PVOID Vdm;
	PVOID ReservedForNtRpc;
	PVOID DbgSsReserved[2];
	ULONG HardErrorMode;
	PVOID Instrumentation[14];
	PVOID SubProcessTag;
	PVOID EtwTraceData;
	PVOID WinSockData;              // WinSock
	ULONG GdiBatchCount;
	BOOLEAN InDbgPrint;
	BOOLEAN FreeStackOnTermination;
	BOOLEAN HasFiberData;
	BOOLEAN IdealProcessor;
	ULONG GuaranteedStackBytes;
	PVOID ReservedForPerf;
	PVOID ReservedForOle;
	ULONG WaitingOnLoaderLock;
	ULONG_PTR SparePointer1;
	ULONG_PTR SoftPatchPtr1;
	ULONG_PTR SoftPatchPtr2;
	PPVOID TlsExpansionSlots;
#if (defined(_WIN64) && !defined(PEBTEB_BITS)) \
    || ((defined(_WIN64) || defined(_X86_)) && defined(PEBTEB_BITS) && PEBTEB_BITS == 64)
	//
	// These are in native Win64 TEB, Win64 TEB64, and x86 TEB64.
	//
	PVOID DeallocationBStore;
	PVOID BStoreLimit;
#endif  
	LCID ImpersonationLocale;       // Current locale of impersonated user
	ULONG IsImpersonating;          // Thread impersonation status
	PVOID NlsCache;                 // NLS thread cache
	PVOID pShimData;                // Per thread data used in the shim
	ULONG HeapVirtualAffinity;
	HANDLE CurrentTransactionHandle;// reserved for TxF transaction context
	struct TEB_ACTIVE_FRAME* ActiveFrame;
	PVOID FlsData;
	BOOLEAN SafeThunkCall;
	BOOLEAN BooleanSpare[3];

} TEB, *PTEB;

typedef struct _LDR_DATA_TABLE_ENTRY {
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	union {
		LIST_ENTRY HashLinks;
		struct {
			PVOID SectionPointer;
			ULONG CheckSum;
		};
	};
	union {
		struct {
			ULONG TimeDateStamp;
		};
		struct {
			PVOID LoadedImports;
		};
	};
	struct _ACTIVATION_CONTEXT * EntryPointActivationContext;

	PVOID PatchInformation;

} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

static PTEB __GetTeb() {
#if defined _M_IX86
	return (PTEB)__readfsdword(0x18);
#elif defined _M_X64
	return (PTEB)__readgsqword(0x30);
#endif
}

static PPEB __GetPeb() {
#if defined _M_IX86
	return (PPEB)__readfsdword(0x30);
#elif defined _M_X64
	return (PPEB)__readgsqword(0x60);
#endif
}