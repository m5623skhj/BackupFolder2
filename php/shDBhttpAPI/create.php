<?php

    // ---------------------------------------------------------------------
    // email 을 기준으로 새로운 accountno 와 dbno 를 할당 받음
    // 할당 받은 db 의 account / contents 테이블에 해당 accountno 로 insert 하여 공간까지 확보함
    // ---------------------------------------------------------------------

    include $_SERVER['DOCUMENT_ROOT'].'/Log/Client__Config_log.php';
    global $cnf_PROFILING_LOG_URL;
    global $cnf_PROFILING_LOG_RATE;

    include_once $_SERVER['DOCUMENT_ROOT'].'/Log/Client__lib_profiling.php';
    $PF = Profiling::getInstance($cnf_PROFILING_LOG_URL, $_SERVER['PHP_SELF'], $cnf_PROFILING_LOG_RATE);
    include_once $_SERVER['DOCUMENT_ROOT'].'/Log/Client__lib_log.php';
    $GameLog = GAMELog::getInstance($cnf_GAME_LOG_URL);
 
    $PF->startCheck(PF_PAGE);
   
    $JsonDecodedContents = json_decode(file_get_contents('php://input'), true);
    $Email = $JsonDecodedContents['email'];

    if ( !isset($Email) )	
    {
        LOG_System(0, 'create.php', 'Parameter Error, '.__LINE__, 3);
        $ResultArray = array('result'=>'-100');
        echo json_encode($ResultArray);
        exit;
    }

    if(!filter_var($Email, FILTER_VALIDATE_EMAIL))
    {
        LOG_System(0, 'create.php', 'Input Data Is Not Email Form, '.__LINE__, 3);
        $ResultArray = array('result'=>'-1000');
        echo json_encode($ResultArray);
        exit;
    }

    // shdb_index 의 allocate 테이블에서
    // 해당 email 이 이전에 가입되었는지를 확인하기 위하여
    // email 을 SELECT 문으로 확인함

    include $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/shDBStartup.php';
    $g_DB_connect;

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(IndexDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'create.php', 'DB Connect Error, $connectError, '.__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // shdb_index 테이블에서 해당 email 이 가입 되었는지를 확인함
    $Query = "SELECT accountno FROM allocate WHERE email='$Email'";
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    } 

    $row = mysqli_fetch_row($QueryResult);
    if($row != NULL)
    {
        // 이미 가입된 이메일임
        DB_Disconnect();
        $ResultArray = array('result'=>'-1');
        echo json_encode($ResultArray);
        exit;
    }
    
    mysqli_free_result($QueryResult);

    DB_Disconnect();

    // shdb_info 의 available 테이블에서
    // 어느 DB가 널널한지 확인하고
    // 그 해당 db의 available 값을 1 감소시킴
    // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectInfoDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(InfoDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'create.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // shdb_info 테이블에서 available 값이 가장 큰 컬럼을 가져옴
    $Query = 'SELECT * FROM available ORDER BY available desc limit 1';
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    }

    $row = array();
    $row = mysqli_fetch_row($QueryResult);
    if($row[0] === NULL || $row[1] === NULL)
    {
        DB_Disconnect();
        LOG_System(0, 'create.php', 'Error : row is NULL, '.__LINE__);
        $ResultArray = array('result'=>'-1000');
        echo json_encode($ResultArray);
        exit;
    }
    $DBNo = $row[0];
    $Available = $row[1];
    mysqli_free_result($QueryResult);

    DB_Disconnect();

    // shdb_index DB 로 연결함
    // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectIndexDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(IndexDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'create.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // allocate 테이블에 사용자의 email 과 DBNo 를 삽입함
    $Query = "INSERT INTO allocate (email, dbno) VALUES ('$Email', $DBNo)";
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 데이터 삽입 에러 -3 (중복 가입)
        if($err == 1062)
        {
            LOG_System(0, 'create.php', "Duplicate Insert Email '$Email', $DBNo, ".__LINE__);
            $ResultArray = array('result'=>'-3');
            echo json_encode($ResultArray);
            exit;         
        }
        // DB 쿼리 에러 -60
        else if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : allocate, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    }

    /////////////////////////////////////////////////////
    // 이후 replication 분산이 있을 때는
    // 읽기 전용 DB 아이디로 다시 접근해야 될 듯 함
    /////////////////////////////////////////////////////

    // 사용자에게 결과로 accountno 를 알려주기 위하여
    // SELECT 쿼리를 보냄
    $Query = "SELECT accountno FROM allocate WHERE email='$Email'";
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {        
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : allocate, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    }

    $row = mysqli_fetch_row($QueryResult);
    if($row === NULL)
    {
        DB_Disconnect();
        LOG_System(0, 'create.php', 'Error : row is NULL, '.__LINE__);
        $ResultArray = array('result'=>'-1000');
        echo json_encode($ResultArray);
        exit;
    }
    $accountno = $row[0];
    mysqli_free_result($QueryResult);

    DB_Disconnect();

    // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectInfoDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(InfoDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'create.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 가입이 완료되었으면 shdb_info 테이블에
    // 원래 가지고 있던 값에서 1 을 차감함
    $Query = 'UPDATE available SET available = available - 1 WHERE dbno = '.$DBNo;
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 쿼리 에러 -60
        if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 존재하지 않는 컬럼 update -61
        else if($err == 1054)
        {
            LOG_System(0, 'create.php', "Column Update Error $err, ".__LINE__);
            $ResultArray = array('result'=>'-61');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }        
    }
    
    /////////////////////////////////////////////////////
    // 이후 replication 분산이 있을 때는
    // 읽기 전용 DB 아이디로 다시 접근해야 될 듯 함
    /////////////////////////////////////////////////////

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
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }        
    }

    $DBInfoRow = mysqli_fetch_assoc($QueryResult);
    if($DBInfoRow == NULL)
    {
        DB_Disconnect();
        LOG_System(0, 'create.php', 'Error : row is NULL, '.__LINE__);
        $ResultArray = array('result'=>'-1000');
        echo json_encode($ResultArray);
        exit;
    }
    
    mysqli_free_result($QueryResult);
    DB_Disconnect();

    // shdb_data_DBNo 에 연결
    // require_once $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectDataDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(DataDB_Connect() === false)
    {
        $err = mysqli_errno($g_DB_connect);
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'create.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // shdb_data_dbno 의 account 테이블과
    // 해당 계정의 accountno 로 insert 하여 공간을 미리 확보함
    $Query = "INSERT INTO account (accountno, email) VALUES ($accountno, '$Email')";
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 데이터 삽입 에러 -3 (중복 가입)
        if($err == 1062)
        {
            LOG_System(0, 'create.php', "Duplicate Insert Email '$Email', $DBNo, ".__LINE__, 3);
            $ResultArray = array('result'=>'-3');
            echo json_encode($ResultArray);
            exit;         
        }
        // DB 쿼리 에러 -60
        else if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    }

    // shdb_data_dbno 의 contents 테이블에
    // 해당 계정의 contents 로 insert 하여 공간을 미리 확보함
    $Query = "INSERT INTO contents (accountno) VALUES ($accountno)";
    $QueryResult = DB_SendQuery($Query);
    if($QueryResult === false)
    {        
        $err = mysqli_errno($g_DB_connect);
        DB_Disconnect();
        // DB 데이터 삽입 에러 -3 (중복 가입)
        if($err == 1062)
        {
            LOG_System(0, 'create.php' ,"Duplicate Insert Email '$Email', $DBNo, ".__LINE__, 3);
            $ResultArray = array('result'=>'-3');
            echo json_encode($ResultArray);
            exit;         
        }
        // DB 쿼리 에러 -60
        else if($err == 1064)
        {
            LOG_System(0, 'create.php', "QueryError : $Query, ".__LINE__);
            $ResultArray = array('result'=>'-60');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'create.php', 'Table Error : available, '.__LINE__);
            $ResultArray = array('result'=>'-62');
            echo json_encode($ResultArray);
            exit;
        }
        else
        {
            LOG_System(0, 'create.php', "Error : $err, ".__LINE__);
            $ResultArray = array('result'=>'-1000');
            echo json_encode($ResultArray);
            exit;
        }
    }

    DB_Disconnect();

    // 최종결과 송신
    // result code
    // accountno
    // email
    // dbno
    $ResultArray = array('result'=>'1', 'accountno'=>$accountno, 'email'=>$Email, 'dbno'=>$DBNo);
    echo json_encode($ResultArray);

    $PF->stopCheck(PF_PAGE, 'Total Page');
    $PF->LOG_Save();
?>