#include "PreCompile.h"
#include "ChatMonitoringLanClient.h"
#include "LanServerSerializeBuf.h"
#include "NetServerSerializeBuffer.h"
#include "Log.h"
#include "MonitorProtocol.h"

CChatMonitoringLanClient::CChatMonitoringLanClient()
{
	PdhOpenQuery(NULL, NULL, &m_PdhQuery);

	PdhAddCounter(m_PdhQuery, L"\\Process(NetServer)\\Working Set", NULL, &m_WorkingSet);

	// 매치 메이킹 이전용 임시 쿼리 등록
	PdhAddCounter(m_PdhQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &m_ProcessorUsage);
	PdhAddCounter(m_PdhQuery, L"\\Memory\\Available MBytes", NULL, &m_AvailableMemory);
	PdhAddCounter(m_PdhQuery, L"\\Network Interface(*)\\Bytes Received/sec", NULL, &m_NetworkRecv);
	PdhAddCounter(m_PdhQuery, L"\\Network Interface(*)\\Bytes Sent/sec", NULL, &m_NetworkSend);
	PdhAddCounter(m_PdhQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_NonPagedMemory);

}

CChatMonitoringLanClient::~CChatMonitoringLanClient()
{
}

bool CChatMonitoringLanClient::MonitoringLanClientStart(const WCHAR *szOptionFileName, UINT *pNumOfSessionAll, UINT *pNumOfLoginCompleteUser)
{
	Start(szOptionFileName);

	m_pNumOfSessionAll = pNumOfSessionAll;
	m_pNumOfLoginCompleteUser = pNumOfLoginCompleteUser;

	return true;
}

void CChatMonitoringLanClient::MonitoringLanClientStop()
{
	SetEvent(m_hMonitoringThreadHandle);
	Stop();
}

void CChatMonitoringLanClient::OnConnectionComplete()
{
	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_MONITOR_LOGIN;
	int ServerType = dfMONITOR_SERVER_TYPE_CHAT;

	*SendBuf << Type << ServerType;

	CSerializationBuf::AddRefCount(SendBuf);
	SendPacket(SendBuf);
	CSerializationBuf::Free(SendBuf);

	m_hMonitoringThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitoringInfoSendThread, this, 0, 0);
}

void CChatMonitoringLanClient::OnRecv(CSerializationBuf *OutReadBuf)
{

}

void CChatMonitoringLanClient::OnSend()
{

}

void CChatMonitoringLanClient::OnWorkerThreadBegin()
{

}

void CChatMonitoringLanClient::OnWorkerThreadEnd()
{

}

void CChatMonitoringLanClient::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

UINT __stdcall CChatMonitoringLanClient::MonitoringInfoSendThread(LPVOID MonitoringClient)
{
	return ((CChatMonitoringLanClient*)MonitoringClient)->MonitoringInfoSender();
}

UINT __stdcall CChatMonitoringLanClient::MonitoringInfoSender()
{
	int TimeStamp;
	
	while (1)
	{
		if (WaitForSingleObject(m_hMonitoringThreadHandle, 1000) == WAIT_TIMEOUT)
		{
			TimeStamp = (int)time(NULL);
			PdhCollectQueryData(m_PdhQuery);

			SendChatServerOn(TimeStamp);
			SendChatCPU(TimeStamp);
			SendChatMemoryCommit(TimeStamp);
			SendChatPacketPool(TimeStamp);
			SendChatSession(TimeStamp);
			SendChatPlayer(TimeStamp);
			//SendChatRoom(TimeStamp);

			// 매치 메이킹 이전용
			SendServerCPUTotal(TimeStamp);
			SendServerAvailableMemroy(TimeStamp);
			SendServerNetworkRecv(TimeStamp);
			SendServerNetworkSend(TimeStamp);
			SendServerNonpagedMemory(TimeStamp);
		}
		else
			break;
	}

	CloseHandle(m_hMonitoringThreadHandle);
	return 0;
}

void CChatMonitoringLanClient::SendChatServerOn(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_CHAT_SERVER_ON;
	DataValue = 1;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CChatMonitoringLanClient::SendChatCPU(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	m_CPUObserver.UpdateCPUTime();
	DataType = dfMONITOR_DATA_TYPE_CHAT_CPU;
	DataValue = (int)m_CPUObserver.ProcessTotal();

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CChatMonitoringLanClient::SendChatMemoryCommit(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_WorkingSet, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_CHAT_MEMORY_COMMIT;
	DataValue = (int)(CounterValue.longValue / dfDIVIDE_MEGABYTE);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CChatMonitoringLanClient::SendChatPacketPool(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	UINT NumOfUsePacket = CSerializationBuf::GetUsingSerializeBufNodeCount() + CNetServerSerializationBuf::GetUsingSerializeBufNodeCount();
	DataType = dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL;
	DataValue = NumOfUsePacket;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CChatMonitoringLanClient::SendChatSession(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_CHAT_SESSION;
	DataValue = (int)*m_pNumOfSessionAll;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CChatMonitoringLanClient::SendChatPlayer(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_CHAT_PLAYER;
	DataValue = (int)*m_pNumOfLoginCompleteUser;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CChatMonitoringLanClient::SendChatRoom(int TimeStamp)
{

}

void CChatMonitoringLanClient::SendServerCPUTotal(int TimeStamp)
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

void CChatMonitoringLanClient::SendServerAvailableMemroy(int TimeStamp)
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

void CChatMonitoringLanClient::SendServerNetworkRecv(int TimeStamp)
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

void CChatMonitoringLanClient::SendServerNetworkSend(int TimeStamp)
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

void CChatMonitoringLanClient::SendServerNonpagedMemory(int TimeStamp)
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