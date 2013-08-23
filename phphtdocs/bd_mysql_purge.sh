echo -e "bd_rx_log \n bd_tx_log \n bd_rx_total_log \n bd_tx_total_log" | while read TABLE; 
do
cat << EOF
BEGIN;

INSERT INTO $TABLE (sensor_id, ip, timestamp, sample_duration, total, icmp, udp, tcp, ftp, http, p2p)
SELECT sensor_id, ip,
IF(EXTRACT(hour FROM timestamp) >= 12, DATE_FORMAT(timestamp, "%y-%m-%d 00:00:00") + INTERVAL 12 hour,
    DATE_FORMAT(timestamp, "%y-%m-%d 00:00:00")) + INTERVAL 12 hour,
60*60*12, SUM(total), SUM(icmp), SUM(udp), SUM(tcp), SUM(ftp), SUM(http), SUM(p2p)
FROM $TABLE
WHERE sample_duration < 60*60*12
AND timestamp < NOW() - INTERVAL 35 day
GROUP BY sensor_id, ip,
IF(EXTRACT(hour FROM timestamp) >= 12, DATE_FORMAT(timestamp, "%y-%m-%d 00:00:00") + INTERVAL 12 hour, 
    DATE_FORMAT(timestamp, "%y-%m-%d 00:00:00"));

DELETE FROM $TABLE WHERE sample_duration < 60*60*12 AND timestamp < NOW() - INTERVAL 35 day;

COMMIT;


BEGIN;

INSERT INTO $TABLE (sensor_id, ip, timestamp, sample_duration, total, icmp, udp, tcp, ftp, http, p2p)
SELECT sensor_id, ip,
DATE_FORMAT(timestamp,"%y-%m-%d %H:00:00") + INTERVAL 1 hour,
60*60, SUM(total), SUM(icmp), SUM(udp), SUM(tcp), SUM(ftp), SUM(http), SUM(p2p)
FROM $TABLE
WHERE sample_duration < 60*60
AND timestamp < NOW() - INTERVAL 7 day
GROUP BY sensor_id, ip, DATE_FORMAT(timestamp,"%y-%m-%d %H:00:00");

DELETE FROM $TABLE WHERE sample_duration < 60*60 AND timestamp < NOW() - INTERVAL 7 day;

COMMIT;


BEGIN;

INSERT INTO $TABLE (sensor_id, ip, timestamp, sample_duration, total, icmp, udp, tcp, ftp, http, p2p)
SELECT sensor_id, ip,
DATE_FORMAT(timestamp,"%y-%m-%d %H:00:00")  + INTERVAL TRUNCATE(EXTRACT(minute FROM timestamp),-1) minute
+ INTERVAL 10 minute,
10*60, SUM(total), SUM(icmp), SUM(udp), SUM(tcp), SUM(ftp), SUM(http), SUM(p2p)
FROM $TABLE
WHERE sample_duration < 10*60
AND timestamp < NOW() - INTERVAL 2 day
GROUP BY sensor_id, ip,
DATE_FORMAT(timestamp,"%y-%m-%d %H:00:00") + INTERVAL TRUNCATE(EXTRACT(minute FROM timestamp),-1) minute;

DELETE FROM $TABLE WHERE sample_duration < 10*60 AND timestamp < NOW() - INTERVAL 2 day;

COMMIT;

OPTIMIZE TABLE $TABLE;
EOF
done
