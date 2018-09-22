#include "PreCompile.h"
#include "LoginServer.h"
#include "LoginLanServer.h"
#include "CommonProtocol.h"
#include "NetServerSerializeBuffer.h"
#include "Log.h"

CLoginServer::CLoginServer()
{
	InitializeCriticalSection(&m_UserSessionIDMapCriticalSection);
}

CLoginServer::~CLoginServer()
{
	DeleteCriticalSection(&m_UserSessionIDMapCriticalSection);
}

bool CLoginServer::LoginServerStart(const WCHAR *szLoginServerFileName, const WCHAR *szLoginLanServerFileName, const WCHAR *szLogInServerOptionFile)
{
	NumOfLoginWaitingUser = 0;
	NumOfLoginCompleteUser = 0;

	m_pLoginLanServer = new CLoginLanServer();
	m_pUserInfoMemeoryPool = new CTLSMemoryPool<st_LoginUserInfo>(3, false);

	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"Start\n");

	SetLogLevel(LOG_LEVEL::LOG_DEBUG);

	if (!Start(szLoginServerFileName))
		return false;
	if (!m_pLoginLanServer->LoginLanServerStart(this, szLoginLanServerFileName))
		return false;
	if (!LoginServerOptionParsing(szLogInServerOptionFile))
		return false;

	return true;
}

void CLoginServer::LoginServerStop()
{
	//m_pDBConnector->Disconnect();
	delete m_pLoginLanServer;

	m_pLoginLanServer->Stop();

	_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR", L"End\n%d");
	WritingProfile();
}

void CLoginServer::OnClientJoin(UINT64 OutClientID)
{
	InterlockedIncrement((UINT*)&NumOfLoginWaitingUser);
}

void CLoginServer::OnClientLeave(UINT64 ClientID)
{
	// UserSessionMap 임계영역 시작
	EnterCriticalSection(&m_UserSessionIDMapCriticalSection); 
	
	st_LoginUserInfo *pLeaveUser = m_LoginUserInfoMap.find(ClientID)->second;
	if (pLeaveUser != m_LoginUserInfoMap.end()->second)
	{
		m_LoginUserInfoMap.erase(ClientID);
		m_pUserInfoMemeoryPool->Free(pLeaveUser);
	}

	// UserSessionMap 임계영역 끝
	LeaveCriticalSection(&m_UserSessionIDMapCriticalSection);
	

	InterlockedDecrement((UINT*)&NumOfLoginWaitingUser);
}

bool CLoginServer::OnConnectionRequest(const WCHAR *IP)
{
	return true;
}

void CLoginServer::OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf)
{
	WORD Type;

	*OutReadBuf >> Type;
	if (Type == en_PACKET_CS_LOGIN_REQ_LOGIN)
	{
		INT64 AccountNo;
		*OutReadBuf >> AccountNo;

		st_LoginUserInfo &LoginInfo = *m_pUserInfoMemeoryPool->Alloc();

		CDBConnector *pDBConnector = CDBConnector::GetDBConnector();
		pDBConnector->SendQuery(L"SELECT userid, userpass, usernick FROM accountdb.account WHERE accountno = %d", AccountNo);
	/*	pDBConnector->SendQuery(L"SELECT status, userid, usernick FROM accountdb.v_account WHERE accountno = %d", AccountNo);
		pDBConnector->SendQuery(L"SELECT status FROM accountdb.status WHERE accountno = %d", AccountNo);*/

		MYSQL_ROW row = pDBConnector->FetchRow();
		if (row != NULL)
		{
			int iStatus = atoi(row[0]);
			//if (iStatus != dfLOGIN_STATUS_NONE)
			//{
			//	DisConnect(ReceivedSessionID);
			//}
			LoginInfo.uiSessionID = ReceivedSessionID;
			UTF8ToUTF16(row[1], LoginInfo.szID, sizeof(LoginInfo.szID));
			UTF8ToUTF16(row[2], LoginInfo.szNick, sizeof(LoginInfo.szNick));
		}
		else
		{
			DisConnect(ReceivedSessionID);
			return;
		}
		pDBConnector->FreeResult();

		//LoginInfo.uiSessionID = ReceivedSessionID;
		////wmemcpy(LoginInfo.szID, L"asdf", sizeof(LoginInfo.szID));
		////wmemcpy(LoginInfo.szNick, L"asdf", sizeof(LoginInfo.szNick));

		//m_pDBConnector->SendQuery(L"SELECT userid, userpass, usernick FROM accountdb.account WHERE accountno = %d", AccountNo);
		//st_UserInfo &JoinUser = *m_pUserMemoryPool->Alloc();
		//if (m_pDBConnector->FetchRow() != NULL)
		//{
		//	MYSQL_ROW row = m_pDBConnector->FetchRow();
		//	//memcpy(JoinUser.szUserId, row[0], dfUSER_INFO_SIZE);
		//	memcpy(JoinUser.szUserId, row[0], sizeof(JoinUser.szUserId));
		//	memcpy(JoinUser.szUserNick, row[0], sizeof(JoinUser.szUserNick));
		//}
		//else
		//{
		//	DisConnect(ReceivedSessionID);
		//	return;
		//}
		//m_UserInfoMap.insert({ AccountNo, &JoinUser });

		// UserSessionMap 임계영역 시작
		EnterCriticalSection(&m_UserSessionIDMapCriticalSection);

		m_LoginUserInfoMap.insert({ AccountNo, &LoginInfo });

		// UserSessionMap 임계영역 끝
		LeaveCriticalSection(&m_UserSessionIDMapCriticalSection);

		m_pLoginLanServer->SendLoginPacketToLanClientAll(AccountNo, OutReadBuf->GetReadBufferPtr());
	}
	else
	{
		DisConnect(ReceivedSessionID);
	}
}

void CLoginServer::LoginComplete(UINT64 AccountNo, BYTE Status)
{
	//st_UserInfo *LoginCompleteUser = m_UserInfoMap.find(AccountNo)->second;
	//if (LoginCompleteUser == NULL)
	//{
	//	st_Error Err;
	//	Err.GetLastErr = 0;
	//	Err.ServerErr = SERVER_ERR::NOT_IN_LOGIN_MAP_ERR;
	//	Err.Line = __LINE__;
	//	OnError(&Err);
	//	return;
	//}
	//st_UserInfo *LoginUserInfo = m_UserInfoMap.find(AccountNo)->second;
	//// 추가적 처리가 필요해 보임
	//if (LoginUserInfo == m_UserInfoMap.end()->second)
	//{
	//	return;
	//}

	CNetServerSerializationBuf &SendBuf = *CNetServerSerializationBuf::Alloc();

	// UserSessionMap 임계영역 시작
	EnterCriticalSection(&m_UserSessionIDMapCriticalSection);

	st_LoginUserInfo &LoginCompleteUser = *m_LoginUserInfoMap.find(AccountNo)->second;
	if (&LoginCompleteUser == m_LoginUserInfoMap.end()->second)
	{
		// 따로 처리 할 것이 있나?
		return;
	}
	m_LoginUserInfoMap.erase(AccountNo);
	m_pUserInfoMemeoryPool->Free(&LoginCompleteUser);

	// UserSessionMap 임계영역 끝
	LeaveCriticalSection(&m_UserSessionIDMapCriticalSection);

	WORD Type = en_PACKET_CS_LOGIN_RES_LOGIN;

	SendBuf << Type << AccountNo << Status;
	SendBuf.WriteBuffer((char*)LoginCompleteUser.szID, sizeof(LoginCompleteUser.szID));
	SendBuf.WriteBuffer((char*)LoginCompleteUser.szNick, sizeof(LoginCompleteUser.szNick));
	SendBuf.WriteBuffer((char*)m_GameServerIP, dfWCHAR_IP_SIZE);
	SendBuf << m_usGameServerPort;
	SendBuf.WriteBuffer((char*)m_ChatServerIP, dfWCHAR_IP_SIZE);
	SendBuf << m_usChatServerPort;

	CNetServerSerializationBuf::AddRefCount(&SendBuf);
	SendPacketAndDisConnect(LoginCompleteUser.uiSessionID, &SendBuf);

	CNetServerSerializationBuf::Free(&SendBuf);
}

bool CLoginServer::ChattingServerJoined()
{
	return m_pLoginLanServer->ChattingServerJoined(); 
}

void CLoginServer::OnSend(UINT64 ClientID, int sendsize)
{
	
}

void CLoginServer::OnWorkerThreadBegin()
{

}

void CLoginServer::OnWorkerThreadEnd()
{

}

void CLoginServer::OnError(st_Error *OutError)
{

}

bool CLoginServer::LoginServerOptionParsing(const WCHAR *szLogInServerOptionFile)
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, szLogInServerOptionFile, L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	if (!parser.GetValue_String(pBuff, L"SERVER", L"GAMESERVER_IP", m_GameServerIP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"SERVER", L"GAMESERVER_PORT", (short*)&m_usGameServerPort))
		return false;
	if (!parser.GetValue_String(pBuff, L"SERVER", L"CHATSERVER_IP", m_ChatServerIP))
		return false;
	if (!parser.GetValue_Short(pBuff, L"SERVER", L"CHATSERVER_PORT", (short*)&m_usChatServerPort))
		return false;

	return true;
}

int CLoginServer::GetLanServerSend()
{
	return m_pLoginLanServer->NumOfSendLanClient; 
}

int CLoginServer::GetLanServerRecv()
{
	return m_pLoginLanServer->NumOfRecvLanClient; 
}

int CLoginServer::GetUsingNetSerializeBufCount() 
{
	return CNetServerSerializationBuf::GetUsingSerializeBufNodeCount(); 
}

int CLoginServer::GetUsingNetUserInfoNodeCount()
{
	return m_pUserInfoMemeoryPool->GetUseNodeCount(); 
}

int CLoginServer::GetUsingLanServerSessionNodeCount()
{
	return m_pLoginLanServer->GetUsingSessionNodeCount(); 
}

int CLoginServer::GetUsingLanSerializeBufCount() 
{
	return m_pLoginLanServer->GetUsingLanSerializeBufCount(); 
}
