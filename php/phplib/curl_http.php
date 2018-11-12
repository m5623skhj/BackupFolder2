<?php
///////////////////////////////////////////////////////
// 사용법
// $postField = array(
// "IP" =>$_SERVER['REMOTE_ADDR']),
// "Query" => $this->QUERY,
// "Comment" => $this->COMMENT);
// 
// $Response = http_post("http://url...path.php", $postField);
//
// 결과 $Response 는 배열로 BODY 와 ResultCode 가 들어옴
//
// $Response['body'] <- 결과 BODY
// $Response['code'] <- 결과 코드
///////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////
    // curl 을 사용하여, 로그저장 URL 을 쏨
    // 해당 결과값을 배열로 리턴함    
    // [body] / [code]
    ///////////////////////////////////////////////////////

    // 얘가 curl 라이브러리를 이용하는 함수임
    function http_post($url, $postField)
    {
        $ci = curl_init();

        curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
        curl_setopt($ci, CURLOPT_CONNECTTIMEOUT, 30);
        curl_setopt($ci, CURLOPT_TIMEOUT, 30);
        curl_setopt($ci, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($ci, CURLOPT_SSL_VERIFYPEER, FALSE);
        curl_setopt($ci, CURLOPT_HEADER, FALSE);
        curl_setopt($ci, CURLOPT_POST, TRUE);

        if(!empty($postField))
        {
            curl_setopt($ci, CURLOPT_POSTFIELDS, $postField);
        }
        curl_setopt($ci, CURLOPT_URL, $url);

        $Response = array();

        ///////////////////////////////////////////////////////
        // 실제 HTTP 쏘기
        ///////////////////////////////////////////////////////
        $Response['body'] = curl_exec($ci);
        $Response['code'] = curl_getinfo($ci, CURLINFO_HTTP_CODE);
        return $Response;
    }
?>