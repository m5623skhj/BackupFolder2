<?php
    include $_SERVER['DOCUMENT_ROOT']."\phplib\DBlib.php";
    // 세션키가 유효한지 확인 한 후 세션키를 재발급
    // Input : accountno / Before sessionkey
    // Ouptput : resultcode / accountno / New SessionKey

    function ChangeSessionKey($AccountNo, $SessionKey)
    {
        $Query = "select accountno from session_tbl where accountno = '{$AccountNo}' AND sessionkey = '{$SessionKey}'";
        $retval = DB_SendQuery($Query);
    
        $account = mysqli_fetch_array($retval, MYSQLI_ASSOC); // 컬럼명으로 불러옴
        mysqli_free_result($retval);
    
        $NewSessionKey;
        if($account === NULL)
        {
            $NewSessionKey = false;
        }
        else
        {
            $NewSessionKey = MakeNewSessionKey($AccountNo);
        }

        return $NewSessionKey;
    }

    function MakeNewSessionKey($AccountNo)
    {
        $TimeStamp = strtotime(date('Y-m-d h:i:s', time()));        
        $SessionKey = ( (($TimeStamp % 97737) + ($AccountNo << 5)) << 4 ) * 11;
        $TimeStamp += 300;
        
        $Query = "update session_tbl set sessionkey = '{$SessionKey}' , publishtime = '{$TimeStamp}' where accountno = '{$AccountNo}'";
        DB_SendQuery($Query);

        return $SessionKey;
    }
?>