<?php

    // ---------------------------------------------------------------------
    // accountno 또는 email 을 기준으로 입력된 account 를 저장함
    // 데이터 규약이 없고 Key -> Column 으로 입력된 JSON 내용대로 DB 에 저장됨
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

    $DataArray = array();
    $ColmnKey = array();
    $NowData = 0;
    $cur = 0;
    while($NowData !== false)
    {
        $NowData = current($ParsingContents);
        if($NowData === false)
            break;

        $ColmnKey[$cur] = key($ParsingContents);
        $DataArray[$cur] = current($ParsingContents);
        // array_push($ColmnKey, key($ParsingContents));
        // array_push($DataArray, $NowData);
        next($ParsingContents);
        $cur++;
    }
    // $ColumnKeySize = count($ColmnKey);
    $ColumnKeySize = $cur;

    // include $_SERVER['DOCUMENT_ROOT'].'shDBhttpAPI/ConnectDB.php';
    // require $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectIndexDB.php';

    $g_DB_connect;

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(IndexDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'update_contents.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 얻어온 정보를 사용하여 해당 유저가 어느 db 에 있는지에 대하여 쿼리를 보냄
    $Query = "SELECT `accountno`, `dbno` FROM allocate WHERE `$ColmnKey[0]`='$DataArray[0]'";
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
            LOG_System(0, 'update_contents.php', 'Table Error : available, '.__LINE__);
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
    if($row === NULL)
    {
        LOG_System(0, 'update_contents.php', "Account Is Null $ColmnKey[0] = $UserData, ".__LINE__);
        $ResultArray = array('result'=>'-10');
        echo json_encode($ResultArray);
        DB_Disconnect();
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
        LOG_System(0, 'update_contents.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 얻어온 DBNo 로 shdb_info 에 연결하여 정보를 가져옴
    // DBNo 에 해당하는 DB 정보를 알아내기 위하여 쿼리를 보냄
    $Query = 'SELECT ip, port, id, pass, dbname FROM dbconnect WHERE dbno='.$DBNo;
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

    $DBInfoRow = mysqli_fetch_assoc($QueryResult);

    mysqli_free_result($QueryResult);
    DB_Disconnect();

    // 얻어온 DBNo 로 해당 DB 에 연결
    // require_once $_SERVER['DOCUMENT_ROOT'].'/shDBhttpAPI/ConnectDataDB.php';

    // DB에 연결에 실패했다면 에러 로그를 남기고 에러 코드를 반환함
    if(DataDB_Connect() === false)
    {
        $connectError = mysqli_connect_errno();
        LOG_System(0, 'update_contents.php', "DB Connect Error, $connectError, ".__LINE__, 3);
        $ResultArray = array('result'=>'-50');
        echo json_encode($ResultArray);
        exit;
    }

    // 해당 테이블에 accountno 정보가 있는지를 확인함
    $Query = 'SELECT `accountno` FROM contents WHERE accountno='.$accountno;
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
        // shdb_data_dbno 의 account 테이블과
        // 해당 계정의 accountno 로 insert 하여 공간을 미리 확보함
        $Query = "INSERT INTO contents (accountno) VALUES ($accountno)";
        $QueryResult = DB_SendQuery($Query);
        if($QueryResult === false)
        {
            $err = mysqli_errno($g_DB_connect);
            DB_Disconnect();
            // DB 데이터 삽입 에러 -3 (중복 가입)
            if($err == 1062)
            {
                LOG_System(0, 'update_contents.php', "Duplicate Insert Email '$Email', $DBNo, ".__LINE__, 3);
                $ResultArray = array('result'=>'-3');
                echo json_encode($ResultArray);
                exit;         
            }
            // DB 쿼리 에러 -60
            else if($err == 1064)
            {
                LOG_System(0, 'update_contents.php', "QueryError : $Query, ".__LINE__);
                $ResultArray = array('result'=>'-60');
                echo json_encode($ResultArray);
                exit;
            }
            // 테이블 오류 -62
            else if($err == 1146)
            {
                LOG_System(0, 'update_contents.php', 'Table Error : available, '.__LINE__);
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
    }

    // 얻어온 Update 용 파라메터들을 한 문장으로 묶음
    $ParameterString = '';
    $Index = 1;
    while($Index < $ColumnKeySize)
    {
        // $NowString = $ColmnKey[$Index]."=".$DataArray[$Index];
        if($Index + 1 != $ColumnKeySize)
            $ParameterString = $ParameterString.'`'.$ColmnKey[$Index].'`=\''.$DataArray[$Index].'\', ';
        else
            $ParameterString = $ParameterString.'`'.$ColmnKey[$Index].'`=\''.$DataArray[$Index].'\'';

        // $ParameterString += $ParameterString.", ".$NowString;
        $Index++;
    }

    // 갱신할 데이터에 대하여 쿼리를 보냄
    $Query = "UPDATE contents SET $ParameterString WHERE accountno=$accountno";
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
        // 존재하지 않는 컬럼 update -61
        else if($err == 1054)
        {
            LOG_System(0, 'update_contents.php', "Column Update Error $err, ".__LINE__);
            $ResultArray = array('result'=>'-61');
            echo json_encode($ResultArray);
            exit;
        }
        // 테이블 오류 -62
        else if($err == 1146)
        {
            LOG_System(0, 'update_contents.php','Table Error : available, '.__LINE__);
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

    DB_Disconnect();

    // 최종결과 송신
    // result code
    $ResultArray = array('result'=>'1');
    echo json_encode($ResultArray);
    
    $PF->stopCheck(PF_PAGE, 'Total Page');
    $PF->LOG_Save();
?>