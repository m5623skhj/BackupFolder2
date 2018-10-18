<?php
/*
* SystemLog 테이블 템플릿,

CREATE TABLE `SystemLog_template` (
  `no`			int(11) NOT NULL AUTO_INCREMENT,
  `date`		datetime NOT NULL,
  `AccountNo`	varchar(50) NOT NULL,
  `Action`		varchar(128) DEFAULT NULL,
  `Message`		varchar(1024) DEFAULT NULL,

  PRIMARY KEY (`no`)
) ENGINE=MyISAM;
*/

//------------------------------------------------------------
// 시스템 로그 남기는 함수 / DB, 테이블 정보는 _Config.php 참고.
//
// POST 방식으로 로그 스트링을 저장한다.
//
// $_POST['AccountNo']	: 유저
// $_POST['Action']		: 액션구분
// $_POST['Message']	: 로그스트링
//------------------------------------------------------------
  // include "_Config.php";

  // 1. AccountNo, Action, Message 인자 확인 및 없을시 디폴트 값 생성
  if(!isset($_POST['AccountNo']))  $_POST['AccountNo'] = 0;
  if(!isset($_POST['Action']))  $_POST['Action'] = NULL;
  if(!isset($_POST['Message']))  $_POST['Message'] = NULL;

  $AccountNo = $_POST['AccountNo'];
  $Action = $_POST['Action'];
  $Message = $_POST['Message'];

  // DB 연결
  include $_SERVER['DOCUMENT_ROOT']."/phplib/DBlib.php";
  $g_DB_connect = mysqli_connect($config_db_host, $config_db_id, $config_db_pass, $config_db_name, $config_db_port);
  // $g_DB_connect = 'log_db';
  // $DBIsOpen = require_once $_SERVER['DOCUMENT_ROOT']."/DBStart.php";
  // if($DBIsOpen === false)
  // {
  //     echo 'DBStart Error<BR>';
  //     return;
  // }
  // 월별 테이블 이름 만들어서 로그 INSERT
  $Table_Name = "systemlog_".date('Ym');
  $Query = "INSERT INTO {$Table_Name} (date, accountno, action, Message) VALUES (NOW(), '{$AccountNo}', '{$Action}', '{$Message}')";
  $QueryResult = mysqli_query($DB, $Query);
  // $QueryResult = DB_SendQuery($Query);

  // 테이블 없을시 (errno = 1146) 테이블 생성 후 재 입력
  //
  // 템플릿 테이블 사용 생성쿼리 - CREATE TABLE 새테이블 LIKE SystemLog_template
  // ex) CREATE TABLE `SystemLog_201711` LIKE SystemLog_template;
  //	if ( errno == 1146 )
  //	{
  //			테이블 생성 쿼리 ..
  //			로그쿼리 다시 쏘기
  //	}
  if(!$QueryResult && mysqli_errno($DB) == 1146)
  {
    DB_SendQuery("CREATE TABLE {$Table_Name} LIKE systemlog_templatetbl");
    DB_SendQuery($Query);
  }
?>
