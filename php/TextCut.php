<?php
	$somestring = $_GET['somestring'];
	
	/////////////////////////////////////////////
	 // $somestring2 = $somestring;
	 // while(1)
	 // {
		 // $temp = strstr($somestring2, '.');
		 // if($temp === FALSE)
			// // break;
		 // echo $temp;
		 // $somestring2 = $somestring2 - ("$temp" + ".");
	 // }
	
	
	/////////////////////////////////////////////
	
	error_reporting(E_ALL);
	ini_set("display_errors", 1);
	
	 $devidedstr = explode(".", $somestring);
	
	 $cnt = count($devidedstr);
	 for($i=0; $i<$cnt; $i++)
	 {
		 echo $devidedstr[$i];
		 echo '<BR>';
	 }
?>