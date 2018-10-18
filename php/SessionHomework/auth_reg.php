<?php
    // 신규가입
    // Input : id / password / nickname
    // Output : resultcode / accountno
    $g_DB_connect = 'sessiondb';
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBStart.php';

    $content = file_get_contents("php://input");
    if($content === false)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Auth_Reg : File Get err');
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        $Response['AccountNo'] = 0;
        $Response['ResultCode'] = 0;
        echo json_encode($Response);
        return;
    }
    $contentArray = explode("\r\n", $content);

    // if(isset($_GET['ID']) === false || isset($_GET['password']) === false || isset($_GET['nickname']) === false)
    // {
    //     echo 'auth_reg pram err<BR>';
    //     return;
    // }

    include $_SERVER['DOCUMENT_ROOT']."/phplib/DBlib.php";
    if(!VersionCheck($contentArray[0], $contentArray[1], $versionMajor, $versionMinor))
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Auth_Reg : Version err');
        $Response['AccountNo'] = 0;
        $Response['ResultCode'] = 0;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }
    $Body = $contentArray[3];
    $BodyArray = json_decode($Body, true);

    $userId = mysqli_real_escape_string($g_DB_connect, $BodyArray['ID']);
    $password = mysqli_real_escape_string($g_DB_connect, $BodyArray['password']);
    $nickname = mysqli_real_escape_string($g_DB_connect, $BodyArray['nickname']);

    $QueryArr = array(
        "INSERT INTO account_tbl (userid, password, nickname) VALUES ('{$userId}', '{$password}', '{$nickname}')"
        );

    $accountno = DB_SendTransactionQuery($QueryArr);
    if($accountno === null)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Same UserID Or Nickname err');
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        $Response['AccountNo'] = 0;
        $Response['ResultCode'] = 0;
        echo json_encode($Response);
        return;       
    }

    $QueryArr = array(
        'insert into login_tbl (accountno, time, ip, logincount) values(\''.$accountno.'\', \''.date('Y-m-d', time()).'\', \''.$_SERVER['REMOTE_ADDR'].'\', 0)',
        'insert into session_tbl (accountno, sessionkey, publishtime) values('.$accountno.', 0, 0)',
        'insert into score_tbl (accountno, score) values('.$accountno.', 0)'
    );
    $retval = DB_SendTransactionQuery($QueryArr);
    if($retval === false)
    {
        $GameLog->AddLog(0, 0, 0, 0, 0, 0, 0, 'Auth_Reg Create Account err');
        $Response['AccountNo'] = 0;
        $Response['ResultCode'] = 0;
        echo json_encode($Response);
        require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
        return;
    }

    $Response['AccountNo'] = $accountno;
    $Response['ResultCode'] = 1;

    echo json_encode($Response);
    require_once $_SERVER['DOCUMENT_ROOT'].'/DBEnd.php'; 
?>