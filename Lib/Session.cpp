#include "PreCompile.h"
#include "Session.h"
#include "NetServerSerializeBuffer.h"

CSession::CSession() : m_bAuthToGame(false), m_bLogOutFlag(false),
m_byNowMode(en_SESSION_MODE::MODE_NONE), m_bSendAndDisConnect(false)/*, m_pUserNetworkInfo(NULL)*/
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

void CSession::SendPacketAndDisConnect(CNetServerSerializationBuf *pSendPacket)
{
	if (!pSendPacket->m_bIsEncoded)
	{
		pSendPacket->m_iWriteLast = pSendPacket->m_iWrite;
		pSendPacket->m_iWrite = 0;
		pSendPacket->m_iRead = 0;
		pSendPacket->Encode();
	}

	m_bSendAndDisConnect = true;
	m_SendIOData.SendQ.Enqueue(pSendPacket);
}

void CSession::DisConnect()
{
	shutdown(m_UserNetworkInfo.AcceptedSock, SD_BOTH);
	//m_bIOCancel = true;
	//CancelIoEx((HANDLE)m_UserNetworkInfo.AcceptedSock, NULL);
	//closesocket(m_UserNetworkInfo.AcceptedSock);
}

void CSession::SessionInit()
{
	m_bAuthToGame = false;
	m_bLogOutFlag = false;
	m_bSendAndDisConnect = false;
	m_byNowMode = en_SESSION_MODE::MODE_NONE;
	//m_pUserNetworkInfo = NULL;
	//m_Socket = INVALID_SOCKET;

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