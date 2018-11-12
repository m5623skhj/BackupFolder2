#pragma once
#include "NetServer.h"
#include <unordered_map>
#include <string>

#include "LockFreeQueue.h"

#define dfMAX_SECTOR_X									51
#define dfMAX_SECTOR_Y									51

#define dfID_NICKNAME_LENGTH							40

#define dfWAIT_UPDATETHREAD_TIMEOUT						10

#define dfSTOP_SERVER									0
#define dfRECVED_PACKET									1

#define dfINIT_SECTOR_VALUE								65535
#define dfACCOUNT_INIT_VALUE							0xffffffffffffffff

#define dfPACKET_TYPE_JOIN								90
#define dfPACKET_TYPE_LEAVE								91
#define dfPACKET_TYPE_RECV_FROM_LOGIN_SERVER			92

#define dfPACKETSIZE_SECTOR_MOVE						12
#define dfPACKETSIZE_CHAT_MESSAGE						10
#define dfPACKETSIZE_LOGIN								152

#define dfSESSIONKEY_SIZE								64

#define dfHEARTBEAT_TIMEMAX								8

// 외부 사용자가 에러를 기록하게 하고 싶은데
// 특정 수 범위 이후에만 사용 가능하게 한다는 전제가 필요함
// 차후 수정 할 것
enum CHATSERVER_ERR
{
	NOT_IN_USER_MAP_ERR = 1001,
	NOT_IN_USER_SECTOR_MAP_ERR,
	INCORRECT_SESSION_KEY_ERR,
	SESSION_KEY_IS_NULL,
	NOT_LOGIN_USER_ERR,
	SERIALIZEBUF_SIZE_ERR,
	SECTOR_RANGE_ERR,
	INCORRECT_ACCOUNTNO,
	INCORREC_PACKET_ERR,
	SAME_ACCOUNTNO_ERR,
};

class Log;
class CNetServerSerializationBuf;
class CChattingLanClient;
class CChatMonitoringLanClient;

struct st_SessionKey
{
	char SessionElement[64];
	UINT64 SessionKeyLifeTime;
};

class CChatServer : public CNetServer
{
private:
	struct st_MESSAGE
	{
		WORD										wType;
		UINT64										uiSessionID;
		CNetServerSerializationBuf					*Packet;
	};

	struct st_USER
	{
		BYTE										byNumOfOverWriteSessionKey = 0;
		BOOL										bIsLoginUser;
		WORD										SectorX;
		WORD										SectorY; 
		UINT64										uiBeforeRecvTime;
		UINT64										uiSessionID;
		UINT64										uiAccountNO;
		WCHAR										szID[20];
		WCHAR										szNickName[20];
		char										SessionKey[64];
	};

	int												m_iNumOfRecvJoinPacket;
	int												m_iNumOfSessionKeyMiss;
	int												m_iNumOfSessionKeyNotFound;

	UINT											m_uiNumOfSessionAll;
	UINT											m_uiNumOfLoginCompleteUser;
	UINT											m_uiUpdateTPS;

	UINT64											m_uiAccountNo;
	CTLSMemoryPool<st_SessionKey>					*m_pSessionKeyMemoryPool;

	CTLSMemoryPool<st_USER>							*m_pUserMemoryPool;
	CRITICAL_SECTION								*m_pSessionKeyMapCS;
	std::unordered_map<UINT64, st_SessionKey*>		m_UserSessionKeyMap;
	std::unordered_map<UINT64, st_USER*>			m_UserSessionMap;
	std::unordered_map<UINT64, st_USER*>			m_UserSectorMap[dfMAX_SECTOR_Y][dfMAX_SECTOR_X];

	CTLSMemoryPool<st_MESSAGE>						*m_pMessageMemoryPool;
	CLockFreeQueue<st_MESSAGE*>						*m_pMessageQueue;

	CChattingLanClient								*m_pChattingLanClient;
	CChatMonitoringLanClient						*m_pMonitoringLanClient;

	HANDLE											m_hUpdateThreadHandle;
	HANDLE											m_hSessionKeyHeartBeatThreadHandle;
	HANDLE											m_hUpdateEvent;
	HANDLE											m_hExitEvent;
	
	void BroadcastSectorAroundAll(WORD CharacterSectorX, WORD CharacterSectorY, CNetServerSerializationBuf *SendBuf);

	static UINT __stdcall UpdateThread(LPVOID pChatServet);
	UINT Updater();
	static UINT __stdcall HeartbeatCheckThread(LPVOID pChatServet);
	UINT __stdcall  HeartbeatChecker();
	static UINT __stdcall SessionKeyHeartbeatCheckThread(LPVOID pChatServet);
	UINT __stdcall  SessionKeyHeartbeatChecker();

	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 JoinClientID/* Client 정보 / ClientID / 기타등등 */);
	// Disconnect 후 호출
	virtual void OnClientLeave(UINT64 LeaveClientID);
	// Accept 직후 IP 차단등을 위한 용도
	virtual bool OnConnectionRequest(const WCHAR *IP);

	// 패킷 수신 완료 후
	virtual void OnRecv(UINT64 ReceivedSessionID, CNetServerSerializationBuf *OutReadBuf);
	// 패킷 송신 완료 후
	virtual void OnSend(UINT64 ClientID, int sendsize);

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin();
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd();

	// LanServer 내부 에러 발생시 호출
	// 파일과 콘솔에 윈도우의 GetLastError() 반환값과 직접 지정한 Error 를 출력하며
	// 1000 번 이후로 상속받은 클래스에서 사용자 정의하여 사용 할 수 있음
	// 단, GetLastError() 의 반환값이 10054 번일 경우는 이를 실행하지 않음
	virtual void OnError(st_Error *OutError);

	bool PacketProc_PlayerJoin(UINT64 SessionID);
	bool PacketProc_PlayerLeave(UINT64 SessionID);
	bool PacketProc_PlayerLoginFromLoginServer(CNetServerSerializationBuf *pRecvPacket);
	void PacketProc_HeartBeat(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket);
	void PacketProc_SessionKeyHeartBeat(CNetServerSerializationBuf *pRecvPacket);
	bool PacketProc_SectorMove(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID);
	bool PacketProc_ChatMessage(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID);
	bool PacketProc_Login(CNetServerSerializationBuf *pRecvPacket, CNetServerSerializationBuf *pOutSendPacket, UINT64 SessionID);

public:

	CChatServer();
	virtual ~CChatServer();

	bool ChattingServerStart(const WCHAR *szChatServerOptionFileName, const WCHAR *szChatServerLanClientFileName, const WCHAR *szMonitoringClientFileName);
	void ChattingServerStop();

	bool LoginPacketRecvedFromLoginServer(UINT64 AccountNo, st_SessionKey *pSessionKey);
	void OverWriteSessionKey(UINT64 AccountNo, st_SessionKey *pSessionKey);

	int GetNumOfPlayer();
	int GetNumOfAllocPlayer();
	int GetNumOfAllocPlayerChunk();

	int GetNumOfMessageInPool();
	int GetNumOfMessageInPoolChunk();
	int GetRestMessageInQueue();
	UINT GetUpdateTPSAndReset();

	int GetNumOfClientRecvPacket();
	int GetNumOfRecvJoinPacket();
	int GetNumOfSessionKeyMiss();
	int GetNumOfSessionKeyNotFound();

	int GetUsingNetServerBufCount();
	int GetUsingLanServerBufCount();

	int GetUsingSessionKeyChunkCount();
	int GetUsingSessionKeyNodeCount();
};