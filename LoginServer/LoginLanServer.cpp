#include "PreCompile.h"
#include "LoginLanServer.h"
#include "LoginServer.h"
#include "LanServerSerializeBuf.h"
#include "CommonProtocol.h"
#include "Log.h"

CLoginLanServer::CLoginLanServer()
{

}

CLoginLanServer::~CLoginLanServer()
{

}

bool CLoginLanServer::LoginLanServerStart(CLoginServer* pLoginServer, const WCHAR *szOptionFileName)
{
	NumOfSendLanClient = 0;
	NumOfRecvLanClient = 0;

	m_pLoginServer = pLoginServer;
	m_pSessionInfoTLS = new CTLSMemoryPool<st_SessionInfo>(5, false);

	InitializeCriticalSection(&m_AccountMapCriticalSection);
	if (!Start(szOptionFileName))
		return false;

	return true;
}

void CLoginLanServer::Stop()
{
	delete m_pSessionInfoTLS;
}

// 다른 LanClient(다른 서버) 가 해당 서버에 접속했음
void CLoginLanServer::OnClientJoin(UINT64 OutClientID)
{
	
}

void CLoginLanServer::OnClientLeave(UINT64 ClientID)
{

}

bool CLoginLanServer::OnConnectionRequest()
{
	return true;
}

// 다른 LanClient(다른 서버) 가 해당 서버에 패킷을 전송했음
void CLoginLanServer::OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	WORD Type;

	*OutReadBuf >> Type;
	switch (Type)
	{
	case en_PACKET_SS_RES_NEW_CLIENT_LOGIN:
	{
		INT64 ClientAccountNo, SessionKeyID;
		*OutReadBuf >> ClientAccountNo >> SessionKeyID;
		
		// AccountMap 임계영역 시작
		EnterCriticalSection(&m_AccountMapCriticalSection);

		st_SessionInfo &SessionInfo = *m_AccountMap.find(ClientAccountNo)->second;
		UINT64 &MapSessionIdentifier = SessionInfo.SessionKeyIdentifier;
		MapSessionIdentifier += ((UINT64)1 << dfACQUIRE_RESPONSE_SHIFT);
		
		if ((MapSessionIdentifier & dfAND_PARAMETER_PART) != SessionKeyID)
			MapSessionIdentifier += ((UINT64)1 << dfIDENTIFIER_ERR_SHIFT);

		if (MapSessionIdentifier >> dfACQUIRE_RESPONSE_SHIFT == dfNUM_OF_SERVER)
		{
			m_AccountMap.erase(ClientAccountNo);
			if (MapSessionIdentifier >> dfIDENTIFIER_ERR_SHIFT == dfNUM_OF_SERVER << 4)
				m_pLoginServer->LoginComplete(ClientAccountNo, dfGAME_LOGIN_OK);
			else
				m_pLoginServer->LoginComplete(ClientAccountNo, dfGAME_LOGIN_FAIL);

			m_pSessionInfoTLS->Free(&SessionInfo);
			InterlockedIncrement((UINT*)&NumOfRecvLanClient);
		}

		// AccountMap 임계영역 끝
		LeaveCriticalSection(&m_AccountMapCriticalSection);
	}
	break;
	case en_PACKET_SS_LOGINSERVER_LOGIN:
	{
		BYTE ServerType;
		*OutReadBuf >> ServerType;
		if (ServerType == dfSERVER_TYPE_CHAT)
		{
			m_ChattingServer.ServerType = ServerType;
			OutReadBuf->ReadBuffer((char*)m_ChattingServer.ServerName, sizeof(m_ChattingServer.ServerName));
			m_ChattingServer.ClientSessionID = ReceivedSessionID;
			m_bChattingServerClientJoined = true;
		}
		// else if
		// ...
	}
	break;
	default:
		g_Dump.Crash();
		break;
	}
}

void CLoginLanServer::OnSend(UINT64 ClientID, int sendsize)
{

}

void CLoginLanServer::SendLoginPacketToLanClientAll(INT64 AccountNo, char *SessionKey)
{
	st_SessionInfo *NewClientInfo = m_pSessionInfoTLS->Alloc();;

	// AccountMap 임계영역 시작
	EnterCriticalSection(&m_AccountMapCriticalSection);

	memcpy(NewClientInfo, SessionKey, dfSESSIONKEY_SIZE);
	NewClientInfo->SessionKeyIdentifier = m_iIdentificationNumber;
	++m_iIdentificationNumber;
	if (m_AccountMap.find(AccountNo) != m_AccountMap.end())
	{
		m_AccountMap.erase(AccountNo);
	}
	m_AccountMap.insert({ AccountNo, NewClientInfo });

	// AccountMap 임계영역 끝
	LeaveCriticalSection(&m_AccountMapCriticalSection);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_REQ_NEW_CLIENT_LOGIN;
	SendBuf << Type << AccountNo;
	SendBuf.WriteBuffer(SessionKey, dfSESSIONKEY_SIZE);
	SendBuf << NewClientInfo->SessionKeyIdentifier;

	InterlockedIncrement((UINT*)&NumOfSendLanClient);

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(m_ChattingServer.ClientSessionID, &SendBuf);
	// SendPacket(OtherSever.ClientSessionID, &SendBuf);
	// ...

	CSerializationBuf::Free(&SendBuf);
}

void CLoginLanServer::OnWorkerThreadBegin()
{

}

void CLoginLanServer::OnWorkerThreadEnd()
{

}

void CLoginLanServer::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

int CLoginLanServer::GetUsingLanSerializeBufCount()
{
	return CSerializationBuf::GetUsingSerializeBufNodeCount(); 
}
