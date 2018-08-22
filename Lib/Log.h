#pragma once
#include <strsafe.h>
#include <Windows.h>

#define LOG_BUF_MAX				2048
#define LOG_LONGTEXT_CUT		1024
#define LOG_FILEPATH_MAX		512
#define LOG_LONGPATH_CUT		256
#define LOG_HEX_MAX				128

extern int g_iLogLevel;
enum LOG_LEVEL {LOG_DEBUG = 0, LOG_WARNING, LOG_ERROR, LOG_SYSTEM, LOGLEVEL_MAX};

// 로그를 남길 경로 지정
void SetLogDirectoryPath(const WCHAR* DirectoryPath);
// 남길 로그들의 최소 수준 지정
void SetLogLevel(int LogLevel);
void Log(const WCHAR *LogType, const WCHAR *szLog, int LogLevel);
void LogHex(const WCHAR *LogType, WCHAR *szLog, int LogLevel, BYTE *HexPtr, int HexLen);

// 지정한 디버그 레벨과 같거나 높은 수준의 로그만 발생시킴
// 버퍼의 최대크기(LOG_BUF_MAX) 보다 클 경우 
// LOG_LONGTEXT_CUT 값 만큼 잘라서 해당 로그가 너무 길다는 로그를 남김
#define _LOG(LogLevel, LogType, ...)															\
do{																								\
	WCHAR szLogBuf[LOG_BUF_MAX];																\
	if(g_iLogLevel <= LogLevel)																	\
	{																							\
		if(FAILED(StringCchPrintfW(szLogBuf, LOG_BUF_MAX, ##__VA_ARGS__)))						\
		{																						\
			StringCchPrintfW(szLogBuf, LOG_LONGTEXT_CUT, ##__VA_ARGS__);						\
			szLogBuf[LOG_LONGTEXT_CUT - 1] = '\0';												\
			Log(L"[Too Long Log]", szLogBuf, LOG_LEVEL::LOG_SYSTEM);							\
		}																						\
		else																					\
		{																						\
			Log(LogType, szLogBuf, LogLevel);													\
		}																						\
	}																							\
}while(0)							

// 지정한 디버그 레벨과 같거나 높은 수준의 로그만 발생시킴
// HexPtr 에는 헥사 데이터를 보고 싶은 곳의 포인터를
// HexLen 에는 해당 길이를 넣어 줘야함
// 버퍼의 최대크기(LOG_BUF_MAX) 보다 클 경우 
// LOG_LONGTEXT_CUT 값 만큼 잘라서 해당 로그가 너무 길다는 로그를 남김
#define _LOGHEX(LogLevel, LogType, HexPtr, HexLen, ...)											\
do{																								\
	WCHAR szLogBuf[LOG_BUF_MAX];																\
	if(g_iLogLevel <= LogLevel)																	\
	{																							\
		if(FAILED(StringCchPrintfW(szLogBuf, LOG_BUF_MAX, ##__VA_ARGS__)))						\
		{																						\
			StringCchPrintfW(szLogBuf, LOG_LONGTEXT_CUT, ##__VA_ARGS__);						\
			szLogBuf[LOG_LONGTEXT_CUT - 1] = '\0';												\
			Log(L"[Too Long Log]", szLogBuf, LOG_LEVEL::LOG_SYSTEM);							\
		}																						\
		else																					\
		{																						\
			LogHex(LogType, szLogBuf, LogLevel, HexPtr, HexLen);								\
		}																						\
	}																							\
}while(0)		