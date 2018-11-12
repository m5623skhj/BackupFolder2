<?php
    include_once $_SERVER['DOCUMENT_ROOT'].'/phplib/curl_http.php';

// 사용법
// $postField = array(
// "IP" =>$_SERVER['REMOTE_ADDR']),
// "Query" => $this->QUERY,
// "Comment" => $this->COMMENT);
// 
// $Response = http_post("http://url...path.php", $postField);

    $field = array(
        'IP'=>$_SERVER['REMOTE_ADDR'],
        'Query'=>'QUERY',
        'Comment'=>'COMMENT');

    $Response = http_post('http://127.0.0.1/1.php', $field);

    echo $Response['body'];
?>