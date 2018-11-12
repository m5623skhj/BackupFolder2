<?php

    // ---------------------------------------------------------------------
    // DB 컨넥트 관련을 전부 여기에 몰아 넣음
    // 외부에서는 함수만 호출하도록 함
    // ---------------------------------------------------------------------

    include $_SERVER['DOCUMENT_ROOT'].'/phplib/DBlib.php';

    function DataDB_Connect()
    {
        global $g_DB_connect;
        global $DBInfoRow;

        $db_host = $DBInfoRow['ip'];
        $db_user = $DBInfoRow['id'];
        $db_password = $DBInfoRow['pass'];
        $db_name = $DBInfoRow['dbname'];
        $db_port = $DBInfoRow['port'];

        // 해당 DB에 연결함
        $g_DB_connect = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);
        if(!$g_DB_connect)
        {
            // 다른곳에다가 로그를 보관해야되는데
            // echo 'Error In Connect '.mysqli_connect_error();

            $txt = mysqli_connect_error().'\n'.mysqli_connect_errno().'\n';
            $file = fopen("connecterr.txt", "w");

            fwrite($file, $txt);
            fclose($file);

            return false;
        }
        // 문자셋 셋팅
        mysqli_set_charset($g_DB_connect, 'utf8');

        return true;
    }

    function IndexDB_Connect()
    {
        global $g_DB_connect;

        $db_host = "127.0.0.1";
        $db_user = "root";
        $db_password = "zxcv123";
        $db_name = "shdb_index";
        $db_port = 3306;

        // 해당 DB에 연결함
        $g_DB_connect = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);
        if(!$g_DB_connect)
        {
            // 다른곳에다가 로그를 보관해야되는데
            // echo 'Error In Connect '.mysqli_connect_error();

            $txt = mysqli_connect_error().'\n'.mysqli_connect_errno().'\n';
            $file = fopen("connecterr.txt", "w");

            fwrite($file, $txt);
            fclose($file);

            return false;
        }
        // 문자셋 셋팅
        mysqli_set_charset($g_DB_connect, 'utf8');

        return true;
    }

    function InfoDB_Connect()
    {
        global $g_DB_connect;

        $db_host = "127.0.0.1";
        $db_user = "root";
        $db_password = "zxcv123";
        $db_name = "shdb_info";
        $db_port = 3306;

        // 해당 DB에 연결함
        $g_DB_connect = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);
        if(!$g_DB_connect)
        {
            // 다른곳에다가 로그를 보관해야되는데
            // echo 'Error In Connect '.mysqli_connect_error();

            $txt = mysqli_connect_error().'\n'.mysqli_connect_errno().'\n';
            $file = fopen("connecterr.txt", "w");

            fwrite($file, $txt);
            fclose($file);

            return false;
        }
        // 문자셋 셋팅
        mysqli_set_charset($g_DB_connect, 'utf8');

        return true;
    }

    function MatchmakingDB_Connect()
    {
        global $g_DB_connect;

        $db_host = "127.0.0.1";
        $db_user = "root";
        $db_password = "zxcv123";
        $db_name = "matchmaking_status";
        $db_port = 3306;

        // 해당 DB에 연결함
        $g_DB_connect = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);
        if(!$g_DB_connect)
        {
            // 다른곳에다가 로그를 보관해야되는데
            // echo 'Error In Connect '.mysqli_connect_error();

            $txt = mysqli_connect_error().'\n'.mysqli_connect_errno().'\n';
            $file = fopen("connecterr.txt", "w");

            fwrite($file, $txt);
            fclose($file);

            return false;
        }
        // 문자셋 셋팅
        mysqli_set_charset($g_DB_connect, 'utf8');

        return true;
    }

?>