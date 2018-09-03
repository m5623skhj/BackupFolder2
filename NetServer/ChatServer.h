#pragma once
#include "NetServer.h"
#include <unordered_map>
#include <string>

#define dfMAX_SECTOR_X				50
#define dfMAX_SECTOR_Y				50

#define dfID_NICKNAME_LENGTH		40

#define dfWAIT_UPDATETHREAD_TIMEOUT 10

#define dfSTOP_SERVER				0
#define dfRECVED_PACKET				1

// �ܺ� ����ڰ� ������ ����ϰ� �ϰ� ������
// Ư�� �� ���� ���Ŀ��� ��� �����ϰ� �Ѵٴ� ������ �ʿ���
// ���� ���� �� ��
enum CHATSERVER_ERR
{
	NOT_IN_USER_MAP_ERR = 1001,
	INCORREC_PACKET_ERR,
};

class Log;
class CNetServerSerializationBuf;

class CChatServer : public CNetServer
{
private:
	struct st_MESSAGE
	{
		UINT64										uiSessionID;
		CNetServerSerializationBuf					*Packet;
	};

	struct st_USER
	{
		WORD										SectorX;
		WORD										SectorY; 
		UINT64										uiSessionID;
		UINT64										uiAccountNO;
		WCHAR										szID[20];
		WCHAR										szNickName[20];
		char										SessionKey[64];
	};

	//BOOL											m_bIsUpdate;

	UINT64											m_uiAccountNo;
	CTLSMemoryPool<st_USER>							*m_pMemoryPool;
	std::unordered_map<UINT64, st_USER*>			m_UserSessionMap;
	std::unordered_map<UINT64, st_USER*>			m_UserSectorMap[dfMAX_SECTOR_Y][dfMAX_SECTOR_X];

	CTLSMemoryPool<st_MESSAGE>						*m_pMessageMemoryPool;
	CLockFreeQueue<st_MESSAGE*>						*m_pMessageQueue;

	HANDLE											m_hUpdateThreadHandle;
	HANDLE											m_hUpdateEvent;
	HANDLE											m_hExitEvent;
	//HANDLE											m_hUpdateIOCP;

	void BroadcastSectorAroundAll(WORD CharacterSectorX, WORD CharacterSectorY, CNetServerSerializationBuf *SendBuf);

	static UINT __stdcall UpdateThread(LPVOID pLanServer);
	UINT Updater();

	// Accept �� ����ó�� �Ϸ� �� ȣ��
	virtual void OnClientJoin(UINT64 JoinClientID/* Client ���� / ClientID / ��Ÿ��� */);
	// Disconnect �� ȣ��
	virtual void OnClientLeave(UINT64 LeaveClientID);
	// Accept ���� IP ���ܵ��� ���� �뵵
	virtual bool OnConnectionRequest(const WCHAR *IP);

	// ��Ŷ ���� �Ϸ� ��
	virtual void OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf);
	// ��Ŷ �۽� �Ϸ� ��
	virtual void OnSend();

	// ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	virtual void OnWorkerThreadBegin();
	// ��Ŀ������ 1���� ���� ��
	virtual void OnWorkerThreadEnd();

	// LanServer ���� ���� �߻��� ȣ��
	// ���ϰ� �ֿܼ� �������� GetLastError() ��ȯ���� ���� ������ Error �� ����ϸ�
	// 1000 �� ���ķ� ��ӹ��� Ŭ�������� ����� �����Ͽ� ��� �� �� ����
	// ��, GetLastError() �� ��ȯ���� 10054 ���� ���� �̸� �������� ����
	virtual void OnError(st_Error *OutError);

	void PacketProc_HeartBeat(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket);
	bool PacketProc_SectorMove(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID);
	bool PacketProc_ChatMessage(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID);
	//bool PacketProc_Login(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID);

public:

	CChatServer(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient);
	virtual ~CChatServer();
};