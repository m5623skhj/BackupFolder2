<?php

    include $_SERVER['DOCUMENT_ROOT']."/Log/Client__lib_profiling.php";

    $PF = Profiling::getInstance("http://127.0.0.1//Log/LogProfiling.php", $_SERVER['PHP_SELF']);
    $PF->startCheck(PF_PAGE);

    usleep(100000);

    $PF->startCheck(PF_MYSQL);
    usleep(100000);
    $PF->stopCheck(PF_MYSQL, "Sleep1");    

    $PF->startCheck(PF_MYSQL);
    usleep(100000);
    $PF->stopCheck(PF_MYSQL, "Sleep2");

    $PF->startCheck(PF_MYSQL);
    usleep(100000);
    $PF->stopCheck(PF_MYSQL, "Sleep3");

    $PF->startCheck(PF_LOG);
    usleep(4000);
    $PF->stopCheck(PF_LOG, "Game Log 1");

    $PF->startCheck(PF_EXTAPI);
    usleep(4000);
    $PF->stopCheck(PF_EXTAPI, "NaverAPI Login");
    
    $PF->startCheck(PF_EXTAPI);
    usleep(4000);
    $PF->stopCheck(PF_EXTAPI, "NaverAPI Cash");

    $PF->stopCheck(PF_PAGE, "Total Page");
    $PF->LOG_Save();
    echo 'OK';
?>