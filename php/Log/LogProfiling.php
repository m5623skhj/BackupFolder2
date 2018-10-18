<pre>
<?php
//------------------------------------------------------------
// 프로파일링 로그 남기는 함수 / DB, 테이블 정보는 _Config.php 참고.
//
// POST 방식으로 프로파일링 로그를 저장한다.
//
// $_POST['IP']			: 유저
// $_POST['MemberNo']	: 유저
// $_POST['Action']		: 액션구분
//
// $_POST['T_Page']			: Time Page
// $_POST['T_Mysql_Conn']	: Time Mysql Connect
// $_POST['T_Mysql']		: Time Mysql
// $_POST['T_ExtAPI']		: Time API
// $_POST['T_Log']			: Time Log
// $_POST['T_ru_u']			: Time user time used 
// $_POST['T_ru_s']			: Time system time used 
// $_POST['Query']			: Mysql 이 있는 경우 쿼리문
// $_POST['Comment']		: 그 외 기타 멘트
//------------------------------------------------------------

/*
CREATE TABLE `ProfilingLog_template` (
	`no` bigint NOT NULL AUTO_INCREMENT,
	`date` datetime NOT NULL,
	`IP`	varchar(40) NOT NULL,
	`AccountNo` bigint NOT NULL,
	`Action` varchar(128) DEFAULT NULL,
	`T_Page` FLOAT NOT NULL,
	`T_Mysql_Conn` FLOAT NOT NULL,
	`T_Mysql` FLOAT NOT NULL,
	`T_ExtAPI` FLOAT NOT NULL,
	`T_Log` FLOAT NOT NULL,
	`T_ru_u` FLOAT NOT NULL,
	`T_ru_s` FLOAT NOT NULL,
	`Query` TEXT NOT NULL,
	`Comment` TEXT NOT NULL,
	PRIMARY KEY (`no`)
)	ENGINE=MyISAM DEFAULT CHARSET=utf8
*/


//------------------------------------------------------------
include "_Config.php";

if ( !isset($_POST['IP']) )				$_POST['IP']			= "None";
if ( !isset($_POST['AccountNo']) )		$_POST['AccountNo']		= 0;
if ( !isset($_POST['Action']) )			$_POST['Action']		= "None";
if ( !isset($_POST['T_Page']) )			$_POST['T_Page']		= 0;
if ( !isset($_POST['T_Mysql_Conn']) )	$_POST['T_Mysql_Conn']	= 0;
if ( !isset($_POST['T_Mysql']) )		$_POST['T_Mysql']		= 0;
if ( !isset($_POST['T_ExtAPI']) )		$_POST['T_ExtAPI']		= 0;
if ( !isset($_POST['T_Log']) )			$_POST['T_Log']			= 0;
// if ( !isset($_POST['T_ru_u']) )			$_POST['T_ru_u']		= 0;
// if ( !isset($_POST['T_ru_s']) )			$_POST['T_ru_s']		= 0;
if ( !isset($_POST['Query']) )			$_POST['Query']			= "None";
if ( !isset($_POST['Comment']) )		$_POST['Comment']		= "None";


//------------------------------------------------------------------
// DB 연결
//------------------------------------------------------------------
$g_DB_connect = mysqli_connect($config_db_host, $config_db_id, $config_db_pass, $config_db_name, $config_db_port);

if ( !$g_DB_connect ) exit;

mysqli_set_charset($g_DB_connect, "utf8");


//------------------------------------------------------------------
// 문자열 인자의 공격 검사는 하지 않음. 
//
// 내부 서버 IP 외에는 본 파일을 호출하지 못하도록 방화벽에서 차단 되어야 함.
//------------------------------------------------------------------
$IP				= $_POST['IP'];
$AccountNo		= $_POST['AccountNo'];
$Action			= $_POST['Action'];
$T_Page			= $_POST['T_Page'];
$T_Mysql_Conn	= $_POST['T_Mysql_Conn'];
$T_Mysql		= $_POST['T_Mysql'];
$T_ExtAPI		= $_POST['T_ExtAPI'];
$T_Log			= $_POST['T_Log'];
// $T_ru_u			= $_POST['T_ru_u'];
// $T_ru_s			= $_POST['T_ru_s'];
$Query			= $_POST['Query'];
$Comment		= $_POST['Comment'];


//------------------------------------------------------------------
// 로그 삽입
//------------------------------------------------------------------
$Table_Name = "profilingLog_" . @date("Ym");

$QuerySentence = "INSERT INTO {$Table_Name} (`date`, `IP`, `AccountNo`, `Action`, `T_Page`, `T_Mysql_Conn`, `T_Mysql`, `T_ExtAPI`, `T_Log`, `Query`, `Comment`) VALUES (NOW(), '{$IP}', '{$AccountNo}', '{$Action}', '{$T_Page}', '{$T_Mysql_Conn}', '{$T_Mysql}', '{$T_ExtAPI}', '{$T_Log}', '{$Query}', '{$Comment}')";
$Result = mysqli_query($g_DB_connect, $QuerySentence);

//------------------------------------------------------------------
// 테이블 없을시 생성 후 입력
//------------------------------------------------------------------
if ( !$Result && mysqli_errno($g_DB_connect) === 1146)
{
	mysqli_query($g_DB_connect, "CREATE TABLE {$Table_Name} LIKE profilinglog_templatetbl");
	mysqli_query($g_DB_connect, $QuerySentence);
}

if ( $g_DB_connect ) mysqli_close($g_DB_connect);
?> 
</pre>
