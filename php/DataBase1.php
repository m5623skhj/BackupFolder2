<?php

    $OpenDB = 'sqldb';
    $DBIsOpen = require_once 'DBStart.php';
    if($DBIsOpen === false)
    {
        echo 'DBStart Error<BR>';
        return;
    }

    $Query = 'use sqldb<BR>';
    mysqli_query($DB, $Query);
    $Query = 'select * from buytbl';
    $result = mysqli_query($DB, $Query);

    while($row = mysqli_fetch_array($result))
    {
        if($row['groupname'] == NULL)
            echo 'NULL<BR>';
        else
            echo $row['groupname'].'<BR>';
    }
    mysqli_free_result($result);

    require_once 'DBEnd.php';
?>