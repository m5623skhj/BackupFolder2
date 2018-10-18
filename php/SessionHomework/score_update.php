<?php
    // 세션키가 유효한지 확인한 후 내 스코어를 갱신함
    // Input : accountno, sessionkey, New Score
    // Output : resultcode

    $g_DB_connect = 'sessiondb';
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBStart.php';

    $content = file_get_contents("php://input");
    if($content === false)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Score_Update : File Get err');
        $Response['ResultCode'] = 0;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    $contentArray = explode("\r\n", $content);

    include $_SERVER['DOCUMENT_ROOT']."/phplib/DBlib.php";
    if(!VersionCheck($contentArray[0], $contentArray[1], $versionMajor, $versionMinor))
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Score_Update : Version err');
        $Response['ResultCode'] = 0;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    $Body = $contentArray[3];
    $BodyArray = json_decode($Body, true);

    // if(isset($_GET['accountno']) === false || isset($_GET['sessionkey']) === false || isset($_GET['new_score']) === false)
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
    $NewScore = mysqli_real_escape_string($g_DB_connect, $BodyArray['new_score']);

    $Query = "select publishtime from session_tbl where accountno = '{$AccountNo}' AND sessionkey = '{$InputSessionKey}'";
    $retval = DB_SendQuery($Query);

    $account = mysqli_fetch_array($retval, MYSQLI_ASSOC);
    mysqli_free_result($retval);

    $publishtime = $account['publishtime'];

    if($publishtime < strtotime(date('Y-m-d h:i', time())))
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Score_Update : Input Expire Session Key err');
        $ResultCode = 0;
        $AccountNo = 0;
    }
    else
    {
        $ResultCode = 1;

        $Query = "UPDATE score_tbl SET score = '{$NewScore}' WHERE accountno = '{$AccountNo}'";
        DB_SendQuery($Query);
    }

    DB_Disconnect();

    $Response['ResultCode'] = $ResultCode;

    echo json_encode($Response);
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
?>