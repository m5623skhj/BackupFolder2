#include "PreComfile.h"
#include "Profiler.h"
//#include <Windows.h>
#include <processthreadsapi.h>

#undef Begin
#undef End
#undef WritingProfile
#undef InitializeProfiler

#if PROFILING == SINGLE_THREAD_PROFILING
CProfiler g_Profiler;
#elif PROFILING == MULTI_THREAD_PROFILING
CProfilerManager g_ProfilerManager;
#endif

#if PROFILING == SINGLE_THREAD_PROFILING || PROFILING == MULTI_THREAD_PROFILING
CProfiler::CProfiler()
{
	QueryPerformanceFrequency(&m_liStandard);
	InitializeProfiler();
}

CProfiler::~CProfiler()
{
	//WritingProfile();
}

void CProfiler::Begin(const char* ProfileName)
{
	int ProfileNumber = 0;
	// Profiler 에 이전에 내가 기록하고 있었는지 혹은
	// 내가 새 자리에 기록할 것인지를 판별함
	for (int i = 0; i < df_MAX_PROFILE; i++)
	{
		// 현재 자리가 비어있다면 인자로 받은 문자열을 복사함
		if (m_ProfileInfo[i].m_pName[0] == 0)
		{
			strcpy_s(m_ProfileInfo[i].m_pName, ProfileName);
			ProfileNumber = i;
			break;
		}
		// 현재 자리가 비어있지 않다면
		// 내가 이전에 인자로 받은 문자열과 같은 문자열인지 확인함
		else if (strcmp(ProfileName, m_ProfileInfo[i].m_pName) == 0)
		{
			ProfileNumber = i;
			break;
		}
	}

	// 현재 자리의 프로파일러가 이전에 기록 후 닫지 않았다면 (Begin 을 두번 연속 호출했다면)
	// 문제로 인식하고 사용자가 처리하도록 유도시킴
	if (m_ProfileInfo[ProfileNumber].m_bIsOpen == true)
		throw;

	m_ProfileInfo[ProfileNumber].m_bIsOpen = true;
	// 시작 시간을 클럭 발생 단위로 정밀하게 측정함
	QueryPerformanceCounter(&m_ProfileInfo[ProfileNumber].m_liStartTime);
}

void CProfiler::End(const char* ProfileName)
{
	LARGE_INTEGER EndTime;
	QueryPerformanceCounter(&EndTime);

	int ProfileNumber = 0;
	// 인자로 받아온 문자열과 이전에 Begin 에서 기록한 문자열을 비교하여
	// 어느 프로파일의 상태를 닫을지 판단함
	// 이전에 열었던 정보가 없을 시 예외를 던져 사용자가 처리하도록 유도시킴
	for (int i = 0; i < 30; i++)
	{
		if (strcmp(ProfileName, m_ProfileInfo[i].m_pName) == 0 &&
			m_ProfileInfo[i].m_bIsOpen == true)
		{
			ProfileNumber = i;
			break;
		}
		if (m_ProfileInfo[i].m_pName[0] == 0)
		{
			throw;
		}
	}
	
	// 방금 얻어온 EndTime 에서 이전에 Begin 에서 얻은 StartTime 의 차를 구하여
	// 클럭 단위의 정밀한 시간을 얻어옴
	long long Result = EndTime.QuadPart - m_ProfileInfo[ProfileNumber].m_liStartTime.QuadPart;
	for (int insertNumber = 0; insertNumber < df_MAX_EXCLUDE_VAULE; insertNumber++)
	{
		// 이번에 얻은 결과가 이전에 기록된 insertNumber 번 째의 값 보다 작을 경우
		// 이번에 얻은 결과값을 insertNumber 의 자리와 교체함 
		if (m_ProfileInfo[ProfileNumber].m_llMin[insertNumber] > Result)
		{
			// insertNumber 가 0일 경우 이전에 자리에 있던 값은 버림
			if (insertNumber != 0)
			{
				m_ProfileInfo[ProfileNumber].m_llMin[insertNumber - 1] = m_ProfileInfo[ProfileNumber].m_llMin[insertNumber];
			}

			m_ProfileInfo[ProfileNumber].m_llMin[insertNumber] = Result;
		}
		else if (m_ProfileInfo[ProfileNumber].m_llMax[insertNumber] < Result)
		{
			// insertNumber 가 0일 경우 이전에 자리에 있던 값은 버림
			if (insertNumber != 0)
			{
				m_ProfileInfo[ProfileNumber].m_llMax[insertNumber - 1] = m_ProfileInfo[ProfileNumber].m_llMax[insertNumber];
			}

			m_ProfileInfo[ProfileNumber].m_llMax[insertNumber] = Result;
		}
	}

	m_ProfileInfo[ProfileNumber].m_llTimeSum += Result;
	++m_ProfileInfo[ProfileNumber].m_iNumOfCall;
	m_ProfileInfo[ProfileNumber].m_bIsOpen = false;
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
	FileName[df_CANCLE_RETURN] = '\0';
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
		if (m_ProfileInfo[i].m_pName[0] == NULL)
			break;
		fprintf_s(fp, "%29s|%20f㎲|%20f㎲|%20f㎲|%10d\n", m_ProfileInfo[i].m_pName, (long double)m_ProfileInfo[i].m_llTimeSum / (long double)m_ProfileInfo[i].m_iNumOfCall * 1000000 / (long double)m_liStandard.QuadPart
			, (long double)m_ProfileInfo[i].m_llMin[0] * 1000000 / (long double)m_liStandard.QuadPart, (long double)m_ProfileInfo[i].m_llMax[0] * 1000000 / (long double)m_liStandard.QuadPart
			, m_ProfileInfo[i].m_iNumOfCall);
	}

	fprintf_s(fp, "-----------------------------|----------------------|----------------------|----------------------|----------\n");
	fclose(fp);
}

void CProfiler::WritingProfile(FILE *fp)
{
	fprintf_s(fp, " Thread : %d\n", m_iThreadID);
	fprintf_s(fp, "                       Name  |             Average  |                 Min  |                 Max  |    Call  \n");
	fprintf_s(fp, "-----------------------------|----------------------|----------------------|----------------------|----------\n");

	for (int i = 0; i < 30; i++)
	{
		if (m_ProfileInfo[i].m_pName[0] == NULL)
			break;
		fprintf_s(fp, "%29s|%20f㎲|%20f㎲|%20f㎲|%10d\n", m_ProfileInfo[i].m_pName, (long double)m_ProfileInfo[i].m_llTimeSum / (long double)m_ProfileInfo[i].m_iNumOfCall * 1000000 / (long double)m_liStandard.QuadPart
			, (long double)m_ProfileInfo[i].m_llMin[0] * 1000000 / (long double)m_liStandard.QuadPart, (long double)m_ProfileInfo[i].m_llMax[0] * 1000000 / (long double)m_liStandard.QuadPart
			, m_ProfileInfo[i].m_iNumOfCall);
	}

	fprintf_s(fp, "-----------------------------|----------------------|----------------------|----------------------|----------\n\n\n\n\n\n");
}

void CProfiler::InitializeProfiler()
{
	for (int ProfileIdx = 0; ProfileIdx < 30; ++ProfileIdx)
	{
		m_ProfileInfo[ProfileIdx].m_pName[0] = 0;
		m_ProfileInfo[ProfileIdx].m_iNumOfCall = 0;
		for (int LeaveValue = 0; LeaveValue < df_MAX_EXCLUDE_VAULE; ++LeaveValue)
		{
			m_ProfileInfo[ProfileIdx].m_llMin[LeaveValue] = 90000000;
			m_ProfileInfo[ProfileIdx].m_llMax[LeaveValue] = 0;
		}
		m_ProfileInfo[ProfileIdx].m_llTimeSum = 0;
		m_ProfileInfo[ProfileIdx].m_bIsOpen = false;
	}
}



///////////////////////////////////////////
// CProfileManager Method
///////////////////////////////////////////

CProfilerManager::CProfilerManager()
{
	m_iThreadIndex = 0;
	for (int ProfileIdx = 0; ProfileIdx < df_MAX_THREAD; ++ProfileIdx)
	{
		m_pProfiler[ProfileIdx] = new CProfiler();
	}

	m_iTLSIndex = TlsAlloc();
}

CProfilerManager::~CProfilerManager()
{
	for (int ProfileIdx = 0; ProfileIdx < df_MAX_THREAD; ++ProfileIdx)
	{
		delete m_pProfiler[ProfileIdx];
	}
}

void CProfilerManager::Begin(const char *pProfileName)
{
	CProfiler *pProfiler = (CProfiler*)TlsGetValue(m_iTLSIndex);
	if (pProfiler == NULL)
	{
		UINT NowIndex = InterlockedIncrement((UINT*)&m_iThreadIndex) - 1;
		pProfiler = m_pProfiler[NowIndex];
		pProfiler->SetThreadID(GetCurrentThreadId());
		TlsSetValue(m_iTLSIndex, pProfiler);
	}

	pProfiler->Begin(pProfileName);
}

void CProfilerManager::End(const char *pProfileName)
{
	((CProfiler*)(TlsGetValue(m_iTLSIndex)))->End(pProfileName);
}

void CProfilerManager::WritingProfileAll()
{
	char FileName[40] = "Profiling ";
	char TimeBuffer[30];
	__time64_t Time;
	tm newTime;
	_time64(&Time);
	localtime_s(&newTime, &Time);
	asctime_s(TimeBuffer, (size_t)sizeof(TimeBuffer), &(const tm)newTime);
	strcat_s(FileName, TimeBuffer);
	FileName[df_CANCLE_RETURN] = '\0';
	strcat_s(FileName, ".txt");

	for (int iCnt = 0; iCnt < 30; iCnt++)
	{
		if (FileName[iCnt] == ':')
			FileName[iCnt] = '_';
	}

	FILE *fp;
	fopen_s(&fp, FileName, "w");

	for (int i = 0; i < df_MAX_THREAD; ++i)
		m_pProfiler[i]->WritingProfile(fp);

	fclose(fp);
	wprintf(L"Profiling Complete\n");
}

void CProfilerManager::ClearProfileAll()
{
	for (int i = 0; i < df_MAX_THREAD; ++i)
		m_pProfiler[i]->InitializeProfiler();

	wprintf(L"Profiling Sample Initialize Complete\n");
}


#endif