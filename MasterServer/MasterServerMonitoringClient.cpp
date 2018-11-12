#include "PreCompile.h"

#include "MasterServerMonitoringClient.h"
#include "MasterServer.h"

#include "LanServerSerializeBuf.h"
#include "NetServerSerializeBuffer.h"
#include "Log.h"

#include "Protocol/CommonProtocol.h"

CMasterMonitoringClient::CMasterMonitoringClient()
{
	// 모니터링을 위한 pdh 정보 얻기
	PdhOpenQuery(NULL, NULL, &m_PdhQuery);

	PdhAddCounter(m_PdhQuery, L"\\Process(MasterServer)\\Private Bytes", NULL, &m_PrivateBytes);
	PdhAddCounter(m_PdhQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &m_ProcessorUsage);
}

CMasterMonitoringClient::~CMasterMonitoringClient()
{

}

bool CMasterMonitoringClient::MasterMonitoringClientStart(const WCHAR *szMatchmakingMonitoringClientOptionFile, CMasterServer *pMasterServer)
{
	if (!Start(szMatchmakingMonitoringClientOptionFile))
		return false;

	m_pMasterServer = pMasterServer;

	m_hMonitoringThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitoringInfoSendThread, this, 0, NULL);
	m_hMonitoringThreadExitHandle = CreateEvent(NULL, TRUE, FALSE, NULL);

	return true;
}

void CMasterMonitoringClient::MasterMonitoringClientStop()
{
	Stop();
	SetEvent(m_hMonitoringThreadExitHandle);

	WaitForSingleObject(m_hMonitoringThreadHandle, INFINITE);

	CloseHandle(m_hMonitoringThreadExitHandle);
	CloseHandle(m_hMonitoringThreadHandle);
}

void CMasterMonitoringClient::OnConnectionComplete()
{
	// 모니터링 서버에 접속을 성공하였다면
	// 모니터링 서버에게 ServerNo 를 보내줌
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_MONITOR_LOGIN;

	SendBuf << Type << m_iServerNo;

	SendPacket(&SendBuf);

	m_hMonitoringThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitoringInfoSendThread, this, 0, 0);
}

void CMasterMonitoringClient::OnRecv(CSerializationBuf *OutReadBuf)
{

}

void CMasterMonitoringClient::OnSend()
{

}

void CMasterMonitoringClient::OnWorkerThreadBegin()
{

}

void CMasterMonitoringClient::OnWorkerThreadEnd()
{

}

void CMasterMonitoringClient::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

UINT __stdcall CMasterMonitoringClient::MonitoringInfoSendThread(LPVOID MonitoringClient)
{
	return ((CMasterMonitoringClient*)MonitoringClient)->MonitoringInfoSender();
}

UINT CMasterMonitoringClient::MonitoringInfoSender()
{
	int TimeStamp;

	while (1)
	{
		// 1 초 마다 일어나서 모니터링 서버에게 현재 모니터링 한 정보들을 보내줌
		if (WaitForSingleObject(m_hMonitoringThreadExitHandle, 1000) == WAIT_TIMEOUT)
		{
			TimeStamp = (int)time(NULL);
			PdhCollectQueryData(m_PdhQuery);

			SendMasterServerOn(TimeStamp);
			SendMasterServerCPU(TimeStamp);
			SendCPUTotal(TimeStamp);
			SendMasterServerMemoryCommit(TimeStamp);
			SendMasterServerPacketPool(TimeStamp);
			SendMasterServerMatchmakingSessionAll(TimeStamp);
			SendMasterServerMatchmakingLoginSession(TimeStamp);
			SendMasterServerStayPlayer(TimeStamp);
			SendMasterServerBattleSessionAll(TimeStamp);
			SendMasterServerBattleLoginSession(TimeStamp);
			SendMasterServerStanbyRoom(TimeStamp);
		}
		// 외부에서 스레드를 종료시킴
		else
			break;
	}

	return 0;
}

void CMasterMonitoringClient::SendMasterServerOn(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_SERVER_ON;
	DataValue = 1;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerCPU(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	m_CPUObserver.UpdateCPUTime();
	DataType = dfMONITOR_DATA_TYPE_MASTER_CPU;
	DataValue = (int)m_CPUObserver.ProcessTotal();

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendCPUTotal(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_ProcessorUsage, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_MASTER_CPU_SERVER;
	DataValue = (int)CounterValue.longValue;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerMemoryCommit(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_PrivateBytes, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_MASTER_MEMORY_COMMIT;
	DataValue = (int)(CounterValue.longValue / dfDIVIDE_MEGABYTE);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerPacketPool(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	UINT NumOfUsePacket = CSerializationBuf::GetUsingSerializeBufNodeCount();
	DataType = dfMONITOR_DATA_TYPE_MASTER_PACKET_POOL;
	DataValue = NumOfUsePacket;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerMatchmakingSessionAll(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_MATCH_CONNECT;
	DataValue = m_pMasterServer->NumOfMatchmakingSessionAll;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerMatchmakingLoginSession(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_MATCH_LOGIN;
	DataValue = m_pMasterServer->NumOfMatchmakingLoginSession;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerStayPlayer(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_STAY_CLIENT;
	DataValue = m_pMasterServer->GetNumOfClientInfo();

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerBattleSessionAll(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_BATTLE_CONNECT;
	DataValue = (int)*m_pMasterServer->pNumOfBattleServerSessionAll;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerBattleLoginSession(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_BATTLE_LOGIN;
	DataValue = (int)*m_pMasterServer->pNumOfBattleServerLoginSession;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMasterMonitoringClient::SendMasterServerStanbyRoom(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_MASTER_BATTLE_STANDBY_ROOM;
	DataValue = m_pMasterServer->CallGetBattleStanbyRoomSize();

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

bool CMasterMonitoringClient::MasterMonitoringClientOptionParsing(const WCHAR *szOptionFileName)
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
