#pragma once
#include "LanClient.h"

#define dfSESSIONKEY_SIZE			64

class CChatServer;

struct st_SessionKey;

class CChattingLanClient : CLanClient
{
public:
	CChattingLanClient();
	virtual ~CChattingLanClient();

	bool ChattingLanClientStart(CChatServer* pChattingServer, const WCHAR *szOptionFileName, CTLSMemoryPool<st_SessionKey> *pSessionKeyMemoryPool);
	void Stop();

	int NumOfRecvPacket = 0;

private:
	// ������ Connect �� �Ϸ� �� ��
	virtual void OnConnectionComplete();

	// ��Ŷ ���� �Ϸ� ��
	virtual void OnRecv(CSerializationBuf *OutReadBuf);
	// ��Ŷ �۽� �Ϸ� ��
	virtual void OnSend();

	// ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	virtual void OnWorkerThreadBegin();
	// ��Ŀ������ 1���� ���� ��
	virtual void OnWorkerThreadEnd();
	// ����� ���� ó�� �Լ�
	virtual void OnError(st_Error *OutError);

private:

	struct st_ServerInfo
	{
		BYTE	ServerType;			// dfSERVER_TYPE_GAME / dfSERVER_TYPE_CHAT
		UINT64  ClientSessionID;
		WCHAR	ServerName[32];		// �ش� ������ �̸�. 
	};

	BYTE										m_byServerIndex;

	UINT64										m_iIdentificationNumber;
	LPCRITICAL_SECTION							m_pAccountMapCriticalSection;
	CTLSMemoryPool<st_SessionKey>				*m_pSessionKeyMemoryPool;

	CChatServer									*m_pChattingServer;
};