<?php
    // 로그인 및 세션키 발급
    // Input : id / password
    // Output : resultcode / accountno / sessionkey

    // if(isset($_GET['ID']) === false || isset($_GET['password']) === false)
    // {
    //     echo 'auth_login pram err<BR>';
    //     return;
    // }

    $content = file_get_contents("php://input");
    if($content === false)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Auth_Login : File Get err');
        $Response['AccountNo'] = 0;
        $Response['ResultCode'] = 0;
        $Response['SessionKey'] = 0;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    $contentArray = explode("\r\n", $content);

    // $DBIsOpen = require_once $_SERVER['DOCUMENT_ROOT'].'\DBStart.php';
    // if($DBIsOpen === false)
    // {
    //     echo 'DBStart Error<BR>';
    //     return;
    // }
    $g_DB_connect = 'sessiondb';
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBStart.php';

    include $_SERVER['DOCUMENT_ROOT']."\phplib\DBlib.php";
    if(!VersionCheck($contentArray[0], $contentArray[1], $versionMajor, $versionMinor))
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Auth_Login : Version err');
        $Response['AccountNo'] = 0;
        $Response['ResultCode'] = 0;
        $Response['SessionKey'] = 0;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    $Body = $contentArray[3];
    $BodyArray = json_decode($Body, true);

    $AccountNo = mysqli_real_escape_string($g_DB_connect, $account['accountno']);
    $userId = mysqli_real_escape_string($g_DB_connect, $BodyArray['ID']);
    $password = mysqli_real_escape_string($g_DB_connect, $BodyArray['password']);

    $Query = "select accountno from account_tbl where userid = '{$userId}' AND password = '{$password}'";
    $retval = DB_SendQuery($Query);

    $account = mysqli_fetch_array($retval, MYSQLI_ASSOC);
    mysqli_free_result($retval);

    $SessionKey = "";
    $Query = "";
    include $_SERVER['DOCUMENT_ROOT']."\SessionHomework\auth_session.php";

    if($account === NULL)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Auth_Login : Account err');
        $AccountNo = 0;
        $ResultCode = 0;
    }
    else
    {
        $ResultCode = 1;

        $SessionKey = MakeNewSessionKey($AccountNo);
        $Query = array(
            "insert into session_tbl (accountno, sessionkey, publishtime values('{$AccountNo}',
            UNIX_TIMESTAMP(NOW())) on duplicate key update sessionkey = '{$SessionKey}', publishtime = UNIX_TIMESTAMP(NOW())",
            "insert into login_tbl (accountno, time, ip, count) values('{$AccountNo}', UNIX_TIMESTAMP(NOW()), '{$_SERVER['REMOTE_ADDR']}', 1 on duplicate key update time 
            = UNIX_TIMESTAMP(NOW()), ip = '{$_SERVER['REMOTE_ADDR']}', count = count + 1"
        );
    }

    if($ResultCode === 1)
    {
        DB_SendTransactionQuery($Query);
    }
    DB_Disconnect();

    $Response['AccountNo'] = $AccountNo;
    $Response['ResultCode'] = $ResultCode;
    $Response['SessionKey'] = $SessionKey;

    echo json_encode($Response);
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
?>