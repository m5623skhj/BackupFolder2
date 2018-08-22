#pragma once

#define HTTP_PORT				80
#define HTTP_COMPLETECODE		200
#define HTTP_PATH_MAX			200
#define HTTP_SIZE_MAX			2048

//------------------------------------------
// 해당 클래스는 다음과 같은 순서로 사용된다.
// 생성 및 초기화								->
// HTTP 프로토콜 데이터 생성					->
// 웹서버에 Connect								->
// 위에서 만들어진 데이터를 send				->
// send 후 바로 recv							->
// 데이터 받은 후 HTTP 헤더에서 완료코드 얻기	->
// HTTP 헤더에서 Content-Length 얻기			->
// 헤더 끝 찾기									->
// 완료코드와 BODY 영역을 돌려줌				->
// 완료코드가 200이 아니라면 오류				
//------------------------------------------
class CCToHTTP
{
private :
	int m_iSockTimeout;
	WORD m_wRecvSize;
	SOCKET m_HTTPSock;
	WCHAR m_IP[16];
public :
	// 블럭 소켓이므로 TimeOut을 지정
	// TimeOut 값은 미리 세컨드 단위로 입력
	// 5~10 초 권장
	// RecvSize 값은 헤더와 바디를 합쳐서 나올 최대 크기로 잡음 (Default 1024)
	CCToHTTP(int SockTimeout, const WCHAR* ConnectIP, WORD RecvSize = 1024);
	~CCToHTTP();

	// BodyDest에 HTTPBody가 넣어짐
	// 반환값으로 HTTP 완료코드가 반환됨
	// 함수 중간에 에러가 발생시 -1이 반환됨
	short GetHTTPBody(char* BodyDest, const char* InputHTTP, WORD InputHTTPSize);
	short GetHTTPBody(WCHAR* BodyDest, const char* InputHTTP, WORD InputHTTPSize);

	// 인자로 받은 IP를 사용하여 소켓 초기화
	// Connect는 해당 함수에서 지원하지 않음
	bool InitSocket();
	// 해당 IP에 Connect
	bool ConnectToHTTP();

	// HTTP 프로토콜의 데이터를 생성
	// IP는 생성자에서 멤버변수로 들어온 값을 사용
	// Path : HTTP 호출 경로
	// ContentLength : 해당 Body의 사이즈
	// Body 인자 안에 따옴표 등은 역슬래시로 묶어줘야 함
	// 반환값으로 HTTP의 크기가 반환됨
	USHORT CreateHTTPData(char* Dest, const WCHAR* PathWithIP, const char* Body);
};