<?php

define("DFLT_WIDTH", 900);
define("DFLT_HEIGHT", 256);
define("DFLT_INTERVAL", INT_DAILY);
define("DFLT_OFFSETY",0.10);
define("DRAW_IP",true);
define("DRAW_SENSOR",true);
define("YSCALE_OVERRIDE",0); // 113800 - 100mbit/s
define("DEBUG",false);

// Select type of database you are using
// Possible values are DB_PGSQL and DB_MYSQL
$dbtype = DB_MYSQL;

// Configuration for PostgreSQL
$pgsql_connect_string = "user = root dbname = bandwidthd";

// Configuration for MySQL
// You can specify port after semicolon, for example "localhost:4067"
$mysql_host = "localhost";
$mysql_user = "bandwidthd";
$mysql_pass = "password";
$mysql_dbname = "bandwidthd";

?>
