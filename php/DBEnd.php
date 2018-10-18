<?php

    // mysqli_close($DB);
    $GameLog->SaveLog();
    $PF->LOG_Save();

    mysqli_close($g_DB_connect);

?>