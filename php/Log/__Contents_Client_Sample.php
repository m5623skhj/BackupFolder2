<?php

// http_request_async 함수는 지난시간에 만들었던 소켓 HTTP 호출 함수 입니다.

include $_SERVER['DOCUMENT_ROOT']."/phplib/socket_http.php";
function LOG_System($AccountNo, $Action, $Message)
{
	$postField = array("AccountNo" => $AccountNo, "Action" => $Action, "Message" => $Message);

	$Response = curl_request_async("http://127.0.0.1/Log/LogSystem.php", $postField, "POST");
	echo $Response.'<BR>';
}

function LOG_Game($AccountNo, $Type, $Code, $Param1, $Param2, $Param3, $Param4, $ParamString)
{	
	$GameLogChunk = array( array(	"AccountNo"	=> $AccountNo,
									"LogType"	=> $Type, 
									"LogCode"	=> $Code,
									"Param1"	=> $Param1,
									"Param2"	=> $Param2,
									"Param3"	=> $Param3,
									"Param4"	=> $Param4,
									"ParamString"	=> $ParamString	));
	
	$Response = curl_request_async("http://127.0.0.1/Log/LogGame.php", array("LogChunk" => json_encode($GameLogChunk)), "POST");
	echo $Response.'<BR>';
}


$Out = "asefsaefasefasefaa";

// 시스템 로그 테스트
LOG_System("e433333", "System Log Test22222", $Out);
// 게임로그 테스트
LOG_Game("1232323231", 11, 22, 1, 2, 3, 4, "dsd");

?>
