<?php
    // 세션키가 유효한지 확인한 후 내 스코어를 얻음
    // Input : accountno, sessionkey
    // Output : resultcode, score

    $g_DB_connect = 'sessiondb';
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBStart.php';

    $content = file_get_contents("php://input");
    if($content === false)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Score_Get : File Get err');
        $Response['ResultCode'] = 0;
        $Response['SessionKey'] = NULL;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    $contentArray = explode("\r\n", $content);

    include $_SERVER['DOCUMENT_ROOT']."/phplib/DBlib.php";
    if(!VersionCheck($contentArray[0], $contentArray[1], $versionMajor, $versionMinor))
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Score_Get : Version err');
        $Response['ResultCode'] = 0;
        $Response['SessionKey'] = NULL;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    // if($contentArray[0] < $versionMajor)
    // {
    //     $GameLog->AddLog($accountno, 0, 0, 0, 0, 0, 0, 'Score_get : Major Version err');
    //     $GameLog->SaveLog();
    //     $Response['ResultCode'] = $ResultCode;

    //     echo json_encode($Response);
    //     return;
    // }
    // else if($contentArray[1] < $versionMinor)
    // {
    //     $GameLog->AddLog($accountno, 0, 0, 0, 0, 0, 0, 'Score_get : Minor Version err');
    //     $GameLog->SaveLog();
    //     $Response['ResultCode'] = $ResultCode;

    //     echo json_encode($Response);
    //     return;       
    // }
    $Body = $contentArray[3];
    $BodyArray = json_decode($Body, true);

    // if(isset($_GET['accountno']) === false || isset($_GET['sessionkey']) === false)
    // {
    //     echo 'auth_login pram err<BR>';
    //     return;
    // }

    // $g_DB_connect = 'sessiondb';
    // $DBIsOpen = require_once 'C:\AutoSet10\public_html\DBStart.php';
    // if($DBIsOpen === false)
    // {
    //     echo 'DBStart Error<BR>';
    //     return;
    // }

    $AccountNo = mysqli_real_escape_string($g_DB_connect, $BodyArray['accountno']);
    $InputSessionKey = mysqli_real_escape_string($g_DB_connect, $BodyArray['sessionkey']);

    $Query = "select publishtime from session_tbl where accountno = '{$AccountNo}' AND sessionkey = '{$InputSessionKey}'";
    $retval = DB_SendQuery($Query);

    $account = mysqli_fetch_array($retval, MYSQLI_ASSOC); // 컬럼명으로 불러옴
    mysqli_free_result($retval);

    $publishtime = $account['publishtime'];
    $Score;

    if($publishtime < strtotime(date('Y-m-d h:i', time())))
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Score_Get : Input Expire Session Key err');
        $ResultCode = 0;
        $Score = 0;
        $AccountNo = 0;
    }
    else
    {
        $ResultCode = 1;

        $Query = "select score from score_tbl where accountno = '{$AccountNo}'";
        $retval = DB_SendQuery($Query);
        $account = mysqli_fetch_array($retval, MYSQLI_ASSOC);
        mysqli_free_result($retval);

        $Score = $account['score'];
    }

    DB_Disconnect();

    $Response['ResultCode'] = $ResultCode;
    $Response['SessionKey'] = $Score;

    echo json_encode($Response);
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
?>