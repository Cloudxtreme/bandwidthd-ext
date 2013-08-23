#include "bandwidthd.h"

#ifdef HAVE_LIBPQ
#include <libpq-fe.h>
#endif

#ifdef HAVE_LIBMYSQLCLIENT
#include <mysql.h>
#include <errmsg.h>
#endif

// We must call regular exit to write out profile data, but child forks are supposed to usually
// call _exit?
#ifdef PROFILE
#define _exit(x) exit(x)
#endif

/*
#ifdef DEBUG
#define fork() (0)
#endif
*/

// ****************************************************************************************
// ** Global Variables
// ****************************************************************************************

static pcap_t *pd;

unsigned int GraphIntervalCount = 0;
unsigned int IpCount = 0;
unsigned int SubnetCount = 0;
time_t IntervalStart;
int RotateLogs = FALSE;

struct SubnetData SubnetTable[SUBNET_NUM];
struct IPData IpTable[IP_NUM];

int DataLink;
int IP_Offset;

struct IPDataStore *IPDataStore = NULL;
extern int bdconfig_parse(void);
extern FILE *bdconfig_in;

struct config config;

pid_t workerchildpids[NR_WORKER_CHILDS];

void signal_handler(int sig)
{
  switch (sig) {
  case SIGHUP:
    signal(SIGHUP, signal_handler);
    RotateLogs++;
    if (config.tag == '1') {
      int i;

      /* signal children if any */
      if(config.graph || config.output_cdf)
	for (i = 0; i < NR_WORKER_CHILDS; i++)
	  kill(workerchildpids[i], SIGHUP);
    }
    break;

  case SIGTERM:
    syslog(LOG_INFO, "Exiting...");

    // This sets a flag, so pcap_loop will end as soon as possible,
    // so we won't be in a middle of writing data to file/database.
    // Graphing is handled by separate process so it won't harm it
    // either.
    pcap_breakloop(pd);

    if (config.tag == '1') {
      int i;

      /* send term signal to children if any */
      if(config.graph || config.output_cdf)
	for (i = 0; i < NR_WORKER_CHILDS; i++)
	  kill(workerchildpids[i], SIGTERM);
    }
    break;
  }
}

#ifdef HAVE_LIBGD
void bd_CollectingData()
{
  char FileName[4][MAX_FILENAME];
  FILE *index;
  int Counter;

  snprintf(FileName[0], MAX_FILENAME, "%s/index.html", config.htdocs_dir);
  snprintf(FileName[1], MAX_FILENAME, "%s/index2.html", config.htdocs_dir);
  snprintf(FileName[2], MAX_FILENAME, "%s/index3.html", config.htdocs_dir);
  snprintf(FileName[3], MAX_FILENAME, "%s/index4.html", config.htdocs_dir);

  for (Counter = 0; Counter < 4; Counter++) {
    index = fopen(FileName[Counter], "wt");
    if (index) {
      fprintf(index, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
      fprintf(index, "<HTML><HEAD><TITLE>Bandwidthd</TITLE>\n");

      if (config.meta_refresh)
	fprintf(index, "<META HTTP-EQUIV=\"REFRESH\" content=\"%u\">\n", config.meta_refresh);
      fprintf(index, "<META HTTP-EQUIV=\"EXPIRES\" content=\"-1\">\n");
      fprintf(index, "<META HTTP-EQUIV=\"PRAGMA\" content=\"no-cache\">\n");
      fprintf(index, "</HEAD>\n<BODY><center><img src=\"logo.gif\" ALT=\"Logo\"><BR>\n");
      fprintf(index, "<BR>\n - <a href=\"index.html\">Daily</a> -- <a href=\"index2.html\">Weekly</a> -- ");
      fprintf(index, "<a href=\"index3.html\">Monthly</a> -- <a href=\"index4.html\">Yearly</a><BR>\n");
      fprintf(index, "</CENTER><BR>bandwidthd has nothing to graph.  This message should be replaced by graphs in a few minutes.  If it's not, please see the section titled \"Known Bugs and Troubleshooting\" in the README");
      fprintf(index, "</BODY></HTML>\n");
      fclose(index);
    } else {
      syslog(LOG_ERR, "Cannot open %s for writing", FileName[Counter]);
      exit(EXIT_FAILURE);
    }
  }
}

int WriteOutWebpages(long int timestamp)
{
  struct IPDataStore *DataStore = IPDataStore;
  struct SummaryData **SummaryData;
  int NumGraphs = 0;
  pid_t graphpid;
  int Counter;

  /* Did we catch any packets since last time? */
  if (!DataStore)
    return -1;

  // break off from the main line so we don't miss any packets while we graph
  graphpid = fork();

  switch (graphpid) {
  case 0:		/* we're the child, graph. */
    {
#ifdef PROFILE
      // Got this incantation from a message board.  Don't forget to set
      // GMON_OUT_PREFIX in the shell
      extern void _start(void), etext(void);
      syslog(LOG_INFO, "Calling profiler startup...");
      monstartup((u_long) & _start, (u_long) & etext);
#endif
      signal(SIGHUP, SIG_IGN);

      nice(4);	// reduce priority so I don't choke out other tasks

      // Count Number of IP's in datastore
      for (DataStore = IPDataStore, Counter = 0; DataStore; Counter++, DataStore = DataStore->Next);

      // +1 because we don't want to accidently allocate 0
      SummaryData = malloc(sizeof(struct SummaryData *) * Counter + 1);

      DataStore = IPDataStore;
      while (DataStore)	// Is not null
	{
	  if (DataStore->FirstBlock->NumEntries > 0) {
	    SummaryData[NumGraphs] = (struct SummaryData *) malloc(sizeof(struct SummaryData));
	    GraphIp(DataStore, SummaryData[NumGraphs++], timestamp + LEAD * config.range);
	  }
	  DataStore = DataStore->Next;
	}

      MakeIndexPages(NumGraphs, SummaryData);

      _exit(EXIT_SUCCESS);
    }
    break;

  case -1:
    syslog(LOG_ERR, "Forking grapher child failed!");
    return -2;
    break;

  default:		/* parent + successful fork, assume graph success */
    return 0;
    break;
  }
}
#endif // HAVE_LIBGD

void setchildconfig(int level)
{
#ifdef HAVE_LIBGD
  static unsigned long long graph_cutoff;
#endif // HAVE_LIBGD

  switch (level) {
  case 0:
    config.range = RANGE1;
    config.interval = INTERVAL1;
    config.tag = '1';
#ifdef HAVE_LIBGD
    graph_cutoff = config.graph_cutoff;
#endif // HAVE_LIBGD
    break;
  case 1:
    config.range = RANGE2;
    config.interval = INTERVAL2;
    config.tag = '2';
#ifdef HAVE_LIBGD
    // Overide skip_intervals for children
    config.skip_intervals = CONFIG_GRAPHINTERVALS;
    config.graph_cutoff = graph_cutoff * (RANGE2 / RANGE1);
#endif // HAVE_LIBGD
    break;
  case 2:
    config.range = RANGE3;
    config.interval = INTERVAL3;
    config.tag = '3';
#ifdef HAVE_LIBGD
    // Overide skip_intervals for children
    config.skip_intervals = CONFIG_GRAPHINTERVALS;
    config.graph_cutoff = graph_cutoff * (RANGE3 / RANGE1);
#endif // HAVE_LIBGD
    break;
  case 3:
    config.range = RANGE4;
    config.interval = INTERVAL4;
    config.tag = '4';
#ifdef HAVE_LIBGD
    // Overide skip_intervals for children
    config.skip_intervals = CONFIG_GRAPHINTERVALS;
    config.graph_cutoff = graph_cutoff * (RANGE4 / RANGE1);
#endif // HAVE_LIBGD
    break;

  default:
    syslog(LOG_ERR, "setchildconfig got an invalid level argument: %d", level);
    _exit(EXIT_FAILURE);
  }
}

void makepidfile(pid_t pid)
{
  FILE *pidfile;

  pidfile = fopen(config.pidfile, "wt");
  if (pidfile) {
    if (fprintf(pidfile, "%d\n", pid) == 0) {
      syslog(LOG_ERR, "Bandwidthd: failed to write '%d' to %s", pid, config.pidfile);
      fclose(pidfile);
      unlink(config.pidfile);
    } else
      fclose(pidfile);
  } else
    syslog(LOG_ERR, "Could not open %s for writing", config.pidfile);
}

void PrintHelp(void)
{
  printf("\nUsage: bandwidthd [OPTION]\n\nOptions:\n");
  printf("\t-D\t\tDo not fork to background\n");
  printf("\t-l\t\tList detected devices\n");
  printf("\t-c filename\tAlternate configuration file\n");
  printf("\t--help\t\tShow this help\n");
  printf("\n");
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
  struct bpf_program fcode;
  u_char *pcap_userdata = 0;
#ifdef HAVE_PCAP_FINDALLDEVS
  pcap_if_t *Devices;
#endif
  char Error[PCAP_ERRBUF_SIZE];
  struct stat StatBuf;
  int i;
  int ForkBackground = TRUE;
  int ListDevices = FALSE;
  int Counter;
  char *bd_conf = NULL;
  struct in_addr addr, addr2;
  char *tmp;

  signal(SIGHUP, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  for (Counter = 1; Counter < argc; Counter++) {
    if (argv[Counter][0] == '-') {
      switch (argv[Counter][1]) {
      case 'D':
	ForkBackground = FALSE;
	break;
      case 'l':
	ListDevices = TRUE;
	break;
      case 'c':
	if (argv[Counter + 1]) {
	  bd_conf = argv[Counter + 1];
	  Counter++;
	} else
	  PrintHelp();
	break;
      default:
	printf("Improper argument: %s\n", argv[Counter]);
      case '-':
	PrintHelp();
      }
    }
  }

  config.dev = NULL;
  config.filter = "ip";
  config.skip_intervals = CONFIG_GRAPHINTERVALS;
  config.graph_cutoff = CONFIG_GRAPHCUTOFF;
  config.promisc = TRUE;
  config.graph = TRUE;
  config.output_cdf = FALSE;
  config.recover_cdf = FALSE;
  config.meta_refresh = CONFIG_METAREFRESH;
  config.output_database = DB_NONE;
  config.pgsql_connect_string = NULL;
  config.sensor_id = "unset";
  config.log_dir = LOG_DIR;
  config.htdocs_dir = HTDOCS_DIR;
  config.mysql_host = NULL;
  config.mysql_user = NULL;
  config.mysql_pass = NULL;
  config.mysql_dbname = NULL;
  config.mysql_port = 0;
  config.pidfile = "/var/run/bandwidthd.pid";

  openlog("bandwidthd", LOG_CONS, LOG_DAEMON);

  // Locate configuration file
  if (!(bd_conf && !stat(bd_conf, &StatBuf))) {
    if (bd_conf) {
      printf("Could not find %s\n", bd_conf);
      exit(EXIT_FAILURE);
    } else if (!stat("bandwidthd.conf", &StatBuf))
      bd_conf = "bandwidthd.conf";
    else if (!stat("./etc/bandwidthd.conf", &StatBuf))
      bd_conf = "./etc/bandwidthd.conf";
    else if (!stat(CONFIG_FILE, &StatBuf))
      bd_conf = CONFIG_FILE;
    else {
      printf ("Cannot find bandwidthd.conf, ./etc/bandwidthd.conf or %s\n", CONFIG_FILE);
      syslog(LOG_ERR, "Cannot find bandwidthd.conf, ./etc/bandwidthd.conf or %s", CONFIG_FILE);
      exit(EXIT_FAILURE);
    }
  }

  bdconfig_in = fopen(bd_conf, "rt");
  if (!bdconfig_in) {
    syslog(LOG_ERR, "Cannot open bandwidthd.conf");
    printf("Cannot open ./etc/bandwidthd.conf\n");
    exit(EXIT_FAILURE);
  }
  bdconfig_parse();

  // Log list of monitored subnets
  for (Counter = 0; Counter < SubnetCount; Counter++) {
    addr.s_addr = ntohl(SubnetTable[Counter].ip);
    addr2.s_addr = ntohl(SubnetTable[Counter].mask);
    tmp = strdup(inet_ntoa(addr));
    syslog(LOG_INFO, "Monitoring subnet %s with netmask %s", tmp, inet_ntoa(addr2));
    free(tmp);
  }

#ifdef HAVE_PCAP_FINDALLDEVS
  pcap_findalldevs(&Devices, Error);
  if (config.dev == NULL && Devices && Devices->name)
    config.dev = strdup(Devices->name);
  if (ListDevices) {
    while (Devices) {
      printf("Description: %s\nName: \"%s\"\n\n", Devices->description, Devices->name);
      Devices = Devices->next;
    }
    exit(EXIT_SUCCESS);
  }
#else
  if (ListDevices) {
    printf("List devices is not supported by you version of libpcap\n");
    exit(EXIT_FAILURE);
  }
#endif

  if (config.graph) {
#ifdef HAVE_LIBGD
    bd_CollectingData();
#else
    printf("Making static graphs not compiled into bandwithd binary\n");
    exit(EXIT_FAILURE);
#endif // HAVE_LIBGD
  }

  /* detach from console. */
  if (ForkBackground)
    if (fork2())
      exit(EXIT_SUCCESS);

  makepidfile(getpid());

  setchildconfig(0);	/* initialize first (day graphing) process config */

  if (config.graph || config.output_cdf) {
    /* fork processes for week, month and year graphing. */
    for (i = 0; i < NR_WORKER_CHILDS; i++) {
      workerchildpids[i] = fork();

      /* initialize children and let them start doing work,
       * while parent continues to fork children.
       */

      if (workerchildpids[i] == 0) {	/* child */
	setchildconfig(i + 1);
	break;
      }

      if (workerchildpids[i] == -1) {	/* fork failed */
	syslog(LOG_ERR, "Failed to fork graphing child (%d)", i);
	/* i--; ..to retry? -> possible infinite loop */
	continue;
      }
    }
  }

  signal(SIGHUP, signal_handler);
  signal(SIGTERM, signal_handler);

  if (config.recover_cdf)
    RecoverDataFromCDF();

  IntervalStart = time(NULL);

  syslog(LOG_INFO, "Opening %s", config.dev);
  pd = pcap_open_live(config.dev, 100, config.promisc, 1000, Error);
  if (pd == NULL) {
    syslog(LOG_ERR, "%s", Error);
    exit(EXIT_SUCCESS);
  }

  if (pcap_compile(pd, &fcode, config.filter, 1, 0) < 0) {
    pcap_perror(pd, "Error");
    printf("Malformed libpcap filter string in bandwidthd.conf\n");
    syslog(LOG_ERR, "Malformed libpcap filter string in bandwidthd.conf");
    exit(EXIT_FAILURE);
  }

  if (pcap_setfilter(pd, &fcode) < 0)
    pcap_perror(pd, "Error");

  switch (DataLink = pcap_datalink(pd)) {
  default:
    if (config.dev)
      printf("Unknown Datalink Type %d, defaulting to ethernet\nPlease forward this error message and a packet sample (captured with \"tcpdump -i %s -s 2000 -n -w capture.cap\") to hinkle@derbyworks.com\n", DataLink, config.dev);
    else
      printf("Unknown Datalink Type %d, defaulting to ethernet\nPlease forward this error message and a packet sample (captured with \"tcpdump -s 2000 -n -w capture.cap\") to hinkle@derbyworks.com\n", DataLink);
    syslog(LOG_INFO, "Unkown datalink type, defaulting to ethernet");
  case DLT_EN10MB:
    syslog(LOG_INFO, "Packet Encoding: Ethernet");
    IP_Offset = 14;	//IP_Offset = sizeof(struct ether_header);
    break;
#ifdef DLT_LINUX_SLL
  case DLT_LINUX_SLL:
    syslog(LOG_INFO, "Packet Encoding: Linux Cooked Socket");
    IP_Offset = 16;
    break;
#endif
#ifdef DLT_RAW
  case DLT_RAW:
    printf("Untested Datalink Type %d\nPlease report to hinkle@derbyworks.net if bandwidthd works for you\non this interface\n", DataLink);
    printf("Packet Encoding:\n\tRaw\n");
    syslog(LOG_INFO, "Untested packet encoding: Raw");
    IP_Offset = 0;
    break;
#endif
  case DLT_IEEE802:
    printf("Untested Datalink Type %d\nPlease report to hinkle@derbyworks.net if bandwidthd works for you\non this interface\n", DataLink);
    printf("Packet Encoding:\nToken Ring\n");
    syslog(LOG_INFO, "Untested packet encoding: Token Ring");
    IP_Offset = 22;
    break;
  }

  if (ForkBackground) {
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
  }

#ifdef HAVE_LIBGD
  if (IPDataStore)	// If there is data in the datastore draw some initial graphs
    {
      syslog(LOG_INFO, "Drawing initial graphs");
      WriteOutWebpages(IntervalStart + config.interval);
    }
#endif // HAVE_LIBGD

  if (pcap_loop(pd, -1, PacketCallback, pcap_userdata) == -1) {
    syslog(LOG_ERR, "Bandwidthd: pcap_loop: %s", pcap_geterr(pd));
    if(config.tag == '1')
      unlink(config.pidfile);
    exit(EXIT_FAILURE);
  }

  pcap_close(pd);

  // We only delete pidfile once
  if(config.tag == '1')
    unlink(config.pidfile);

  exit(EXIT_SUCCESS);
}

void PacketCallback(u_char * user, const struct pcap_pkthdr *h,
		    const u_char * p)
{
  unsigned int Counter;

  u_int caplen = h->caplen;
  const struct ip *ip;

  uint32_t srcip;
  uint32_t dstip;

  struct IPData *ptrIPData;

  if (h->ts.tv_sec > IntervalStart + config.interval)	// Then write out this intervals data and possibly kick off the grapher
    {
      GraphIntervalCount++;
      CommitData(IntervalStart + config.interval);
      IpCount = 0;
      IntervalStart = h->ts.tv_sec;
    }

  caplen -= IP_Offset;	// We're only measuring ip size, so pull off the ethernet header
  p += IP_Offset;		// Move the pointer past the datalink header

  ip = (const struct ip *) p;	// Point ip at the ip header

  if (ip->ip_v != 4)	// then not an ip packet so skip it
    return;

  srcip = ntohl(*(uint32_t *) (&ip->ip_src));
  dstip = ntohl(*(uint32_t *) (&ip->ip_dst));

  for (Counter = 0; Counter < SubnetCount; Counter++) {
    // Packets from a monitored subnet to a monitored subnet will be
    // credited to both ip's

    if (SubnetTable[Counter].ip == (srcip & SubnetTable[Counter].mask)) {
      ptrIPData = FindIp(srcip);	// Return or create this ip's data structure
      if (ptrIPData)
	Credit(&(ptrIPData->Send), ip);

      ptrIPData = FindIp(0);	// Totals
      if (ptrIPData)
	Credit(&(ptrIPData->Send), ip);
    }

    if (SubnetTable[Counter].ip == (dstip & SubnetTable[Counter].mask)) {
      ptrIPData = FindIp(dstip);
      if (ptrIPData)
	Credit(&(ptrIPData->Receive), ip);

      ptrIPData = FindIp(0);
      if (ptrIPData)
	Credit(&(ptrIPData->Receive), ip);
    }
  }
}


// Eliminates duplicate entries and fully included subnets so packets don't get
// counted multiple times
void MonitorSubnet(unsigned int ip, unsigned int mask)
{
  unsigned int subnet = ip & mask;
  int Counter, Counter2;
  struct in_addr addr, addr2;

  addr.s_addr = ntohl(subnet);
  addr2.s_addr = ntohl(mask);

  for (Counter = 0; Counter < SubnetCount; Counter++) {
    if ((SubnetTable[Counter].ip == subnet) && (SubnetTable[Counter].mask == mask)) {
      syslog(LOG_ERR, "Subnet %s/%s already exists, skipping.", inet_ntoa(addr), inet_ntoa(addr2));
      return;
    }
  }

  for (Counter = 0; Counter < SubnetCount; Counter++) {
    if ((SubnetTable[Counter].ip == (ip & SubnetTable[Counter].mask)) && (SubnetTable[Counter].mask < mask)) {
      syslog(LOG_ERR, "Subnet %s/%s is already included, skipping.", inet_ntoa(addr), inet_ntoa(addr2));
      return;
    }
  }

  for (Counter = 0; Counter < SubnetCount; Counter++) {
    if (((SubnetTable[Counter].ip & mask) == subnet) && (SubnetTable[Counter].mask > mask)) {
      syslog(LOG_ERR, "Subnet %s/%s includes already listed subnet, removing smaller entry", inet_ntoa(addr), inet_ntoa(addr2));
      // Shift everything down
      for (Counter2 = Counter; Counter2 < SubnetCount - 1; Counter2++) {
	SubnetTable[Counter2].ip = SubnetTable[Counter2 + 1].ip;
	SubnetTable[Counter2].mask = SubnetTable[Counter2 + 1].mask;
      }
      SubnetCount--;
      Counter--;	// Retest this entry because we replaced it 
    }
  }

  SubnetTable[SubnetCount].mask = mask;
  SubnetTable[SubnetCount].ip = subnet;
  SubnetCount++;
}

inline void Credit(struct Statistics *Stats, const struct ip *ip)
{
  unsigned long size;
  const struct tcphdr *tcp;
  uint16_t sport, dport;

  size = ntohs(ip->ip_len);

  Stats->total += size;

  switch (ip->ip_p) {
  case 6:		// TCP
    tcp = (struct tcphdr *) (ip + 1);
    tcp = (struct tcphdr *) (((char *) tcp) + ((ip->ip_hl - 5) * 4));	// Compensate for IP Options
    Stats->tcp += size;
    sport = ntohs(tcp->TCPHDR_SPORT);
    dport = ntohs(tcp->TCPHDR_DPORT);
    if (sport == 80 || dport == 80 || sport == 443 || dport == 443 ||
	sport == 8080 || dport == 8080 || sport == 3128 || dport == 3128)
      Stats->http += size;

    if (sport == 20 || dport == 20 || sport == 21 || dport == 21)
      Stats->ftp += size;

    if (sport == 1044 || dport == 1044 ||	// Direct File Express
	sport == 1045 || dport == 1045 ||	// ''  <- Dito Marks
	sport == 1214 || dport == 1214 ||	// Grokster, Kaza, Morpheus
	sport == 4661 || dport == 4661 ||	// EDonkey 2000
	sport == 4662 || dport == 4662 ||	// ''
	sport == 4665 || dport == 4665 ||	// ''
	sport == 5190 || dport == 5190 ||	// Song Spy
	sport == 5500 || dport == 5500 ||	// Hotline Connect
	sport == 5501 || dport == 5501 ||	// ''
	sport == 5502 || dport == 5502 ||	// ''
	sport == 5503 || dport == 5503 ||	// ''
	sport == 6346 || dport == 6346 ||	// Gnutella Engine
	sport == 6347 || dport == 6347 ||	// ''
	sport == 6666 || dport == 6666 ||	// Yoink
	sport == 6667 || dport == 6667 ||	// ''
	sport == 7788 || dport == 7788 ||	// Budy Share
	sport == 8888 || dport == 8888 ||	// AudioGnome, OpenNap, Swaptor
	sport == 8889 || dport == 8889 ||	// AudioGnome, OpenNap
	sport == 28864 || dport == 28864 ||	// hotComm                              
	sport == 28865 || dport == 28865)	// hotComm
      Stats->p2p += size;
    break;
  case 17:
    Stats->udp += size;
    break;
  case 1:
    Stats->icmp += size;
    break;
  }
}

// TODO:  Throw away old data!
void DropOldData(long int timestamp)	// Go through the ram datastore and dump old data
{
  struct IPDataStore *DataStore;
  struct IPDataStore *PrevDataStore;
  struct DataStoreBlock *DeletedBlock;

  PrevDataStore = NULL;
  DataStore = IPDataStore;

  // Progress through the linked list until we reach the end
  while (DataStore)	// we have data
    {
      // If the First block is out of date, purge it, if it is the only block
      // purge the node
      while (DataStore->FirstBlock->LatestTimestamp < timestamp - config.range) {
	if ((!DataStore->FirstBlock->Next) && PrevDataStore)	// There is no valid block of data for this ip, so unlink the whole ip
	  {	// Don't bother unlinking the ip if it's the first one, that's too much
	    // Trouble
	    PrevDataStore->Next = DataStore->Next;	// Unlink the node
	    free(DataStore->FirstBlock->Data);	// Free the memory
	    free(DataStore->FirstBlock);
	    free(DataStore);
	    DataStore = PrevDataStore->Next;	// Go to the next node
	    if (!DataStore)
	      return;	// We're done
	  } else if (!DataStore->FirstBlock->Next) {
	    // There is no valid block of data for this ip, and we are 
	    // the first ip, so do nothing 
	    break;	// break out of this loop so the outside loop increments us
	  } else	// Just unlink this block
	    {
	      DeletedBlock = DataStore->FirstBlock;
	      DataStore->FirstBlock = DataStore->FirstBlock->Next;	// Unlink the block
	      free(DeletedBlock->Data);
	      free(DeletedBlock);
	    }
      }

      PrevDataStore = DataStore;
      DataStore = DataStore->Next;
    }
}

#ifdef HAVE_LIBPQ
// Check that tables exist and create them if not
PGconn *CheckPgsqlTables(PGconn * conn)
{
  PGresult *res;

  res = PQexec(conn, "select tablename from pg_tables where tablename='sensors';");

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    syslog(LOG_ERR, "Postresql Select failed: %s", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return (NULL);
  }

  if (PQntuples(res) != 1) {
    PQclear(res);
    res = PQexec(conn, "CREATE TABLE bd_rx_log (sensor_id int, ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, total int, icmp int, udp int, tcp int, ftp int, http int, p2p int); create index bd_rx_log_sensor_id_ip_timestamp_idx on bd_rx_log (sensor_id, ip, timestamp); create index bd_rx_log_sensor_id_timestamp_idx on bd_rx_log(sensor_id, timestamp);");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      syslog(LOG_ERR, "Postresql create table failed: %s", PQerrorMessage(conn));
      PQclear(res);
      PQfinish(conn);
      return (NULL);
    }
    PQclear(res);

    res = PQexec(conn, "CREATE TABLE bd_tx_log (sensor_id int, ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, total int, icmp int, udp int, tcp int, ftp int, http int, p2p int); create index bd_tx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index bd_tx_log_sensor_id_timestamp_idx on bd_tx_log(sensor_id, timestamp);");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      syslog(LOG_ERR, "Postresql create table failed: %s", PQerrorMessage(conn));
      PQclear(res);
      PQfinish(conn);
      return (NULL);
    }
    PQclear(res);

    res = PQexec(conn, "CREATE TABLE bd_rx_total_log (sensor_id int, ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, total int, icmp int, udp int, tcp int, ftp int, http int, p2p int); create index bd_rx_total_log_sensor_id_timestamp_idx on bd_rx_total_log (sensor_id, timestamp);");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      syslog(LOG_ERR, "Postresql create table failed: %s", PQerrorMessage(conn));
      PQclear(res);
      PQfinish(conn);
      return (NULL);
    }
    PQclear(res);

    res = PQexec(conn, "CREATE TABLE bd_tx_total_log (sensor_id int, ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, total int, icmp int, udp int, tcp int, ftp int, http int, p2p int); create index bd_tx_total_log_sensor_id_timestamp_idx on bd_tx_total_log (sensor_id, timestamp);");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      syslog(LOG_ERR, "Postresql create table failed: %s", PQerrorMessage(conn));
      PQclear(res);
      PQfinish(conn);
      return (NULL);
    }
    PQclear(res);

    res = PQexec(conn, "CREATE TABLE sensors ( sensor_id serial PRIMARY KEY, sensor_name varchar, last_connection timestamp with time zone );");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      syslog(LOG_ERR, "Postresql create table failed: %s", PQerrorMessage(conn));
      PQclear(res);
      PQfinish(conn);
      return (NULL);
    }
    PQclear(res);
  } else
    PQclear(res);

  return (conn);
}
#endif

void StoreIPDataInPostgresql(struct IPData IncData[])
{
#ifdef HAVE_LIBPQ
  struct IPData *IPData;
  unsigned int Counter;
  struct Statistics *Stats;
  PGresult *res;
  static PGconn *conn = NULL;
  static char sensor_id[50];
  const char *paramValues[10];
  char *sql1;
  char *sql2;
  char Values[10][50];

  if (!config.output_database == DB_PGSQL)
    return;

  paramValues[0] = Values[0];
  paramValues[1] = Values[1];
  paramValues[2] = Values[2];
  paramValues[3] = Values[3];
  paramValues[4] = Values[4];
  paramValues[5] = Values[5];
  paramValues[6] = Values[6];
  paramValues[7] = Values[7];
  paramValues[8] = Values[8];
  paramValues[9] = Values[9];

  // ************ Initialize the db if it's not already
  if (!conn) {
    /* Connect to the database */
    conn = PQconnectdb(config.pgsql_connect_string);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK) {
      syslog(LOG_ERR, "Connection to database '%s' failed: %s", config.pgsql_connect_string, PQerrorMessage(conn));
      PQfinish(conn);
      conn = NULL;
      return;
    }

    conn = CheckPgsqlTables(conn);
    if (!conn)
      return;

    strncpy(Values[0], config.sensor_id, 50);
    res = PQexecParams(conn, "select sensor_id from sensors where sensor_name = $1;", 1,	/* one param */
		       NULL,	/* let the backend deduce param type */
		       paramValues, NULL,	/* don't need param lengths since text */
		       NULL,	/* default to all text params */
		       0);	/* ask for binary results */

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
      syslog(LOG_ERR, "Postresql SELECT failed: %s", PQerrorMessage(conn));
      PQclear(res);
      PQfinish(conn);
      conn = NULL;
      return;
    }

    if (PQntuples(res)) {
      strncpy(sensor_id, PQgetvalue(res, 0, 0), 50);
      PQclear(res);
    } else {
      res = PQexec(conn, "select nextval('sensors_sensor_id_seq'::Text);");
      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
	syslog(LOG_ERR, "Postresql select failed: %s", PQerrorMessage(conn));
	PQclear(res);
	PQfinish(conn);
	conn = NULL;
	return;
      }

      strncpy(sensor_id, PQgetvalue(res, 0, 0), 50);
      PQclear(res);

      strncpy(Values[1], sensor_id, 50);
      // Insert new sensor id
      res = PQexecParams(conn, "insert into sensors (sensor_name, last_connection, sensor_id) VALUES ($1, now(), $2);", 2,	/* two param */
			 NULL,	/* let the backend deduce param type */
			 paramValues, NULL,	/* don't need param lengths since text */
			 NULL,	/* default to all text params */
			 0);	/* ask for binary results */

      if (PQresultStatus(res) != PGRES_COMMAND_OK) {
	syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
	PQclear(res);
	PQfinish(conn);
	conn = NULL;
	return;
      }
      PQclear(res);
    }
  }

  // Begin transaction

  // **** Perform inserts
  res = PQexecParams(conn, "BEGIN;", 0,	/* zero param */
		     NULL,	/* let the backend deduce param type */
		     NULL, NULL,	/* don't need param lengths since text */
		     NULL,	/* default to all text params */
		     0);	/* ask for binary results */

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    syslog(LOG_ERR, "Postresql BEGIN failed: %s", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    conn = NULL;
    return;
  }
  PQclear(res);

  strncpy(Values[0], sensor_id, 50);

  res = PQexecParams(conn, "update sensors set last_connection = now() where sensor_id = $1;", 1,	/* one param */
		     NULL,	/* let the backend deduce param type */
		     paramValues, NULL,	/* don't need param lengths since text */
		     NULL,	/* default to all text params */
		     0);	/* ask for binary results */

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    syslog(LOG_ERR, "Postresql UPDATE failed: %s", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    conn = NULL;
    return;
  }
  PQclear(res);

  Values[0][49] = '\0';
  snprintf(Values[1], 50, "%llu", config.interval);
  for (Counter = 0; Counter < IpCount; Counter++) {
    IPData = &IncData[Counter];

    if (IPData->ip == 0) {
      // This optimization allows us to quickly draw totals graphs for a sensor
      sql1 = "INSERT INTO bd_tx_total_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);";
      sql2 = "INSERT INTO bd_rx_total_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);";
    } else {
      sql1 = "INSERT INTO bd_tx_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);";
      sql2 = "INSERT INTO bd_rx_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);";
    }

    HostIp2CharIp(IPData->ip, Values[2]);

    Stats = &(IPData->Send);
    if (Stats->total > 512)	// Don't log empty sets
      {
	// Log data in kilobytes
	snprintf(Values[3], 50, "%llu", (long long unsigned int) ((((double) Stats->total) / 1024.0) + 0.5));
	snprintf(Values[4], 50, "%llu", (long long unsigned int) ((((double) Stats->icmp) / 1024.0) + 0.5));
	snprintf(Values[5], 50, "%llu", (long long unsigned int) ((((double) Stats->udp) / 1024.0) + 0.5));
	snprintf(Values[6], 50, "%llu", (long long unsigned int) ((((double) Stats->tcp) / 1024.0) + 0.5));
	snprintf(Values[7], 50, "%llu", (long long unsigned int) ((((double) Stats->ftp) / 1024.0) + 0.5));
	snprintf(Values[8], 50, "%llu", (long long unsigned int) ((((double) Stats->http) / 1024.0) + 0.5));
	snprintf(Values[9], 50, "%llu", (long long unsigned int) ((((double) Stats->p2p) / 1024.0) + 0.5));

	res = PQexecParams(conn, sql1, 10,	/* nine param */
			   NULL,	/* let the backend deduce param type */
			   paramValues, NULL,	/* don't need param lengths since text */
			   NULL,	/* default to all text params */
			   1);	/* ask for binary results */

	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
	  syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
	  PQclear(res);
	  PQfinish(conn);
	  conn = NULL;
	  return;
	}
	PQclear(res);
      }
    Stats = &(IPData->Receive);
    if (Stats->total > 512)	// Don't log empty sets
      {
	snprintf(Values[3], 50, "%llu", (long long unsigned int) ((((double) Stats->total) / 1024.0) + 0.5));
	snprintf(Values[4], 50, "%llu", (long long unsigned int) ((((double) Stats->icmp) / 1024.0) + 0.5));
	snprintf(Values[5], 50, "%llu", (long long unsigned int) ((((double) Stats->udp) / 1024.0) + 0.5));
	snprintf(Values[6], 50, "%llu", (long long unsigned int) ((((double) Stats->tcp) / 1024.0) + 0.5));
	snprintf(Values[7], 50, "%llu", (long long unsigned int) ((((double) Stats->ftp) / 1024.0) + 0.5));
	snprintf(Values[8], 50, "%llu", (long long unsigned int) ((((double) Stats->http) / 1024.0) + 0.5));
	snprintf(Values[9], 50, "%llu", (long long unsigned int) ((((double) Stats->p2p) / 1024.0) + 0.5));

	res = PQexecParams(conn, sql2, 10,	/* seven param */
			   NULL,	/* let the backend deduce param type */
			   paramValues, NULL,	/* don't need param lengths since text */
			   NULL,	/* default to all text params */
			   1);	/* ask for binary results */

	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
	  syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
	  PQclear(res);
	  PQfinish(conn);
	  conn = NULL;
	  return;
	}
	PQclear(res);
      }
  }
  // Commit transaction
  res = PQexecParams(conn, "COMMIT;", 0,	/* zero param */
		     NULL,	/* let the backend deduce param type */
		     NULL, NULL,	/* don't need param lengths since text */
		     NULL,	/* default to all text params */
		     0);	/* ask for binary results */

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    syslog(LOG_ERR, "Postresql COMMIT failed: %s", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    conn = NULL;
    return;
  }
  PQclear(res);
#else
  syslog(LOG_ERR, "Postgresql logging selected but postgresql support is not compiled into binary.  Please check the documentation in README, distributed with this software.");
#endif
}

#ifdef HAVE_LIBMYSQLCLIENT
// Check that tables exist and create them if not
MYSQL *CheckMysqlTables(MYSQL * conn)
{
  MYSQL_RES *res;
  int ret;

  ret = mysql_query(conn, "SHOW TABLES LIKE 'sensors'");
  if (ret) {
    syslog(LOG_ERR, "MySQL SHOW TABLES failed: %s", mysql_error(conn));
    mysql_close(conn);
    return (NULL);
  }

  res = mysql_store_result(conn);
  if (!res) {
    syslog(LOG_ERR, "MySQL store_result failed: %s", mysql_error(conn));
    mysql_close(conn);
    return (NULL);
  }

  if (mysql_num_rows(res) != 1) {
    mysql_free_result(res);

    ret = mysql_query(conn, "CREATE TABLE bd_rx_log (sensor_id INT, ip INT UNSIGNED, timestamp TIMESTAMP, sample_duration INT, total INT, icmp INT, udp INT, tcp INT, ftp INT, http INT, p2p INT, INDEX bd_rx_log_sensors_id_ip_timestamp_idx (sensor_id, ip, timestamp), INDEX bd_rx_log_sensor_id_timestamp_idx (sensor_id, timestamp)) TYPE=innodb");
    if (ret) {
      syslog(LOG_ERR, "MySQL CREATE TABLE failed: %s", mysql_error(conn));
      mysql_close(conn);
      return (NULL);
    }

    ret = mysql_query(conn, "CREATE TABLE bd_tx_log (sensor_id INT, ip INT UNSIGNED, timestamp TIMESTAMP, sample_duration INT, total INT, icmp INT, udp INT, tcp INT, ftp INT, http INT, p2p INT, INDEX bd_tx_log_sensors_id_ip_timestamp_idx (sensor_id, ip, timestamp), INDEX bd_tx_log_sensor_id_timestamp_idx (sensor_id, timestamp)) TYPE=innodb");
    if (ret) {
      syslog(LOG_ERR, "MySQL CREATE TABLE failed: %s", mysql_error(conn));
      mysql_close(conn);
      return (NULL);
    }

    ret = mysql_query(conn, "CREATE TABLE bd_rx_total_log (sensor_id INT, ip INT UNSIGNED, timestamp TIMESTAMP, sample_duration INT, total INT, icmp INT, udp INT, tcp INT, ftp INT, http INT, p2p INT, INDEX bd_rx_total_log_sensors_id_timestamp_idx (sensor_id, timestamp)) TYPE=innodb");
    if (ret) {
      syslog(LOG_ERR, "MySQL CREATE TABLE failed: %s", mysql_error(conn));
      mysql_close(conn);
      return (NULL);
    }

    ret = mysql_query(conn, "CREATE TABLE bd_tx_total_log (sensor_id INT, ip INT UNSIGNED, timestamp TIMESTAMP, sample_duration INT, total INT, icmp INT, udp INT, tcp INT, ftp INT, http INT, p2p INT, INDEX bd_tx_total_log_sensors_id_timestamp_idx (sensor_id, timestamp)) TYPE=innodb");
    if (ret) {
      syslog(LOG_ERR, "MySQL CREATE TABLE failed: %s", mysql_error(conn));
      mysql_close(conn);
      return (NULL);
    }

    ret = mysql_query(conn, "CREATE TABLE sensors (sensor_id INT PRIMARY KEY AUTO_INCREMENT, sensor_name VARCHAR(255) UNIQUE NOT NULL, last_connection TIMESTAMP) TYPE=innodb");
    if (ret) {
      syslog(LOG_ERR, "MySQL CREATE TABLE failed: %s", mysql_error(conn));
      mysql_close(conn);
      return (NULL);
    }
  } else
    mysql_free_result(res);

  return (conn);
}

// MySQL prepared statements queries
#define MYSQL_TX_TOTAL "INSERT INTO bd_tx_total_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES(?, ?, 0, ?, ?, ?, ?, ?, ?, ?)"
#define MYSQL_TX "INSERT INTO bd_tx_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define MYSQL_RX_TOTAL "INSERT INTO bd_rx_total_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES(?, ?, 0, ?, ?, ?, ?, ?, ?, ?)"
#define MYSQL_RX "INSERT INTO bd_rx_log (sensor_id, sample_duration, ip, total, icmp, udp, tcp, ftp, http, p2p) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"

void MysqlStmtsClose(MYSQL_STMT **stmts)
{
  mysql_stmt_close(stmts[0]);
  mysql_stmt_close(stmts[1]);
  mysql_stmt_close(stmts[2]);
  mysql_stmt_close(stmts[3]);
}

MYSQL *MysqlStmtsInit(MYSQL *conn, MYSQL_STMT **stmts,
		      unsigned long long *data, my_ulonglong *sensor_id, uint32_t *ip)
{
  unsigned int Counter;
  MYSQL_BIND binds[10];
  
  stmts[0] = mysql_stmt_init(conn);
  if(!stmts[0]) {
    syslog(LOG_ERR, "MySQL stmt_init failed: %s", mysql_error(conn));
    mysql_close(conn);
    return NULL;
  }

  stmts[1] = mysql_stmt_init(conn);
  if(!stmts[1]) {
    syslog(LOG_ERR, "MySQL stmt_init failed: %s", mysql_error(conn));
    mysql_stmt_close(stmts[0]);
    mysql_close(conn);
    return NULL;
  }

  stmts[2] = mysql_stmt_init(conn);
  if(!stmts[2]) {
    syslog(LOG_ERR, "MySQL stmt_init failed: %s", mysql_error(conn));
    mysql_stmt_close(stmts[0]);
    mysql_stmt_close(stmts[1]);
    mysql_close(conn);
    return NULL;
  }

  stmts[3] = mysql_stmt_init(conn);
  if(!stmts[3]) {
    syslog(LOG_ERR, "MySQL stmt_init failed: %s", mysql_error(conn));
    mysql_stmt_close(stmts[0]);
    mysql_stmt_close(stmts[1]);
    mysql_stmt_close(stmts[2]);
    mysql_close(conn);
    return NULL;
  }

  if(mysql_stmt_prepare(stmts[0], MYSQL_TX_TOTAL, strlen(MYSQL_TX_TOTAL))) {
    syslog(LOG_ERR, "MySQL stmt_prepare failed: %s", mysql_stmt_error(stmts[0]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  if(mysql_stmt_prepare(stmts[1], MYSQL_TX, strlen(MYSQL_TX))) {
    syslog(LOG_ERR, "MySQL stmt_prepare failed: %s", mysql_stmt_error(stmts[1]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  if(mysql_stmt_prepare(stmts[2], MYSQL_RX_TOTAL, strlen(MYSQL_RX_TOTAL))) {
    syslog(LOG_ERR, "MySQL stmt_prepare failed: %s", mysql_stmt_error(stmts[2]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  if(mysql_stmt_prepare(stmts[3], MYSQL_RX, strlen(MYSQL_RX))) {
    syslog(LOG_ERR, "MySQL stmt_prepare failed: %s", mysql_stmt_error(stmts[3]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }  

  memset(binds, 0, sizeof(binds));

  // sensor_id
  binds[0].buffer_type = MYSQL_TYPE_LONGLONG;
  binds[0].buffer = sensor_id;
  binds[0].is_unsigned = TRUE;

  // sample_duration
  binds[1].buffer_type = MYSQL_TYPE_LONGLONG;
  binds[1].buffer = &config.interval;
  binds[1].is_unsigned = TRUE;

  // statistics
  for(Counter = 2; Counter < 9; Counter++) {
    binds[Counter].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[Counter].buffer = &data[Counter-2];
    binds[Counter].is_unsigned = TRUE;
  }

  if(mysql_stmt_bind_param(stmts[0], binds)) {
    syslog(LOG_ERR, "MySQL bind_param failed: %s", mysql_stmt_error(stmts[0]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  if(mysql_stmt_bind_param(stmts[2], binds)) {
    syslog(LOG_ERR, "MySQL bind_param failed: %s", mysql_stmt_error(stmts[2]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  // ip
  binds[2].buffer_type = MYSQL_TYPE_LONG;
  binds[2].buffer = ip;
  binds[2].is_unsigned = TRUE;

  for(Counter = 3; Counter < 10; Counter++) {
    binds[Counter].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[Counter].buffer = &data[Counter-3];
    binds[Counter].is_unsigned = TRUE;
  }

  if(mysql_stmt_bind_param(stmts[1], binds)) {
    syslog(LOG_ERR, "MySQL bind_param failed: %s", mysql_stmt_error(stmts[1]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  if(mysql_stmt_bind_param(stmts[3], binds)) {
    syslog(LOG_ERR, "MySQL bind_param failed: %s", mysql_stmt_error(stmts[3]));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    return NULL;
  }

  return conn;
}

#endif				// HAVE_LIBMYSQLCLIENT

void StoreIPDataInMysql(struct IPData IncData[])
{
#ifdef HAVE_LIBMYSQLCLIENT
  struct IPData *IPData;
  unsigned int Counter;
  struct Statistics *Stats;

  static MYSQL *conn = NULL;
  static MYSQL mysql;
  static MYSQL_STMT *stmts[4];
  static my_ulonglong sensor_id;
  static unsigned long long data[7];
  static uint32_t ip;
  MYSQL_RES *res;
  MYSQL_ROW row;
  unsigned long *lengths;

  int ret;
  char sql[1024];
  char tmp[20];
  char *sensor_name;

  if (!config.output_database == DB_MYSQL)
    return;

  // ************ Initialize the db if it's not already
  if (!conn) {
    if(!mysql_init(&mysql)) {
      syslog(LOG_ERR, "Insufficient memory to allocate MYSQL structure");
      return;
    }

    /* Connect to the database */
    conn = mysql_real_connect(&mysql, config.mysql_host, config.mysql_user,
			      config.mysql_pass, config.mysql_dbname, config.mysql_port, NULL, 0);

    /* Check to see that the backend connection was successfully made */
    if (!conn) {
      syslog(LOG_ERR, "Connection to database '%s' on '%s' with user '%s' failed: %s",
	     config.mysql_dbname, config.mysql_host, config.mysql_user, mysql_error(&mysql));
      mysql_close(&mysql); // Deallocates memory used by MYSQL structure
      return;
    }

    conn = CheckMysqlTables(conn);
    if (!conn)
      return;

    conn = MysqlStmtsInit(conn, stmts, data, &sensor_id, &ip);
    if (!conn)
      return;

    // Escape sensor_id
    sensor_name = (char *) malloc(2*strlen(config.sensor_id)+1);
    mysql_real_escape_string(conn, sensor_name, config.sensor_id, strlen(config.sensor_id));

    // Retrieve sensor_id from database
    snprintf(sql, sizeof(sql), "SELECT sensor_id FROM sensors WHERE sensor_name='%s'", sensor_name);
    ret = mysql_query(conn, sql);
    if (ret) {
      syslog(LOG_ERR, "MySQL SELECT failed: %s", mysql_error(conn));
      free(sensor_name);
      MysqlStmtsClose(stmts);
      mysql_close(conn);
      conn = NULL;
      return;
    }

    res = mysql_store_result(conn);
    if (!res) {
      syslog(LOG_ERR, "MySQL store_result failed: %s", mysql_error(conn));
      free(sensor_name);
      MysqlStmtsClose(stmts);
      mysql_close(conn);
      conn = NULL;
      return;
    }

    // Sensor is already in database
    if (mysql_num_rows(res)) {
      // No longer needed
      free(sensor_name);

      row = mysql_fetch_row(res);
      if (!row) {
	syslog(LOG_ERR, "MySQL fetch_row failed: %s", mysql_error(conn));
	mysql_free_result(res);
	MysqlStmtsClose(stmts);
	mysql_close(conn);
	conn = NULL;
	return;
      }

      if (!row[0]) {
	syslog(LOG_ERR, "MySQL NULL value encountered");
	mysql_free_result(res);
	MysqlStmtsClose(stmts);
	mysql_close(conn);
	conn = NULL;
	return;
      }

      lengths = mysql_fetch_lengths(res);
      if (!lengths) {
	syslog(LOG_ERR, "MySQL fetch_lengths failed: %s", mysql_error(conn));
	mysql_free_result(res);
	MysqlStmtsClose(stmts);
	mysql_close(conn);
	return;
      }

      snprintf(tmp, sizeof(tmp), "%.*s", (int) lengths[0], row[0]);
      sensor_id = atoll(tmp);

      mysql_free_result(res);
    } else { // First connection of this sensor, create new entry in sensors table
      mysql_free_result(res);
      snprintf(sql, sizeof(sql), "INSERT INTO sensors (sensor_name, last_connection, sensor_id) VALUES('%s', NOW(), NULL)", sensor_name);
      free(sensor_name);
      ret = mysql_query(conn, sql);
      if (ret) {
	syslog(LOG_ERR, "MySQL INSERT failed: %s", mysql_error(conn));
	MysqlStmtsClose(stmts);
	mysql_close(conn);
	conn = NULL;
	return;
      }

      sensor_id = mysql_insert_id(conn);
    }
  }

  // Begin transaction
  ret = mysql_query(conn, "BEGIN");
  if (ret) {
    syslog(LOG_ERR, "MySQL BEGIN failed: %s", mysql_error(conn));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    conn = NULL;
    return;
  }

  snprintf(sql, sizeof(sql), "UPDATE sensors SET last_connection = NOW() WHERE sensor_id = %llu", sensor_id);
  ret = mysql_query(conn, sql);
  if (ret) {
    syslog(LOG_ERR, "MySQL UPDATE failed: %s", mysql_error(conn));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    conn = NULL;
    return;
  }

  for (Counter = 0; Counter < IpCount; Counter++) {
    IPData = &IncData[Counter];

    Stats = &(IPData->Send);
    if (Stats->total > 512) {
      data[0] = (long long unsigned int) ((((double) Stats->total) / 1024.0) + 0.5);
      data[1] = (long long unsigned int) ((((double) Stats->icmp) / 1024.0) + 0.5);
      data[2] = (long long unsigned int) ((((double) Stats->udp) / 1024.0) + 0.5);
      data[3] = (long long unsigned int) ((((double) Stats->tcp) / 1024.0) + 0.5);
      data[4] = (long long unsigned int) ((((double) Stats->ftp) / 1024.0) + 0.5);
      data[5] = (long long unsigned int) ((((double) Stats->http) / 1024.0) + 0.5);
      data[6] = (long long unsigned int) ((((double) Stats->p2p) / 1024.0) + 0.5);

      if (IPData->ip == 0) {
	if(mysql_stmt_execute(stmts[0])) {
	  syslog(LOG_ERR, "MySQL INSERT failed: %s", mysql_stmt_error(stmts[0]));
	  if(mysql_stmt_errno(stmts[0]) == CR_SERVER_LOST) {
	    // First, deallocate memory from previously used prepared statements
	    MysqlStmtsClose(stmts);
	    MysqlStmtsInit(conn, stmts, data, &sensor_id, &ip);
	    if(mysql_stmt_execute(stmts[0])) {
	      syslog(LOG_ERR, "MySQL INSERT retry failed: %s", mysql_stmt_error(stmts[0]));
	      MysqlStmtsClose(stmts);
	      mysql_close(conn);
	      conn = NULL;
	      return;
	    } else
	      // Not quite reconnect since MySQL library handles it automatically,
	      // but rather reinitialization of prepared statements
	      syslog(LOG_ERR, "Reconnect succeded");
	  } else {
	    MysqlStmtsClose(stmts);
	    mysql_close(conn);
	    conn = NULL;
	    return;
	  }
	}
      }
      else {
	ip = IPData->ip;
	if(mysql_stmt_execute(stmts[1])) {
 	  syslog(LOG_ERR, "MySQL INSERT failed: %s", mysql_stmt_error(stmts[1]));
	  if(mysql_stmt_errno(stmts[1]) == CR_SERVER_LOST) {
	    // First, deallocate memory from previously used prepared statements
	    MysqlStmtsClose(stmts);
	    MysqlStmtsInit(conn, stmts, data, &sensor_id, &ip);
	    if(mysql_stmt_execute(stmts[1])) {
	      syslog(LOG_ERR, "MySQL INSERT retry failed: %s", mysql_stmt_error(stmts[1]));
	      MysqlStmtsClose(stmts);
	      mysql_close(conn);
	      conn = NULL;
	      return;
	    } else
	      // Not quite reconnect since MySQL library handles it automatically,
	      // but rather reinitialization of prepared statements
	      syslog(LOG_ERR, "Reconnect succeded");
	  } else {
	    MysqlStmtsClose(stmts);
	    mysql_close(conn);
	    conn = NULL;
	    return;
	  }
	}
      }
    }

    Stats = &(IPData->Receive);
    if (Stats->total > 512) {
      data[0] = (long long unsigned int) ((((double) Stats->total) / 1024.0) + 0.5);
      data[1] = (long long unsigned int) ((((double) Stats->icmp) / 1024.0) + 0.5);
      data[2] = (long long unsigned int) ((((double) Stats->udp) / 1024.0) + 0.5);
      data[3] = (long long unsigned int) ((((double) Stats->tcp) / 1024.0) + 0.5);
      data[4] = (long long unsigned int) ((((double) Stats->ftp) / 1024.0) + 0.5);
      data[5] = (long long unsigned int) ((((double) Stats->http) / 1024.0) + 0.5);
      data[6] = (long long unsigned int) ((((double) Stats->p2p) / 1024.0) + 0.5);
      if (IPData->ip == 0) {
	if(mysql_stmt_execute(stmts[2])) {
	  syslog(LOG_ERR, "MySQL INSERT failed: %s", mysql_stmt_error(stmts[2]));
	  if(mysql_stmt_errno(stmts[2]) == CR_SERVER_LOST) {
	    // First, deallocate memory from previously used prepared statements
	    MysqlStmtsClose(stmts);
	    MysqlStmtsInit(conn, stmts, data, &sensor_id, &ip);
	    if(mysql_stmt_execute(stmts[2])) {
	      syslog(LOG_ERR, "MySQL INSERT retry failed: %s", mysql_stmt_error(stmts[2]));
	      MysqlStmtsClose(stmts);
	      mysql_close(conn);
	      conn = NULL;
	      return;
	    } else
	      // Not quite reconnect since MySQL library handles it automatically,
	      // but rather reinitialization of prepared statements
	      syslog(LOG_ERR, "Reconnect succeded");
	  } else {
	    MysqlStmtsClose(stmts);
	    mysql_close(conn);
	    conn = NULL;
	    return;
	  }
	}
      } else {
	ip = IPData->ip;
	if(mysql_stmt_execute(stmts[3])) {
	  syslog(LOG_ERR, "MySQL INSERT failed: %s", mysql_stmt_error(stmts[3]));
	  if(mysql_stmt_errno(stmts[3]) == CR_SERVER_LOST) {
	    // First, deallocate memory from previously used prepared statements
	    MysqlStmtsClose(stmts);
	    MysqlStmtsInit(conn, stmts, data, &sensor_id, &ip);
	    if(mysql_stmt_execute(stmts[3])) {
	      syslog(LOG_ERR, "MySQL INSERT retry failed: %s", mysql_stmt_error(stmts[3]));
	      MysqlStmtsClose(stmts);
	      mysql_close(conn);
	      conn = NULL;
	      return;
	    } else
	      // Not quite reconnect since MySQL library handles it automatically,
	      // but rather reinitialization of prepared statements
	      syslog(LOG_ERR, "Reconnect succeded");
	  } else {
	    MysqlStmtsClose(stmts);
	    mysql_close(conn);
	    conn = NULL;
	    return;
	  }
	}
      }
    }
  }

  ret = mysql_query(conn, "COMMIT");
  if (ret) {
    syslog(LOG_ERR, "MySQL COMMIT failed: %s", mysql_error(conn));
    MysqlStmtsClose(stmts);
    mysql_close(conn);
    conn = NULL;
    return;
  }
#else
  syslog(LOG_ERR, "MySQL logging selected but MySQL support is not compiled into binary.  Please check the documentation in README, distributed with this software.");
#endif				// HAVE_LIBMYSQLCLIENT
}

void StoreIPDataInDatabase(struct IPData IncData[])
{
  if (config.output_database == DB_PGSQL)
    StoreIPDataInPostgresql(IncData);
  else if (config.output_database == DB_MYSQL)
    StoreIPDataInMysql(IncData);
}

void StoreIPDataInCDF(struct IPData IncData[])
{
  struct IPData *IPData;
  unsigned int Counter;
  FILE *cdf;
  struct Statistics *Stats;
  char IPBuffer[50];
  char logfile[MAX_FILENAME];

  snprintf(logfile, MAX_FILENAME, "%s/log.%c.0.cdf", config.log_dir, config.tag);

  cdf = fopen(logfile, "at");

  for (Counter = 0; Counter < IpCount; Counter++) {
    IPData = &IncData[Counter];
    HostIp2CharIp(IPData->ip, IPBuffer);
    fprintf(cdf, "%s,%lu,", IPBuffer, IPData->timestamp);
    Stats = &(IPData->Send);
    fprintf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
	    Stats->total, Stats->icmp, Stats->udp, Stats->tcp,
	    Stats->ftp, Stats->http, Stats->p2p);
    Stats = &(IPData->Receive);
    fprintf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
	    Stats->total, Stats->icmp, Stats->udp, Stats->tcp,
	    Stats->ftp, Stats->http, Stats->p2p);
  }
  fclose(cdf);
}

void _StoreIPDataInRam(struct IPData *IPData)
{
  struct IPDataStore *DataStore;
  struct DataStoreBlock *DataStoreBlock;

  if (!IPDataStore)	// we need to create the first entry
    {
      // Allocate Datastore for this IP
      IPDataStore = malloc(sizeof(struct IPDataStore));

      IPDataStore->ip = IPData->ip;
      IPDataStore->Next = NULL;

      // Allocate it's first block of storage
      IPDataStore->FirstBlock =
	malloc(sizeof(struct DataStoreBlock));
      IPDataStore->FirstBlock->LatestTimestamp = 0;

      IPDataStore->FirstBlock->NumEntries = 0;
      IPDataStore->FirstBlock->Data =
	calloc(IPDATAALLOCCHUNKS, sizeof(struct IPData));
      IPDataStore->FirstBlock->Next = NULL;
      if (!IPDataStore->FirstBlock || !IPDataStore->FirstBlock->Data) {
	syslog(LOG_ERR,
	       "Could not allocate datastore! Exiting!");
	exit(EXIT_FAILURE);
      }
    }

  DataStore = IPDataStore;

  // Take care of first case
  while (DataStore)	// Is not null
    {
      if (DataStore->ip == IPData->ip)	// then we have the right store
	{
	  DataStoreBlock = DataStore->FirstBlock;

	  while (DataStoreBlock)	// is not null
	    {
	      if (DataStoreBlock->NumEntries < IPDATAALLOCCHUNKS)	// We have a free spot
		{
		  memcpy(&DataStoreBlock->Data[DataStoreBlock->NumEntries++], IPData, sizeof(struct IPData));
		  DataStoreBlock->LatestTimestamp = IPData->timestamp;
		  return;
		} else {
		  if (!DataStoreBlock->Next)	// there isn't another block, add one
		    {
		      DataStoreBlock->Next = malloc(sizeof(struct DataStoreBlock));
		      DataStoreBlock->Next->LatestTimestamp = 0;
		      DataStoreBlock->Next->NumEntries = 0;
		      DataStoreBlock->Next->Data =calloc(IPDATAALLOCCHUNKS, sizeof(struct IPData));
		      DataStoreBlock->Next->Next = NULL;
		    }

		  DataStoreBlock = DataStoreBlock->Next;
		}
	    }

	  return;
	} else {
	  if (!DataStore->Next)	// there is no entry for this ip, so lets make one.
	    {
	      // Allocate Datastore for this IP
	      DataStore->Next = malloc(sizeof(struct IPDataStore));

	      DataStore->Next->ip = IPData->ip;
	      DataStore->Next->Next = NULL;

	      // Allocate it's first block of storage
	      DataStore->Next->FirstBlock = malloc(sizeof(struct DataStoreBlock));
	      DataStore->Next->FirstBlock->LatestTimestamp = 0;
	      DataStore->Next->FirstBlock->NumEntries = 0;
	      DataStore->Next->FirstBlock->Data = calloc(IPDATAALLOCCHUNKS, sizeof(struct IPData));
	      DataStore->Next->FirstBlock->Next = NULL;
	    }

	  DataStore = DataStore->Next;
	}
    }
}

void StoreIPDataInRam(struct IPData IncData[])
{
  unsigned int Counter;

  for (Counter = 0; Counter < IpCount; Counter++)
    _StoreIPDataInRam(&IncData[Counter]);
}

void CommitData(time_t timestamp)
{
#ifdef HAVE_LIBGD
  static int MayGraph = TRUE;
#endif // HAVE_LIBGD
  unsigned int Counter;
  struct stat StatBuf;
  char logname1[MAX_FILENAME];
  char logname2[MAX_FILENAME];
  int offset;
  // Set the timestamps
  for (Counter = 0; Counter < IpCount; Counter++)
    IpTable[Counter].timestamp = timestamp;

  // Output modules
  // Only call this from first thread
  if (config.output_database && config.tag == '1')
    StoreIPDataInDatabase(IpTable);

  if (config.output_cdf) {
    // TODO: This needs to be moved into the forked section, but I don't want to 
    //      deal with that right now (Heavy disk io may make us drop packets)
    StoreIPDataInCDF(IpTable);

    if (RotateLogs >= config.range / RANGE1)	// We set this++ on HUP
      {
	snprintf(logname1, MAX_FILENAME, "%s/log.%c.%n4.cdf", config.log_dir, config.tag, &offset);
	snprintf(logname2, MAX_FILENAME, "%s/log.%c.5.cdf", config.log_dir, config.tag);

	if (!stat(logname2, &StatBuf))	// File exists
	  unlink(logname2);

	if (!stat(logname1, &StatBuf))	// File exists
	  rename(logname1, logname2);
	logname1[offset] = '3';
	logname2[offset] = '4';
	if (!stat(logname1, &StatBuf))	// File exists
	  rename(logname1, logname2);
	logname1[offset] = '2';
	logname2[offset] = '3';
	if (!stat(logname1, &StatBuf))	// File exists
	  rename(logname1, logname2);
	logname1[offset] = '1';
	logname2[offset] = '2';
	if (!stat(logname1, &StatBuf))	// File exists
	  rename(logname1, logname2);
	logname1[offset] = '0';
	logname2[offset] = '1';
	if (!stat(logname1, &StatBuf))	// File exists
	  rename(logname1, logname2);
	fclose(fopen(logname1, "at"));	// Touch file
	RotateLogs = FALSE;
      }
  }

#ifdef HAVE_LIBGD
  if (config.graph) {
    StoreIPDataInRam(IpTable);

    // Reap a couple zombies
    if (waitpid(-1, NULL, WNOHANG))	// A child was reaped
      MayGraph = TRUE;

    if (GraphIntervalCount % config.skip_intervals == 0 && MayGraph) {
      MayGraph = FALSE;
      /* If WriteOutWebpages fails, reenable graphing since there won't
       * be any children to reap.
       */
      if (WriteOutWebpages(timestamp))
	MayGraph = TRUE;
    } else if (GraphIntervalCount % config.skip_intervals == 0)
      syslog(LOG_INFO, "Previouse graphing run not complete... Skipping current run");

    DropOldData(timestamp);
  }
#endif // HAVE_LIBGD
}


int RCDF_Test(char *filename)
{
  // Determine if the first date in the file is before the cutoff
  // return FALSE on error
  FILE *cdf;
  char ipaddrBuffer[16];
  time_t timestamp;

  if (!(cdf = fopen(filename, "rt")))
    return FALSE;
  fseek(cdf, 10, SEEK_END);	// fseek to near end of file
  while (fgetc(cdf) != '\n')	// rewind to last newline
    {
      if (fseek(cdf, -2, SEEK_CUR) == -1)
	break;
    }
  if (fscanf(cdf, " %15[0-9.],%lu,", ipaddrBuffer, &timestamp) != 2) {
    syslog(LOG_ERR, "%s is corrupted, skipping", filename);
    return FALSE;
  }
  fclose(cdf);
  if (timestamp < time(NULL) - config.range)
    return FALSE;	// There is no data in this file from after the cutoff
  else
    return TRUE;	// This file has data from after the cutoff
}


void RCDF_PositionStream(FILE * cdf)
{
  time_t timestamp;
  time_t current_timestamp;
  char ipaddrBuffer[16];

  current_timestamp = time(NULL);

  fseek(cdf, 0, SEEK_END);
  timestamp = current_timestamp;
  while (timestamp > current_timestamp - config.range) {
    // What happenes if we seek past the beginning of the file?
    if (fseek(cdf, -512 * 1024, SEEK_CUR) == -1 || ferror(cdf)) {	// fseek returned error, just seek to beginning
      clearerr(cdf);
      fseek(cdf, 0, SEEK_SET);
      return;
    }
    while (fgetc(cdf) != '\n' && !feof(cdf));	// Read to next line
    ungetc('\n', cdf);	// Just so the fscanf mask stays identical
    if (fscanf(cdf, " %15[0-9.],%lu,", ipaddrBuffer, &timestamp) != 2) {
      syslog(LOG_ERR, "Unknown error while scanning for beginning of data...\n");
      return;
    }
  }
  while (fgetc(cdf) != '\n' && !feof(cdf));
  ungetc('\n', cdf);
}

void RCDF_Load(FILE * cdf)
{
  time_t timestamp;
  time_t current_timestamp = 0;
  struct in_addr ipaddr;
  struct IPData *ip = NULL;
  char ipaddrBuffer[16];
  unsigned long int Counter = 0;
  unsigned long int IntervalsRead = 0;

  for (Counter = 0; !feof(cdf) && !ferror(cdf); Counter++) {
    if (fscanf(cdf, " %15[0-9.],%lu,", ipaddrBuffer, &timestamp) != 2)
      goto End_RecoverDataFromCdf;

    if (!timestamp)	// First run through loop
      current_timestamp = timestamp;

    if (timestamp != current_timestamp) {	// Dump to datastore
      StoreIPDataInRam(IpTable);
      IpCount = 0;	// Reset Traffic Counters
      current_timestamp = timestamp;
      IntervalsRead++;
    }
    inet_aton(ipaddrBuffer, &ipaddr);
    ip = FindIp(ntohl(ipaddr.s_addr));
    ip->timestamp = timestamp;

    if (fscanf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
	       &ip->Send.total, &ip->Send.icmp, &ip->Send.udp,
	       &ip->Send.tcp, &ip->Send.ftp, &ip->Send.http,
	       &ip->Send.p2p) != 7
	|| fscanf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu",
		  &ip->Receive.total, &ip->Receive.icmp,
		  &ip->Receive.udp, &ip->Receive.tcp,
		  &ip->Receive.ftp, &ip->Receive.http,
		  &ip->Receive.p2p) != 7)
      goto End_RecoverDataFromCdf;
  }

 End_RecoverDataFromCdf:
  StoreIPDataInRam(IpTable);
  syslog(LOG_INFO, "Finished recovering %lu records", Counter);
  DropOldData(time(NULL));	// Dump the extra data
  if (!feof(cdf))
    syslog(LOG_ERR, "Failed to parse part of log file. Giving up on the file");
  IpCount = 0;		// Reset traffic Counters
  fclose(cdf);
}

void RecoverDataFromCDF(void)
{
  FILE *cdf;
  char index[] = "012345";
  char logname[MAX_FILENAME];
  int offset;
  int Counter;
  int First = FALSE;

  snprintf(logname, MAX_FILENAME, "%s/log.%c.%n0.cdf",
	   config.log_dir, config.tag, &offset);

  for (Counter = 5; Counter >= 0; Counter--) {
    logname[offset] = index[Counter];
    if (RCDF_Test(logname))
      break;
  }

  First = TRUE;
  for (; Counter >= 0; Counter--) {
    logname[offset] = index[Counter];
    if ((cdf = fopen(logname, "rt"))) {
      syslog(LOG_INFO, "Recovering from %s", logname);
      if (First) {
	RCDF_PositionStream(cdf);
	First = FALSE;
      }
      RCDF_Load(cdf);
    }
  }
}

// ****** FindIp **********
// ****** Returns or allocates an Ip's data structure

inline struct IPData *FindIp(uint32_t ipaddr)
{
  unsigned int Counter;

  for (Counter = 0; Counter < IpCount; Counter++)
    if (IpTable[Counter].ip == ipaddr)
      return (&IpTable[Counter]);

  if (IpCount >= IP_NUM) {
    syslog(LOG_ERR, "IP_NUM is too low, dropping ip....");
    return (NULL);
  }

  memset(&IpTable[IpCount], 0, sizeof(struct IPData));

  IpTable[IpCount].ip = ipaddr;
  return (&IpTable[IpCount++]);
}

size_t ICGrandTotalDataPoints = 0;

char inline *HostIp2CharIp(unsigned long ipaddr, char *buffer)
{
  struct in_addr in_addr;
  char *s;

  in_addr.s_addr = htonl(ipaddr);
  s = inet_ntoa(in_addr);
  strncpy(buffer, s, 16);
  buffer[15] = '\0';
  return (buffer);
  /*  uint32_t ip = *(uint32_t *)ipaddr;

  sprintf(buffer, "%d.%d.%d.%d", (ip << 24)  >> 24, (ip << 16) >> 24, (ip << 8) >> 24, (ip << 0) >> 24);
  */
}

// Add better error checking

int fork2()
{
  pid_t pid;

  if (!(pid = fork())) {
    if (!fork()) {
#ifdef PROFILE
      // Got this incantation from a message board.  Don't forget to set
      // GMON_OUT_PREFIX in the shell
      extern void _start(void), etext(void);
      syslog(LOG_INFO, "Calling profiler startup...");
      monstartup((u_long) & _start, (u_long) & etext);
#endif
      return (0);
    }

    _exit(EXIT_SUCCESS);
  }

  waitpid(pid, NULL, 0);
  return (1);
}
