// NetServer.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "PreCompile.h"
#include "ChatServer.h"
#include <iostream>


int main()
{
	timeBeginPeriod(1);
	system("mode con cols=84 lines=28"); 
	
	CChatServer ChattingServer;
	for (int i = 0; i < 40; ++i)
	{
		wprintf(L"\n");
	}

	bool bIsServerRunning = false;
	bool LockServer = true;

	while (1)
	{
		Sleep(1000);
		if (LockServer)
		{
			wprintf(L"\n\nLogin Server Start : F2 / Login Server Stop : X / Exit Program : ESC / UnLock : Y\n");
			if (GetAsyncKeyState(0x59) & 0x8000)
				LockServer = false;
		}
		else
		{
			wprintf(L"\n\nLogin Server Start : F2 / Login Server Stop : X / Exit Program : ESC / Lock : K\n");
			if (GetAsyncKeyState(0x4B) & 0x8000)
				LockServer = true;
		}

		if (!LockServer && GetAsyncKeyState(VK_ESCAPE) & 0x8000)
		{
			if (bIsServerRunning)
				ChattingServer.ChattingServerStop();
			break;
		}	
		if (bIsServerRunning)
		{
			if (!LockServer && GetAsyncKeyState(0x58) & 0x8000)
			{
				bIsServerRunning = false;
				ChattingServer.ChattingServerStop();
			}

			wprintf(L"-------------------------------------------------------------------------\n\n\n");
			wprintf(L"Session Count				:	%15d\n", ChattingServer.GetNumOfUser());
			wprintf(L"Session PacketPool Alloc Count		:	%15d\n\n", ChattingServer.GetNumOfAllocPlayer());
			wprintf(L"Session PacketPool Chunk Count		:	%15d\n\n", ChattingServer.GetNumOfAllocPlayerChunk());
			wprintf(L"Update Message Pool Node Count		:	%15d\n", ChattingServer.GetNumOfMessageInPool());
			wprintf(L"Update Message Pool Chunk Count		:	%15d\n", ChattingServer.GetNumOfMessageInPoolChunk());
			wprintf(L"Update Message Queue Node Count		:	%15d\n\n", ChattingServer.GetRestMessageInQueue());
			wprintf(L"Player Count				:	%15d\n\n", ChattingServer.GetNumOfPlayer());
			wprintf(L"Accept Total Count			:	%15d\n", ChattingServer.GetAcceptTotal());
			wprintf(L"Accept TPS				: 	%15d\n", ChattingServer.GetAcceptTPSAndReset());
			wprintf(L"Update TPS				:	%15d\n\n", ChattingServer.GetUpdateTPSAndReset());
			wprintf(L"LanClient Recved : %d\n", ChattingServer.GetNumOfClientRecvPacket());
			wprintf(L"Recv Join Packet : %d\n\n", ChattingServer.GetNumOfRecvJoinPacket());
			wprintf(L"------------------------------- ErrorPrint ------------------------------\n");
			wprintf(L"checksum %d / payload %d / HeaderCode %d\n", ChattingServer.checksum, ChattingServer.payloadOver, ChattingServer.HeaderCode);
			wprintf(L"SessionKey Miss %d / SessionKey Not Found %d\n", ChattingServer.GetNumOfSessionKeyMiss(), ChattingServer.GetNumOfSessionKeyNotFound());
			wprintf(L"-------------------------------------------------------------------------\n");
		}
		else if (!LockServer && GetAsyncKeyState(VK_F2) & 0x8000)
		{
			if (!ChattingServer.ChattingServerStart(L"ChatServerOption.txt", L"ChatServerLanClientOption.txt"))
				break;
			bIsServerRunning = true;
		}
	}
}

