#include "stdafx.h"
#include "Profiler.h"

#undef Begin
#undef End
#undef WritingProfile
#undef InitializeProfiler

CProfiler oProfiler;

#if PROFILING == TRUE
CProfiler::CProfiler()
{
	QueryPerformanceFrequency(&m_liStandard);
	InitializeProfiler();
}

CProfiler::~CProfiler()
{
	WritingProfile();
}

void CProfiler::Begin(const char* ProfileName)
{
	int iCnt = 0;
	for (int i = 0; i < 30; i++)
	{
		if (m_tProfileStruct[i].m_pName == nullptr)
		{
			m_tProfileStruct[i].m_pName = (char*)ProfileName;
			iCnt = i;
			break;
		}
		if (strcmp(ProfileName, m_tProfileStruct[i].m_pName) == 0)
		{
			iCnt = i;
			break;
		}
	}
	
	if (m_tProfileStruct[iCnt].m_bIsOpen == true)
		throw;

	m_tProfileStruct[iCnt].m_bIsOpen = true;
	QueryPerformanceCounter(&m_tProfileStruct[iCnt].m_liStartTime);
}

void CProfiler::End(const char* ProfileName)
{
	int iCnt = 0;
	for (int i = 0; i < 30; i++)
	{
		if (strcmp(ProfileName, m_tProfileStruct[i].m_pName) == 0 &&
			m_tProfileStruct[i].m_bIsOpen == true)
		{
			iCnt = i;
			break;
		}
		if (m_tProfileStruct[i].m_pName == nullptr)
		{
			throw;
		}
	}

	LARGE_INTEGER EndTime;
	QueryPerformanceCounter(&EndTime);
	
	long long Result = EndTime.QuadPart - m_tProfileStruct[iCnt].m_liStartTime.QuadPart;
	for (int i = 0; i < LEAVE_VAULE; i++)
	{
		if (m_tProfileStruct[iCnt].m_llMin[i] > Result)
		{
			if (i != 0)
			{
				m_tProfileStruct[iCnt].m_llMin[i - 1] = m_tProfileStruct[iCnt].m_llMin[i];
			}

			m_tProfileStruct[iCnt].m_llMin[i] = Result;
		}
		else if (m_tProfileStruct[iCnt].m_llMax[i] < Result)
		{
			if (i != 0)
			{
				m_tProfileStruct[iCnt].m_llMax[i - 1] = m_tProfileStruct[iCnt].m_llMax[i];
			}

			m_tProfileStruct[iCnt].m_llMax[i] = Result;
		}
	}

	m_tProfileStruct[iCnt].m_llTimeSum += Result;
	++m_tProfileStruct[iCnt].m_iNumOfCall;
	m_tProfileStruct[iCnt].m_bIsOpen = false;
}

void CProfiler::WritingProfile()
{
	char FileName[40] = "Profiling ";
	char TimeBuffer[30];
	__time64_t Time;
	tm newTime;
	_time64(&Time);
	localtime_s(&newTime, &Time);
	asctime_s(TimeBuffer, (size_t)sizeof(TimeBuffer), &(const tm)newTime);
	strcat_s(FileName, TimeBuffer);
	FileName[CANCLE_RETURN] = '\0';
	strcat_s(FileName, ".txt");

	for (int iCnt = 0; iCnt < 30; iCnt++)
	{
		if (FileName[iCnt] == ':')
			FileName[iCnt] = '_';
	}

	FILE *fp;
	fopen_s(&fp, FileName, "w");

	fprintf_s(fp, "                       Name  |             Average  |                 Min  |                 Max  |    Call  \n");
	fprintf_s(fp, "-----------------------------|----------------------|----------------------|----------------------|----------\n");

	for (int i = 0; i < 30; i++)
	{
		if (m_tProfileStruct[i].m_pName == nullptr)
			break;
		fprintf_s(fp, "%29s|%20f§Á|%20f§Á|%20f§Á|%10d\n", m_tProfileStruct[i].m_pName, (long double)m_tProfileStruct[i].m_llTimeSum / (long double)m_tProfileStruct[i].m_iNumOfCall * 1000000 / (long double)m_liStandard.QuadPart
			, (long double)m_tProfileStruct[i].m_llMin[0] * 1000000 / (long double)m_liStandard.QuadPart, (long double)m_tProfileStruct[i].m_llMax[0] * 1000000 / (long double)m_liStandard.QuadPart
			, m_tProfileStruct[i].m_iNumOfCall);
	}
	
	fprintf_s(fp, "-----------------------------|----------------------|----------------------|----------------------|----------\n");
	fclose(fp);
}

void CProfiler::InitializeProfiler()
{
	for (int j = 0; j < 30; ++j)
	{
		m_tProfileStruct[j].m_pName = nullptr;
		m_tProfileStruct[j].m_iNumOfCall = 0;
		for (int i = 0; i < 3; i++)
		{
			m_tProfileStruct[j].m_llMin[i] = 90000000;
			m_tProfileStruct[j].m_llMax[i] = 0;
		}
		m_tProfileStruct[j].m_llTimeSum = 0;
		m_tProfileStruct[j].m_bIsOpen = false;
	}
}

#endif