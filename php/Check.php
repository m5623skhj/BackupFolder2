<?php
	$Input = $_SERVER['QUERY_STRING'];
	
	$HasExit = stripos($Input, 'EXIT');	
	if($HasExit === false)
	{
		echo 'EXIT Not Find';
	}
	else
	{
		echo 'EXIT Find';
	}
?>