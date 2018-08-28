#pragma once
#include "LanServer.h"
#include <unordered_map>

class Log;
class CSerializationBuf;

class CEcho : public CLanServer
{
private :
	struct User
	{
		int SomeData;
	};
	std::unordered_map<UINT64, User*> m_UserMap;
public :
	// Accept 후 접속처리 완료 후 호출
	virtual void OnClientJoin(UINT64 OutClientID/* Client 정보 / ClientID / 기타등등 */);
	// Disconnect 후 호출
	virtual void OnClientLeave(UINT64 LeaveClientID);
	// Accept 직후 IP 차단등을 위한 용도
	virtual bool OnConnectionRequest();

	// 패킷 수신 완료 후
	virtual void OnRecv(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf);
	// 패킷 송신 완료 후
	virtual void OnSend();

	// 워커스레드 GQCS 바로 하단에서 호출
	virtual void OnWorkerThreadBegin();
	// 워커스레드 1루프 종료 후
	virtual void OnWorkerThreadEnd();

	// LanServer 내부 에러 발생시 호출
	// 파일과 콘솔에 윈도우의 GetLastError() 반환값과 직접 지정한 Error 를 출력하며
	// 1000 번 이후로 상속받은 클래스에서 사용자 정의하여 사용 할 수 있음
	// 단, GetLastError() 의 반환값이 10054 번일 경우는 이를 실행하지 않음
	virtual void OnError(st_Error *OutError);

	CEcho(const WCHAR *IP, UINT PORT, BYTE NumOfWorkerThread, bool IsNagle, UINT MaxClient);
	virtual ~CEcho();

	//////////////////////////////////////////////////////////////
	//ULONGLONG m_ULLCountOfNew;
	//////////////////////////////////////////////////////////////
};