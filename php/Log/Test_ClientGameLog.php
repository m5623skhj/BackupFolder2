<?php

    include_once "Client__lib_log.php";
    $PF = Profiling::getInstance("http://127.0.0.1/Log/LogProfiling.php", "test.php");

    LOG_System(100, "andfdfdfdf", "오류메시지");

    $GameLog = GAMELog::getInstance($cnf_GAME_LOG_URL);

    $GameLog->AddLog(0, 1, 2, 0, 0, 1, 2, 'PS');
    $GameLog->AddLog(0, 1, 3, 0, 0, 1, 2, 'PS');
    $GameLog->AddLog(0, 1, 4, 0, 0, 1, 2, 'PS');
    $GameLog->AddLog(0, 2, 12, 0, 0, 1, 2, 'PS');
    $GameLog->AddLog(0, 3, 13, 0, 0, 1, 2, 'PS');
    $GameLog->AddLog(0, 3, 14, 0, 0, 1, 2, 'PS');
    $GameLog->AddLog(0, 4, 15, 0, 0, 1, 2, 'PS');

    $GameLog->SaveLog();
?>