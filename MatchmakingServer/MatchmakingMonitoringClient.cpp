#include "PreCompile.h"

#include "MatchmakingMonitoringClient.h"
#include "LanServerSerializeBuf.h"
#include "NetServerSerializeBuffer.h"
#include "Log.h"
#include "Protocol/CommonProtocol.h"

CMatchmakingMonitoringClient::CMatchmakingMonitoringClient()
{
	// 모니터링을 위한 pdh 정보 얻기
	PdhOpenQuery(NULL, NULL, &m_PdhQuery);

	PdhAddCounter(m_PdhQuery, L"\\Process(MatchmakingServer)\\Private Bytes", NULL, &m_PrivateBytes);

	PdhAddCounter(m_PdhQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &m_ProcessorUsage);
	PdhAddCounter(m_PdhQuery, L"\\Memory\\Available MBytes", NULL, &m_AvailableMemory);
	PdhAddCounter(m_PdhQuery, L"\\Network Interface(*)\\Bytes Received/sec", NULL, &m_NetworkRecv);
	PdhAddCounter(m_PdhQuery, L"\\Network Interface(*)\\Bytes Sent/sec", NULL, &m_NetworkSend);
	PdhAddCounter(m_PdhQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_NonPagedMemory);
}

CMatchmakingMonitoringClient::~CMatchmakingMonitoringClient()
{

}

bool CMatchmakingMonitoringClient::MatchmakingMonitoringClientStart(const WCHAR *szMatchmakingMonitoringClientOptionFile, UINT *SessionAll, UINT *LoginPlayer)
{
	if(!Start(szMatchmakingMonitoringClientOptionFile))
		return false;

	m_pNumOfSessionAll = SessionAll;
	m_pNumOfLoginCompleteUser = LoginPlayer;

	m_hMonitoringThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitoringInfoSendThread, this, 0, NULL);
	m_hMonitoringThreadExitHandle = CreateEvent(NULL, TRUE, FALSE, NULL);

	return true;
}

void CMatchmakingMonitoringClient::MatchmakingMonitoringClientStop()
{
	Stop();
	SetEvent(m_hMonitoringThreadExitHandle);

	WaitForSingleObject(m_hMonitoringThreadHandle, INFINITE);

	CloseHandle(m_hMonitoringThreadExitHandle);
	CloseHandle(m_hMonitoringThreadHandle);
}

void CMatchmakingMonitoringClient::OnConnectionComplete()
{
	// 모니터링 서버에 접속을 성공하였다면
	// 모니터링 서버에게 ServerNo 를 보내줌
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_MONITOR_LOGIN;

	SendBuf << Type << m_iServerNo;

	SendPacket(&SendBuf);

	m_hMonitoringThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitoringInfoSendThread, this, 0, 0);
}

void CMatchmakingMonitoringClient::OnRecv(CSerializationBuf *OutReadBuf)
{

}

void CMatchmakingMonitoringClient::OnSend()
{

}

void CMatchmakingMonitoringClient::OnWorkerThreadBegin()
{

}

void CMatchmakingMonitoringClient::OnWorkerThreadEnd()
{

}

void CMatchmakingMonitoringClient::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

UINT __stdcall CMatchmakingMonitoringClient::MonitoringInfoSendThread(LPVOID MonitoringClient)
{
	return ((CMatchmakingMonitoringClient*)MonitoringClient)->MonitoringInfoSender();
}

UINT CMatchmakingMonitoringClient::MonitoringInfoSender()
{
	int TimeStamp;

	while (1)
	{
		// 1 초 마다 일어나서 모니터링 서버에게 현재 모니터링 한 정보들을 보내줌
		if (WaitForSingleObject(m_hMonitoringThreadExitHandle, 1000) == WAIT_TIMEOUT)
		{
			TimeStamp = (int)time(NULL);
			PdhCollectQueryData(m_PdhQuery);

			SendMatchmakingServerOn(TimeStamp);
			SendMatchmakingCPU(TimeStamp);
			SendMatchmakingMemoryCommit(TimeStamp);
			SendMatchmakingPacketPool(TimeStamp);
			SendMatchmakingSessionAll(TimeStamp);
			SendMatchmakingLoginPlayer(TimeStamp);
			SendMatchmakingEnterRoomSuccessTPS(TimeStamp);

			SendCPUTotal(TimeStamp);
			SendAvailableMemroy(TimeStamp);
			SendNetworkRecv(TimeStamp);
			SendNetworkSend(TimeStamp);
			SendNonpagedMemory(TimeStamp);
		}
		// 외부에서 스레드를 정지시킴
		else
			break;
	}

	return 0;
}

void CMatchmakingMonitoringClient::SendMatchmakingServerOn(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MATCH_SERVER_ON;
	DataValue = 1;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendMatchmakingCPU(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	m_CPUObserver.UpdateCPUTime();
	DataType = dfMONITOR_DATA_TYPE_MATCH_CPU;
	DataValue = (int)m_CPUObserver.ProcessTotal();

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendMatchmakingMemoryCommit(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_PrivateBytes, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_MATCH_MEMORY_COMMIT;
	DataValue = (int)(CounterValue.longValue / dfDIVIDE_MEGABYTE);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendMatchmakingPacketPool(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	UINT NumOfUsePacket = CSerializationBuf::GetUsingSerializeBufNodeCount() + CNetServerSerializationBuf::GetUsingSerializeBufNodeCount();
	DataType = dfMONITOR_DATA_TYPE_MATCH_PACKET_POOL;
	DataValue = NumOfUsePacket;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendMatchmakingSessionAll(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MATCH_SESSION;
	DataValue = (int)*m_pNumOfSessionAll;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendMatchmakingLoginPlayer(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MATCH_PLAYER;
	DataValue = (int)*m_pNumOfLoginCompleteUser;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendMatchmakingEnterRoomSuccessTPS(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MATCH_PLAYER;
	DataValue = (int)*m_pNumOfLoginCompleteUser;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendCPUTotal(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_ProcessorUsage, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_SERVER_CPU_TOTAL;
	DataValue = (int)CounterValue.longValue;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendAvailableMemroy(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_AvailableMemory, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_SERVER_AVAILABLE_MEMORY;
	DataValue = (int)(CounterValue.longValue);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendNetworkRecv(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_NetworkRecv, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_SERVER_NETWORK_RECV;
	DataValue = (int)CounterValue.longValue / dfDIVIDE_KILOBYTE;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendNetworkSend(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_NetworkSend, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_SERVER_NETWORK_SEND;
	DataValue = (int)CounterValue.longValue / dfDIVIDE_KILOBYTE;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMatchmakingMonitoringClient::SendNonpagedMemory(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_NonPagedMemory, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_SERVER_NONPAGED_MEMORY;
	DataValue = (int)CounterValue.longValue / dfDIVIDE_MEGABYTE;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

bool CMatchmakingMonitoringClient::MatchmakingMonitoringClientOptionParsing(const WCHAR *szOptionFileName)
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, szOptionFileName, L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	if (!parser.GetValue_Int(pBuff, L"MONITORING", L"SERVER_NO", &m_iServerNo))
		return false;

	return true;
}
