<?PHP
include("include.php");
include("header.php");

// Get variables from url

$sensor_name = "";

if (isset($_GET['sensor_name']) && $_GET['sensor_name'] != "none")
  $sensor_name = $_GET['sensor_name'];

if (isset($_GET['interval']) && $_GET['interval'] != "none")
  $interval = $_GET['interval'];

if (isset($_GET['timestamp']) && $_GET['timestamp'] != "none")
  $timestamp = $_GET['timestamp'];

if (isset($_GET['subnet']) && $_GET['subnet'] != "none")
  $subnet = $_GET['subnet'];

if (isset($_GET['limit']) && $_GET['limit'] != "none")
  $limit = $_GET['limit'];


$db = ConnectDb();
?>
<FORM name="navigation" method=get action=<?=$_SERVER["PHP_SELF"]?>>
<table width=100% cellspacing=0 cellpadding=5 border=1>
<tr>
<td>
<?PHP
$sql = "SELECT sensor_name,last_connection FROM sensors ORDER BY sensor_name;";
if($dbtype == DB_PGSQL)
  $result = @pg_query($sql);
else if($dbtype == DB_MYSQL)
  $result = @mysql_query($sql);
else {
  echo "<center>DB Error, unknown database type</center>";
  exit;
}
if (!$result) {
  echo "<center>Collecting data...</center>";
  exit;
}
?>
<SELECT name="sensor_name">
<OPTION value="none">--Select A Sensor--
<?PHP
if($dbtype == DB_PGSQL) {
  while ($r = pg_fetch_array($result)) {
    if($sensor_name == $r['sensor_name'])
      $last = $r['last_connection'];
    echo "<option value=\"".$r['sensor_name']."\" ".($sensor_name==$r['sensor_name']?"SELECTED":"").">".$r['sensor_name']."\n";
  }
} else if($dbtype = DB_MYSQL) {
  while ($r = mysql_fetch_array($result)) {
    if($sensor_name == $r['sensor_name'])
      $last = $r['last_connection'];
    echo "<option value=\"".$r['sensor_name']."\" ".($sensor_name==$r['sensor_name']?"SELECTED":"").">".$r['sensor_name']."\n";
  }
}

// Set defaults
if (!isset($interval))
  $interval = DFLT_INTERVAL;

if (!isset($timestamp))
  $timestamp = time() - $interval + (0.05*$interval);

if (!isset($limit))
  $limit = 20;
?>
</SELECT>
<td><SELECT name="interval">
<OPTION value="none">--Select An Interval--
<OPTION value=<?=INT_DAILY?> <?=$interval==INT_DAILY?"SELECTED":""?>>Daily
<OPTION value=<?=INT_WEEKLY?> <?=$interval==INT_WEEKLY?"SELECTED":""?>>Weekly
<OPTION value=<?=INT_MONTHLY?> <?=$interval==INT_MONTHLY?"SELECTED":""?>>Monthly
<OPTION value=<?=INT_YEARLY?> <?=$interval==INT_YEARLY?"SELECTED":""?>>Yearly
<OPTION value=<?=24*60*60?> <?=$interval==24*60*60?"SELECTED":""?>>24hrs
<OPTION value=<?=30*24*60*60?> <?=$interval==30*24*60*60?"SELECTED":""?>>30days
</select>

<td><SELECT name="limit">
<OPTION value="none">--How Many Results--
<OPTION value=20 <?=$limit==20?"SELECTED":""?>>20
<OPTION value=50 <?=$limit==50?"SELECTED":""?>>50
<OPTION value=100 <?=$limit==100?"SELECTED":""?>>100
<OPTION value=all <?=$limit=="all"?"SELECTED":""?>>All
</select>

<td>Subnet Filter:<input name=subnet value="<?=isset($subnet)?$subnet:"0.0.0.0/0"?>"> 
<input type=submit value="Go">
</table>
</FORM>
<?PHP
// Validation
if (!isset($sensor_name) || !$sensor_name)
  exit(0);

// Print Title

if (isset($limit))
  echo "<h2>Top $limit - $sensor_name</h2>";
else
  echo "<h2>All Records - $sensor_name</h2>";

if(isset($last))
  echo "Last connection: " . $last . "</BR>\n";

// Sqlize the incomming variables
if (isset($subnet)) {
  if($dbtype == DB_PGSQL)
    $sql_subnet = "and ip <<= '$subnet'";
  else if($dbtype == DB_MYSQL) {
    $p = parse_addr($subnet);
    $net = $p["ip"] & $p["mask"];
    $sql_subnet = "and (ip & ".sprintf("%u", $p["mask"]).") = ".sprintf("%u", $net);
  }
}

// Sql Statement
$pg_sql = "select tx.ip, rx.scale as rxscale, tx.scale as txscale, tx.total+rx.total as total, tx.total as sent, 
rx.total as received, tx.tcp+rx.tcp as tcp, tx.udp+rx.udp as udp,
tx.icmp+rx.icmp as icmp, tx.http+rx.http as http,
tx.p2p+rx.p2p as p2p, tx.ftp+rx.ftp as ftp
from

(SELECT ip, max(total/sample_duration)*8 as scale, sum(total) as total, sum(tcp) as tcp, sum(udp) as udp, sum(icmp) as icmp,
sum(http) as http, sum(p2p) as p2p, sum(ftp) as ftp
from sensors, bd_tx_log
where sensor_name = '".bd_escape_string($sensor_name)."'
and sensors.sensor_id = bd_tx_log.sensor_id
$sql_subnet
and timestamp > $timestamp::abstime and timestamp < ".($timestamp+$interval)."::abstime
group by ip) as tx,

(SELECT ip, max(total/sample_duration)*8 as scale, sum(total) as total, sum(tcp) as tcp, sum(udp) as udp, sum(icmp) as icmp,
sum(http) as http, sum(p2p) as p2p, sum(ftp) as ftp
from sensors, bd_rx_log
where sensor_name = '".bd_escape_string($sensor_name)."'
and sensors.sensor_id = bd_rx_log.sensor_id
$sql_subnet
and timestamp > $timestamp::abstime and timestamp < ".($timestamp+$interval)."::abstime
group by ip) as rx

where tx.ip = rx.ip
order by total desc;";

$my_sql = "select inet_ntoa(tx.ip) as ip, rx.scale as rxscale, tx.scale as txscale, tx.total+rx.total as total, tx.total as sent, 
rx.total as received, tx.tcp+rx.tcp as tcp, tx.udp+rx.udp as udp,
tx.icmp+rx.icmp as icmp, tx.http+rx.http as http,
tx.p2p+rx.p2p as p2p, tx.ftp+rx.ftp as ftp
from

(SELECT ip, max(total/sample_duration)*8 as scale, sum(total) as total, sum(tcp) as tcp, sum(udp) as udp, sum(icmp) as icmp,
sum(http) as http, sum(p2p) as p2p, sum(ftp) as ftp
from sensors, bd_tx_log
where sensor_name = '".bd_escape_string($sensor_name)."'
and sensors.sensor_id = bd_tx_log.sensor_id
$sql_subnet
and unix_timestamp(timestamp) > $timestamp and unix_timestamp(timestamp) < ".($timestamp+$interval)."
group by ip) as tx,

(SELECT ip, max(total/sample_duration)*8 as scale, sum(total) as total, sum(tcp) as tcp, sum(udp) as udp, sum(icmp) as icmp,
sum(http) as http, sum(p2p) as p2p, sum(ftp) as ftp
from sensors, bd_rx_log
where sensor_name = '".bd_escape_string($sensor_name)."'
and sensors.sensor_id = bd_rx_log.sensor_id
$sql_subnet
and unix_timestamp(timestamp) > $timestamp and unix_timestamp(timestamp) < ".($timestamp+$interval)."
group by ip) as rx

where tx.ip = rx.ip
order by total desc";

//echo "</center><pre>$my_sql</pre><center>"; exit(0);
if($dbtype == DB_PGSQL) {
  pg_query("SET sort_mem TO 30000;");
  $result = pg_query($pg_sql);
} else if($dbtype == DB_MYSQL)
  $result = mysql_query($my_sql);
if($dbtype == DB_PGSQL)
  pg_query("set sort_mem to default;");

if ($limit == "all") {
  if($dbtype == DB_PGSQL)
    $limit = pg_num_rows($result);
  else if($dbtype == DB_MYSQL)
    $limit  = mysql_num_rows($result);
}

echo "<table width=100% border=1 cellspacing=0><tr><td>Ip<td>Name<td>Total<td>Sent<td>Received<td>tcp<td>udp<td>icmp<td>http<td>p2p<td>ftp";

if (!isset($subnet)) // Set this now for total graphs
  $subnet = "0.0.0.0/0";

// Output Total Line
echo "<TR><TD><a href=\"#Total\">Total</a><TD>$subnet";
foreach (array("total", "sent", "received", "tcp", "udp", "icmp", "http", "p2p", "ftp") as $key) {
  if($dbtype == DB_PGSQL) {
    for($Counter=0, $Total = 0; $Counter < pg_num_rows($result); $Counter++) {
      $r = pg_fetch_array($result, $Counter);
      $Total += $r[$key];
    }
  } else if($dbtype == DB_MYSQL) {
    if(mysql_num_rows($result) > 0)
      mysql_data_seek($result, 0);
    for($Counter = 0, $Total = 0; $Counter < mysql_num_rows($result); $Counter++) {
      $r = mysql_fetch_array($result);
      $Total += $r[$key];
    }
  }
  echo fmtb($Total);
}
echo "\n";

// Output Other Lines
if($dbtype == DB_PGSQL) {
  for($Counter=0; $Counter < pg_num_rows($result) && $Counter < $limit; $Counter++) {
    $r = pg_fetch_array($result, $Counter);
    echo "<tr><td><a href=#".$r['ip'].">";
    echo $r['ip']."<td>".gethostbyaddr($r['ip']);
    echo "</a>";
    echo fmtb($r['total']).fmtb($r['sent']).fmtb($r['received']).
	      fmtb($r['tcp']).fmtb($r['udp']).fmtb($r['icmp']).fmtb($r['http']).
	      fmtb($r['p2p']).fmtb($r['ftp'])."\n";
  }
} else if($dbtype == DB_MYSQL) {
  if(mysql_num_rows($result) > 0)
    mysql_data_seek($result, 0);
  for($Counter=0; $Counter < mysql_num_rows($result) && $Counter < $limit; $Counter++) {
    $r = mysql_fetch_array($result);
    echo "<tr><td><a href=#".$r['ip'].">";
    echo $r['ip']."<td>".gethostbyaddr($r['ip']);
    echo "</a>";
    echo fmtb($r['total']).fmtb($r['sent']).fmtb($r['received']).
	      fmtb($r['tcp']).fmtb($r['udp']).fmtb($r['icmp']).fmtb($r['http']).
	      fmtb($r['p2p']).fmtb($r['ftp'])."\n";
  }
}
echo "</table></center>";

// Output Total Graph
//$scale = 0;
//if($dbtype == DB_PGSQL) {
//  for($Counter=0; $Counter < pg_num_rows($result); $Counter++) {
//    $r = pg_fetch_array($result, $Counter);
//    $scale = max($r['txscale'], $scale);
//    $scale = max($r['rxscale'], $scale);
//  }
//} else if($dbtype == DB_MYSQL) {
//  if(mysql_num_rows($result) > 0)
//    mysql_data_seek($result, 0);
//  for($Counter = 0; $Counter < mysql_num_rows($result); $Counter++) {
//    $r = mysql_fetch_array($result);
//    $scale = max($r['txscale'], $scale);
//    $scale = max($r['rxscale'], $scale);
//  }
//}

if ($subnet == "0.0.0.0/0")
  $total_table = "bd_tx_total_log";
else
  $total_table = "bd_tx_log";
echo "<a name=Total><h3><a href=details.php?sensor_name=$sensor_name&ip=$subnet>";
echo "Total - Total of $subnet</h3>";
echo "</a>";
echo "Send:<br><img src=graph.php?ip=$subnet&interval=$interval&sensor_name=".$sensor_name."&table=$total_table><br>";
echo "<img src=legend.png><br>\n";
if ($subnet == "0.0.0.0/0")
  $total_table = "bd_rx_total_log";
else
  $total_table = "bd_rx_log";
echo "Receive:<br><img src=graph.php?ip=$subnet&interval=$interval&sensor_name=".$sensor_name."&table=$total_table><br>";
echo "<img src=legend.png><br>\n";


// Output Other Graphs
if($dbtype == DB_PGSQL) {
  for($Counter=0; $Counter < pg_num_rows($result) && $Counter < $limit; $Counter++) {
    $r = pg_fetch_array($result, $Counter);
    echo "<a name=".$r['ip']."><h3><a href=details.php?sensor_name=$sensor_name&ip=".$r['ip'].">";
    if ($r['ip'] == "0.0.0.0")
      echo "Total - Total of all subnets</h3>";
    else
      echo $r['ip']." - ".gethostbyaddr($r['ip'])."</h3>";
    echo "</a>";
    echo "Send:<br><img src=graph.php?ip=".$r['ip']."&interval=$interval&sensor_name=".$sensor_name."&table=bd_tx_log&yscale=".(max($r['txscale'], $r['rxscale']))."><br>";
    echo "<img src=legend.png><br>\n";
    echo "Receive:<br><img src=graph.php?ip=".$r['ip']."&interval=$interval&sensor_name=".$sensor_name."&table=bd_rx_log&yscale=".(max($r['txscale'], $r['rxscale']))."><br>";
    echo "<img src=legend.png><br>\n";
  }
} else if($dbtype == DB_MYSQL) {
  if(mysql_num_rows($result) > 0)
    mysql_data_seek($result, 0);
  for($Counter=0; $Counter < mysql_num_rows($result) && $Counter < $limit; $Counter++) {
    $r = mysql_fetch_array($result);
    echo "<a name=".$r['ip']."><h3><a href=details.php?sensor_name=$sensor_name&ip=".$r['ip'].">";
    if ($r['ip'] == "0.0.0.0")
      echo "Total - Total of all subnets</h3>";
    else
      echo $r['ip']." - ".gethostbyaddr($r['ip'])."</h3>";
    echo "</a>";
    echo "Send:<br><img src=graph.php?ip=".$r['ip']."&interval=$interval&sensor_name=".$sensor_name."&table=bd_tx_log&yscale=".(max($r['txscale'], $r['rxscale']))."><br>";
    echo "<img src=legend.png><br>\n";
    echo "Receive:<br><img src=graph.php?ip=".$r['ip']."&interval=$interval&sensor_name=".$sensor_name."&table=bd_rx_log&yscale=".(max($r['txscale'], $r['rxscale']))."><br>";
    echo "<img src=legend.png><br>\n";
  }
}

include('footer.php');
?>
