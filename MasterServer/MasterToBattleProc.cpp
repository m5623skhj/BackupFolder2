#include "PreCompile.h"

#include "MasterToBattle.h"
#include "MasterServer.h"

#include "LanServerSerializeBuf.h"
#include "Log.h"

#include "Protocol/CommonProtocol.h"

void CMasterToBattleServer::MasterToBattleServerOnProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	CSerializationBuf &RecvBuf = *OutReadBuf;

	if (RecvBuf.GetUseSize() != dfSIZE_BATTLE_ON)
	{
		DisConnect(ReceivedSessionID);
		return;
	}

	st_BATTLE_SERVER_INFO *BattleServerInfo = *m_pBattleServerInfoMemoryPool->Alloc();
	char MasterToken[32];

	RecvBuf.ReadBuffer((char*)BattleServerInfo->ServerIP, sizeof(BattleServerInfo->ServerIP));
	RecvBuf >> BattleServerInfo->wPort;
	RecvBuf.ReadBuffer((char*)BattleServerInfo->ConnectTokenNow, sizeof(BattleServerInfo->ConnectTokenNow));
	RecvBuf.ReadBuffer(MasterToken, sizeof(MasterToken));
	memcpy(BattleServerInfo->ConnectTokenNow, BattleServerInfo->ConnectTokenBefore, sizeof(BattleServerInfo->ConnectTokenBefore));
	InitializeSRWLock(&BattleServerInfo->ConnectTokenSRWLock);

	if (memcmp(MasterToken, m_pMasterServer->m_MasterToken, sizeof(MasterToken)) != 0)
	{
		// 마스터 키가 다르면 다른 통보 없이 끊어버림
		DisConnect(ReceivedSessionID);
		return;
	}

	RecvBuf.ReadBuffer((char*)BattleServerInfo->ChatServerIP, sizeof(BattleServerInfo->ChatServerIP));
	RecvBuf >> BattleServerInfo->wPort;

	// 배틀 서버의 번호를 부여함
	int BattleServerNo = InterlockedIncrement(&m_iBattleServerNo);
	BattleServerInfo->iServerNo = BattleServerNo;

	// m_BattleClientMap 임계영역 시작(쓰기)
	AcquireSRWLockExclusive(&m_BattleServerInfoMapSRWLock);
	//EnterCriticalSection(&m_csBattleServerInfoLock);

	m_BattleServerInfoMap.insert({ ReceivedSessionID , BattleServerInfo });

	// m_BattleClientMap 임계영역 끝  (쓰기)
	ReleaseSRWLockExclusive(&m_BattleServerInfoMapSRWLock);
	//LeaveCriticalSection(&m_csBattleServerInfoLock);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type;

	Type = en_PACKET_BAT_MAS_RES_SERVER_ON;

	SendBuf << Type << BattleServerInfo->iServerNo;
	SendPacket(ReceivedSessionID, &SendBuf);
}

void CMasterToBattleServer::MasterToBattleConnectTokenProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	CSerializationBuf &RecvBuf = *OutReadBuf;

	if (RecvBuf.GetUseSize() != dfSIZE_BATTLE_REISSUE_TOKEN)
	{
		DisConnect(ReceivedSessionID);
		return;
	}

	// m_BattleServerInfoMap 임계영역 시작
	AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);

	map<UINT64, st_BATTLE_SERVER_INFO*>::iterator BattleServerIter = m_BattleServerInfoMap.find(ReceivedSessionID);
	if (BattleServerIter == m_BattleServerInfoMap.end())
	{
		DisConnect(ReceivedSessionID);
		return;
	}
	st_BATTLE_SERVER_INFO *pBattleServerInfo = BattleServerIter->second;

	// ConnectToken 임계영역 시작 (쓰기)
	AcquireSRWLockExclusive(&pBattleServerInfo->ConnectTokenSRWLock);

	memcpy(pBattleServerInfo->ConnectTokenBefore, pBattleServerInfo->ConnectTokenNow, dfSIZE_CONNECT_TOKEN);
	RecvBuf.ReadBuffer(pBattleServerInfo->ConnectTokenNow, dfSIZE_CONNECT_TOKEN);

	// ConnectToken 임계영역 끝   (쓰기)
	ReleaseSRWLockExclusive(&pBattleServerInfo->ConnectTokenSRWLock);

	// m_BattleServerInfoMap 임계영역 끝
	ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);

	UINT ReqSequence;
	RecvBuf >> ReqSequence;

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type;

	Type = en_PACKET_BAT_MAS_RES_CONNECT_TOKEN;

	SendBuf << Type << ReqSequence + 1;
	SendPacket(ReceivedSessionID, &SendBuf);
}

void CMasterToBattleServer::MasterToBattleLeftUserProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	CSerializationBuf &RecvBuf = *OutReadBuf;

	if (RecvBuf.GetUseSize() != dfSIZE_LEFT_USER)
	{
		DisConnect(ReceivedSessionID);
		return;
	}

	// m_BattleServerInfoMap 임계영역 시작
	AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);

	map<UINT64, st_BATTLE_SERVER_INFO*>::iterator BattleServerIter = m_BattleServerInfoMap.find(ReceivedSessionID);
	if (BattleServerIter == m_BattleServerInfoMap.end())
	{
		DisConnect(ReceivedSessionID);
		return;
	}
	st_BATTLE_SERVER_INFO *pBattleServerInfo = BattleServerIter->second;

	UINT64 MapFindKey = pBattleServerInfo->iServerNo;
	MapFindKey = MapFindKey << dfSHIFT_RIGHT_SERVERNO;

	// m_BattleServerInfoMap 임계영역 끝
	ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);

	int RoomNo;
	INT64 ClientKey;
	UINT ReqSequence;

	RecvBuf >> RoomNo >> ClientKey >> ReqSequence;
	MapFindKey += RoomNo;

	bool RoomFind = false;
	st_ROOM_INFO *pUserInRoom;

	// 우선적으로 방 리스트의 front 부터 확인함
	// 풀 방 맵 부터 살펴볼 경우 유저가 꽉 차서 리스트에서 맵으로 넘어가버리면
	// 해당 인원을 인지 할 수 없을 가능성이 존재하기 때문임
	// m_RoomInfoList 임계영역 시작
	EnterCriticalSection(&m_csRoomInfoLock);

	pUserInRoom = m_RoomInfoList.front();

	if (pUserInRoom->iRoomNo == RoomNo)
	{
		RoomFind = true;
		
		// pUserInRoom->UserClientKeySet 임계영역 시작(쓰기)
		//EnterCriticalSection(&pUserInRoom->UserSetLock);

		if (pUserInRoom->UserClientKeySet.erase(ClientKey) != 0)
		{
			pUserInRoom->iMaxUser++;
		}

		// pUserInRoom->UserClientKeySet 임계영역 끝  (쓰기)
		//LeaveCriticalSection(&pUserInRoom->UserSetLock);
	}

	// m_RoomInfoList 임계영역 끝
	LeaveCriticalSection(&m_csRoomInfoLock);

	// m_RoomInfoList 의 헤더에 없었다면일 시작 대기 맵에 있을수도 있으므로 확인함
	if (!RoomFind)
	{
		// m_PlayStandbyRoomMap 임계영역 시작
		EnterCriticalSection(&m_csPlayStandbyRoomLock);
	
		map<UINT64, st_ROOM_INFO*>::iterator UserRoomMapIter = m_PlayStandbyRoomMap.find(MapFindKey);
		if (UserRoomMapIter != m_PlayStandbyRoomMap.end())
		{
			pUserInRoom = UserRoomMapIter->second;
			if (pUserInRoom->UserClientKeySet.erase(ClientKey) != 0)
			{
				pUserInRoom->iMaxUser++;
			}
		}

		// m_PlayStandbyRoomMap 임계영역 끝
		LeaveCriticalSection(&m_csPlayStandbyRoomLock);
	}

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type;

	Type = en_PACKET_BAT_MAS_RES_LEFT_USER;

	SendBuf << Type << RoomNo << ReqSequence + 1;
	SendPacket(ReceivedSessionID, &SendBuf);
}

void CMasterToBattleServer::MasterToBattleCreatedRoomProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	CSerializationBuf &RecvBuf = *OutReadBuf;

	AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);
	//EnterCriticalSection(&m_csBattleServerInfoLock);
	map<UINT64, st_BATTLE_SERVER_INFO*>::iterator BattleServerIter = m_BattleServerInfoMap.find(ReceivedSessionID);
	if (BattleServerIter == m_BattleServerInfoMap.end())
	{
		DisConnect(ReceivedSessionID);
		return;
	}
	st_BATTLE_SERVER_INFO *pBattleServerInfo = BattleServerIter->second;

	if (pBattleServerInfo == NULL)
	{
		DisConnect(ReceivedSessionID);
		return;
	}

	if (RecvBuf.GetUseSize() != dfSIZE_CREATE_ROOM)
	{
		DisConnect(ReceivedSessionID);
		return;
	}

	ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);
	//LeaveCriticalSection(&m_csBattleServerInfoLock);

	// 방 할당 받기
	st_ROOM_INFO &RoomInfo = *m_pRoomInfoMemoryPool->Alloc();
	int ReqSequence;

	RecvBuf >> RoomInfo.iBattleServerNo >> RoomInfo.iRoomNo >> RoomInfo.iMaxUser;
	//RoomInfo.iRemainingUser = RoomInfo.iMaxUser;
	RecvBuf.ReadBuffer(RoomInfo.EnterToken, sizeof(RoomInfo.EnterToken));
	RecvBuf >> ReqSequence;

	// m_RoomInfoList 임계영역 시작
	EnterCriticalSection(&m_csRoomInfoLock);

	// 할당 받은 방은 인원이 한 명도 없을 것이므로 리스트 가장 뒤에 넣음
	m_RoomInfoList.push_back(&RoomInfo);
	
	// m_RoomInfoList 임계영역 끝
	LeaveCriticalSection(&m_csRoomInfoLock);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type;

	Type = en_PACKET_BAT_MAS_RES_CREATED_ROOM;

	SendBuf << Type << RoomInfo.iBattleServerNo << ReqSequence + 1;
	SendPacket(ReceivedSessionID, &SendBuf);
}

void CMasterToBattleServer::MasterToBattleClosedRoomProc(UINT64 ReceivedSessionID, CSerializationBuf *OutReadBuf)
{
	CSerializationBuf &RecvBuf = *OutReadBuf;

	if (RecvBuf.GetUseSize() != dfSIZE_CLOSE_ROOM)
	{
		DisConnect(ReceivedSessionID);
		return;
	}
	
	// m_BattleServerInfoMap 임계영역 시작
	AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);

	map<UINT64, st_BATTLE_SERVER_INFO*>::iterator BattleServerIter = m_BattleServerInfoMap.find(ReceivedSessionID);
	if (BattleServerIter == m_BattleServerInfoMap.end())
	{
		DisConnect(ReceivedSessionID);
		return;
	}
	st_BATTLE_SERVER_INFO *pBattleServerInfo = BattleServerIter->second;

	UINT64 MapFindKey = pBattleServerInfo->iServerNo;
	MapFindKey = MapFindKey << dfSHIFT_RIGHT_SERVERNO;

	// m_BattleServerInfoMap 임계영역 끝
	ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);

	int RoomNo;
	UINT ReqSequence;

	RecvBuf >> RoomNo >> ReqSequence;
	MapFindKey += RoomNo;

	// 방 지우기 처리
	// 시작 대기 방 맵 안에 있는 것만을 대상으로 검색함
	// 리스트를 안보는 이유는 방 지우기 처리 패킷을 보내려면
	// 해당 방이 마스터 상에서 한 번이라도 꽉 차야하기 때문임

	// m_PlayStandbyRoomMap 임계영역 시작
	EnterCriticalSection(&m_csPlayStandbyRoomLock);

	map<UINT64, st_ROOM_INFO*>::iterator UserRoomMapIter = m_PlayStandbyRoomMap.find(MapFindKey);
	if (UserRoomMapIter == m_PlayStandbyRoomMap.end())
	{
		// 방 지우기 요청을 받았지만 이미 없는방임
		// 오류 상황
		g_Dump.Crash();
	}

	st_ROOM_INFO *pDeleteRoom = UserRoomMapIter->second;

	pDeleteRoom->UserClientKeySet.clear();
	m_PlayStandbyRoomMap.erase(MapFindKey);

	// m_PlayStandbyRoomMap 임계영역 끝
	LeaveCriticalSection(&m_csPlayStandbyRoomLock);

	m_pRoomInfoMemoryPool->Free(pDeleteRoom);

	CSerializationBuf &SendBuf = *CSerializationBuf::Alloc();

	WORD Type;

	Type = en_PACKET_BAT_MAS_RES_CLOSED_ROOM;

	SendBuf << Type << RoomNo << ReqSequence + 1;
	SendPacket(ReceivedSessionID, &SendBuf);
}

void CMasterToBattleServer::MatchToMasterRequireRoom(CSerializationBuf *OutSendBuf)
{
	st_ROOM_INFO *pUserEnterRoom = NULL;
	BYTE Status;

	// m_PlayStandbyRoomMap 임계영역 시작
	EnterCriticalSection(&m_csPlayStandbyRoomLock);

	map<UINT64, st_ROOM_INFO*>::iterator MapEndIter = m_PlayStandbyRoomMap.end();

	// 맵 전체를 순회하여 인원이 모자란 방을 찾아본다
	for (auto MapTravelIter = m_PlayStandbyRoomMap.begin(); MapTravelIter != MapEndIter; ++MapTravelIter)
	{
		if (MapTravelIter->second->iMaxUser != 0)
		{
			// 방 정보를 pUserEnterRoom 에 넣음
			pUserEnterRoom = MapTravelIter->second;
			// 맵 내에 입장 가능 인원수가 0 이 아닌 방이 존재한다면
			// 해당 방의 입장 가능 인원수를 1 줄인다
			pUserEnterRoom->iMaxUser--;
			int RoomNo = pUserEnterRoom->iRoomNo;
			int ServerNo = pUserEnterRoom->iBattleServerNo;


			// m_PlayStandbyRoomMap 임계영역 끝
			LeaveCriticalSection(&m_csPlayStandbyRoomLock);
			
			// m_BattleServerInfoMap 임계영역 시작(읽기)
			AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);

			map<UINT64, st_BATTLE_SERVER_INFO*>::iterator EndIter = m_BattleServerInfoMap.end();
			map<UINT64, st_BATTLE_SERVER_INFO*>::iterator TravelIter = m_BattleServerInfoMap.begin();

			for (; TravelIter != EndIter; ++TravelIter)
			{
				if (TravelIter->second->iServerNo == ServerNo)
					break;
			}

			if (TravelIter == EndIter)
			{
				// 위에서 접속할 방을 찾았으나 그 방에 해당하는 서버가 없다면
				// 방 입장 실패를 보내고 유저가 다시 방을 요청하도록 유도함
				Status = 0;
				*OutSendBuf << Status;
				
				return;
			}

			st_BATTLE_SERVER_INFO &BattleServer = *TravelIter->second;

			Status = 1;
			*OutSendBuf << Status << ServerNo;
			OutSendBuf->WriteBuffer((char*)BattleServer.ServerIP, sizeof(BattleServer.ServerIP));
			*OutSendBuf << BattleServer.wPort << RoomNo;
			AcquireSRWLockShared(&BattleServer.ConnectTokenSRWLock);
			OutSendBuf->WriteBuffer(BattleServer.ConnectTokenNow, sizeof(BattleServer.ConnectTokenNow));
			AcquireSRWLockShared(&BattleServer.ConnectTokenSRWLock);
			OutSendBuf->WriteBuffer(pUserEnterRoom->EnterToken, sizeof(pUserEnterRoom->EnterToken));
			OutSendBuf->WriteBuffer((char*)BattleServer.ChatServerIP, sizeof(BattleServer.ChatServerIP));
			*OutSendBuf << BattleServer.wChatServerPort;

			// m_BattleServerInfoMap 임계영역 끝  (읽기)
			ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);

			return;
		}
	}

	// m_PlayStandbyRoomMap 임계영역 끝
	LeaveCriticalSection(&m_csPlayStandbyRoomLock);

	if (pUserEnterRoom == NULL)
	{
		// m_RoomInfoList 임계영역 시작
		EnterCriticalSection(&m_csRoomInfoLock);

		// 맵에 인원이 전부 꽉 차있다면 리스트의 가장 처음 부분에다가 인원을 할당 할 수 있는지 확인함
		if (m_RoomInfoList.begin() != m_RoomInfoList.end())
		{
			// 리스트에 인원을 추가할 수 있다면
			// 방 정보를 pUserEnterRoom 에 넣음
			pUserEnterRoom = m_RoomInfoList.front();
			// 리스트 내에 입장 가능 인원수가 0 이 아닌 방이 존재한다면
			// 해당 방의 입장 가능 인원수를 1 줄인다
			pUserEnterRoom->iMaxUser--;
			int RoomNo = pUserEnterRoom->iRoomNo;
			int ServerNo = pUserEnterRoom->iBattleServerNo;


			// m_RoomInfoList 임계영역 끝
			LeaveCriticalSection(&m_csRoomInfoLock);

			// m_BattleServerInfoMap 임계영역 시작(읽기)
			AcquireSRWLockShared(&m_BattleServerInfoMapSRWLock);

			map<UINT64, st_BATTLE_SERVER_INFO*>::iterator EndIter = m_BattleServerInfoMap.end();
			map<UINT64, st_BATTLE_SERVER_INFO*>::iterator TravelIter = m_BattleServerInfoMap.begin();

			for (; TravelIter != EndIter; ++TravelIter)
			{
				if (TravelIter->second->iServerNo == ServerNo)
					break;
			}

			if (TravelIter == EndIter)
			{
				// 위에서 접속할 방을 찾았으나 그 방에 해당하는 서버가 없다면
				// 방 입장 실패를 보내고 유저가 다시 방을 요청하도록 유도함
				Status = 0;
				*OutSendBuf << Status;

				return;
			}

			st_BATTLE_SERVER_INFO &BattleServer = *TravelIter->second;

			Status = 1;
			*OutSendBuf << Status << ServerNo;
			OutSendBuf->WriteBuffer((char*)BattleServer.ServerIP, sizeof(BattleServer.ServerIP));
			*OutSendBuf << BattleServer.wPort << RoomNo;
			AcquireSRWLockShared(&BattleServer.ConnectTokenSRWLock);
			OutSendBuf->WriteBuffer(BattleServer.ConnectTokenNow, sizeof(BattleServer.ConnectTokenNow));
			AcquireSRWLockShared(&BattleServer.ConnectTokenSRWLock);
			OutSendBuf->WriteBuffer(pUserEnterRoom->EnterToken, sizeof(pUserEnterRoom->EnterToken));
			OutSendBuf->WriteBuffer((char*)BattleServer.ChatServerIP, sizeof(BattleServer.ChatServerIP));
			*OutSendBuf << BattleServer.wChatServerPort;

			// m_BattleServerInfoMap 임계영역 끝  (읽기)
			ReleaseSRWLockShared(&m_BattleServerInfoMapSRWLock);

			return;
		}

		// m_RoomInfoList 임계영역 끝
		LeaveCriticalSection(&m_csRoomInfoLock);
	}

	// 맵이나 리스트에 유저를 넣을 수 있는 적절한 방이 존재하지 않는다면
	// 방 정보 얻기 실패 패킷을 보냄
	Status = 0;
	*OutSendBuf << Status;
}

void CMasterToBattleServer::MatchToMasterEnterRoomSuccess(UINT64 ClientKey, int BattleServerNo, int BattleRoomNo)
{
	// m_RoomInfoList 임계영역 시작
	EnterCriticalSection(&m_csRoomInfoLock);

	st_ROOM_INFO &RoomInfoListFront = *m_RoomInfoList.front();

	if (RoomInfoListFront.UserClientKeySet.find(ClientKey) == RoomInfoListFront.UserClientKeySet.end())
	{
		// m_RoomInfoList 임계영역 끝
		LeaveCriticalSection(&m_csRoomInfoLock);
		
		return;
	}

	// m_RoomInfoList 임계영역 끝
	LeaveCriticalSection(&m_csRoomInfoLock);

	UINT64 MapFindKey = BattleServerNo;
	MapFindKey = MapFindKey << dfSHIFT_RIGHT_SERVERNO;
	MapFindKey += BattleRoomNo;

	// m_PlayStandbyRoomMap 임계영역 시작
	EnterCriticalSection(&m_csPlayStandbyRoomLock);

	map<UINT64, st_ROOM_INFO*>::iterator RoomInfoMapIter = m_PlayStandbyRoomMap.find(MapFindKey);
	
	if (RoomInfoMapIter != m_PlayStandbyRoomMap.end())
	{
		st_ROOM_INFO &ClientKeySetIter = *RoomInfoMapIter->second;

		if (ClientKeySetIter.UserClientKeySet.find(ClientKey) != ClientKeySetIter.UserClientKeySet.end())
		{
			// m_PlayStandbyRoomMap 임계영역 끝
			LeaveCriticalSection(&m_csPlayStandbyRoomLock);
			return;
		}
	}

	// m_PlayStandbyRoomMap 임계영역 끝
	LeaveCriticalSection(&m_csPlayStandbyRoomLock);
}

void CMasterToBattleServer::MatchToMasterEnterRoomFail(UINT64 ClientKey, UINT64 BattleRoomInfo)
{
	// m_pMasterToBattleServer 의 m_RoomInfoList 임계영역 시작
	EnterCriticalSection(&m_csRoomInfoLock);

	st_ROOM_INFO &ListIter = *m_RoomInfoList.front();

	if (ListIter.iRoomNo == (int)BattleRoomInfo)
	{
		// 해당 클라이언트 키를 가지고 있는 노드를 지움
		if (ListIter.UserClientKeySet.erase(ClientKey) != 0)
		{
			// 클라이언트 키 셋에서 지우는 것을 성공하였다면
			// 해당 방의 접속 가능 인원을 1 증가시킴
			ListIter.iMaxUser++;
		}

		// m_pMasterToBattleServer 의 m_RoomInfoList 임계영역 끝
		LeaveCriticalSection(&m_csRoomInfoLock);
		return;
	}

	// m_pMasterToBattleServer 의 m_RoomInfoList 임계영역 끝
	LeaveCriticalSection(&m_csRoomInfoLock);


	// m_pMasterToBattleServer 의 m_BattleServerInfoMap 임계영역 시작
	EnterCriticalSection(&m_csPlayStandbyRoomLock);

	map<UINT64, CMasterToBattleServer::st_ROOM_INFO*>::iterator RoomInfoIter = m_PlayStandbyRoomMap.find(BattleRoomInfo);
	
	if (RoomInfoIter != m_PlayStandbyRoomMap.end())
	{
		// 해당 클라이언트 키를 가지고 있는 노드를 지움
		if (RoomInfoIter->second->UserClientKeySet.erase(ClientKey) != 0)
		{
			// 클라이언트 키 셋에서 지우는 것을 성공하였다면
			// 해당 방의 접속 가능 인원을 1 증가시킴
			RoomInfoIter->second->iMaxUser++;
		}

		// m_pMasterToBattleServer 의 m_BattleServerInfoMap 임계영역 끝
		LeaveCriticalSection(&m_csPlayStandbyRoomLock);
		return;
	}

	// m_pMasterToBattleServer 의 m_BattleServerInfoMap 임계영역 끝
	LeaveCriticalSection(&m_csPlayStandbyRoomLock);
}

int CMasterToBattleServer::GetRoomInfoMemoryPoolChunkAllocSize()
{
	return m_pRoomInfoMemoryPool->GetAllocChunkCount();
}

int CMasterToBattleServer::GetRoomInfoMemoryPoolChunkUseSize()
{
	return m_pRoomInfoMemoryPool->GetUseChunkCount();
}

int CMasterToBattleServer::GetRoomInfoMemoryPoolBufUseSize()
{
	return m_pRoomInfoMemoryPool->GetUseNodeCount();
}

int CMasterToBattleServer::GetBattleStanbyRoomSize()
{
	return (int)(m_PlayStandbyRoomMap.size() + m_RoomInfoList.size());
}