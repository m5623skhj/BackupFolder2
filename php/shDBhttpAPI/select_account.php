<?php

    // ---------------------------------------------------------------------
    // accountnot 혹은 email 을 기준으로 회원 데이터를 얻어온다
    // 해당 회원의 모든 데이터를 JSON 으로 얻어온다
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
   
    if( !isset($ParsingContents) )
    {
        LOG_System(0, 'select_account', 'Json Decode Parameter Error', 3);
        $ResultArray = array('result'=>'-100');
        echo json_encode($ResultArray);
        exit;
    }

    $ColmnKey = key($ParsingContents);
    $UserData = $ParsingContents[$ColmnKey];

    // include $_SERVER['DOCUMENT_ROOT'].'shDBhttpAPI/ConnectDB.php';
    // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectIndexDB.php';

    $g_DB_connect;

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(IndexDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'select_account', "shdb_info DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 얻어온 정보를 사용하여 해당 유저가 어느 db 에 있는지에 대하여 쿼리를 보냄
    $Query = "SELECT `accountno`, `dbno` FROM allocate WHERE `$ColmnKey`='$UserData'";
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'select_account.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'select_account.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'select_account.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    }

    $row = mysqli_fetch_assoc($QueryResult);
    if($row === NULL)
    {
        // 회원가입 안돼있음 -10
        DB_Disconnect();
        LOG_System(0, 'select_account.php', "Account Is Null $ColmnKey = $UserData, ".__LINE__);
        $ResultArray = array('result'=>'-10');
        echo json_encode($ResultArray);
        exit;
    }
    $accountno = $row['accountno'];
    $DBNo = $row['dbno'];
    mysqli_free_result($QueryResult);

    // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectInfoDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(InfoDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'select_account.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }
    
    // 얻어온 DBNo 로 shdb_info 에 연결하여 정보를 가져옴
    // DBNo 에 해당하는 DB 정보를 알아내기 위하여 쿼리를 보냄
    $Query = 'SELECT ip, port, id, pass, dbname FROM dbconnect WHERE dbno = '.$DBNo;
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'select_account.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'select_account.php', 'Table Error : dbconnect, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'select_account.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }        
    }
    
    $DBInfoRow = mysqli_fetch_assoc($QueryResult);
    if($DBInfoRow == NULL)
    {
        LOG_System(0, 'select_account.php', "Error : $err, ".__LINE__);
        $ResultArray = array('result'=>'-1000');
        echo json_encode($ResultArray);
        exit;
    }
  
    mysqli_free_result($QueryResult);
    DB_Disconnect();

    // 얻어온 DBNo 로 해당 DB 에 연결
    // require_once $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectDataDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(DataDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'select_account.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 연결된 DB 에서 처음에 얻어온 정보를 가지고
    // 해당 회원의 모든 데이터를 사용자에게 보냄
    $Query = 'SELECT * FROM account WHERE `accountno`='.$accountno;
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'update_contents.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'update_contents.php', 'Table Error : dbconnect, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'update_contents.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }    
    }

    $row = mysqli_fetch_assoc($QueryResult);
    mysqli_free_result($QueryResult);
    // 해당 테이블에 정보가 존재하지 않으면 index 테이블에서 accountno 와 email 정보를 얻어서
    // 해당 테이블에 다시 세팅하고 계속 진행함
    if($row === NULL)
    {
        DB_Disconnect();
        // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectIndexDB.php';
    
        // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
        if(IndexDB_Connect() === false)
        {
            $connectError = mysqli_connect_errno();
            LOG_System(0, 'select_account.php', "DB Connect Error, $connectError, ".__LINE__, 3);
            $ResultArray = array('result'=>'-50');
            echo json_encode($ResultArray);
            exit;
        }

        // 얻어온 정보를 사용하여 해당 유저가 어느 db 에 있는지에 대하여 쿼리를 보냄
        $Query = 'SELECT `accountno`, `email` FROM allocate WHERE `accountno`='.$accountno;
        $QueryResult = DB_SendQuery($Query);
        if($QueryResult === false)
        {
            $err = mysqli_errno($g_DB_connect);
            DB_Disconnect();
            // DB 쿼리 에러 -60
            if($err == 1064)
            {
                LOG_System(0, 'select_account.php', "QueryError : $Query, ".__LINE__);
                $ResultArray = array('result'=>'-60');
                echo json_encode($ResultArray);
                exit;
            }
            // 테이블 오류 -62
            else if($err == 1146)
            {
                LOG_System(0, 'select_account.php', 'Table Error : available, '.__LINE__);
                $ResultArray = array('result'=>'-62');
                echo json_encode($ResultArray);
                exit;
            }
            else
            {
                LOG_System(0, 'select_account.php', "Error : $err, ".__LINE__);
                $ResultArray = array('result'=>'-1000');
                echo json_encode($ResultArray);
                exit;
            }  
        }

        $row = mysqli_fetch_assoc($QueryResult);
        $accountno = $row['accountno'];
        $email = $row['email'];
        mysqli_free_result($QueryResult);
        DB_Disconnect();

        // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectDataDB.php';

        // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
        if(DataDB_Connect() === false)
        {
            $connectError = mysqli_connect_errno();
            LOG_System(0, 'select_account.php', "DB Connect Error, $connectError, ".__LINE__, 3);
            $ResultArray = array('result'=>'-50');
            echo json_encode($ResultArray);
            exit;
        }

        // shdb_data_dbno 의 account 테이블과
        // 해당 계정의 accountno 로 insert 하여 공간을 미리 확보함
        $Query = "INSERT INTO account (accountno, email) VALUES ($accountno, '$email')";
        $QueryResult = DB_SendQuery($Query);
        if($QueryResult === false)
        {
            $err = mysqli_errno($g_DB_connect);
            DB_Disconnect();
            // DB 데이터 삽입 에러 -3 (중복 가입)
            if($err == 1062)
            {
                LOG_System(0, 'select_account.php', "Duplicate Insert Email '$email', $DBNo, ".__LINE__, 3);
                $ResultArray = array('result'=>'-3');
                echo json_encode($ResultArray);
                exit;         
            }
            // DB 쿼리 에러 -60
            else if($err == 1064)
            {
                LOG_System(0, 'select_account.php', "QueryError : $Query, ".__LINE__);
                $ResultArray = array('result'=>'-60');
                echo json_encode($ResultArray);
                exit;
            }
            // 테이블 오류 -62
            else if($err == 1146)
            {
                LOG_System(0, 'select_account.php', 'Table Error : available, '.__LINE__);
                $ResultArray = array('result'=>'-62');
                echo json_encode($ResultArray);
                exit;
            }
            else
            {
                LOG_System(0, 'select_account.php', "Error : $err, ".__LINE__);
                $ResultArray = array('result'=>'-1000');
                echo json_encode($ResultArray);
                exit;
            }
        }
    }

    DB_Disconnect();

    // result code 를 넣기 위하여
    // 연관배열을 새로 할당함
    $ResultArray = array('result'=>'1') + $row;

    // 최종결과 송신
    // result code
    // 얻어온 유저의 계정 정보
    echo json_encode($ResultArray);
    
    $PF->stopCheck(PF_PAGE, 'Total Page');
    $PF->LOG_Save();
?>