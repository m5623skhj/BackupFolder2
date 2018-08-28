#pragma once

#pragma comment(lib, "DbgHelp.lib")

#include <Windows.h>
#include <signal.h>
#include <crtdbg.h>
#include <Psapi.h>
#include <DbgHelp.h>
#include <stdio.h>

class CCrashDump
{
public:
	static long _DumpCount;

	CCrashDump()
	{
		_DumpCount = 0;

		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = myInvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler); // crt 함수에 null 포인터 등을 넣었을 때
		_CrtSetReportMode(_CRT_WARN, 0);						 // crt 오류 메시지 표시 중단 및 덤프로 바로 남김
		_CrtSetReportMode(_CRT_ASSERT, 0);						 // crt 오류 메시지 표시 중단 및 덤프로 바로 남김
		_CrtSetReportMode(_CRT_ERROR, 0);						 // crt 오류 메시지 표시 중단 및 덤프로 바로 남김

																 // pure virtual function called 에러 핸들러를 사용자 정의 함수로 우회시킨다
		_set_purecall_handler(myPurecallHandler);

		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		signal(SIGABRT, signalHandler);
		signal(SIGINT, signalHandler);
		signal(SIGILL, signalHandler);
		signal(SIGFPE, signalHandler);
		signal(SIGSEGV, signalHandler);
		signal(SIGTERM, signalHandler);

		SetHandlerDump();
	}
	static void Crash()
	{
		int *p = nullptr;
		*p = 0;
	}
	static long __stdcall MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		int iWorkingMemory = 0;
		SYSTEMTIME stNowTime;

		long DumpCount = InterlockedIncrement(&_DumpCount);
		// 현재 프로세스의 메모리 사용량을 얻어온다
		HANDLE hProcess = 0;
		PROCESS_MEMORY_COUNTERS pmc;

		hProcess = GetCurrentProcess();

		if (NULL == hProcess)
			return 0;

		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		{
			iWorkingMemory = (int)(pmc.WorkingSetSize / 1024 / 1024);
		}
		CloseHandle(hProcess);

		// 현재 날짜와 시간을 알아온다
		WCHAR filename[MAX_PATH];

		GetLocalTime(&stNowTime);
		wsprintf(filename, L"Dump_%d02%d02%d_%02d.%02d.%02d_%d_%dMB.dmp", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
			stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount, iWorkingMemory);
		wprintf(filename, L"\n\n\n ----- Crash Error ----- %d.%d.%d / %d:%d:%d \n\n\n", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
			stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
		wprintf(L"Now Save dump file ....\n\n\n");

		HANDLE hDumpFile = ::CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE,
			NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION Minidump;
			Minidump.ThreadId = ::GetCurrentThreadId();
			Minidump.ExceptionPointers = pExceptionPointer;
			Minidump.ClientPointers = TRUE;

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile,
				MiniDumpWithFullMemory, &Minidump, NULL, NULL);

			CloseHandle(hDumpFile);
			wprintf(L"CrashDump Save Finish\n\n\n");
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}
	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);
	}
	static long __stdcall RedirectedSetUnHandledExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
	{
		MyExceptionFilter(exceptionInfo);
		return EXCEPTION_EXECUTE_HANDLER;
	}
	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* filfile, unsigned int line, uintptr_t pReserved)
	{
		Crash();
	}
	static void myPurecallHandler()
	{
		Crash();
	}
	static void signalHandler(int Error)
	{
		Crash();
	}
};

long CCrashDump::_DumpCount = 0;
