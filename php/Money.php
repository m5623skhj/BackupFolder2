<?php
	$InputMoney = $_GET['money'];
	$DevidedMoney = explode(",", $InputMoney);
	
	$cnt = count($DevidedMoney);
	$MoneySum = 0;
	for($i=0; $i<$cnt; $i++)
	{
		$MoneySum += $DevidedMoney[$i];
	}
	
	echo number_format($MoneySum); 
?>