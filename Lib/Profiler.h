#pragma once
#include <stdio.h>
#include <time.h>

// 한 스레드에 대하여 최대로 프로파일링 할 수 있는 갯수
#define df_MAX_PROFILE				30

// 한 프로파일링 항목에 대하여 최소 최대값을 제외할 갯수
// 양 극값은 의미 없는 값이 될 것이므로 제외함
#define df_MAX_EXCLUDE_VAULE		10

// 프로파일링 이름 최대 길이
#define df_NAME_LENGTH				30
// 파일 이름 전용
#define df_CANCLE_RETURN			34

// 프로파일링 매니저가 최대로 가질 수 있는 스레드 갯수
// 스레드 갯수가 이 값보다 클 경우 프로파일링이 망가지거나 
// 혹은 예외가 일어날 수 있음
#define df_MAX_THREAD				5

// 프로파일링 안함 상태
// 해당 헤더는 아무것도 없는 상태의 헤더가 됨
#define NOT_PROFILING				 0
// 싱글 스레드 전용 프로파일링 상태
// 싱글 스레드 전용으로 돌아가며
// 이 상태로 멀티 스레드를 프로파일링 시 
// 프로파일링이 망가지거나 예외가 일어날 수 있음
#define SINGLE_THREAD_PROFILING		 1
// 멀티 스레드 전용 프로파일링 상태
#define MULTI_THREAD_PROFILING		 2

#define PROFILING NOT_PROFILING

#if PROFILING == SINGLE_THREAD_PROFILING

struct st_PROFILE_INFO
{
	char				m_pName[df_NAME_LENGTH];
	bool				m_bIsOpen;
	int					m_iNumOfCall;
	long long			m_llMin[df_MAX_EXCLUDE_VAULE];
	long long			m_llMax[df_MAX_EXCLUDE_VAULE];
	long long			m_llTimeSum;
	LARGE_INTEGER		m_liStartTime;
};

class CProfiler
{
private:
	LARGE_INTEGER		m_liStandard;
	st_PROFILE_INFO		m_ProfileInfo[30];
public:
	CProfiler();
	~CProfiler();

	void Begin(const char* ProfileName);
	void End(const char* ProfileName);
	void WritingProfile();
	void WritingProfile(FILE *fp);
	void InitializeProfiler();
};

extern CProfiler g_Profiler;

#define Begin(x) g_Profiler.Begin(x)
#define End(x) g_Profiler.End(x)
#define WritingProfile() g_Profiler.WritingProfile()
#define InitializeProfiler() g_Profiler.InitializeProfiler()

#elif PROFILING == MULTI_THREAD_PROFILING

struct st_PROFILE_INFO
{
	char				m_pName[df_NAME_LENGTH];
	bool				m_bIsOpen;
	int					m_iNumOfCall;
	long long			m_llMin[df_MAX_EXCLUDE_VAULE];
	long long			m_llMax[df_MAX_EXCLUDE_VAULE];
	long long			m_llTimeSum;
	LARGE_INTEGER		m_liStartTime;
};

class CProfiler
{
private:
	int					m_iThreadID;
	LARGE_INTEGER		m_liStandard;
	st_PROFILE_INFO		m_ProfileInfo[30];
public:
	CProfiler();
	~CProfiler();

	void Begin(const char* ProfileName);
	void End(const char* ProfileName);
	void WritingProfile();
	void WritingProfile(FILE *fp);
	void InitializeProfiler();

	void SetThreadID(int ThreadID) { m_iThreadID = ThreadID; }
};

class CProfilerManager
{
private:
	int						m_iThreadIndex;
	int						m_iTLSIndex;
	CProfiler				*m_pProfiler[df_MAX_THREAD];

public:
	CProfilerManager();
	~CProfilerManager();

	void Begin(const char *pProfileName);
	void End(const char *pProfileName);
	void WritingProfileAll();
	void ClearProfileAll();
};

extern CProfilerManager g_ProfilerManager;

#define Begin(x) g_ProfilerManager.Begin(x)
#define End(x) g_ProfilerManager.End(x)
#define WritingProfile() g_ProfilerManager.WritingProfileAll()
#define InitializeProfiler() g_ProfilerManager.ClearProfileAll()

#else

#define Begin(x)
#define End(x)
#define WritingProfile()
#define InitializeProfiler()

#endif



