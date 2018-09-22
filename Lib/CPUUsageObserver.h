#pragma once


class CCPUUsageObserver
{
public :
	
	/////////////////////////////////////////////////////////
	// 생성자, 확인대상 프로세스 핸들
	// 미입력시 자기 자신이 디폴트로 들어감
	/////////////////////////////////////////////////////////
	CCPUUsageObserver(HANDLE hProcess = INVALID_HANDLE_VALUE);
	~CCPUUsageObserver();

	void UpdateCPUTime();
	
	float ProcessorTotal() { return m_fProcessorTotal; }
	float ProcessorUser() { return m_fProcessorUser; }
	float ProcessorKernel() { return m_fProcessorKernel; }

	float ProcessTotal() { return m_fProcessTotal; }
	float ProcessUser() { return m_fProcessUser; }
	float ProcessKernel() { return m_fProcessKernel; }

private :

	HANDLE m_hProcess;
	// 하이퍼 스레딩까지 포함된 프로세서 개수
	// 사용 시간을 해당 변수로 나눔
	int	m_iNumOfProcessors;

	float m_fProcessorTotal;
	float m_fProcessorUser;
	float m_fProcessorKernel;

	float m_fProcessTotal;
	float m_fProcessUser;
	float m_fProcessKernel;

	ULARGE_INTEGER m_uiProcessor_LastKernel;
	ULARGE_INTEGER m_uiProcessor_LastUser;
	ULARGE_INTEGER m_uiProcessor_LastIdle;

	ULARGE_INTEGER m_uiProcess_LastKernel;
	ULARGE_INTEGER m_uiProcess_LastUser;
	ULARGE_INTEGER m_uiProcess_LastTime;
};