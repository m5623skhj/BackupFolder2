#include "PreCompile.h"
#include "MMOMonitoringLanClient.h"
#include "LanServerSerializeBuf.h"
#include "NetServerSerializeBuffer.h"
#include "Log.h"
#include "MonitorProtocol.h"

CMMOMonitoringLanClient::CMMOMonitoringLanClient()
{
	PdhOpenQuery(NULL, NULL, &m_PdhQuery);

	PdhAddCounter(m_PdhQuery, L"\\Process(MMOServer)\\Private Bytes", NULL, &m_PrivateByte);
}

CMMOMonitoringLanClient::~CMMOMonitoringLanClient()
{
}

bool CMMOMonitoringLanClient::MonitoringLanClientStart(const WCHAR *szOptionFileName, UINT *pNumOfSessionAll,
	UINT *NumOfSessionAuth, UINT *NumOfSessionGame, UINT *AuthFPS, UINT *GameFPS,
	UINT *NumOfRoomWait, UINT *NumOfRoomPlay)
{
	Start(szOptionFileName);

	m_pAuthFPS = AuthFPS;
	m_pGameFPS = GameFPS;
	m_pNumOfSessionAll = pNumOfSessionAll;
	m_pNumOfSessionAuth = NumOfSessionAuth;
	m_pNumOfSessionGame = NumOfSessionGame;
	m_pNumOfRoomWait = NumOfRoomWait;
	m_pNumOfRoomGame = NumOfRoomPlay;

	return true;
}

void CMMOMonitoringLanClient::MonitoringLanClientStop()
{
	SetEvent(m_hMonitoringThreadHandle);
	Stop();
}

void CMMOMonitoringLanClient::OnConnectionComplete()
{
	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_MONITOR_LOGIN;
	int ServerType = dfMONITOR_SERVER_TYPE_GAME;

	*SendBuf << Type << ServerType;

	CSerializationBuf::AddRefCount(SendBuf);
	SendPacket(SendBuf);
	CSerializationBuf::Free(SendBuf);

	m_hMonitoringThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitoringInfoSendThread, this, 0, 0);
}

void CMMOMonitoringLanClient::OnRecv(CSerializationBuf *OutReadBuf)
{

}

void CMMOMonitoringLanClient::OnSend()
{

}

void CMMOMonitoringLanClient::OnWorkerThreadBegin()
{

}

void CMMOMonitoringLanClient::OnWorkerThreadEnd()
{

}

void CMMOMonitoringLanClient::OnError(st_Error *OutError)
{
	g_Dump.Crash();
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

UINT __stdcall CMMOMonitoringLanClient::MonitoringInfoSendThread(LPVOID MonitoringClient)
{
	return ((CMMOMonitoringLanClient*)MonitoringClient)->MonitoringInfoSender();
}

UINT __stdcall CMMOMonitoringLanClient::MonitoringInfoSender()
{
	int TimeStamp;

	while (1)
	{
		if (WaitForSingleObject(m_hMonitoringThreadHandle, 1000) == WAIT_TIMEOUT)
		{
			TimeStamp = (int)time(NULL);
			PdhCollectQueryData(m_PdhQuery);

			SendGameServerOn(TimeStamp);
			SendGameCPU(TimeStamp);
			SendGameMemoryCommit(TimeStamp);
			SendGamePacketPool(TimeStamp);
			SendGameAuthFPS(TimeStamp);
			SendGameGameFPS(TimeStamp);
			SendGameSessionAll(TimeStamp);
			SendGameAuthSession(TimeStamp);
			SendGameGameSession(TimeStamp);
			//SendGameRoomWait(TimeStamp);
			//SendGameRoomPlay(TimeStamp);
		}
		else
			break;
	}

	CloseHandle(m_hMonitoringThreadHandle);
	return 0;
}

void CMMOMonitoringLanClient::SendGameServerOn(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON;
	DataValue = 1;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameCPU(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	m_CPUObserver.UpdateCPUTime();
	DataType = dfMONITOR_DATA_TYPE_BATTLE_CPU;
	DataValue = (int)m_CPUObserver.ProcessTotal();

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameMemoryCommit(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	int DataValue;

	PDH_FMT_COUNTERVALUE CounterValue;
	PDH_STATUS Status = PdhGetFormattedCounterValue(m_PrivateByte, PDH_FMT_LONG, NULL, &CounterValue);
	DataType = dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT;
	DataValue = (int)(CounterValue.longValue / dfDIVIDE_MEGABYTE);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << DataValue << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGamePacketPool(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << CNetServerSerializationBuf::GetUsingSerializeBufNodeCount() << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameAuthFPS(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pAuthFPS << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameGameFPS(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pGameFPS << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameSessionAll(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pNumOfSessionAll << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameAuthSession(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pNumOfSessionAuth << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameGameSession(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pNumOfSessionGame << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

//////////////////////////////////////////////////////////////
// 현재 return NULL 중
// MMOServer 헤더를 참조할 것
//////////////////////////////////////////////////////////////
void CMMOMonitoringLanClient::SendGameRoomWait(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_ROOM_WAIT;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pNumOfRoomWait << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

void CMMOMonitoringLanClient::SendGameRoomPlay(int TimeStamp)
{
	BYTE DataType;
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;

	DataType = dfMONITOR_DATA_TYPE_BATTLE_ROOM_PLAY;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	SendBuf << Type << DataType << *m_pNumOfRoomGame << TimeStamp;

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);
	CSerializationBuf::Free(&SendBuf);
}

