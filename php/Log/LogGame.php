<pre>
<?php
//------------------------------------------------------------
// 게임 로그 남기는 함수 / DB, 테이블 정보는 _Config.php 참고.
//
// POST 방식으로 로그덩어리 (LogChunk) 에 JSON 포멧으로 들어오면 이를 배열로 풀어서 하나하나 저장한다.
// 게임로그는 하나하나 개별적인 저장이 아닌 컨텐츠 별로 관련 로그를 한방에 모아서 저장한다.
//
// array(	array(MemberNo, LogType, LogCode, Param1, Param2, Param3, Param4, ParamString),
//			array(MemberNo, LogType, LogCode, Param1, Param2, Param3, Param4, ParamString));
//
// 2차원 배열로 한 번에 여러개의 로그가 몰아서 들어옴.
//------------------------------------------------------------
    include "_Config.php";

//------------------------------------------------------------
// 현재 들어온 로그 정보에 대한 체크(없는 정보인가, 배열이 맞는가) 
//------------------------------------------------------------

    // 1. $_POST['LogChunk'] 의 셋팅유무 확인 후 예외처리
    if(!isset($_POST['LogChunk'])) return;
    var_dump($_POST['LogChunk']);
    // 2. JSON 디코딩
    $LogChunk = json_decode($_POST['LogChunk'], true);

    // 3. JSON 은 무조건 배열형태를 띄기로 했으므로 배열 확인
    if(!is_array($LogChunk)) return;

    // 4. DB 연결
    include $_SERVER['DOCUMENT_ROOT']."/phplib/DBlib.php";
    $g_DB_connect = 'log_db';
    $g_DB_connect = mysqli_connect($config_db_host, $config_db_id, $config_db_pass, $config_db_name, $config_db_port);
    // $DBIsOpen = require_once $_SERVER['DOCUMENT_ROOT']."/DBStart.php";
    // if($DBIsOpen === false)
    // {
    //     //echo 'DBStart Error<BR>';
    //     return;
    // }

    // 6. 월별 테이블 이름 문자열 만들어서
    $Table_Name = "GameLog_" . @date("Ym");

    // 5. 입력 LogChunk JSON 의 배열 개수만큼 반복문을 돌면서 
    // $LogChunk[N] 의 ['AccountNo'] ['LogType'] .... 데이터를 뽑아서 INSERT 쿼리 생성
    foreach($LogChunk as $ArrayIndex => $ArrayValue)
    {
        $AccountNo = $ArrayValue['AccountNo'];
        $LogType = $ArrayValue['LogType'];
        $LogCode = $ArrayValue['LogCode'];
        $Param1 = $ArrayValue['Param1'];
        $Param2 = $ArrayValue['Param2'];
        $Param3 = $ArrayValue['Param3'];
        $Param4 = $ArrayValue['Param4'];
        $ParamString = $ArrayValue['ParamString'];
        $Query[$ArrayIndex] = "insert into {$Table_Name} (date, accountno, LogType, LogCode, Param1, Param2, Param3, Param4, ParamString) values 
        (NOW(), {$AccountNo}, {$LogType}, {$LogCode}, {$Param1}, {$Param2}, {$Param3}, {$Param4}, '{$ParamString}')";
    }

    // 7. INSERT 쿼리전송	
    $errno = 0;
    $QueryResult = DB_SendTransactionQueryAndError($Query, $errno);

    // 8. 테이블 없음 에러 발생시  errno - 1146		
    // 테이블을 생성시켜서 다시 넣는다.
    // CREATE TABLE 새테이블 이름  LIKE GameLog_template
    if(!$QueryResult && $errno == 1146)
    {
        DB_SendQuery("CREATE TABLE {$Table_Name} LIKE gamelog_templatetbl");
        DB_SendTransactionQuery($Query);
    }
    
?>
</pre>