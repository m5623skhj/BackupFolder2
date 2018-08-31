#include "PreComfile.h"
#include "CToHTTP.h"


CCToHTTP::CCToHTTP(int SockTimeout, const WCHAR* ConnectIP, WORD RecvSize)
{
	m_iSockTimeout = SockTimeout;
	m_wRecvSize = RecvSize;
	wcscpy_s(m_IP, ConnectIP);
}

CCToHTTP::~CCToHTTP()
{
	closesocket(m_HTTPSock);
}

bool CCToHTTP::InitSocket()
{
	m_HTTPSock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_HTTPSock == INVALID_SOCKET)
	{
		printf_s("Socket Err %d\n", WSAGetLastError());
		return false;
	}

	int retval = setsockopt(m_HTTPSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&m_iSockTimeout, sizeof(m_iSockTimeout));
	if (retval == SOCKET_ERROR)
	{
		printf_s("Recv SockOpr Err %d\n", WSAGetLastError());
		return false;
	}
	retval = setsockopt(m_HTTPSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&m_iSockTimeout, sizeof(m_iSockTimeout));
	if (retval == SOCKET_ERROR)
	{
		printf_s("Send SockOpr Err %d\n", WSAGetLastError());
		return false;
	}


	return true;
}

USHORT CCToHTTP::CreateHTTPData(char* Dest, const WCHAR* PathWithIP, const char* Body)
{
	/*
		POST http:// m_IP / Path~~~ HTTP/1.1\r\n
		User-Agent: ~~~~\r\n
		Host: m_IP\r\n
		Connection: Close\r\n
		Content-Length: ContentLegnth\r\n
		\r\n
		1\r\n
		1\r\n
		0\r\n
		{ Body }
	*/
	
	const char* HTTPComponent[3] = 
	{ " HTTP/1.1\r\nHost: ",
	  "\r\nConnection: Close\r\nContent-Length: ",
	  "\r\n\r\n1\r\n1\r\n0\r\n"
	};
	
	char Path[HTTP_PATH_MAX];
	int PathSize = lstrlenW(PathWithIP);
	WideCharToMultiByte(CP_UTF8, 0, PathWithIP, PathSize, Path, PathSize, NULL, NULL);
	Path[PathSize] = '\0';
	char IP[16];
	WideCharToMultiByte(CP_UTF8, 0, m_IP, 16, IP, 16, NULL, NULL);
	char ContentLengthArr[5];
	WORD ContentLength = (WORD)strlen(Body);

	char HTTPData[HTTP_SIZE_MAX] = "POST ";
	strcat_s(HTTPData, Path);
	strcat_s(HTTPData, HTTPComponent[0]);
	strcat_s(HTTPData, IP);
	strcat_s(HTTPData, HTTPComponent[1]);
	sprintf_s(ContentLengthArr, "%d", ContentLength);
	strcat_s(HTTPData, ContentLengthArr);
	strcat_s(HTTPData, HTTPComponent[2]);
	// Json 중괄호
	// 바디에만 사용함
	// 이후에는 rapidJson을 이용해야 할지도 모름
	strcat_s(HTTPData, "{");
	strcat_s(HTTPData, Body);
	strcat_s(HTTPData, "}");

	USHORT HTTPSize = (USHORT)strlen(HTTPData);

	strcpy_s(Dest, HTTP_SIZE_MAX, HTTPData);
	return HTTPSize;
}

bool CCToHTTP::ConnectToHTTP()
{
	// 컨넥트를 계속 대기하고 있으면 안되므로
	// HTTP 소켓을 넌블락 소켓으로 만듦
	u_long on = 1;
	int retval = ioctlsocket(m_HTTPSock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		printf_s("On ioctlsocket Err %d\n", WSAGetLastError());
		return false;
	}

	SOCKADDR_IN serveraddr;
	IN_ADDR Addr;
	InetPton(AF_INET, m_IP, &Addr);

	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr = Addr;
	serveraddr.sin_port = htons(HTTP_PORT);

	retval = connect(m_HTTPSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
	{
		printf_s("Connect Err %d\n", WSAGetLastError());
		return false;
	}

	FD_SET wset;
	// 셀렉트 타임아웃 지정
	timeval t;
	t.tv_sec = 0;
	t.tv_usec = 100;
	FD_ZERO(&wset);
	FD_SET(m_HTTPSock, &wset);

	// select 함수를 통해 일정시간 기다리고
	// 시간이 초과하면 타임아웃시킴
	retval = select(0, NULL, &wset, NULL, &t);
	if (retval == SOCKET_ERROR)
	{
		printf_s("Select Err%d\n", WSAGetLastError());
		return false;
	}
	else if (!FD_ISSET(m_HTTPSock, &wset))
	{
		printf_s("Connect timeout");
		return false;
	}

	// 다시 HTTP 소켓을 블락 소켓으로 변경
	on = 0;
	retval = ioctlsocket(m_HTTPSock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		printf_s("Off ioctlsocket Err %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

short CCToHTTP::GetHTTPBody(char* BodyDest, const char* InputHTTP, WORD InputHTTPSize)
{
	int retval = send(m_HTTPSock, (char*)InputHTTP, InputHTTPSize, 0);
	if (retval == 0)
	{
		printf_s("Send Err %d\n", WSAGetLastError());
		return -1;
	}

	// 처음 ' ' ~ 다음 0x20 까지가 완료코드
	char recvData[HTTP_SIZE_MAX];
	retval = recv(m_HTTPSock, (char*)recvData, m_wRecvSize, 0);
	char* CompleteCodePtr = strchr(recvData, ' ');
	// 공백 문자를 뛰어넘기 위한 포인터 이동
	CompleteCodePtr += 1;
	int CompleteCode = atoi(CompleteCodePtr);

	// Content-Length ~ 다음 0x0d 까지가 Body의 사이즈
	char* ContentLengthPtr = strstr((char*)recvData, "Content-Length");
	// Content-Length 를 뛰어넘기 위한 포인터 이동
	ContentLengthPtr += 16;
	int ContentLength = atoi(ContentLengthPtr);

	// \r\n\r\n 이후부터 Body
	char* EndOfHeader = strstr((char*)recvData, "\r\n\r\n");
	// \r\n\r\n 을 뛰어넘기 위한 포인터 이동
	EndOfHeader += 4;
	strcpy_s(BodyDest, HTTP_SIZE_MAX, EndOfHeader);
	// ContentLength 만큼만 사용하기 위하여 널문자를 삽입
	BodyDest[ContentLength] = '\0';

	return CompleteCode;
}

short CCToHTTP::GetHTTPBody(WCHAR* BodyDest, const char* InputHTTP, WORD InputHTTPSize)
{
	int retval = send(m_HTTPSock, (char*)InputHTTP, InputHTTPSize, 0);
	if (retval == 0)
	{
		printf_s("Send Err %d\n", WSAGetLastError());
		return -1;
	}

	// 처음 ' ' ~ 다음 0x20 까지가 완료코드
	char recvData[HTTP_SIZE_MAX];
	retval = recv(m_HTTPSock, (char*)recvData, m_wRecvSize, 0);
	char* CompleteCodePtr = strchr(recvData, ' ');
	// 공백 문자를 뛰어넘기 위한 포인터 이동
	CompleteCodePtr += 1;
	int CompleteCode = atoi(CompleteCodePtr);

	// Content-Length ~ 다음 0x0d 까지가 Body의 사이즈
	char* ContentLengthPtr = strstr((char*)recvData, "Content-Length");
	// Content-Length 를 뛰어넘기 위한 포인터 이동
	ContentLengthPtr += 16;
	int ContentLength = atoi(ContentLengthPtr);

	// \r\n\r\n 이후부터 Body
	char* EndOfHeader = strstr((char*)recvData, "\r\n\r\n");
	// \r\n\r\n 을 뛰어넘기 위한 포인터 이동
	EndOfHeader += 4;
	char cBody[HTTP_SIZE_MAX];
	strcpy_s(cBody, HTTP_SIZE_MAX, EndOfHeader);
	int cBodySize = strlen(cBody);
	MultiByteToWideChar(CP_ACP, 0, cBody, cBodySize, BodyDest, cBodySize * sizeof(WCHAR));
	// ContentLength 만큼만 사용하기 위하여 널문자를 삽입
	BodyDest[cBodySize] = '\0';

	return CompleteCode;
}
