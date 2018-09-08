// NetServer.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "PreComfile.h"
#include "ChatServer.h"
#include <iostream>


bool OptionParsing(WCHAR *BindIP, int *BindPort, int *NumOfWorkerThread, int *ClientMax, int *MonitorNo);

int main()
{
	timeBeginPeriod(1);
	system("mode con cols=76 lines=29"); 
	WCHAR IP[16];
	int BindPort, NumOfThread, ClientMax, MonitorNo;
	if (!OptionParsing(IP, &BindPort, &NumOfThread, &ClientMax, &MonitorNo))
	{
		wprintf(L"OptionParsing Err\n");
		return 0;
	}
	CChatServer ChattingServer;
	for (int i = 0; i < 40; ++i)
	{
		wprintf(L"\n");
	}

	bool bIsServerRunning = false;

	while (1)
	{
		Sleep(1000);
		wprintf(L"\nServer Start : Home / Server Stop : End / Exit Program : ESC\n");
		if (GetAsyncKeyState(VK_ESCAPE))
		{
			if (bIsServerRunning)
				ChattingServer.ChattingServerStop();
			break;
		}	
		if (bIsServerRunning)
		{
			if (GetAsyncKeyState(VK_END))
			{
				bIsServerRunning = false;
				ChattingServer.ChattingServerStop();
			}

			wprintf(L"-------------------------------------------------------------------------\n\n\n\n\n");
			wprintf(L"Session Count				:	%15d\n", ChattingServer.GetNumOfUser());
			wprintf(L"Session PacketPool Alloc Count		:	%15d\n\n\n", ChattingServer.GetNumOfAllocPlayer());
			wprintf(L"Session PacketPool Chunk Count		:	%15d\n\n\n", ChattingServer.GetNumOfAllocPlayerChunk());
			wprintf(L"Update Message Pool Node Count		:	%15d\n", ChattingServer.GetNumOfMessageInPool());
			wprintf(L"Update Message Pool Chunk Count		:	%15d\n", ChattingServer.GetNumOfMessageInPoolChunk());
			wprintf(L"Update Message Queue Node Count		:	%15d\n\n\n", ChattingServer.GetRestMessageInQueue());
			wprintf(L"Player Count				:	%15d\n\n\n", ChattingServer.GetNumOfPlayer());
			wprintf(L"Accept Total Count			:	%15d\n", ChattingServer.GetAcceptTotal());
			wprintf(L"Accept TPS				: 	%15d\n", ChattingServer.GetAcceptTPSAndReset());
			wprintf(L"Update TPS				:	%15d\n\n\n", ChattingServer.GetUpdateTPSAndReset());
			wprintf(L"checksum %d / payload %d / HeaderCode %d\n", ChattingServer.checksum, ChattingServer.payloadOver, ChattingServer.HeaderCode);
			wprintf(L"-------------------------------------------------------------------------\n");
		}
		else if (GetAsyncKeyState(VK_HOME))
		{
			if (!ChattingServer.ChattingServerStart(IP, BindPort, NumOfThread, false, ClientMax))
				break;
			bIsServerRunning = true;
		}
	}
}

bool OptionParsing(WCHAR *BindIP, int *BindPort, int *NumOfWorkerThread, int *ClientMax, int *MonitorNo)
{
	_wsetlocale(LC_ALL, L"Korean");

	CParser parser;
	WCHAR cBuffer[BUFFER_MAX];

	FILE *fp;
	_wfopen_s(&fp, L"NetServerOption.txt", L"rt, ccs=UNICODE");

	int iJumpBOM = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int iFileSize = ftell(fp);
	fseek(fp, iJumpBOM, SEEK_SET);
	int FileSize = (int)fread_s(cBuffer, BUFFER_MAX, sizeof(WCHAR), BUFFER_MAX / 2, fp);
	int iAmend = iFileSize - FileSize; // 개행 문자와 파일 사이즈에 대한 보정값
	fclose(fp);

	cBuffer[iFileSize - iAmend] = '\0';
	WCHAR *pBuff = cBuffer;

	if (!parser.GetValue_String(pBuff, L"SERVER", L"BIND_IP", BindIP))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SERVER", L"BIND_PORT", BindPort))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SERVER", L"WORKER_THREAD", NumOfWorkerThread))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SERVER", L"CLIENT_MAX", ClientMax))
		return false;
	if (!parser.GetValue_Int(pBuff, L"SERVER", L"MONITOR_NO", MonitorNo))
		return false;

	return true;
}
