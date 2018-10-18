<?php

    // 해당 php를 require 하기 이전에 
    // 외부에서 $OpenDB를 선언하여 
    // 해당 변수를 사용할 DB의 이름으로 초기화 해줘야 함

    $db_host = '127.0.0.1';
    $db_user = 'root';
    $db_password = 'zxcv123';
    $db_name = $g_DB_connect;
    $db_port = '3306';

    // $DB = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);
    // if(!$DB)
    // {
    //     echo mysqli_connect_error().'<BR>';
    //     return false;
    // }
    
    $g_DB_connect = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);

    include $_SERVER['DOCUMENT_ROOT']."/Log/Client__lib_profiling.php";
    $PF = Profiling::getInstance("http://127.0.0.1//Log/LogProfiling.php", $_SERVER['PHP_SELF']);
    include_once $_SERVER['DOCUMENT_ROOT']."/Log/Client__lib_log.php";
    $GameLog = GAMELog::getInstance($cnf_GAME_LOG_URL);

    $retval = mysqli_query($g_DB_connect, "select * from serverinfo_tbl");
    $versionInfo = mysqli_fetch_array($retval, MYSQLI_ASSOC);

    $versionMajor = $versionInfo["major"];
    $versionMinor = $versionInfo["minor"];
?>