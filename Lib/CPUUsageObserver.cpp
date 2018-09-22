#include "PreCompile.h"
#include <Windows.h>
#include "CPUUsageObserver.h"

CCPUUsageObserver::CCPUUsageObserver(HANDLE hProcess)
{
	if (hProcess == INVALID_HANDLE_VALUE)
		m_hProcess = GetCurrentProcess();

	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);

	m_iNumOfProcessors = SystemInfo.dwNumberOfProcessors;

	m_fProcessorTotal = 0;
	m_fProcessorKernel = 0;
	m_fProcessorUser = 0;

	m_fProcessTotal = 0;
	m_fProcessKernel = 0;
	m_fProcessUser = 0;

	m_uiProcessor_LastKernel.QuadPart = 0;
	m_uiProcessor_LastUser.QuadPart = 0;
	m_uiProcessor_LastIdle.QuadPart = 0;

	m_uiProcess_LastKernel.QuadPart = 0;
	m_uiProcess_LastUser.QuadPart = 0;
	m_uiProcess_LastTime.QuadPart = 0;

	UpdateCPUTime();
}

CCPUUsageObserver::~CCPUUsageObserver()
{

}

void CCPUUsageObserver::UpdateCPUTime()
{
	////////////////////////////////////////////
	// 프로세서 사용률을 갱신함
	////////////////////////////////////////////
	ULARGE_INTEGER IdleTime;
	ULARGE_INTEGER KernelTime;
	ULARGE_INTEGER UserTime;

	////////////////////////////////////////////
	// 시스템 사용 시간을 구함
	////////////////////////////////////////////
	if (GetSystemTimes((PFILETIME)&IdleTime, (PFILETIME)&KernelTime, (PFILETIME)&UserTime) == false)
		return;

	ULONGLONG IdleTimeDif = IdleTime.QuadPart - m_uiProcessor_LastIdle.QuadPart;
	// KernelTime 에는 IdleTime 이 포함되어 있음
	ULONGLONG KernelTimeDif = KernelTime.QuadPart - m_uiProcessor_LastKernel.QuadPart;
	ULONGLONG UserTimeDif = UserTime.QuadPart - m_uiProcessor_LastUser.QuadPart;

	ULONGLONG TotalTime = KernelTimeDif + UserTimeDif;
	ULONGLONG TimeDif;

	if (TotalTime == 0)
	{
		m_fProcessorTotal = 0;
		m_fProcessorKernel = 0;
		m_fProcessorUser = 0;
	}
	else
	{
		// KernelTime 에 IdleTime 이 포함되어 있으므로
		// IdleTime 을 빼서 계산함
		m_fProcessorTotal = (float)((double)(TotalTime - IdleTimeDif) / TotalTime * 100.0f);
		m_fProcessorKernel = (float)((double)UserTimeDif / TotalTime * 100.0f);
		m_fProcessorUser = (float)((double)(KernelTimeDif - IdleTimeDif) / TotalTime * 100.0f);
	}

	ULARGE_INTEGER m_uiProcessor_LastKernel = KernelTime;
	ULARGE_INTEGER m_uiProcessor_LastUser = UserTime;
	ULARGE_INTEGER m_uiProcessor_LastIdle = IdleTime;

	// 지정된 프로세스 사용률을 갱신함
	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;

	////////////////////////////////////////////
	// 현재의 100 나노세컨드 단위 시간을 구함 (UTC 시간)
	// 프로세스 사용률 판단의 공식
	// a = 샘플간격의 시스템 시간 (실제 지나간 시간)
	// b = 프로세스의 CPU 사용 시간
	// a : 100 = b : 사용률
	////////////////////////////////////////////

	// 얼마나 시간이 지났는지 100 나노세컨드 시간을 구함
	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);

	// 해당 프로세스가 사용한 시간을 구함
	// 두번째, 세번째는 실행 / 종료 시간이므로 사용하지 않음
	GetProcessTimes(m_hProcess, (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&KernelTime, (LPFILETIME)&UserTime);

	// 이전에 저장된 프로세스 시간과의 차를 구하여
	// 시간이 얼마나 지났는지를 확인함
	TimeDif = NowTime.QuadPart - m_uiProcess_LastTime.QuadPart;
	UserTimeDif = UserTime.QuadPart - m_uiProcess_LastUser.QuadPart;
	KernelTimeDif = KernelTime.QuadPart - m_uiProcess_LastKernel.QuadPart;

	TotalTime = KernelTimeDif + UserTimeDif;

	m_fProcessTotal = (float)(TotalTime / (double)m_iNumOfProcessors / (double)TimeDif * 100.0f);
	m_fProcessKernel = (float)(KernelTimeDif / (double)m_iNumOfProcessors / (double)TimeDif * 100.0f);
	m_fProcessUser = (float)(UserTimeDif / (double)m_iNumOfProcessors / (double)TimeDif * 100.0f);

	m_uiProcess_LastTime = NowTime;
	m_uiProcess_LastKernel = KernelTime;
	m_uiProcess_LastUser = UserTime;
}

