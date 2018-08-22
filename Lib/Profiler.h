#pragma once
#include <Windows.h>
#include <stdio.h>
#include <time.h>

#define LEAVE_VAULE 30

#define NAME_LENGTH 30
#define CANCLE_RETURN 34

#define PROFILING TRUE
#if PROFILING == TRUE


struct ProfileStruct
{
	char				*m_pName;
	bool				m_bIsOpen;
	int					m_iNumOfCall;
	long long			m_llMin[LEAVE_VAULE];
	long long			m_llMax[LEAVE_VAULE];
	long long			m_llTimeSum;
	LARGE_INTEGER		m_liStartTime;
};

class CProfiler
{
private:
	LARGE_INTEGER		m_liStandard;
	ProfileStruct		m_tProfileStruct[30];
public:
	CProfiler();
	~CProfiler();
	void Begin(const char* ProfileName);
	void End(const char* ProfileName);
	void WritingProfile();
	void InitializeProfiler();
};
extern CProfiler oProfiler;

#define Begin(x) oProfiler.Begin(x)
#define End(x) oProfiler.End(x)
#define WritingProfile() oProfiler.WritingProfile()
#define InitializeProfiler() oProfiler.InitializeProfiler()

#else
#define Begin(x)  
#define End(x)  
#define WritingProfile()  
#define InitializeProfiler()  
#endif



