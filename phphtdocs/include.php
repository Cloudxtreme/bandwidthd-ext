<?PHP
// Emulate register_globals off
if (ini_get('register_globals')) {
  $superglobals = array($_SERVER, $_ENV,
			$_FILES, $_COOKIE, $_POST, $_GET);
  if (isset($_SESSION)) {
    array_unshift($superglobals, $_SESSION);
  }
  foreach ($superglobals as $superglobal) {
    foreach ($superglobal as $global => $value) {
      unset($GLOBALS[$global]);
    }
  }
}

define("INT_DAILY", 60*60*24*2);
define("INT_WEEKLY", 60*60*24*8);
define("INT_MONTHLY", 60*60*24*35);
define("INT_YEARLY", 60*60*24*400);

define("XOFFSET", 90);
define("YOFFSET", 45);

define("DB_PGSQL", 1);
define("DB_MYSQL", 2);

require("config.conf");

function ConnectDb()
{
  global $dbtype;
  global $pgsql_connect_string;
  global $mysql_host, $mysql_user, $mysql_pass, $mysql_dbname;

  if($dbtype == DB_PGSQL)
    $db = pg_pconnect($pgsql_connect_string);
  else if($dbtype == DB_MYSQL)
    $db = mysql_connect($mysql_host, $mysql_user, $mysql_pass);
  else {
    printf("DB Error, unknown database type");
    exit(1);
  }

  if (!$db) {
    printf("DB Error, could not connect to database");
    exit(1);
  }

  if($dbtype == DB_MYSQL)
    mysql_select_db($mysql_dbname);

  return($db);
}
                                                                                                                             
function fmtb($kbytes)
{
  $Max = 1024;
  $Output = $kbytes;
  $Suffix = 'K';

  if ($Output > $Max) {
    $Output /= 1024;
    $Suffix = 'M';
  }

  if ($Output > $Max) {
    $Output /= 1024;
    $Suffix = 'G';
  }

  if ($Output > $Max) {
    $Output /= 1024;
    $Suffix = 'T';
  }

  return(sprintf("<td align=right><tt>%.1f%s</td>", $Output, $Suffix));
}

$masks = array(0 => "0.0.0.0",
	       1 => "128.0.0.0",
	       2 => "192.0.0.0",
	       3 => "224.0.0.0",
	       4 => "240.0.0.0",
	       5 => "248.0.0.0",
	       6 => "252.0.0.0",
	       7 => "254.0.0.0",
	       8 => "255.0.0.0",
	       9 => "255.128.0.0",
	       10 => "255.192.0.0",
	       11 => "255.224.0.0",
	       12 => "255.240.0.0",
	       13 => "255.248.0.0",
	       14 => "255.252.0.0",
	       15 => "255.254.0.0",
	       16 => "255.255.0.0",
	       17 => "255.255.128.0",
	       18 => "255.255.192.0",
	       19 => "255.255.224.0",
	       20 => "255.255.240.0",
	       21 => "255.255.248.0",
	       22 => "255.255.252.0",
	       23 => "255.255.254.0",
	       24 => "255.255.255.0",
	       25 => "255.255.255.128",
	       26 => "255.255.255.192",
	       27 => "255.255.255.224",
	       28 => "255.255.255.240",
	       29 => "255.255.255.248",
	       30 => "255.255.255.252",
	       31 => "255.255.255.254",
	       32 => "255.255.255.255");

function parse_addr($addr)
{
  global $masks;

  if(!$addr)
    return array("ip" => 0, "mask" => 0);

  $pos = strpos($addr, "/");
  if(!$pos) {
    $ip = $addr;
    $mask = "255.255.255.255";
  }
  else {
    $ip = substr($addr, 0, $pos);
    $mask = substr($addr, $pos+1);
    if(!strpos($mask, "."))
      $mask = $masks[(int)$mask];
  }

  return array("ip" => ip2long($ip), "mask" => ip2long($mask));
}

function bd_escape_string($string)
{
  global $dbtype;
  
  if($dbtype == DB_PGSQL)
    return pg_escape_string($string);
  else if($dbtype == DB_MYSQL)
    return mysql_real_escape_string($string);
  else
    return $string;
}

$starttime = time();
set_time_limit(300);
?>
