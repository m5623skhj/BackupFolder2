<?php

    // DB 연결
    function DB_Connect()
    {
        // 외부에서 global 변수들을 초기화 시켜줘야 됨
        global $db_host;
        global $db_user;
        global $db_password;
        global $db_name;
        global $db_port;
        global $g_DB_connect;

        // 해당 DB에 연결함
        $g_DB_connect = mysqli_connect($db_host, $db_user, $db_password, $db_name, $db_port);
        if(!$g_DB_connect)
        {
            // 다른곳에다가 로그를 보관해야되는데
            // echo 'Error In Connect '.mysqli_connect_error();
            return false;
        }
        // 문자셋 셋팅
        mysqli_set_charset($g_DB_connect, 'utf8');

        return true;
    }

    // DB 연결 해제
    function DB_Disconnect()
    {
        global $g_DB_connect;
        if($g_DB_connect)
            mysqli_close($g_DB_connect);
    }

    // DB에게 인자로 받아온 쿼리를 날림
    function DB_SendQuery($Query)
    {
        global $g_DB_connect;
        $result = mysqli_query($g_DB_connect, $Query);
        if($result === FALSE)
        {
            // 다른곳에다가 로그를 보관해야되는데
            // echo 'Send Query Error '.mysqli_error($DB);
            return false;
        }

        return $result;
    }

    // DB에게 트랜잭션 쿼리를 날림
    // 트랜잭션 쿼리 또는 롤백커밋 쿼리를 전송함
    // DB에 저장하는 용도로만 사용하기 때문에 리턴값이 존재하지 않음
    // 단 insertId가 필요한 경우 그것을 돌려주며
    // 또한 보낸 쿼리문들에 대하여 insertId가 모두 필요할 경우
    // 다른 함수(이후에 이 곳에 함수명을 적을것)으로 대체해야 함
    // 저장만 하는 용도이기 때문에 셀렉트 문에는 사용하지 않음
    // 
    // DB 연결시 오토커밋을 사용하지 않기 때문에
    // DB가 변경되는 쿼리는 무조건 이 함수를 사용해야 함
    function DB_SendTransactionQuery($QueryArr)
    {
        global $g_DB_connect;

        // 해당 DB에 대하여 처리과정의 무결성을 위해 잠금을 설정함
        // InnoDB 방식에서만 작동되며 MyISAM 방식에서는 오토커밋 설정이 불가능함
        // 오토커밋을 FALSE로 해두고 데이터 반영 후 commit을 해야만 실제로 적용이 됨
        mysqli_begin_transaction($g_DB_connect);

        foreach($QueryArr as $Value)
        {
            if(is_string($Value))
            {
                // 다른곳에다가 로그를 보관해야되는데
                // echo 'Query Log : '.$Value.'<BR>';
                if(mysqli_query($g_DB_connect, $Value) === FALSE)
                {
                    // 쿼리문을 날리는데 실패했다면
                    // DB를 롤백시키고 함수를 나감
                    // 다른곳에다가 로그를 보관해야되는데
                    // echo 'Send Transaction Query Error '.mysqli_error($DB);
                    mysqli_rollback($g_DB_connect);
                    return false;
                }
            }
            // 쿼리문 자체에 오류가 있을 경우
            else
            {
                // 다른곳에다가 로그를 보관해야되는데
                // echo 'Send Transaction Query Error '.mysqli_error($DB);
                mysqli_rollback($g_DB_connect);
                return false;
            }

        }
        // commit 이후에는 insertId가 나오지 않기 때문에
        // insertId 가 있던 없던 그 값을 반환해줌
        $InsertID = mysqli_insert_id($g_DB_connect);

        // 최종적으로 해당 DB에 반영함
        mysqli_commit($g_DB_connect);
        return $InsertID;
    }

    function DB_SendTransactionQueryAndError($QueryArr)
    {
        global $g_DB_connect;
        global $errno;

        // 해당 DB에 대하여 처리과정의 무결성을 위해 잠금을 설정함
        // InnoDB 방식에서만 작동되며 MyISAM 방식에서는 오토커밋 설정이 불가능함
        // 오토커밋을 FALSE로 해두고 데이터 반영 후 commit을 해야만 실제로 적용이 됨
        mysqli_begin_transaction($g_DB_connect);

        foreach($QueryArr as $Value)
        {
            if(is_string($Value))
            {
                // 다른곳에다가 로그를 보관해야되는데
                // echo 'Query Log : '.$Value.'<BR>';
                if(mysqli_query($g_DB_connect, $Value) === FALSE)
                {
                    // 쿼리문을 날리는데 실패했다면
                    // DB를 롤백시키고 함수를 나감
                    $errno = mysqli_errno($g_DB_connect);
                    // 다른곳에다가 로그를 보관해야되는데
                    // echo 'Send Transaction Query Error '.mysqli_error($DB);
                    mysqli_rollback($g_DB_connect);
                    return false;
                }
            }
            // 쿼리문 자체에 오류가 있을 경우
            else
            {
                $errno = mysqli_errno($g_DB_connect);
                // 다른곳에다가 로그를 보관해야되는데
                // echo 'Send Transaction Query Error '.mysqli_error($DB);
                mysqli_rollback($g_DB_connect);
                return false;
            }

        }
        // commit 이후에는 insertId가 나오지 않기 때문에
        // insertId 가 있던 없던 그 값을 반환해줌
        $InsertID = mysqli_insert_id($g_DB_connect);

        // 최종적으로 해당 DB에 반영함
        mysqli_commit($g_DB_connect);
        return $InsertID;
    }

    function VersionCheck($InputMajor, $InputMinor, $VersionMajor, $VersionMinor)
    {
        if($InputMajor < $VersionMajor || $InputMinor < $VersionMinor)
            return false;

        return true;
    }

    // function ResponseJSON($JOSNObject)
    // {
    // }
?>
