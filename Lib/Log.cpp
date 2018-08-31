#include "PreComfile.h"
#include "Log.h"
#include <time.h>

// 출력 저장 대상의 로그 레벨
int g_iLogLevel = LOG_LEVEL::LOG_DEBUG;
// 로그를 남긴 것들에 대한 카운터
// 완전히 신뢰할 수 있는 값은 아니며
// 대략적인 순서를 파악하는 것에만 적용이 가능하다
static int g_iLogCounter = 0;
// 로그의 경로
static WCHAR g_szFilePath[256];
// 파일을 쓸 때 동시에 접근하지 못하게 하기 위해서
// 해당 부분에 락을 걸어둠
static SRWLOCK g_SRWLock;

void SetLogLevel(int LogLevel)
{
	// 로그 수준에 오류가 있을시 디버그 레벨로 로그를 남김
	if (LogLevel >= LOG_LEVEL::LOGLEVEL_MAX || LogLevel < LOG_LEVEL::LOG_DEBUG)
	{
		wprintf_s(L"Incorsrect Log Level\nSet Default Log Level : LOG_DEBUG\n\n");
		g_iLogLevel = LOG_LEVEL::LOG_DEBUG;
	}
	else
		g_iLogLevel = LogLevel;
	InitializeSRWLock(&g_SRWLock);
}

void SetLogDirectoryPath(const WCHAR* DirectoryPath)
{
	wcscpy_s(g_szFilePath, DirectoryPath);
	wcscat_s(g_szFilePath, L"\\");
}

void Log(const WCHAR *LogType, const WCHAR *szLog, int LogLevel)
{
	WCHAR szLogBuf[LOG_BUF_MAX];
	WCHAR FilePath[LOG_FILEPATH_MAX];

	time_t nowTime = 0;
	tm NowTime;
	nowTime = time(NULL);
	localtime_s(&NowTime, &nowTime);
	NowTime.tm_year -= 100;
	NowTime.tm_mon += 1;
	// 받아온 로그의 타입이 너무 길 경우 Too Long Type 에러로 교체하여 로그를 남김
	if (FAILED(StringCchPrintfW(FilePath, LOG_FILEPATH_MAX, L"%s%02d%02d_%s.txt", g_szFilePath, NowTime.tm_year, NowTime.tm_mon, LogType)))
	{
		memcpy(FilePath, FilePath, LOG_LONGPATH_CUT);
		FilePath[LOG_LONGPATH_CUT - 1] = '\0';
		Log(L"[Too Long Type]", FilePath, LOG_LEVEL::LOG_SYSTEM);
		return;
	}

	// 받아온 로그가 너무 길 경우 Too Long Type 에러로 교체하여 로그를 남김
	if (FAILED(StringCchPrintfW(szLogBuf, LOG_BUF_MAX, L"[%s] [%02d-%02d-%02d %02d:%02d:%02d] LEVEL : %d / [%09d] %s\n",
		LogType, NowTime.tm_year, NowTime.tm_mon, NowTime.tm_mday, NowTime.tm_hour, NowTime.tm_min, NowTime.tm_sec,
		LogLevel, InterlockedIncrement((volatile ULONGLONG*)&g_iLogCounter), szLog)))
	{
		memcpy(szLogBuf, szLogBuf, LOG_LONGTEXT_CUT);
		FilePath[LOG_LONGTEXT_CUT - 1] = '\0';
		Log(L"[Too Long Log]", szLogBuf, LOG_LEVEL::LOG_SYSTEM);
		return;
	}
	wprintf(L"Level : %d\n%s\n", LogLevel, szLog);

	AcquireSRWLockExclusive(&g_SRWLock);
	
	FILE *fp;
	// 그냥 a 모드로 열었을 시 생성할 때는 ANSI 로 파일을 생성하기 때문에
	// 파일이 없다면 w 모드로 파일을 생성하고 
	// 파일이 존재한다면 r+ 모드로 파일 마지막 부분부터 씀
	_wfopen_s(&fp, FilePath, L"r+, ccs=UNICODE");
	if (fp == NULL)
	{
		// 파일이 없다면 파일을 생성하고 거기에 씀
		_wfopen_s(&fp, FilePath, L"wt, ccs=UNICODE");
		fwrite(szLogBuf, sizeof(WCHAR), wcslen(szLogBuf), fp);
		fclose(fp);
	}
	else
	{
		// 파일이 있다면 파일 마지막 부분으로 넘어가고 거기에 씀
		fseek(fp, 0, SEEK_END);
		fwrite(szLogBuf, sizeof(WCHAR), wcslen(szLogBuf), fp);
		fclose(fp);
	}
	ReleaseSRWLockExclusive(&g_SRWLock);
}

void LogHex(const WCHAR *LogType, WCHAR *szLog, int LogLevel, BYTE *HexPtr, int HexLen)
{
	WCHAR HexBuf[LOG_HEX_MAX];

	BYTE *pHexPtr = (BYTE*)HexBuf;
	const char *hex = "0123456789ABCDEF";
	for (int i = 0; i < HexLen; ++i)
	{
		HexBuf[i*3]   = hex[(*HexPtr >> 4) & 0xF];
		HexBuf[i*3+1] = hex[(*HexPtr) & 0xF];
		HexBuf[i*3+2] = ' ';
		HexPtr++;
	}

	HexBuf[HexLen * 3 - 1] = '\0';
	StringCchCatW(szLog, LOG_BUF_MAX, HexBuf);
	Log(LogType, szLog, LogLevel);
}