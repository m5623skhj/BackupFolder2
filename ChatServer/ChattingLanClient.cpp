#include "PreCompile.h"
#include "ChattingLanClient.h"
#include "LanServerSerializeBuf.h"
#include "ChatServer.h"
#include "Log.h"
#include "CommonProtocol.h"

CChattingLanClient::CChattingLanClient()
{
}

CChattingLanClient::~CChattingLanClient()
{
}

bool CChattingLanClient::ChattingLanClientStart(CChatServer* pChattingServer, const WCHAR *szOptionFileName, CTLSMemoryPool<st_SessionKey> *pSessionKeyMemoryPool, UINT64 SessionKeyMapAddr)
{
	m_pChattingServer = pChattingServer;
	m_pSessionKeyMemoryPool = pSessionKeyMemoryPool;
	if (!Start(szOptionFileName))
		return false;

	InitializeCriticalSection(&m_SessionKeyMapCS);
	m_pSessionKeyMap = (std::unordered_map<UINT64, st_SessionKey*>*)SessionKeyMapAddr;

	return true;
}

void CChattingLanClient::ChattingLanClientStop()
{
	Stop();
	DeleteCriticalSection(&m_SessionKeyMapCS);
}

void CChattingLanClient::OnConnectionComplete()
{
	CSerializationBuf *SendBuf = CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_LOGINSERVER_LOGIN;
	BYTE ServerType = dfSERVER_TYPE_CHAT;
	WCHAR ServerName[32] = L"ChattingServer";

	*SendBuf << Type << ServerType;
	SendBuf->WriteBuffer((char*)ServerName, dfSERVER_NAME_SIZE);

	CSerializationBuf::AddRefCount(SendBuf);
	SendPacket(SendBuf);
	CSerializationBuf::Free(SendBuf);
}

void CChattingLanClient::OnRecv(CSerializationBuf *OutReadBuf)
{
	WORD Type;
	*OutReadBuf >> Type;

	InterlockedIncrement((UINT*)&NumOfRecvPacket);

	if (Type == en_PACKET_SS_REQ_NEW_CLIENT_LOGIN)
	{
		// ...

		INT64 AccountNo, SessionKeyID;
		st_SessionKey *pSessionKey = m_pSessionKeyMemoryPool->Alloc();
		*OutReadBuf >> AccountNo;
		OutReadBuf->ReadBuffer((char*)pSessionKey, dfSESSIONKEY_SIZE);
		*OutReadBuf >> SessionKeyID;
		pSessionKey->SessionKeyLifeTime = dfHEARTBEAT_TIMEMAX;

		//if(!)
			//++SessionKeyID;
		//m_pChattingServer->LoginPacketRecvedFromLoginServer(AccountNo, SessionKey);

		EnterCriticalSection(&m_SessionKeyMapCS);

		if (m_pSessionKeyMap->insert({ AccountNo, pSessionKey }).second == false)
		{
			st_SessionKey **pDuplicateAccountNoSessionKey = &m_pSessionKeyMap->find(AccountNo)->second;
			m_pSessionKeyMemoryPool->Free(*pDuplicateAccountNoSessionKey);

			//if (memcmp(*pDuplicateAccountNoSessionKey, pSessionKey, dfSESSIONKEY_SIZE) == 0)
			//	m_pSessionKeyMemoryPool->Free(*pDuplicateAccountNoSessionKey);

			*pDuplicateAccountNoSessionKey = pSessionKey;
		}
			
		LeaveCriticalSection(&m_SessionKeyMapCS);

		CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
		Type = en_PACKET_SS_RES_NEW_CLIENT_LOGIN;

		SendBuf << Type << AccountNo << SessionKeyID;

		CSerializationBuf::AddRefCount(&SendBuf);
		SendPacket(&SendBuf);
		CSerializationBuf::Free(&SendBuf);
	}
	else
	{
		g_Dump.Crash();
	}
}

void CChattingLanClient::OnSend()
{

}

void CChattingLanClient::OnWorkerThreadBegin()
{

}

void CChattingLanClient::OnWorkerThreadEnd()
{

}

void CChattingLanClient::OnError(st_Error *OutError)
{
	if (OutError->GetLastErr != 10054)
	{
		_LOG(LOG_LEVEL::LOG_DEBUG, L"LANCLIENT_ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}

int CChattingLanClient::GetUsingLanServerBufCount()
{
	return CSerializationBuf::GetUsingSerializeBufNodeCount();
}