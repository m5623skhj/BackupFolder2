<meta http-equiv="refresh" content="1">

<?php

    $ClientNum = $_GET['ClientNum'];
    if($ClientNum == 0 || $ClientNum === NULL)
    {
        echo 'Must Input ClientNum or Non Zero';
        return;
    }

    $NowTime = date('Y-m-d H:i', time());
    $NowtimeStamp = strtotime($NowTime);
    $BeforeTimeStamp = $NowtimeStamp - 60;

    $TimeStampArr = array(
        strtotime($NowTime) - 60,
        strtotime($NowTime),
        strtotime($NowTime) + 60);

    $CharTable = array(
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'n', 'm', 'o', 'p', 'q', 'r', 's', 't',
        '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'
    ); // size is 40
    $ClientNumXOR = 99773;
    $MainXOR = 997352267;
    $MainMod = 32582657;

    $OTPResult[3];

    for($i=0; $i<3; $i++)
    {
        $TimeStampArr[$i] *= ($ClientNum << 5) ^ $ClientNumXOR;
        $TimeStampArr[$i] %= $MainMod;
        $TimeStampArr[$i] = $TimeStampArr[$i] << 10;
        $TimeStampArr[$i] = $TimeStampArr[$i] ^ $MainXOR;
        $TimeStampArr[$i] = $TimeStampArr[$i] << 10;

        echo $TimeStampArr[$i].'<BR>';
        
        $TimeStampArr[$i] = strval($TimeStampArr[$i]);
        $length = strlen($TimeStampArr[$i]);

        for($j=0; $j<$length; $j=$j+2)
        {
            $data = substr($TimeStampArr[$i], $j, 2);
            $data %= sizeof($CharTable);
            $data = abs($data);
            $data = $CharTable[$data];
            $OTPResult[$i] = $OTPResult[$i].$data;
        }

        echo $OTPResult[$i].'<BR>';
    }

?>