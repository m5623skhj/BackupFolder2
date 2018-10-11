#pragma once
#include "Ringbuffer.h"
#include "LockFreeQueue.h"
#include "Queue.h"

// 해당 소켓이 송신중이 아님
#define NONSENDING	0
// 해당 소켓이 송신중임
#define SENDING		1

// WSASend 한 번에 보낼 수 있는 최대 수
#define dfONE_SEND_WSABUF_MAX									500

class CNetServerSerializationBuf;
class CMMOServer;

///////////////////////////////////////////////////
// 이 라이브러리를 이용하려면 해당 클래스를 상속받은 배열을 미리 할당하고
// 할당된 배열의 각각을 MMOServer 의 LinkSession 함수를 호출하여
// MMOServer 와 연결시켜줘야만 함
///////////////////////////////////////////////////
class CSession
{
public :
	// 해당 유저에게 패킷을 전송함
	void SendPacket(CNetServerSerializationBuf *pSendPacket);
	// 해당 유저의 접속을 끊음
	void DisConnect();
	// 해당 유저의 상태를 인증 상태에서 게임 상태로 변경함
	// 이 함수를 호출 시켜야 유저가 비로소 게임에 진입함
	bool ChangeMode_AuthToGame();

protected :
	CSession();
	virtual ~CSession();

	// 인증 스레드에 유저가 접속함
	virtual void OnAuth_ClientJoin() = 0;
	// 인증 스레드에서 유저가 나감
	virtual void OnAuth_ClientLeave(bool IsClientLeaveServer) = 0;
	// 인증 스레드에 있는 유저가 패킷을 받음
	virtual void OnAuth_Packet(CNetServerSerializationBuf *pRecvPacket) = 0;
	// 게임 스레드에 유저가 접속함
	virtual void OnGame_ClientJoin() = 0;
	// 게임 스레드에 유저가 나감
	virtual void OnGame_ClientLeave() = 0;
	// 게임 스레드에 있는 유저가 패킷을 받음
	virtual void OnGame_Packet(CNetServerSerializationBuf *pRecvPacket) = 0;
	// 최종적으로 유저가 릴리즈 됨
	virtual void OnGame_ClientRelease() = 0;

private :
	// 세션 초기화 함수
	void SessionInit();

private :
	friend CMMOServer;

	// 세션이 현재 어떤 모드인지에 대한 표기
	enum en_SESSION_MODE
	{
		MODE_NONE = 0,			 // 세션 미사용
		MODE_AUTH,				 // 연결 후 인증모드 상태
		MODE_AUTH_TO_GAME,		 // 게임 모드로 전환
		MODE_GAME,				 // 인증 후 게임모드 상태
		MODE_LOGOUT_IN_AUTH, 	 // 종료준비
		MODE_LOGOUT_IN_GAME,	 // 종료준비
		MODE_WAIT_LOGOUT		 // 최종 종료
	};

	// CMMOServer 에서 스택으로 관리됨
	// 각 세션들은 이 구조체의 포인터를 얻어서 아래의 정보들을 관리함
	struct AcceptUserInfo
	{
		WORD												wSessionIndex;
		WORD												wAcceptedPort;
		SOCKET												AcceptedSock;
		WCHAR												AcceptedIP[16];
	};

	struct st_RECV_IO_DATA
	{
		UINT												uiBufferCount;
		OVERLAPPED											RecvOverlapped;
		CRingbuffer											RecvRingBuffer;
	};

	struct st_SEND_IO_DATA
	{
		UINT												uiBufferCount;
		OVERLAPPED											SendOverlapped;
		//CLockFreeQueue<CNetServerSerializationBuf*>			SendQ;
		CListBaseQueue<CNetServerSerializationBuf*>			SendQ;
	};

	// Auth 스레드에서 Game 스레드로 넘어갈 때 변경 해 주는 값
	// 외부의 호출에 의하여 변경됨
	bool													m_bAuthToGame;
	// m_uiIOCount 가 0이 되었을 때 true 가 됨
	// 해당 값이 true 가 되면 Game 스레드에서 최종적으로 유저를 삭제한다
	bool													m_bLogOutFlag;
	// 해당 세션이 어느 상태에 있는지에 대한 값
	// 각 상태들은 en_SESSION_MODE 에 정의 되어있음
	BYTE													m_byNowMode;
	// 해당 세션이 Send 중인지 아닌지에 대한 값
	UINT													m_uiSendIOMode;
	// 해당 세션의 IO Count
	// 0이 되면 m_bLogOutFlag 가 true 가 됨
	UINT													m_uiIOCount;
	// 해당 세션의 고유 식별값
	UINT64													m_uiSessionID;
	// 해당 세션의 Socket
	// m_pUserNetworkInfo 에 포함되어있는 값이나
	// 접근이 많으므로 따로 꺼내놓음
	SOCKET													m_Socket;
	// Recv 가 완료되면 해당 큐에 값을 집어넣게 되고
	// 이후 Auth 혹은 Game 스레드가 이 큐에서 패킷을 뽑아
	// 사용자에게 처리를 넘김
	CListBaseQueue<CNetServerSerializationBuf*>				m_RecvCompleteQueue;

	AcceptUserInfo											*m_pUserNetworkInfo;
	st_RECV_IO_DATA											m_RecvIOData;
	st_SEND_IO_DATA											m_SendIOData;

	// WSASend 시 직렬화 버퍼의 해제를 위하여 그 포인터를 넣어둘 공간
	CNetServerSerializationBuf*								pSeirializeBufStore[dfONE_SEND_WSABUF_MAX];
};