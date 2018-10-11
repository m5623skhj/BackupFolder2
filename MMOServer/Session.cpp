#include "PreCompile.h"
#include "Session.h"
#include "NetServerSerializeBuffer.h"

CSession::CSession() : m_bAuthToGame(false), m_bLogOutFlag(false),
m_byNowMode(en_SESSION_MODE::MODE_NONE), m_pUserNetworkInfo(NULL)
{

}

CSession::~CSession()
{
	
}

void CSession::SendPacket(CNetServerSerializationBuf *pSendPacket)
{
	if (!pSendPacket->m_bIsEncoded)
	{
		pSendPacket->m_iWriteLast = pSendPacket->m_iWrite;
		pSendPacket->m_iWrite = 0;
		pSendPacket->m_iRead = 0;
		pSendPacket->Encode();
	}

	m_SendIOData.SendQ.Enqueue(pSendPacket);
}

void CSession::DisConnect()
{
	shutdown(m_Socket, SD_BOTH);
}

void CSession::SessionInit()
{
	m_bAuthToGame = false;
	m_bLogOutFlag = false;
	m_byNowMode = en_SESSION_MODE::MODE_NONE;
	m_pUserNetworkInfo = NULL;
	m_Socket = INVALID_SOCKET;

	m_RecvIOData.RecvRingBuffer.InitPointer();
}

bool CSession::ChangeMode_AuthToGame()
{
	bool &AuthToGameFlag = m_bAuthToGame;

	if (AuthToGameFlag == true)
		return false;
	else if (m_byNowMode != en_SESSION_MODE::MODE_AUTH)
		return false;

	AuthToGameFlag = true;
	return true;
}