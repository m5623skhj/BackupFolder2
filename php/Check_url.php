<?php
	$urlCheck_arr = array('REMOTE_ADDR', 'CHAR(', 'CHR(', 'EVAL(');
	$cnt = count($urlCheck_arr);
	
	$Input = $_SERVER['QUERY_STRING'];//$_GET['Input'];
	
	for($i=0; $i<$cnt; $i++)
	{
		$check = stripos($Input, $urlCheck_arr[$i]);
		if($check !== false)
		{
			echo 'BLOCK!';
			break;
		}
	}
?>