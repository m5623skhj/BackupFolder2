#include "PreCompile.h"
#include "ChattingLanClient.h"
#include "LanServerSerializeBuf.h"
#include "CommonProtocol.h"
#include "ChatServer.h"
#include "Log.h"

CChattingLanClient::CChattingLanClient()
{

}

CChattingLanClient::~CChattingLanClient()
{

}

bool CChattingLanClient::ChattingLanClientStart(CChatServer* pChattingServer, const WCHAR *szOptionFileName, CTLSMemoryPool<st_SessionKey> *pSessionKeyMemoryPool)
{
	m_pChattingServer = pChattingServer;
	m_pSessionKeyMemoryPool = pSessionKeyMemoryPool;
	if (!Start(szOptionFileName))
		return false;

	return true;
}

void CChattingLanClient::Stop()
{

}

void CChattingLanClient::OnConnectionComplete()
{
	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();
	WORD Type = en_PACKET_SS_LOGINSERVER_LOGIN;
	BYTE ServerType = dfSERVER_TYPE_CHAT;
	WCHAR ServerName[32] = L"ChattingServer";

	SendBuf << Type << ServerType;
	SendBuf.WriteBuffer((char*)ServerName, dfSERVER_NAME_SIZE);

	CSerializationBuf::AddRefCount(&SendBuf);
	SendPacket(&SendBuf);

	CSerializationBuf::Free(&SendBuf);
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
		st_SessionKey *SessionKey = m_pSessionKeyMemoryPool->Alloc();
		*OutReadBuf >> AccountNo;
		OutReadBuf->ReadBuffer((char*)SessionKey, dfSESSIONKEY_SIZE);
		*OutReadBuf >> SessionKeyID;

		m_pChattingServer->LoginPacketRecvedFromLoginServer(AccountNo, SessionKey);

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
		_LOG(LOG_LEVEL::LOG_DEBUG, L"ERR ", L"%d\n%d\n", OutError->GetLastErr, OutError->ServerErr, OutError->Line);
		printf_s("==============================================================\n");
	}
}
