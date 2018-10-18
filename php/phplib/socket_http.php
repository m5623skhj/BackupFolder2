<?php

    // $Param = array("id" => "tset1", "pass" => "test1");
    // echo curl_request_async("http://127.0.0.1/auth_login.php", $Param, 'GET');

    //------------------------------------------------------------------------------
    // php 에서 소켓을 열어 URL 호출 후 끝낸다.
    //
    // 본래 php 에서는 curl 라이브러리를 사용하여 외부 URL 호출을 하지만
    // 이는 비동기 호출이 되지 않음. URL 호출 결과가 올 때 까지 블럭이 걸린다는 이야기.
    //
    // 그래서 직접 소켓을 열고 웹서버에 데이터 전송 후 바로 종료.
    // 물론 상대 서버로 데이터를 전송 하기까지는 블럭이 걸림 하지만 웹서버의 처리 시간까지 대기하지 않는다는 것!
    //
    // 결과가 필요없는 경우에만 사용. (로그)
    //
    // $url : http:// or https:// 가 포함된 전체 URL
    // $params : ['key'] = value 타입으로 배열형태로 데이터 입력. array("id" => "test1", "pass" => "test1");
    // $type : GET / POST
    //------------------------------------------------------------------------------

    function curl_request_async($url, $params, $type = 'POST')
    {
        // 입력된 params 를 key 와 value 로 분리하여
        // post_param 이라는 배열로 key=value 타입으로 생성.
        // 혹시나 value 가 배열인 경우는 , 로 나열
        foreach($params as $key => &$val)
        {
            if(is_array($val))
                $val = implode(',', $val);
            $post_params[] = $key.'='.urlencode($val);
        }
        // $post_params 에는 [0] id=test1 / [1] pass = test1 형태로 들어감
        // 이를 & 기준으로 하나의 스트링으로 붙임
        $post_string = implode('&', $post_params);

        // 입력된 url 을 URL 요소별로 분석
        // $url = 'http://username:password@hostname:9090/path?arg=value#anchor';
        // http - e.g. http
        // host
        // post
        // user
        // pass
        // path
        // query - after hte question mark ?
        // fragment - after the hashmark #
        $parts = parse_url($url);

        // http / https 에 따라 소켓접속 타임아웃 30초
        if($parts['scheme'] == 'http')
        {
            $fp = fsockopen($parts['host'], isset($parts['port'])?$parts['port']:80, $errnom, $errstr, 10);
        }
        else if($parts['scheme'] == 'https')
        {
            $fp = fsockopen("ssl:/".$parts['host'], isset($parts['port'])?$parts['port']:443, $errno, $errstr, 30);
        }

        if(!$fp)
        {
            // 에러로그 실제로는 화면출력 하면 안됨
            echo "$errstr ($errno)<br />\n";
            return 0;
        }
        // Data goes in the path for a GET request

        $ContentsLength = strlen($post_string);

        // GET 방식은 URL 에 파라메터를 붙임
        if('GET' == $type)
        {
            $parts['path'].='?'.$post_string;
            $ContentsLength = 0;
        }

        //HTTP 프로토콜 생성
        $out = "$type ".$parts['path']." HTTP/1.1\r\n";
        $out .= "Host: ".$parts['host']." \r\n";
        $out .= "Content-Type: application/x-www-form-urlencoded\r\n";
        $out .= "Content-Length:".$ContentsLength."\r\n";
        $out .= "Connection: Close\r\n\r\n";
        // Data goes in the request body for a POST request

        // POST 방식이면 프로토콜 뒤에 파라메터를 붙임
        if('POST' == $type && isset($post_string))
            $out.=$post_string;

        // echo $out;
        // echo "<BR><BR>";

        // 전송
        $Result = fwrite($fp, $out);

        // 바로 끊어버리는 경우 서버측에서 이를 무시 해버리는 경우가 있음
        // 대표적으로 cafe24 서버의 경우 그러함.
        // fread 를 한번 호출하여 조금이라도 받아주는 것으로 이는 해결이 가능
        // $Response = fread($fp, 1000);
        // echo $Response;

        fclose($fp);
        return $Result;
    }

/*
//  $postField = array("IP" => $_SERVER['REMOTE_ADDR']),
                       "Query" => $this->QUERY,
                       "Comment" => $this->COMMENT);
//  $Response = http_post("http://url...path.php", $postField);
//
//  결과 $Response 는 배열로 BODY 와 ResultCode 가 들어옴
//
//  $Response['body'] < 결과 BODY
//  $Response['code'] < 결과 코드
//
*/

    //-----------------------------------------------------------------------
    // curl 을 사용하여, 로그 저장 URL 을 쏜다
    // 해당 결과값을 배열로 리턴한다. [body] / code]
    //-----------------------------------------------------------------------
    // function http_post($url, $postFields)
    // {
    //     $ci = curl_init();

    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     curl_setopt($ci, CURLOPT_USERAGENT, "TEST AGENT ");
    //     if(!empty($postFields))
    //     {
    //         curl_setopt($ci, CURLOPT_POSTFIELDS, $postFields);
    //     }
    //     curl_setopt($ci, CURLOPT_URL, $url);

    //     $respon = array();
    //     //-----------------------------------------------------------------------
    //     //  실제 HTTP 쏘기
    //     //-----------------------------------------------------------------------
    //     $respon['body'] = curl_exec($ci);
    //     $response['code'] = curl_getinfo($ci, CURLINFO_HTTP_CODE);
    //     curl_close($ci);
    //     return $response;
    // }

?>

