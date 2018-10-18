#include "PreCompile.h"
#include "MonitoringNetServer.h"
#include "Log.h"
#include "NetServerSerializeBuffer.h"
#include "MonitorProtocol.h"
#include "Parse.h"

CMonitoringNetServer::CMonitoringNetServer()
{
	
}

CMonitoringNetServer::~CMonitoringNetServer()
{

}

bool CMonitoringNetServer::MonitoringNetServerStart(const WCHAR *szNetServerOptionFileName, const WCHAR *szMonitoringServerOption)
{
	if (!Start(szNetServerOptionFileName))
		return false;
	if (!MonitoringServerOptionParsing(szMonitoringServerOption))
		return false;
	//m_wNowMonitoringClientIndex = 0;
	InitializeCriticalSection(&m_AccountNoArrayCS);

	return true;
}

void CMonitoringNetServer::MonitoringNetServerStop()
{
	Stop();
	DeleteCriticalSection(&m_AccountNoArrayCS);
	//delete[] m_pNetServerLoginInfo;
}

void CMonitoringNetServer::OnClientJoin(UINT64 OutClientID)
{

}

void CMonitoringNetServer::OnClientLeave(UINT64 ClientID)
{
	EnterCriticalSection(&m_AccountNoArrayCS);

	for (int i = 0; i < dfACCOUNT_MAX; ++i)
	{
		if (ClientID == m_uiAccountNoArray[i])
		{
			if (i != m_NumOfLoginUser - 1)
			{
				m_uiAccountNoArray[i] = m_uiAccountNoArray[m_NumOfLoginUser - 1];
			}

			--m_NumOfLoginUser;
			break;
		}
	}

	LeaveCriticalSection(&m_AccountNoArrayCS);
}

bool CMonitoringNetServer::OnConnectionRequest(const WCHAR *IP)
{
	return true;
}

void CMonitoringNetServer::OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf)
{
	BYTE Status;
	WORD Type;
	*OutReadBuf >> Type;
	CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();

	if (Type == en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN)
	{
		Type = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;
		if (OutReadBuf->GetUseSize() != 32)
		{
			Status = 0;
			SendBuf << Type << Status;

			CNetServerSerializationBuf::AddRefCount(&SendBuf);
			SendPacketAndDisConnect(ReceivedSessionID, &SendBuf);
			CNetServerSerializationBuf::Free(&SendBuf);
			return;
		}
		
		char RecvLoginSessionKey[32];
		OutReadBuf->ReadBuffer(RecvLoginSessionKey, sizeof(RecvLoginSessionKey));
		if (memcmp(m_LoginSessionKey, RecvLoginSessionKey, sizeof(RecvLoginSessionKey)) != 0)
		{
			Status = 0;
			SendBuf << Type << Status;

			CNetServerSerializationBuf::AddRefCount(&SendBuf);
			SendPacketAndDisConnect(ReceivedSessionID, &SendBuf);
			CNetServerSerializationBuf::Free(&SendBuf);
			return;
		}

		Status = 1;
		SendBuf << Type << Status;

		EnterCriticalSection(&m_AccountNoArrayCS);
		
		m_uiAccountNoArray[m_NumOfLoginUser] = ReceivedSessionID;
		++m_NumOfLoginUser;
		
		LeaveCriticalSection(&m_AccountNoArrayCS);

		CNetServerSerializationBuf::AddRefCount(&SendBuf);
		SendPacket(ReceivedSessionID, &SendBuf);
		CNetServerSerializationBuf::Free(&SendBuf);

		//st_ServerLoginInfo NewClient;
		//NewClient.bIsLoginServer = TRUE;
		//NewClient.uiSessionID = ReceivedSessionID;
		//m_LoginInfoMap.insert()

		//m_NetServerLoginInfo.bIsLoginServer = TRUE;
		//m_NetServerLoginInfo.uiSessionID = ReceivedSessionID;

		//m_pNetServerLoginInfo[m_wNowMonitoringClientIndex].uiSessionID = ReceivedSessionID;
		//m_pNetServerLoginInfo[m_wNowMonitoringClientIndex].bIsLoginServer = TRUE;
		//++m_wNowMonitoringClientIndex;

		return;
	}
	else
	{
		CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();
		Status = 0;
		Type = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;
		SendBuf << Type << Status;

		CNetServerSerializationBuf::AddRefCount(&SendBuf);
		SendPacketAndDisConnect(ReceivedSessionID, &SendBuf);
		CNetServerSerializationBuf::Free(&SendBuf);
		return;
	}
}

void CMonitoringNetServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CMonitoringNetServer::OnWorkerThreadBegin()
{

}

void CMonitoringNetServer::OnWorkerThreadEnd()
{

}

void CMonitoringNetServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

void CMonitoringNetServer::SendMonitoringInfoToNetClient(st_MonitorToolUpdateData &SendToolData)
{
	CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();
	WORD wType = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;

	SendBuf << wType << SendToolData.byServerNo << SendToolData.byDataType 
		<< SendToolData.iDataValue << SendToolData.iTimeStamp;

	//WORD NumOfClient = m_wNowMonitoringClientIndex;
	//for (int i = 0; i < NumOfClient; ++i)
	//{
	//	CNetServerSerializationBuf::AddRefCount(&SendBuf);
	//	SendPacket(m_pNetServerLoginInfo[i].uiSessionID, &SendBuf);
	//	CNetServerSerializationBuf::Free(&SendBuf);
	//}

	EnterCriticalSection(&m_AccountNoArrayCS);

	for (int i = 0; i < m_NumOfLoginUser; ++i)
	{
		CNetServerSerializationBuf::AddRefCount(&SendBuf);
		SendPacket(/*m_NetServerLoginInfo.uiSessionID*/m_uiAccountNoArray[i], &SendBuf);
	}

	LeaveCriticalSection(&m_AccountNoArrayCS);

	CNetServerSerializationBuf::Free(&SendBuf);
}

bool CMonitoringNetServer::MonitoringServerOptionParsing(const WCHAR *szOptionFileName)
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

	WCHAR LoginSessionKey[32];

	if (!parser.GetValue_String(pBuff, L"OPTION", L"SessionKey", LoginSessionKey))
		return false;
	//if (!parser.GetValue_Short(pBuff, L"OPTION", L"NumOfServer", (short*)&m_wNumOfMaxMonitoringClient))
	//	return false;
	//m_pNetServerLoginInfo = new st_ServerLoginInfo[m_wNumOfMaxMonitoringClient];

	int Length = WideCharToMultiByte(CP_UTF8, 0, LoginSessionKey, lstrlenW(LoginSessionKey), NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, LoginSessionKey, Length, m_LoginSessionKey, Length, NULL, NULL);

	return true;
}