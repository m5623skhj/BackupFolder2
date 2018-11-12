<?php

    // ---------------------------------------------------------------------
    // 사용자는 accountno 혹은 email 과 sessionkey 를 가지고 오며
    // Lobby Server 에서는 sessionkey 를 조회하여 인증을 확인함
    // matchmaking_status DB 의 server 정보를 활용하여
    // 사용자가 가장 적고 TIMEOUT 시간 이내에 갱신된 서버를 결과로 줌
    // ---------------------------------------------------------------------

    include $_SERVER['DOCUMENT_ROOT'].'/Log/Client__Config_log.php';
    global $cnf_PROFILING_LOG_URL;
    global $cnf_PROFILING_LOG_RATE;

    include_once $_SERVER['DOCUMENT_ROOT'].'/Log/Client__lib_profiling.php';
    $PF = Profiling::getInstance($cnf_PROFILING_LOG_URL, $_SERVER['PHP_SELF'], $cnf_PROFILING_LOG_RATE);
    include_once $_SERVER['DOCUMENT_ROOT'].'/Log/Client__lib_log.php';
    $GameLog = GAMELog::getInstance($cnf_GAME_LOG_URL);

    $PF->startCheck(PF_PAGE);
   
    $ParsingContents = json_decode(file_get_contents('php://input'), true);
    include $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/shDBStartup.php';

    if($ParsingContents === NULL)
    {
        LOG_System(0, 'update_account.php', "json_decode return NULL, ".__LINE__, 3);
        $ResultArray = array('result'=>'-1000');
        echo json_encode($ResultArray);
        exit;
    }

    $InputDataArray = array();
    $InputCoulmnArray = array();
    $CoulmnNumber = 0;
    while($NowData = current($ParsingContents))
    {
        $InputCoulmnArray[$CoulmnNumber] = key($ParsingContents);
        $InputDataArray[$InputCoulmnArray[$CoulmnNumber]] = $NowData;
        next($ParsingContents);
        $CoulmnNumber++;
    }

    if( ( !isset($InputDataArray['accountno']) && !isset($InputDataArray['email']) )
    || !isset($InputDataArray['sessionkey']))
    {
        LOG_System(0, 'get_matchmaking.php', 'Error : User Info Input Error, '.__LINE__);
        $ResultArray = array('result'=>'-100');
        echo json_encode($ResultArray);
        exit;
    }

    include_once $_SERVER['DOCUMENT_ROOT'].'/phplib/curl_http.php';

    $SendPostField  = array($InputCoulmnArray[0]=>$InputDataArray[$InputCoulmnArray[0]]);
    $Response = http_post('http://127.0.0.1/shDBhttpAPI/select_account.php', json_encode($SendPostField));

    $AccountData = json_decode($Response['body'], true);

    // 요청한 php 파일에서 오류가 발생 하였는지를 확인함
    if($Response['code'] != 200)
    {
        // API 서버 HTTP 에러 -110
        LOG_System(0, 'get_matchmaking.php', 'Error : APIServer Errror '.$Response['code'].' '.__LINE__);
        $ResultArray = array('result'=>'-110');
        echo json_encode($ResultArray);
        exit;
    }
    // 정상적으로 php 응답을 받았지만, select_account.php 에서 
    // result code 가 1(정상 응답) 이 아닌지를 확인함
    if($AccountData['result'] != 1)
    {
        // 내부에서 에러를 남겼을 것이므로 추가적인 에러 처리를 하지 않고
        // 사용자에게 에러 상황만을 보냄
        // LOG_System(0, 'get_matchmaking.php', 'Error : APIServer Error, '.__LINE__);
        $ResultArray = array('result'=>$AccountData['result']);
        echo json_encode($ResultArray);
        exit;
    }
    
    // 유저가 보낸 세션키와 실제로 저장되어있던 세션키를 비교함
    if(strcmp($InputDataArray['sessionkey'], $AccountData['sessionkey']) != 0)
    {
        // 세션키 인증 오류
        LOG_System(0, 'get_matchmaking.php', 'Error : SessionKey Error, '.__LINE__);
        $ResultArray = array('result'=>'-3');
        echo json_encode($ResultArray);
        exit;
    }

    $g_DB_connect;

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(MatchmakingDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'get_matchmaking.php', "matchmaking_status DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 매치메이킹 서버에 사용자가 가장 적고 TIMEOUT 시간 이내에 갱신된 서버를 얻어옴
    $Query = 'SELECT `serverno`, `ip`, `port` FROM matchmaking_status.server WHERE (timestampdiff(second, `heartbeat`, NOW()) < 5) ORDER BY `connectuser` ASC LIMIT 1';
    // $Query = 'SELECT `serverno`, `ip`, `port` FROM matchmaking_status.server WHERE (timestampdiff(second, `heartbeat`, NOW()) < 5)';
    // $Query = 'SELECT * FROM matchmaking_status.server WHERE (timestampdiff(second, `heartbeat`, NOW()) < 5)';
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'get_matchmaking.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'get_matchmaking.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'get_matchmaking.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        } 
    }

    $row = mysqli_fetch_assoc($QueryResult);
    if($row === NULL)
    {
        DB_Disconnect();
        LOG_System(0, 'get_matchmaking.php', "Error : No Available Server, $Query, ".__LINE__);
        $ResultArray = array('result'=>'-4');
        echo json_encode($ResultArray);
        exit;
    }
    mysqli_free_result($QueryResult);

    DB_Disconnect();

    // result code 를 넣기 위하여
    // 연관배열을 새로 할당함
    $ResultArray = array('result'=>'1') + $row;

    // 최종결과 송신
    // result code
    // serverno
    // ip
    // port
    echo json_encode($ResultArray);
    
    $PF->stopCheck(PF_PAGE, 'Total Page');
    $PF->LOG_Save();
?>