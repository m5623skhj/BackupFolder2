#pragma once
#include <Windows.h>
#include <stdio.h>
#include <time.h>

#define df_MAX_PROFILE				30

#define df_LEAVE_VAULE				30

#define df_NAME_LENGTH				30
#define df_CANCLE_RETURN			34

#define PROFILING TRUE
#if PROFILING == TRUE


struct st_PROFILE_INFO
{
	char				m_pName[df_NAME_LENGTH];
	bool				m_bIsOpen;
	int					m_iNumOfCall;
	long long			m_llMin[df_LEAVE_VAULE];
	long long			m_llMax[df_LEAVE_VAULE];
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
	void InitializeProfiler();
};
extern CProfiler g_Profiler;

#define Begin(x) g_Profiler.Begin(x)
#define End(x) g_Profiler.End(x)
#define WritingProfile() g_Profiler.WritingProfile()
#define InitializeProfiler() g_Profiler.InitializeProfiler()

#else
#define Begin(x)  
#define End(x)  
#define WritingProfile()  
#define InitializeProfiler()  
#endif



