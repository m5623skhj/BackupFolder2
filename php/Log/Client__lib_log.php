<?php
    // -----------------------------------------------
    /*
        로그 함수 및 클래스
        외부 URL 을 호출하여 로그를 남기도록 한다

        * 필수사항
        _Config_Log.php 가 include 되어 있어야 하며, 다음 변수가 전역으로 셋팅 되어있어야 한다.

        $cnf_SYSTEM_LOG_URL
        $cnf_GAME_LOG_URL

        * 로그 부분에도 프로파일링이 들어가므로
          본 파일을 사용하는 소스에서 lib_Profiling.php 가 include 되어야 하고
          $PF = Profiling::getInstance("http://xx.xx.xx.xx/Log/LogProfiling.php", "test.php");
          가 전역으로 존재 해야만 한다.

          * 실 사용 함수 - 시스템 로그
          LOG_System($AccountNo, $Action, "오류메시지");

          * 실 사용 함수 - 게임 로그
          $GameLog = GAMELog::getInstance($cnf_GAME_LOG_URL); // 싱글턴 인스턴스 생성
          $GameLog->AddLog($MemberNo, Type, Code, P1, P2, P3, P4, 'PS'); // 로그 추가.
          $GameLog->SaveLog(); // 로그 쏘기
    */
    include_once "Client__Config_log.php";
    include_once "Client__lib_profiling.php";
    include_once $_SERVER['DOCUMENT_ROOT']."/phplib/socket_http.php";

    // -----------------------------------------------
    // 필요 할 때 마다 그때 그때 호출
    // -----------------------------------------------
    function LOG_System($AccountNo, $Action, $Message, $Level = 1)
    {
        global $cnf_SYSTEM_LOG_URL;
        global $cnf_LOG_LEVEL;
        global $PF; // 프로파일링

        if($cnf_LOG_LEVEL < $Level) return; // 로그 레벨이 로그저장 설정보다 크면 남기지 않는다.
        if($PF) $PF->startCheck(PF_LOG); // 프로파일링
        if($AccountNo < 0)
        {
            if(array_key_exists('HTTP_X_FORWARDED_FOR', $_SERVER))
                $AccountNo = $_SERVER['HTTP_X_FORWARDED_FOR'];
            else if(array_key_exists('REMOTE_ADDR', $_SERVER))
                $AccountNo = $_SERVER['REMOTE_ADDR'];
            else
                $AccountNo = 'local';
        }

        $postField = array("AccountNo" => $AccountNo, "Action" => $Action, "Message" => $Message);
        curl_request_async($cnf_SYSTEM_LOG_URL, $postField, "POST");

        if($PF) $PF->stopCheck(PF_LOG); // 프로파일링
    }

    //////////////////////////////////////////////////
    // -----------------------------------------------
    // M2 GameLog,
    //
    // 싱글톤으로 전역 생성되며, AddLog() 를 호출하여 로그를 추가 한 뒤에
    // 마지막에 (DB적용 후) SaveLog() 를 호출하여 실제로 저장한다.
    // -----------------------------------------------
    //////////////////////////////////////////////////
    class GAMELog
    {
        private $LOG_URL = '';
        private $LogArray = array();

        //-----------------------------------------------------
        // 싱글톤 객체 얻기
        // 
        // $SaveURL - 게임로그 저장호출 서버 URL / LogGame.php 의 위치
        //-----------------------------------------------------
        static function getInstance($GameLogURL)
        {
            static $instance;

            if(!isset($instance))
                $instance = new GAMELog();
            //----------------------------------------------------------------
            // 로그 저장 URL 셋팅.
            //----------------------------------------------------------------
            $instance->LOG_URL = $GameLogURL;
            return $instance;
        }

        //-----------------------------------------------------
        // 로그 누적
        //
        // 멤버변수 $LogArray 에 누적 저장시킨다.
        //
        // $AccountNo, $Type, $Code, $Param1, $Param2, $Param3, $Param4, $ParamString)
        //-----------------------------------------------------
        function AddLog($AccountNo, $Type, $Code, $Param1 = 0, $Param2 = 0, $Param3 = 0, $Param4 = 0, $ParamString = '')
        {
            array_push($this->LogArray, array(
            "AccountNo" => $AccountNo,
            "LogType" => $Type,
            "LogCode" => $Code,
            "Param1" => $Param1,
            "Param2" => $Param2,
            "Param3" => $Param3,
            "Param4" => $Param4,
            "ParamString" => $ParamString));
        }

        //-----------------------------------------------------
        // 로그 저장
        //
        // 멤버변수 $LogArray 에 쌓인 로그를 한번에 저장한다.
        //-----------------------------------------------------    
        function SaveLog()
        {
            if(count($this->LogArray) > 0)
            {
                curl_request_async($this->LOG_URL, array("LogChunk" => json_encode($this->LogArray)), "POST");
            }
        }
    }

?>
