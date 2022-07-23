#include <PEMapper/pefile.h>
#include <MemoryTracker/memorytracker.h>

#include "provider.h"
#include "ntoskrnl_provider.h"
#include "handle_manager.h"


using fnFreeCall = uint64_t(__fastcall*)(...);

template <typename... Params>
static NTSTATUS __NtRoutine(const char* Name, Params&&... params) {
	auto fn = (fnFreeCall)GetProcAddress(GetModuleHandleA("ntdll.dll"), Name);
	return fn(std::forward<Params>(params)...);
}

#define NtQuerySystemInformation(...) __NtRoutine("NtQuerySystemInformation", __VA_ARGS__)


void* hM_AllocPoolTag(uint32_t pooltype, size_t size, ULONG tag) {

	auto ptr = _aligned_malloc(size, 0x1000);;
	return ptr;
}


void* hM_AllocPool(uint32_t pooltype, size_t size) {
	auto ptr = _aligned_malloc(size, 0x1000);;
	return ptr;
}

void h_DeAllocPoolTag(uintptr_t ptr, ULONG tag)
{
	_aligned_free((PVOID)ptr);
	return;
}

void h_DeAllocPool(uintptr_t ptr)
{
	_aligned_free((PVOID)ptr);
	return;
}

_ETHREAD* h_KeGetCurrentThread()
{
	return (_ETHREAD*)__readgsqword(0x188);
}

NTSTATUS h_NtQuerySystemInformation(uint32_t SystemInformationClass, uintptr_t SystemInformation,
	ULONG SystemInformationLength, PULONG ReturnLength)
{

	auto x = NtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);

	Logger::Log("\tClass %08x status : %08x\n", SystemInformationClass, x);
	if (x == 0) {
		Logger::Log("\tClass %08x success\n", SystemInformationClass);
		if (SystemInformationClass == 0xb) { //SystemModuleInformation
			auto ptr = (char*)SystemInformation;
			//*(uint64_t*)(ptr + 0x18) = GetModuleBase("ntoskrnl.exe");
			;
			RTL_PROCESS_MODULES* loadedmodules = (RTL_PROCESS_MODULES*)(SystemInformation);
			// __NtRoutine("randededom", castTest->NumberOfModules);
			for (int i = 0; i < loadedmodules->NumberOfModules; i++) {
				char* modulename = (char*)loadedmodules->Modules[i].FullPathName;
				while (strstr(modulename, "\\"))
					modulename++;

				auto modulebase = GetModuleBase(modulename);
				if (modulebase) {
					Logger::Log("\tPatching %s base from %llx to %llx\n", modulename, (PVOID)loadedmodules->Modules[i].ImageBase, (PVOID)modulebase);
					loadedmodules->Modules[i].ImageBase = modulebase;
				}
				else { //We're gonna pass the real module to the driver
					//loadedmodules->Modules[i].ImageBase = 0;
					//loadedmodules->Modules[i].LoadCount = 0;
				}
			}
			//MemoryTracker::TrackVariable((uintptr_t)ptr, SystemInformationLength, (char*)"NtQuerySystemInformation"); BAD IDEA

			Logger::Log("\tBase is : %llx\n", *(uint64_t*)(ptr + 0x18));

		}
		else if (SystemInformationClass == 0x4D) { //SystemModuleInformation
			auto ptr = (char*)SystemInformation;
			//*(uint64_t*)(ptr + 0x18) = GetModuleBase("ntoskrnl.exe");
			_SYSTEM_MODULE_EX* pMods = (_SYSTEM_MODULE_EX*)(SystemInformation);
			ulong SizeRead = 0;
			ulong NumModules = 0;

			while ((SizeRead + sizeof(_SYSTEM_MODULE_EX)) <= *ReturnLength)
			{


				char* modulename = (char*)pMods->FullDllName;
				while (strstr(modulename, "\\"))
					modulename++;


				auto modulebase = GetModuleBase(modulename);
				if (modulebase) {
					Logger::Log("\tPatching %s base from %llx to %llx\n", modulename, pMods->ImageBase, modulebase);
					pMods->ImageBase = (PVOID)modulebase;

				}
				else { //We're gonna pass the real module to the driver
					pMods->ImageBase = 0;


					pMods->LoadCount = 0;
				}

				NumModules++;
				pMods++;
				SizeRead += sizeof(_SYSTEM_MODULE_EX);
			}

			Logger::Log("\tBase is : %llx\n", *(uint64_t*)(ptr + 0x18));

		}
		else if (SystemInformationClass == 0x5a) {
			SYSTEM_BOOT_ENVIRONMENT_INFORMATION* pBootInfo = (SYSTEM_BOOT_ENVIRONMENT_INFORMATION*)SystemInformation;
			Logger::Log("\tBoot info buffer : %llx\n", (void*)pBootInfo);

		}

	}
	return x;
}

uint64_t h_RtlRandomEx(unsigned long* seed)
{
	Logger::Log("\tSeed is %llx\n", *seed);
	auto ret = __NtRoutine("RtlRandomEx", seed);
	*seed = ret; //Keep behavior kinda same as Kernel equivalent in case of check
	return ret;
}

NTSTATUS h_IoCreateDevice(_DRIVER_OBJECT* DriverObject, ULONG DeviceExtensionSize, PUNICODE_STRING DeviceName,
	DWORD DeviceType, ULONG DeviceCharacteristics, BOOLEAN Exclusive, _DEVICE_OBJECT** DeviceObject)
{
	*DeviceObject = (_DEVICE_OBJECT*)malloc(sizeof(_DEVICE_OBJECT));
	auto realDevice = *DeviceObject;

	memset(*DeviceObject, 0, sizeof(_DEVICE_OBJECT));

	realDevice->DeviceType = DeviceType;
	realDevice->Type = 3;
	realDevice->Size = sizeof(*realDevice);
	realDevice->ReferenceCount = 1;
	realDevice->DriverObject = DriverObject;
	realDevice->NextDevice = 0;

	return 0;
}

NTSTATUS h_IoCreateFileEx(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes,
	void* IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess, ULONG Disposition,
	ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength, void* CreateFileType, PVOID InternalParameters, ULONG Options,
	void* DriverContext)
{

	PUNICODE_STRING OLDBuffer;
	OLDBuffer = ObjectAttributes->ObjectName;
	UNICODE_STRING TempBuffer;
	TempBuffer.Buffer = (wchar_t*)malloc(512);
	memset(TempBuffer.Buffer, 0, 512);

	wcscat(TempBuffer.Buffer, L"\\??\\C:\\kace");
	wcscat(TempBuffer.Buffer, OLDBuffer->Buffer);
	TempBuffer.Buffer[12] = 'c';
	TempBuffer.Buffer[13] = 'a';
	TempBuffer.Buffer[16] = 'a';
	TempBuffer.Length = wcslen(TempBuffer.Buffer) * 2;
	TempBuffer.MaximumLength = wcslen(TempBuffer.Buffer) * 2;
	ObjectAttributes->ObjectName = &TempBuffer;
	ObjectAttributes->Attributes = 0x00000040;
	Logger::Log("\tCreating file : %ls\n", ObjectAttributes->ObjectName->Buffer);
	if (DesiredAccess == 0xC0000000)
		DesiredAccess = 0xC0100080;
	auto ret = __NtRoutine("NtCreateFile", FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, Disposition, CreateOptions, EaBuffer, EaLength);
	Logger::Log("\tReturn : %08x\n", ret);
	ObjectAttributes->ObjectName = OLDBuffer;
	free(TempBuffer.Buffer);

	return 0;
}

void h_KeInitializeEvent(_KEVENT* Event, _EVENT_TYPE Type, BOOLEAN State)
{

	/* Initialize the Dispatcher Header */
	Event->Header.SignalState = State;
	InitializeListHead(&Event->Header.WaitListHead);
	Event->Header.Type = Type;
	*(WORD*)((char*)&Event->Header.Lock + 1) = 0x600; //saw this on ida, someone explain me

	auto hEvent = CreateEvent(NULL, NULL, State, NULL);
	HandleManager::AddMap((uintptr_t)Event, (uintptr_t)hEvent);
	Logger::Log("\tEvent object : %llx\n", Event);
}

NTSTATUS h_RtlGetVersion(RTL_OSVERSIONINFOW* lpVersionInformation)
{
	auto ret = __NtRoutine("RtlGetVersion", lpVersionInformation);
	Logger::Log("\t%d.%d.%d\n", lpVersionInformation->dwMajorVersion, lpVersionInformation->dwMinorVersion, lpVersionInformation->dwBuildNumber);
	return ret;
}

EXCEPTION_DISPOSITION _c_exception(_EXCEPTION_RECORD* ExceptionRecord, void* EstablisherFrame, _CONTEXT* ContextRecord,
	_DISPATCHER_CONTEXT* DispatcherContext)
{
	return (EXCEPTION_DISPOSITION)__NtRoutine("__C_specific_handler", ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}

NTSTATUS h_RtlMultiByteToUnicodeN(PWCH UnicodeString, ULONG MaxBytesInUnicodeString, PULONG BytesInUnicodeString,
	const CHAR* MultiByteString, ULONG BytesInMultiByteString)
{
	Logger::Log("\tTrying to convert : %s\n", MultiByteString);
	return __NtRoutine("RtlMultiByteToUnicodeN", UnicodeString, MaxBytesInUnicodeString, BytesInUnicodeString, MultiByteString, BytesInMultiByteString);
}

bool h_KeAreAllApcsDisabled()
{ //Track thread IRQL ideally
	return false;
}

bool h_KeAreApcsDisabled()
{
	return false;
}

NTSTATUS h_NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes,
	PVOID IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
	ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength)
{
	Logger::Log("\tCreating file : %ls\n", ObjectAttributes->ObjectName->Buffer);
	auto ret = __NtRoutine("NtCreateFile", FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
	Logger::Log("\tReturn : %08x\n", ret);
	return ret;
}

NTSTATUS h_NtReadFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext, PVOID IoStatusBlock,
	PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key)
{
	auto ret = __NtRoutine("NtReadFile", FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);
	return ret;
}

NTSTATUS h_NtQueryInformationFile(HANDLE FileHandle, PVOID IoStatusBlock, PVOID FileInformation, ULONG Length,
	FILE_INFORMATION_CLASS FileInformationClass)
{
	Logger::Log("\tQueryInformationFile with class %08x\n", FileInformationClass);
	auto ret = __NtRoutine("NtQueryInformationFile", FileHandle, IoStatusBlock, FileInformation, Length, FileInformationClass);
	return ret;
}

NTSTATUS h_ZwQueryValueKey(HANDLE KeyHandle, PUNICODE_STRING ValueName,
	KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength)
{
	auto ret = __NtRoutine("NtQueryValueKey", KeyHandle, ValueName, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);
	return ret;
}

NTSTATUS h_ZwOpenKey(PHANDLE KeyHandle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes)
{
	auto ret = __NtRoutine("NtOpenKey", KeyHandle, DesiredAccess, ObjectAttributes);
	Logger::Log("\tTry to open %ls : %08x\n", ObjectAttributes->ObjectName->Buffer, ret);
	return ret;
}

NTSTATUS h_ZwFlushKey(PHANDLE KeyHandle)
{
	auto ret = __NtRoutine("NtFlushKey", KeyHandle);
	return ret;
}

NTSTATUS h_ZwClose(HANDLE Handle)
{
	Logger::Log("\tClosing Kernel Handle : %llx\n", Handle);
	if (!Handle)
		return STATUS_NOT_FOUND;
	auto ret = __NtRoutine("NtClose", Handle);
	return ret;
}

NTSTATUS h_RtlWriteRegistryValue(ULONG RelativeTo, PCWSTR Path, PCWSTR ValueName, ULONG ValueType, PVOID ValueData,
	ULONG ValueLength)
{
	Logger::Log("\tWriting to %ls - %ls  %llx\n", Path, ValueName, *(const PVOID*)ValueData);
	auto ret = __NtRoutine("RtlWriteRegistryValue", RelativeTo, Path, ValueName, ValueType, ValueData, ValueLength);
	return ret;
}

NTSTATUS h_RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString)
{
	auto ret = __NtRoutine("RtlInitUnicodeString", DestinationString, SourceString);
	return ret;
}

NTSTATUS h_ZwQueryFullAttributesFile(OBJECT_ATTRIBUTES* ObjectAttributes,
	PFILE_NETWORK_OPEN_INFORMATION FileInformation)
{

	auto ret = __NtRoutine("NtQueryFullAttributesFile", ObjectAttributes, FileInformation);
	Logger::Log("\tQuerying information for %ls : %08x\n", ObjectAttributes->ObjectName->Buffer, ret);
	return ret;
}

PVOID h_PsGetProcessWow64Process(_EPROCESS* Process)
{
	Logger::Log("\tRequesting WoW64 for process : %llx (id : %llx)\n", (const PVOID)Process, Process->UniqueProcessId);
	return Process->WoW64Process;
}

NTSTATUS h_IoWMIOpenBlock(LPCGUID Guid, ULONG DesiredAccess, PVOID* DataBlockObject)
{
	Logger::Log("\tWMI GUID : %llx-%llx-%llx-%llx with access : %llx\n", Guid->Data1, Guid->Data2, Guid->Data3, Guid->Data4, DesiredAccess);
	return STATUS_SUCCESS;
}

NTSTATUS h_IoWMIQueryAllData(PVOID DataBlockObject, PULONG InOutBufferSize, PVOID OutBuffer)
{

	return STATUS_SUCCESS;
}

uint64_t h_ObfDereferenceObject(PVOID obj)
{ //TODO

	return 0;
}

NTSTATUS h_PsLookupThreadByThreadId(HANDLE ThreadId, PVOID* Thread)
{
	//Logger::Log("\tThread ID : %llx is being investigated.\n", ThreadId);
	auto ct = h_KeGetCurrentThread();

	if (ThreadId == (HANDLE)4) {
		*Thread = (PVOID)&FakeKernelThread;
	}
	else {
		*Thread = 0;
		return STATUS_INVALID_PARAMETER;
	}
	return 0;
}

HANDLE h_PsGetThreadId(_ETHREAD* Thread) {
	if (Thread)
		return Thread->Cid.UniqueThread;
	else
		return 0;
}

_PEB* h_PsGetProcessPeb(_EPROCESS* process) {
	return process->Peb;
}

HANDLE h_PsGetProcessInheritedFromUniqueProcessId(_EPROCESS* Process) {
	return Process->InheritedFromUniqueProcessId;
}


NTSTATUS h_IoQueryFileDosDeviceName(PVOID fileObject, PVOID* name_info) {
	typedef struct _OBJECT_NAME_INFORMATION {
		UNICODE_STRING Name;
	} aids;
	static aids n;
	name_info = (PVOID*)&n;

	return STATUS_SUCCESS;
}

NTSTATUS h_ObOpenObjectByPointer(
	PVOID           Object,
	ULONG           HandleAttributes,
	PVOID   PassedAccessState,
	ACCESS_MASK     DesiredAccess,
	uint64_t    ObjectType,
	uint64_t AccessMode,
	PHANDLE         Handle
) {
	return STATUS_SUCCESS;
}


NTSTATUS h_ObQueryNameString(PVOID Object, PVOID ObjectNameInfo, ULONG Length, PULONG ReturnLength) {
	Logger::Log("\tUnimplemented function call detected\n");
	return STATUS_SUCCESS;
}


void h_ExAcquireFastMutex(PFAST_MUTEX FastMutex)
{
	auto fm = &FastMutex[0];
	fm->OldIrql = 0; //PASSIVE_LEVEL
	fm->Owner = (_KTHREAD*)h_KeGetCurrentThread();
	//fm = &FastMutex[0];
	//fm->OldIrql = 0; //PASSIVE_LEVEL
	// fm->Owner = (_KTHREAD*)&FakeKernelThread;
	fm->Count--;
	return;
}

void h_ExReleaseFastMutex(PFAST_MUTEX FastMutex)
{
	FastMutex->OldIrql = 0; //PASSIVE_LEVEL
	FastMutex->Owner = 0;
	FastMutex->Count++;
	return;
}

LONG_PTR h_ObfReferenceObject(PVOID Object)
{
	//  Logger::Log("Trying to get reference for %llx", Object);
	if (!Object)
		return -1;
	if (Object == (PVOID)&FakeSystemProcess) {
		Logger::Log("\tIncreasing ref by 1\n");
		return (LONG_PTR)&FakeSystemProcess;
	}
	else {
		Logger::Log("\tFailed - ");
		Logger::Log("%llx\n", Object);
	}

	return 0;
}

LONGLONG h_PsGetProcessCreateTimeQuadPart(_EPROCESS* process)
{
	Logger::Log("\t\tTrying to get creation time for %llx\n", (const void*)process);
	return process->CreateTime.QuadPart;
}

LONG h_RtlCompareString(const STRING* String1, const STRING* String2, BOOLEAN CaseInSensitive)
{
	Logger::Log("\tComparing %s to %s\n", String1->Buffer, String2->Buffer);
	auto ret = __NtRoutine("RtlCompareString", String1, String2, CaseInSensitive);
	return ret;
}

NTSTATUS h_PsLookupProcessByProcessId(HANDLE ProcessId, _EPROCESS** Process)
{

	Logger::Log("\tProcess %08x EPROCESS being retrieved\n", ProcessId);

	if (ProcessId == (HANDLE)4) {
		*Process = &FakeSystemProcess;
	}
	else {
		*Process = 0;
		return 0xC000000B; //INVALID_CID
	}
	return 0;
}

HANDLE h_PsGetProcessId(_EPROCESS* Process)
{

	if (!Process)
		return 0;

	return Process->UniqueProcessId;
}

_EPROCESS* h_PsGetCurrentProcess()
{
	auto val = (_EPROCESS*)h_KeGetCurrentThread()->Tcb.ApcState.Process;
	Logger::Log("\tReturning : %llx\n", val);
	return val;
}

_EPROCESS* h_PsGetCurrentThreadProcess()
{
	return (_EPROCESS*)h_KeGetCurrentThread()->Tcb.Process;
}

HANDLE h_PsGetCurrentThreadId()
{
	return h_KeGetCurrentThread()->Cid.UniqueThread;
}

HANDLE h_PsGetCurrentThreadProcessId()
{
	return h_KeGetCurrentThread()->Cid.UniqueProcess;
}

NTSTATUS h_NtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass,
	PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength)
{

	if (ProcessHandle == (HANDLE)-1) { //self-check


		auto ret = __NtRoutine("NtQueryInformationProcess", ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
		Logger::Log("\tProcessInformation for handle %llx - class %llx - ret : %llx\n", ProcessHandle, ProcessInformationClass, ret);
		*(DWORD*)ProcessInformation = 1; //We are critical
		return ret;
	}
	else {
		auto ret = __NtRoutine("NtQueryInformationProcess", ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
		Logger::Log("\tProcessInformation for handle %llx - class %llx - ret : %llx\n", ProcessHandle, ProcessInformationClass, ret);
		return ret;
	}

}

bool h_PsIsProtectedProcess(_EPROCESS* process)
{
	if (process->UniqueProcessId == (PVOID)4) {
		return true;
	}
	return (process->Protection.Level & 7) != 0;
}

PACCESS_TOKEN h_PsReferencePrimaryToken(_EPROCESS* Process)
{
	//Process->Token.RefCnt++;
	_EX_FAST_REF* a1 = &Process->Token;
	auto Value = a1->Value;
	signed __int64 v3;
	signed __int64 v4; // rdi
	unsigned int v5; // r8d
	unsigned __int64 v6; // rdi

	if ((a1->Value & 0xF) != 0)
	{
		do
		{
			v3 = _InterlockedCompareExchange64((volatile long long*)a1, Value - 1, Value);
			if (Value == v3)
				break;
			Value = v3;
		} while ((v3 & 0xF) != 0);
	}
	v4 = Value;
	v5 = Value & 0xF;
	v6 = v4 & 0xFFFFFFFFFFFFFFF0ui64;
	if (v5 > 1)
		a1 = (_EX_FAST_REF*)v6;

	Logger::Log("\tReturning Token : %llx\n", (const void*)a1);
	return a1;
}

TOKEN_PRIVILEGES kernelToken[31] = { 0 };

NTSTATUS h_SeQueryInformationToken(PACCESS_TOKEN Token, TOKEN_INFORMATION_CLASS TokenInformationClass,
	PVOID* TokenInformation)
{
	//TODO NOT IMPLEMENTED
	Logger::Log("\tToken : %llx - Class : %llx\n", (const void*)Token, (int)TokenInformationClass);
	if (TokenInformationClass == 0x19) { //IsAppContainer
		*(DWORD*)TokenInformation = 0; //We are not a appcontainer.
	}
	else if (TokenInformationClass == 0x3) {
		return 0xC0000002;
	}
	return 0xC0000022;
}

void h_IoDeleteController(PVOID ControllerObject)
{
	_EX_FAST_REF* ref = (_EX_FAST_REF*)ControllerObject;
	//TODO This needs to dereference the object  -- Check ntoskrnl.exe code
	Logger::Log("\tDeleting controller : %llx\n", static_cast<const void*>(ControllerObject));
	return;
}

NTSTATUS h_RtlDuplicateUnicodeString(int add_nul, const UNICODE_STRING* source, UNICODE_STRING* destination)
{

	auto ret = __NtRoutine("RtlDuplicateUnicodeString", add_nul, source, destination);
	Logger::Log("\tRtlDuplicateUnicodeString : %ls = %llx\n", source->Buffer, ret);
	return ret;
}

void h_ExSystemTimeToLocalTime(PLARGE_INTEGER SystemTime, PLARGE_INTEGER LocalTime)
{
	memcpy(LocalTime, SystemTime, sizeof(LARGE_INTEGER));
}

int h_vswprintf_s(wchar_t* buffer, size_t numberOfElements, const wchar_t* format, va_list argptr)
{
	const wchar_t* s = format;
	int count = 0;
	uint64_t variables[16] = { 0 };
	memset(variables, 0, sizeof(variables));
	for (count = 0; s[count]; s[count] == '%' ? count++ : *s++);

	for (int i = 0; i < count; i++) {
		variables[i] = va_arg(argptr, uint64_t);
		if (variables[i] >= 0xFFFFF78000000000 && variables[i] <= 0xFFFFF78000001000) {
			variables[i] -= 0xFFFFF77F80020000;
		}
	}


	auto ret = vswprintf_s(buffer, numberOfElements, format, (va_list)variables);
	Logger::Log("\tResult : %ls\n", buffer);
	return ret;
}


int h_swprintf_s(wchar_t* buffer, size_t sizeOfBuffer, const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const wchar_t* s = format;
	int count = 0;
	uint64_t variables[16] = { 0 };
	memset(variables, 0, sizeof(variables));
	for (count = 0; s[count]; s[count] == '%' ? count++ : *s++);

	for (int i = 0; i < count; i++) {
		variables[i] = va_arg(ap, uint64_t);
		if (variables[i] >= 0xFFFFF78000000000 && variables[i] <= 0xFFFFF78000001000) {
			variables[i] -= 0xFFFFF77F80020000;
		}
	}
	va_end(ap);

	auto ret = vswprintf_s(buffer, sizeOfBuffer, format, (va_list)variables);


	Logger::Log("\tResult : %ls\n", buffer);
	return ret;
}


errno_t h_wcscpy_s(wchar_t* dest, rsize_t dest_size, const wchar_t* src)
{
	return wcscpy_s(dest, dest_size, src);
}

errno_t h_wcscat_s(
	wchar_t* strDestination,
	size_t numberOfElements,
	const wchar_t* strSource
) {
	if ((uint64_t)strSource >= 0xFFFFF78000000000 && (uint64_t)strSource <= 0xFFFFF78000001000) {
		strSource = (wchar_t*)((uint64_t)strSource - 0xFFFFF77F80020000);
	}
	return wcscat_s(strDestination, numberOfElements, strSource);

}

void h_RtlTimeToTimeFields(long long Time, long long TimeFields)
{

	__NtRoutine("RtlTimeToTimeFields", Time, TimeFields);
}

BOOLEAN h_KeSetTimer(_KTIMER* Timer, LARGE_INTEGER DueTime, _KDPC* Dpc)
{
	Logger::Log("\tTimer object : %llx\n", Timer);
	Logger::Log("\tDPC object : %llx\n", Dpc);


	memcpy(&Timer->DueTime, &DueTime, sizeof(DueTime));
	

	
	return true;
}

void h_KeInitializeTimer(_KTIMER* Timer) {
	InitializeListHead(&Timer->TimerListEntry);
}
ULONG_PTR h_KeIpiGenericCall(PVOID BroadcastFunction, ULONG_PTR Context)
{
	Logger::Log("\tBroadcastFunction: %llx\n", static_cast<const void*>(BroadcastFunction));
	Logger::Log("\tContent: %llx\n", reinterpret_cast<const void*>(Context));
	auto ret = static_cast<long long(*)(ULONG_PTR)>(BroadcastFunction)(Context);
	Logger::Log("\tIPI Returned : %d\n", ret);
	return ret;

	//return 0;
}

_SLIST_ENTRY* h_ExpInterlockedPopEntrySList(PSLIST_HEADER SListHead)
{
	return 0;
}

typedef struct _CALLBACK_OBJECT
{
	ULONG Signature;
	KSPIN_LOCK Lock;
	LIST_ENTRY RegisteredCallbacks;
	BOOLEAN AllowMultipleCallbacks;
	UCHAR reserved[3];
} CALLBACK_OBJECT;

CALLBACK_OBJECT test = { 0 };

NTSTATUS h_ExCreateCallback(void* CallbackObject, void* ObjectAttributes, bool Create, bool AllowMultipleCallbacks)
{
	OBJECT_ATTRIBUTES* oa = (OBJECT_ATTRIBUTES*)ObjectAttributes;
	_CALLBACK_OBJECT** co = (_CALLBACK_OBJECT**)CallbackObject;
	Logger::Log("\tCallback object : %llx\n", CallbackObject);
	Logger::Log("\t*Callback object : %llx\n", *co);
	Logger::Log("\tCallback name : %ls\n", oa->ObjectName->Buffer);
	*co = (_CALLBACK_OBJECT*)0x10e4e9c820;
	return 0;
}

NTSTATUS h_KeDelayExecutionThread(char WaitMode, BOOLEAN Alertable, PLARGE_INTEGER Interval)
{
	Sleep(Interval->QuadPart * -1 / 10000);
	return STATUS_SUCCESS;
}

ULONG h_DbgPrompt(PCCH Prompt, PCH Response, ULONG Length)
{
	RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, 0);
	return 0x0;
}

NTSTATUS h_KdChangeOption(
	ULONG Option,
	ULONG     InBufferBytes,
	PVOID     InBuffer,
	ULONG     OutBufferBytes,
	PVOID     OutBuffer,
	PULONG    OutBufferNeeded
) {
	return 0xC0000354; // STATUS_DEBUGGER_INACTIVE
}

NTSTATUS h_IoDeleteSymbolicLink(PUNICODE_STRING SymbolicLinkName)
{

	int TemporaryObject; // ebx
	OBJECT_ATTRIBUTES ObjectAttributes; // [rsp+20h] [rbp-30h] BYREF
	HANDLE LinkHandle; // [rsp+60h] [rbp+10h] BYREF

	memset(&ObjectAttributes.Attributes + 1, 0, 20);
	LinkHandle = 0;
	ObjectAttributes.RootDirectory = 0;
	ObjectAttributes.ObjectName = SymbolicLinkName;
	*(uintptr_t*)&ObjectAttributes.Length = 48;
	ObjectAttributes.Attributes = 576;
	TemporaryObject = __NtRoutine("ZwOpenSymbolicLinkObject", &LinkHandle, 0x10000u, &ObjectAttributes);
	if (TemporaryObject >= 0)
	{
		TemporaryObject = __NtRoutine("ZwMakeTemporaryObject", LinkHandle);
		if (TemporaryObject >= 0)
			h_ZwClose(&LinkHandle);
	}

	return TemporaryObject;
}

LONG h_KeSetEvent(_KEVENT* Event, LONG Increment, BOOLEAN Wait)
{
	LONG PreviousState;
	_KTHREAD* Thread;


	/*
	 * Check if this is an signaled notification event without an upcoming wait.
	 * In this case, we can immediately return TRUE, without locking.
	 */
	if ((Event->Header.Type == 0) &&
		(Event->Header.SignalState == 1) &&
		!(Wait))
	{
		/* Return the signal state (TRUE/Signalled) */
		return TRUE;
	}

	/* Save the Previous State */
	PreviousState = Event->Header.SignalState;

	auto hEvent = HandleManager::GetHandle((uintptr_t)Event);
	SetEvent((HANDLE)hEvent);
	return PreviousState;
}

#define STATUS_SUCCESS                ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_OVERFLOW        ((NTSTATUS)0x80000005L)
#define STATUS_UNSUCCESSFUL           ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED        ((NTSTATUS)0xC0000002L)
#define STATUS_INFO_LENGTH_MISMATCH   ((NTSTATUS)0xC0000004L)
#ifndef STATUS_INVALID_PARAMETER
// It is now defined in Windows 2008 SDK.
#define STATUS_INVALID_PARAMETER      ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_CONFLICTING_ADDRESSES  ((NTSTATUS)0xC0000018L)
#define STATUS_ACCESS_DENIED          ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL       ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_NAME_NOT_FOUND  ((NTSTATUS)0xC0000034L)
#define STATUS_PROCEDURE_NOT_FOUND    ((NTSTATUS)0xC000007AL)
#define STATUS_INVALID_IMAGE_FORMAT   ((NTSTATUS)0xC000007BL)
#define STATUS_NO_TOKEN               ((NTSTATUS)0xC000007CL)

#define CURRENT_PROCESS ((HANDLE) -1)
#define CURRENT_THREAD  ((HANDLE) -2)
#define NtCurrentProcess CURRENT_PROCESS

NTSTATUS h_PsRemoveLoadImageNotifyRoutine(void* NotifyRoutine)
{

	return STATUS_PROCEDURE_NOT_FOUND;
}

NTSTATUS h_PsSetCreateProcessNotifyRoutineEx(void* NotifyRoutine, BOOLEAN Remove)
{
	if (Remove) {
		return STATUS_INVALID_PARAMETER;
	}
	else {
		return STATUS_SUCCESS;
	}
}

UCHAR h_KeAcquireSpinLockRaiseToDpc(PKSPIN_LOCK SpinLock)
{

	return (UCHAR)0x00;
}

void h_KeReleaseSpinLock(PKSPIN_LOCK SpinLock, UCHAR NewIrql)
{


}

void h_ExWaitForRundownProtectionRelease(_EX_RUNDOWN_REF* RunRef)
{

}

BOOLEAN h_KeCancelTimer(_KTIMER* Timer)
{

	return true;
}

PVOID h_MmGetSystemRoutineAddress(PUNICODE_STRING SystemRoutineName)
{

	char cStr[512] = { 0 };
	wchar_t* wStr = SystemRoutineName->Buffer;
	PVOID funcptr = 0;
	wcstombs(cStr, SystemRoutineName->Buffer, 256);
	Logger::Log("\tRetrieving %s ptr ", cStr);


	if (constantTimeExportProvider.contains(cStr)) {
		funcptr = constantTimeExportProvider[cStr];
	}

	if (funcptr) {//Was it static exported variable 
		Logger::Log(prototypedMsg);
		return funcptr;
	}

	if (myConstantProvider.contains(cStr))
		funcptr = myConstantProvider[cStr].hook;

	if (funcptr == nullptr) {
		funcptr = GetProcAddress(ntdll, cStr);
		if (funcptr == nullptr) {

#ifdef STUB_UNIMPLEMENTED
			Logger::Log("\033[38;5;9mUSING STUB\033[0m\n");
			funcptr = unimplemented_stub;
#else
			Logger::Log("\033[38;5;9mNOT_IMPLEMENTED\033[0m\n");
			funcptr = 0;
			exit(0);
#endif
		}
		else {
			Logger::Log(passthroughMsg);
		}
	}
	else {
		Logger::Log(prototypedMsg);
	}

	return funcptr;
}

HANDLE h_PsGetThreadProcessId(_ETHREAD* Thread) {
	if (Thread) {
		Thread->Cid.UniqueProcess;
	}return 0;
}

HANDLE h_PsGetThreadProcess(_ETHREAD* Thread) {
	if (Thread) {
		//todo impl
		Logger::Log("\th_PsGetThreadProcess un impl!\n");
		return 0;
	} return 0;
}

void h_ProbeForRead(void* address, size_t len, ULONG align) {
	Logger::Log("\tProbeForRead -> {%llx}(len: %d) align: %d\n", address, len, align);
}
void h_ProbeForWrite(void* address, size_t len, ULONG align) {
	Logger::Log("\tProbeForWrite -> {%llx}(len: %d) align: %d\n", address, len, align);
}




int h__vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list argptr)
{

	return __NtRoutine("_vsnwprintf", buffer, count, format, argptr);
}




//todo fix mutex bs
void h_KeInitializeMutex(PVOID Mutex, ULONG level)
{

}

LONG h_KeReleaseMutex(PVOID Mutex, BOOLEAN Wait) { return 0; }

//todo object might be invalid
NTSTATUS h_KeWaitForSingleObject(
	PVOID Object,
	void* WaitReason,
	void* WaitMode, BOOLEAN Alertable,
	PLARGE_INTEGER Timeout) {

	auto hEvent = HandleManager::GetHandle((uintptr_t)Object);
	WaitForSingleObject((HANDLE)hEvent, INFINITE);
	return STATUS_SUCCESS;
};

ULONG h_KeQueryTimeIncrement() {
	return 1000;
}

//todo impl might be broken
NTSTATUS h_PsCreateSystemThread(
	PHANDLE ThreadHandle, ULONG DesiredAccess,
	void* ObjectAttributes,
	HANDLE ProcessHandle, void* ClientId, void* StartRoutine,
	PVOID StartContext) {
	auto ret = CreateThread(nullptr, 4096, (LPTHREAD_START_ROUTINE)StartRoutine, StartContext, 0, 0);
	*ThreadHandle = ret;
	//Sleep(100000);
	return STATUS_SUCCESS;
}

//todo impl 
NTSTATUS h_PsTerminateSystemThread(
	NTSTATUS exitstatus) {
	Logger::Log("\tthread boom\n");
	//__debugbreak(); int* a = 0; *a = 1; return 0;
	ExitThread(exitstatus);
}

//todo impl
void h_IofCompleteRequest(void* pirp, CHAR boost) {

}

//todo impl
NTSTATUS h_IoCreateSymbolicLink(PUNICODE_STRING SymbolicLinkName, PUNICODE_STRING DeviceName) {
	return STATUS_SUCCESS;
}




BOOL h_IoIsSystemThread(_ETHREAD* thread) {
	return true;
}



void h_IoDeleteDevice(_DEVICE_OBJECT* obj) {

}

//todo definitely will blowup
void* h_IoGetTopLevelIrp() {
	Logger::Log("\tIoGetTopLevelIrp blows up sorry\n");
	static int irp = 0;
	return &irp;
}

NTSTATUS h_ObReferenceObjectByHandle(
	HANDLE handle,
	ACCESS_MASK DesiredAccess,
	GUID* ObjectType,
	uint64_t AccessMode,
	PVOID* Object,
	void* HandleInformation) {
	Logger::Log("\th_ObReferenceObjectByHandle blows up sorry\n");
	*(PHANDLE)(Object) = &FakeKernelThread;
	if (HandleInformation) {
		*(PHANDLE)(HandleInformation) = handle;
	}

	return 0;
}

//todo more logic required
NTSTATUS h_ObRegisterCallbacks(PVOID CallbackRegistration, PVOID* RegistrationHandle) {
	*RegistrationHandle = (PVOID)0xDEADBEEFCAFE;
	return STATUS_SUCCESS;
}

void h_ObUnRegisterCallbacks(PVOID RegistrationHandle) {

}

void* h_ObGetFilterVersion(void* arg) {
	return 0;
}

BOOLEAN h_MmIsAddressValid(PVOID VirtualAddress) {
	Logger::Log("\tChecking for %llx\n", VirtualAddress);
	if (VirtualAddress == 0)
		return false;
	return true; // rand() % 2 :troll:
}

NTSTATUS h_PsSetCreateThreadNotifyRoutine(PVOID NotifyRoutine) {
	return STATUS_SUCCESS;
}

NTSTATUS h_PsSetLoadImageNotifyRoutine(PVOID NotifyRoutine) { return STATUS_SUCCESS; }

BOOLEAN h_ExAcquireResourceExclusiveLite(
	_ERESOURCE* Resource,
	BOOLEAN    Wait
) {
	//Resource->OwnerEntry.OwnerThread = (uint64_t)h_KeGetCurrentThread();

	return true;
}

NTSTATUS h_KdSystemDebugControl(int Command, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnLength,
	/*KPROCESSOR_MODE*/ int PreviousMode)
{
	// expected behaviour when no debugger is attached
	return 0xC0000022;
}


NTSTATUS h_ZwOpenSection(
	PHANDLE            SectionHandle,
	ACCESS_MASK        DesiredAccess,
	OBJECT_ATTRIBUTES* ObjectAttributes
) {

	auto ret = __NtRoutine("ZwOpenSection", SectionHandle, DesiredAccess, ObjectAttributes);
	Logger::Log("\tSection name : %ls, access : %llx, ret : %08x\n", ObjectAttributes->ObjectName->Buffer, DesiredAccess, ret);

	return ret;
}


/*
uint64_t TranslateLinearAddress(uint64_t directoryTableBase, uint64_t virtualAddress)
{
	uint16_t PML4 = (uint16_t)((virtualAddress >> 39) & 0x1FF);         //<! PML4 Entry Index
	uint16_t DirectoryPtr = (uint16_t)((virtualAddress >> 30) & 0x1FF); //<! Page-Directory-Pointer Table Index
	uint16_t Directory = (uint16_t)((virtualAddress >> 21) & 0x1FF);    //<! Page Directory Table Index
	uint16_t Table = (uint16_t)((virtualAddress >> 12) & 0x1FF);        //<! Page Table Index

																	// Read the PML4 Entry. DirectoryTableBase has the base address of the table.
																	// It can be read from the CR3 register or from the kernel process object.
	uint64_t PML4E = 0;// ReadPhysicalAddress<ulong>(directoryTableBase + (ulong)PML4 * sizeof(ulong));
	Read(directoryTableBase + (uint64_t)PML4 * sizeof(uint64_t), (uint8_t*)&PML4E, sizeof(PML4E));

	if (PML4E == 0)
		return 0;

	// The PML4E that we read is the base address of the next table on the chain,
	// the Page-Directory-Pointer Table.
	uint64_t PDPTE = 0;// ReadPhysicalAddress<ulong>((PML4E & 0xFFFF1FFFFFF000) + (ulong)DirectoryPtr * sizeof(ulong));
	Read((PML4E & 0xFFFF1FFFFFF000) + (uint64_t)DirectoryPtr * sizeof(uint64_t), (uint8_t*)&PDPTE, sizeof(PDPTE));

	if (PDPTE == 0)
		return 0;

	//Check the PS bit
	if ((PDPTE & (1 << 7)) != 0)
	{
		// If the PDPTE¨s PS flag is 1, the PDPTE maps a 1-GByte page. The
		// final physical address is computed as follows:
		// ！ Bits 51:30 are from the PDPTE.
		// ！ Bits 29:0 are from the original va address.
		return (PDPTE & 0xFFFFFC0000000) + (virtualAddress & 0x3FFFFFFF);
	}

	// PS bit was 0. That means that the PDPTE references the next table
	// on the chain, the Page Directory Table. Read it.
	uint64_t PDE = 0;// ReadPhysicalAddress<ulong>((PDPTE & 0xFFFFFFFFFF000) + (ulong)Directory * sizeof(ulong));
	Read((PDPTE & 0xFFFFFFFFFF000) + (uint64_t)Directory * sizeof(uint64_t), (uint8_t*)&PDE, sizeof(PDE));

	if (PDE == 0)
		return 0;

	if ((PDE & (1 << 7)) != 0)
	{
		// If the PDE¨s PS flag is 1, the PDE maps a 2-MByte page. The
		// final physical address is computed as follows:
		// ！ Bits 51:21 are from the PDE.
		// ！ Bits 20:0 are from the original va address.
		return (PDE & 0xFFFFFFFE00000) + (virtualAddress & 0x1FFFFF);
	}

	// PS bit was 0. That means that the PDE references a Page Table.
	uint64_t PTE = 0;// ReadPhysicalAddress<ulong>((PDE & 0xFFFFFFFFFF000) + (ulong)Table * sizeof(ulong));
	Read((PDE & 0xFFFFFFFFFF000) + (uint64_t)Table * sizeof(uint64_t), (uint8_t*)&PTE, sizeof(PTE));

	if (PTE == 0)
		return 0;

	// The PTE maps a 4-KByte page. The
	// final physical address is computed as follows:
	// ！ Bits 51:12 are from the PTE.
	// ！ Bits 11:0 are from the original va address.
	return (PTE & 0xFFFFFFFFFF000) + (virtualAddress & 0xFFF);
}*/

typedef struct _MMPTE_HARDWARE
{
	union
	{
		struct
		{
			UINT64 Valid : 1;
			UINT64 Write : 1;
			UINT64 Owner : 1;
			UINT64 WriteThrough : 1;
			UINT64 CacheDisable : 1;
			UINT64 Accessed : 1;
			UINT64 Dirty : 1;
			UINT64 LargePage : 1;
			UINT64 Available : 4;
			UINT64 PageFrameNumber : 36;
			UINT64 ReservedForHardware : 4;
			UINT64 ReservedForSoftware : 11;
			UINT64 NoExecute : 1;
		};
		UINT64 AsUlonglong;
	};
} MMPTE_HARDWARE, * PMMPTE_HARDWARE;

static constexpr auto s_1MB = 1ULL * 1024 * 1024;
static constexpr auto s_1GB = 1ULL * 1024 * 1024 * 1024;
static constexpr auto s_256GB = 256ULL * s_1GB;
static constexpr auto s_512GB = 512ULL * s_1GB;

static const auto s_UserSharedData = reinterpret_cast<PVOID>(0x7FFE0000ULL);
static const auto s_UserSharedDataEnd = reinterpret_cast<PVOID>(0x7FFF0000ULL);
static const auto s_LowestValidAddress = reinterpret_cast<PVOID>(0x10000ULL);
static const auto s_HighestValidAddress = reinterpret_cast<PVOID>(s_256GB - 1);

static constexpr auto s_Pml4PhysicalAddress = s_512GB - s_1GB;


struct RamRange {
	uint64_t base;
	uint64_t size;
};

RamRange myRam[9] = {
	{0x1000, 0x57000},
	{0x59000, 0x46000},
	{0x100000, 0xb81b9000},
	{0xb82f1000, 0x3b0000},
	{0xb86a3000, 0xcc58000},
	{0xc6b99000, 0xfd000},
	{0xc7ba2000, 0x5e000},
	{0x100000000, 0x337000000},
	{0, 0}
};

uint64_t AllocatedContiguous = 0;

RamRange* h_MmGetPhysicalMemoryRanges() {
	return myRam;

}

PVOID h_MmAllocateContiguousMemorySpecifyCache(
	SIZE_T              NumberOfBytes,
	uintptr_t    LowestAcceptableAddress,
	uintptr_t    HighestAcceptableAddress,
	uintptr_t    BoundaryAddressMultiple,
	uint32_t CacheType
) {
	Logger::Log("\tLowest : %llx - Highest : %llx - Boundary : %llx - Cache Type : %d - Size : %08x\n", LowestAcceptableAddress, HighestAcceptableAddress, BoundaryAddressMultiple, CacheType, NumberOfBytes);
	AllocatedContiguous = (uint64_t)hM_AllocPool(CacheType, NumberOfBytes);
	return (PVOID)AllocatedContiguous;

}
/*
Result for cfe7f3f9f000 : 1ad000
0 : 1000
0 : 57000
1 : 59000
1 : 46000
2 : 100000
2 : b81b9000
3 : b82f1000
3 : 3b0000
4 : b86a3000
4 : cc58000
5 : c6b99000
5 : fd000
6 : c7ba2000
6 : 5e000
7 : 100000000
7 : 337000000
*/

unsigned long long h_MmGetPhysicalAddress(uint64_t BaseAddress) { //To test shit
	auto virtualAddress = BaseAddress;

	uint16_t PML4 = (uint16_t)((virtualAddress >> 39) & 0x1FF);         //<! PML4 Entry Index
	uint16_t DirectoryPtr = (uint16_t)((virtualAddress >> 30) & 0x1FF); //<! Page-Directory-Pointer Table Index
	uint16_t Directory = (uint16_t)((virtualAddress >> 21) & 0x1FF);    //<! Page Directory Table Index
	uint16_t Table = (uint16_t)((virtualAddress >> 12) & 0x1FF);
	/*


	Logger::Log("\tPML4 : %llx\n", PML4);
	Logger::Log("\tDirectoryPtr : %llx\n", DirectoryPtr);
	Logger::Log("\tDirectory : %llx\n", Directory);
	Logger::Log("\tTable : %llx\n", Table);

	*/
	Logger::Log("\tGetting Physical address for %llx\n", BaseAddress);
	uint64_t ret = 0;

	if (BaseAddress == AllocatedContiguous) {
		Logger::Log("\tGetting physical for last Contiguous Allocated Memory.\n");
		ret = 0xb0000000;
	}
	if (BaseAddress == 0xcfe7f3f9f000) {
		ret = 0x1ad000;
	}
	else if (BaseAddress == 0xfb7dbedf6000) {
		ret = 0x200000;
	}
	else if (BaseAddress == 0xfbfdfeff7000) {
		ret = 0x200000;
	}
	else if (BaseAddress == 0xfc7e3f1f8000) {
		ret = 0x200000;
	}
	else if (BaseAddress == 0xfcfe7f3f9000) {
		ret = 0x200000;
	}

	Logger::Log("\tReturn : %llx\n", ret);
	return ret;
}


#define ADDRESS_AND_SIZE_TO_SPAN_PAGES(_Va, _Size) \
   ((ULONG) ((((ULONG_PTR) (_Va) & (PAGE_SIZE - 1)) \
     + (_Size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT))

typedef ULONG PFN_COUNT;
typedef ULONG PFN_NUMBER, * PPFN_NUMBER;
typedef LONG SPFN_NUMBER, * PSPFN_NUMBER;

#define MDL_ALLOCATED_FIXED_SIZE   0x0008

#define BYTE_OFFSET(Va) \
   ((ULONG) ((ULONG_PTR) (Va) & (PAGE_SIZE - 1)))

#define PAGE_ALIGN(Va) \
   ((PVOID) ((ULONG_PTR)(Va) & ~(PAGE_SIZE - 1)))

void MmInitializeMdl(
	PMDL MemoryDescriptorList,
	PVOID BaseVa,
	SIZE_T Length) {

	MemoryDescriptorList->Next = (PMDL)NULL;
	MemoryDescriptorList->Size = (USHORT)(sizeof(MDL) + (sizeof(PFN_NUMBER) * ADDRESS_AND_SIZE_TO_SPAN_PAGES(BaseVa, Length)));
	MemoryDescriptorList->MdlFlags = 0;
	MemoryDescriptorList->StartVa = (PVOID)PAGE_ALIGN(BaseVa);
	MemoryDescriptorList->ByteOffset = BYTE_OFFSET(BaseVa);
	MemoryDescriptorList->ByteCount = (ULONG)Length;
}

PMDL NTAPI h_IoAllocateMdl(IN PVOID 	VirtualAddress,
	IN ULONG 	Length,
	IN BOOLEAN 	SecondaryBuffer,
	IN BOOLEAN 	ChargeQuota,
	IN _IRP* Irp
) {
	PMDL Mdl = NULL, p;
	ULONG Flags = 0;
	ULONG Size;

	if (Length & 0x80000000) return NULL;
	Size = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);
	if (Size > 23)
	{
		/* This is bigger then our fixed-size MDLs. Calculate real size */
		Size *= sizeof(PFN_NUMBER);
		Size += sizeof(MDL);
		if (Size > 0xFFFF) return NULL;
	}
	else
	{
		/* Use an internal fixed MDL size */
		Size = (23 * sizeof(PFN_NUMBER)) + sizeof(MDL);
		Flags |= MDL_ALLOCATED_FIXED_SIZE;

	}

	/* Check if we don't have an mdl yet */
	if (!Mdl)
	{
		/* Allocate one from pool */
		Mdl = (PMDL)hM_AllocPoolTag(0, Size, 'MdlT');
		if (!Mdl) return NULL;
	}

	/* Initialize it */

	MmInitializeMdl(Mdl, VirtualAddress, Length);
	Mdl->MdlFlags |= Flags;

	/* Check if an IRP was given too */
	if (Irp)
	{
		/* Check if it came with a secondary buffer */
		if (SecondaryBuffer)
		{
			/* Insert the MDL at the end */
			p = Irp->MdlAddress;
			while (p->Next) p = p->Next;
			p->Next = Mdl;
		}
		else
		{
			/* Otherwise, insert it directly */
			Irp->MdlAddress = Mdl;
		}
	}

	/* Return the allocated mdl */
	return Mdl;


}

PVOID h_ExRegisterCallback(
	PVOID   CallbackObject,
	PVOID CallbackFunction,
	PVOID              CallbackContext
) {
	return CallbackObject;
}

void h_KeInitializeGuardedMutex(
	_KGUARDED_MUTEX* Mutex
) {
	Mutex->Owner = 0i64;
	Mutex->Count = 1;
	Mutex->Contention = 0;
	Mutex->Gate.Header.Type = 1;
	Mutex->Gate.Header.Size = 6;
	Mutex->Gate.Header.Signalling = 0;
	Mutex->Gate.Header.SignalState = 0;
	Mutex->Gate.Header.WaitListHead.Flink = &Mutex->Gate.Header.WaitListHead;
	Mutex->Gate.Header.WaitListHead.Blink = &Mutex->Gate.Header.WaitListHead;

}

NTSTATUS
h_KeWaitForMultipleObjects(
	ULONG Count,
	PVOID Object[],
	uint32_t WaitType,
	_KWAIT_REASON WaitReason,
	uint32_t WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout,
	_KWAIT_BLOCK* WaitBlockArray
) {
	uintptr_t* handleList = (uintptr_t*)malloc(sizeof(uintptr_t*) * Count);
	for (int i = 0; i < Count; i++) {
		handleList[i] = (uintptr_t)HandleManager::GetHandle((uintptr_t)Object[i]);
	}
	bool waitAll = false;
	if (WaitType == 0)
		waitAll = true;
	else if (WaitType == 1)
		waitAll = false;
	else {
		DebugBreak();
	}
	DWORD WaitMS = 0;
	if (Timeout && Timeout->QuadPart < 0) {
		WaitMS = Timeout->QuadPart * -1 / 10000;
	}
	else if (!Timeout) {
		WaitMS = INFINITE;

	}
	else {
		DebugBreak();
	}
	auto ret = WaitForMultipleObjects(Count, (const HANDLE*)handleList, waitAll, WaitMS);

	return STATUS_SUCCESS;

}

void h_KeClearEvent(_KEVENT *Event) { //This should set the Event to non-signaled

	auto hEvent = HandleManager::GetHandle((uintptr_t)Event);

	if (!hEvent) {
		DebugBreak;
	}

	ResetEvent((HANDLE)hEvent);
	return;

}

void Initialize() {
	myConstantProvider.insert({ "KeClearEvent", {1, h_KeClearEvent} });
	myConstantProvider.insert({ "KeWaitForMutexObject", {1, h_KeWaitForSingleObject} }); //KeWaitForMutexObject = KeWaitForSingleObject
	myConstantProvider.insert({ "ExRegisterCallback", {1, h_ExRegisterCallback} });
	myConstantProvider.insert({ "KeWaitForMultipleObjects", {1, h_KeWaitForMultipleObjects} });
	myConstantProvider.insert({ "KeInitializeGuardedMutex", {1, h_KeInitializeGuardedMutex} });
	myConstantProvider.insert({ "IoAllocateMdl", {1, h_IoAllocateMdl} });
	myConstantProvider.insert({ "MmAllocateContiguousMemorySpecifyCache", {1, h_MmAllocateContiguousMemorySpecifyCache} });
	myConstantProvider.insert({ "MmGetPhysicalMemoryRanges", {1, h_MmGetPhysicalMemoryRanges} });
	myConstantProvider.insert({ "MmGetPhysicalAddress", {1, h_MmGetPhysicalAddress} });
	myConstantProvider.insert({ "_vsnwprintf",	 {1, h__vsnwprintf} });
	myConstantProvider.insert({ "ZwOpenSection", {1, h_ZwOpenSection} });
	myConstantProvider.insert({ "MmGetSystemRoutineAddress", {1, h_MmGetSystemRoutineAddress} });
	myConstantProvider.insert({ "IoDeleteSymbolicLink", { 1, h_IoDeleteSymbolicLink } });
	myConstantProvider.insert({ "PsRemoveLoadImageNotifyRoutine", {1, h_PsRemoveLoadImageNotifyRoutine } });
	myConstantProvider.insert({ "PsSetCreateProcessNotifyRoutineEx", {2, h_PsSetCreateProcessNotifyRoutineEx } });
	myConstantProvider.insert({ "PsSetCreateProcessNotifyRoutine", {2, h_PsSetCreateProcessNotifyRoutineEx} });
	myConstantProvider.insert({ "KeAcquireSpinLockRaiseToDpc",{ 1, h_KeAcquireSpinLockRaiseToDpc } });
	myConstantProvider.insert({ "PsRemoveCreateThreadNotifyRoutine", {1, h_PsRemoveLoadImageNotifyRoutine} });
	myConstantProvider.insert({ "KeReleaseSpinLock",{ 2, h_KeReleaseSpinLock} });
	myConstantProvider.insert({ "ExpInterlockedPopEntrySList", {1, h_ExpInterlockedPopEntrySList} });
	myConstantProvider.insert({ "KeDelayExecutionThread", {3, h_KeDelayExecutionThread} });
	myConstantProvider.insert({ "ExWaitForRundownProtectionRelease", {1, h_ExWaitForRundownProtectionRelease} });
	myConstantProvider.insert({ "KeCancelTimer", {1, h_KeCancelTimer} });
	myConstantProvider.insert({ "KeSetEvent", {3, h_KeSetEvent} });
	myConstantProvider.insert({ "KeSetTimer",{ 3, h_KeSetTimer} });
	myConstantProvider.insert({ "ExCreateCallback", {4, h_ExCreateCallback } });
	myConstantProvider.insert({ "IoCreateFileEx",{ 1, h_IoCreateFileEx } });
	myConstantProvider.insert({ "RtlDuplicateUnicodeString", {1, h_RtlDuplicateUnicodeString } });
	myConstantProvider.insert({ "IoDeleteController", {1, h_IoDeleteController } });
	myConstantProvider.insert({ "SeQueryInformationToken", {1, h_SeQueryInformationToken } });
	myConstantProvider.insert({ "PsReferencePrimaryToken",{ 1, h_PsReferencePrimaryToken } });
	myConstantProvider.insert({ "PsIsProtectedProcess",{ 1, h_PsIsProtectedProcess } });
	myConstantProvider.insert({ "NtQueryInformationProcess", {1, h_NtQueryInformationProcess } });
	myConstantProvider.insert({ "PsGetCurrentThreadProcessId", {1, h_PsGetCurrentThreadProcessId } });
	myConstantProvider.insert({ "IoGetCurrentThreadProcessId", {1, h_PsGetCurrentThreadProcessId} });
	myConstantProvider.insert({ "PsGetCurrentThreadId", {1, h_PsGetCurrentThreadId} });
	myConstantProvider.insert({ "IoGetCurrentThreadId", {1, h_PsGetCurrentThreadId} });
	myConstantProvider.insert({ "PsGetCurrentProcess",{ 1, h_PsGetCurrentProcess } });
	myConstantProvider.insert({ "IoGetCurrentProcess",{ 1, h_PsGetCurrentProcess } });
	myConstantProvider.insert({ "PsGetProcessId", {1, h_PsGetProcessId} });
	myConstantProvider.insert({ "PsGetProcessWow64Process",{ 1, h_PsGetProcessWow64Process} });
	myConstantProvider.insert({ "PsLookupProcessByProcessId", {1, h_PsLookupProcessByProcessId} });
	myConstantProvider.insert({ "RtlCompareString", {1, h_RtlCompareString} });
	myConstantProvider.insert({ "PsGetProcessCreateTimeQuadPart", {1, h_PsGetProcessCreateTimeQuadPart} });
	myConstantProvider.insert({ "ObfReferenceObject", {1, h_ObfReferenceObject} });
	myConstantProvider.insert({ "ExAcquireFastMutex",{ 1, h_ExAcquireFastMutex } });
	myConstantProvider.insert({ "ExReleaseFastMutex", {1, h_ExReleaseFastMutex} });
	myConstantProvider.insert({ "ZwQueryFullAttributesFile", {2, h_ZwQueryFullAttributesFile} });
	myConstantProvider.insert({ "RtlWriteRegistryValue",{ 6, h_RtlWriteRegistryValue} });
	myConstantProvider.insert({ "RtlInitUnicodeString", {2, h_RtlInitUnicodeString} });
	myConstantProvider.insert({ "ZwOpenKey", {3, h_ZwOpenKey} });
	myConstantProvider.insert({ "ZwFlushKey",{ 1, h_ZwFlushKey} });
	myConstantProvider.insert({ "ZwClose", {1, h_ZwClose} });
	myConstantProvider.insert({ "NtClose",{ 1, h_ZwClose} });
	myConstantProvider.insert({ "ZwQuerySystemInformation",{ 4, h_NtQuerySystemInformation } });
	myConstantProvider.insert({ "NtQuerySystemInformation", {4, h_NtQuerySystemInformation } });
	myConstantProvider.insert({ "ExAllocatePoolWithTag", {3, hM_AllocPoolTag} });
	myConstantProvider.insert({ "ExAllocatePool", {2, hM_AllocPool} });
	myConstantProvider.insert({ "ExFreePoolWithTag", {2, h_DeAllocPoolTag} });
	myConstantProvider.insert({ "ExFreePool", {1, h_DeAllocPool} });
	myConstantProvider.insert({ "RtlRandomEx",{ 1, h_RtlRandomEx } });
	myConstantProvider.insert({ "IoCreateDevice", {7, h_IoCreateDevice} });
	myConstantProvider.insert({ "IoIsSystemThread", {1, h_IoIsSystemThread} });
	myConstantProvider.insert({ "KeInitializeEvent",{ 3, h_KeInitializeEvent } });
	myConstantProvider.insert({ "RtlGetVersion", {1, h_RtlGetVersion} });
	myConstantProvider.insert({ "DbgPrint", {1, printf } });
	myConstantProvider.insert({ "__C_specific_handler",{ 1, _c_exception} });
	myConstantProvider.insert({ "RtlMultiByteToUnicodeN", {1, h_RtlMultiByteToUnicodeN } });
	myConstantProvider.insert({ "KeAreAllApcsDisabled", {1, h_KeAreAllApcsDisabled} });
	myConstantProvider.insert({ "KeAreApcsDisabled", {1, h_KeAreApcsDisabled } });
	myConstantProvider.insert({ "ZwCreateFile", {1, h_NtCreateFile} });
	myConstantProvider.insert({ "ZwQueryInformationFile",{ 1, h_NtQueryInformationFile} });
	myConstantProvider.insert({ "ZwReadFile", {1, h_NtReadFile} });
	myConstantProvider.insert({ "ZwQueryValueKey", {1, h_ZwQueryValueKey} });
	myConstantProvider.insert({ "IoWMIOpenBlock",{ 1, h_IoWMIOpenBlock} });
	myConstantProvider.insert({ "IoWMIQueryAllData", {1, h_IoWMIQueryAllData} });
	myConstantProvider.insert({ "ObfDereferenceObject", {1, h_ObfDereferenceObject } });
	myConstantProvider.insert({ "PsLookupThreadByThreadId", {1, h_PsLookupThreadByThreadId } });
	myConstantProvider.insert({ "RtlDuplicateUnicodeString", {3, h_RtlDuplicateUnicodeString } });
	myConstantProvider.insert({ "ExSystemTimeToLocalTime", {2, h_ExSystemTimeToLocalTime} });
	myConstantProvider.insert({ "ProbeForRead", { 3, h_ProbeForRead } });
	myConstantProvider.insert({ "ProbeForWrite", { 3, h_ProbeForWrite } });
	myConstantProvider.insert({ "RtlTimeToTimeFields", { 2, h_RtlTimeToTimeFields } });
	myConstantProvider.insert({ "KeInitializeMutex", { 2, h_KeInitializeMutex } });
	myConstantProvider.insert({ "KeReleaseMutex", { 2, h_KeReleaseMutex } });
	myConstantProvider.insert({ "KeWaitForSingleObject", { 5, h_KeWaitForSingleObject } });
	myConstantProvider.insert({ "PsCreateSystemThread", { 7, h_PsCreateSystemThread } });
	myConstantProvider.insert({ "PsTerminateSystemThread", { 1, h_PsTerminateSystemThread } });
	myConstantProvider.insert({ "IofCompleteRequest", { 2, h_IofCompleteRequest } });
	myConstantProvider.insert({ "IoCreateSymbolicLink", { 2, h_IoCreateSymbolicLink } });
	myConstantProvider.insert({ "IoDeleteDevice", { 1, h_IoDeleteDevice } });
	myConstantProvider.insert({ "IoGetTopLevelIrp", { 0, h_IoGetTopLevelIrp } });
	myConstantProvider.insert({ "ObReferenceObjectByHandle", { 6, h_ObReferenceObjectByHandle } });
	myConstantProvider.insert({ "ObRegisterCallbacks", { 3, h_ObRegisterCallbacks } });
	myConstantProvider.insert({ "ObUnRegisterCallbacks", { 1, h_ObUnRegisterCallbacks } });
	myConstantProvider.insert({ "ObGetFilterVersion", { 1, h_ObGetFilterVersion } }); // undoc func
	myConstantProvider.insert({ "MmIsAddressValid", { 1, h_MmIsAddressValid } });
	myConstantProvider.insert({ "PsSetCreateThreadNotifyRoutine", { 1, h_PsSetCreateThreadNotifyRoutine } });
	myConstantProvider.insert({ "PsSetLoadImageNotifyRoutine", { 1, h_PsSetLoadImageNotifyRoutine } });
	myConstantProvider.insert({ "PsGetCurrentProcessId", { 1, h_PsGetCurrentThreadProcessId } });
	myConstantProvider.insert({ "PsGetThreadId", { 1, h_PsGetThreadId } });
	myConstantProvider.insert({ "PsGetThreadProcessId", { 1, h_PsGetThreadProcessId } });
	myConstantProvider.insert({ "PsGetThreadProcess", { 1, h_PsGetThreadProcess } });
	myConstantProvider.insert({ "IoQueryFileDosDeviceName", { 1, h_IoQueryFileDosDeviceName } });
	myConstantProvider.insert({ "ObOpenObjectByPointer", { 1, h_ObOpenObjectByPointer } });
	myConstantProvider.insert({ "ObQueryNameString", { 1, h_ObQueryNameString } });
	myConstantProvider.insert({ "PsGetProcessInheritedFromUniqueProcessId", { 1, h_PsGetProcessInheritedFromUniqueProcessId } });
	myConstantProvider.insert({ "PsGetProcessPeb", { 1, h_PsGetProcessPeb } });
	myConstantProvider.insert({ "KeQueryTimeIncrement", {1, h_KeQueryTimeIncrement} });
	myConstantProvider.insert({ "ExAcquireResourceExclusiveLite", {1, h_ExAcquireResourceExclusiveLite} });
	myConstantProvider.insert({ "vswprintf_s", {1, h_vswprintf_s} });
	myConstantProvider.insert({ "swprintf_s", {1, h_swprintf_s} });
	myConstantProvider.insert({ "wcscpy_s", {1, h_wcscpy_s} });
	myConstantProvider.insert({ "wcscat_s", {1, h_wcscat_s} });
	myConstantProvider.insert({ "KeIpiGenericCall", {1, h_KeIpiGenericCall} });
	myConstantProvider.insert({ "KeInitializeTimer", {1, h_KeInitializeTimer} });
	myConstantProvider.insert({ "DbgPrompt", {1, h_DbgPrompt} });
	myConstantProvider.insert({ "KdChangeOption", {1, h_KdChangeOption} });
	myConstantProvider.insert({ "KdSystemDebugControl", { 1, h_KdSystemDebugControl } });
}