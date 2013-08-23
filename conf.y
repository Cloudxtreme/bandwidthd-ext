 %{
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "bandwidthd.h"

extern unsigned int SubnetCount;
extern struct SubnetData SubnetTable[];
extern struct config config;

int bdconfig_lex(void);
int LineNo = 1;

void bdconfig_error(const char *str)
    {
        fprintf(stderr, "Syntax Error \"%s\" on line %d\n", str, LineNo);
	syslog(LOG_ERR, "Syntax Error \"%s\" on line %d", str, LineNo);
	exit(1);
    }

int bdconfig_wrap()
    {
	return(1);
    }
%}

%token TOKJUNK TOKSUBNET TOKDEV TOKSLASH TOKSKIPINTERVALS TOKGRAPHCUTOFF 
%token TOKPROMISC TOKOUTPUTCDF TOKRECOVERCDF TOKGRAPH TOKNEWLINE TOKFILTER
%token TOKMETAREFRESH TOKPGSQLCONNECTSTRING TOKSENSORID TOKHTDOCSDIR TOKLOGDIR
%token TOKMYSQLHOST TOKMYSQLUSER TOKMYSQLPASS TOKMYSQLDBNAME TOKMYSQLPORT
%token TOKNONE TOKPGSQL TOKMYSQL TOKOUTPUTDATABASE TOKPIDFILE
%union
{
    int number;
    char *string;
}

%token <string> IPADDR
%token <number> NUMBER
%token <string> STRING
%token <number> STATE
%type <string> string
%%

commands: /* EMPTY */
    | commands command
    ;

command:
    subnet
    |
    device
    |
    skip_intervals
    |
    graph_cutoff
    |
    promisc
    |
    output_cdf
    |
    recover_cdf
    |
    graph
    |
    newline
    |
    filter
    |
    meta_refresh
    |
    pgsql_connect_string
    |
    sensor_id
    |
    htdocs_dir
    |
    log_dir
    |
    mysql_host
    |
    mysql_user
    |
    mysql_pass
    |
    mysql_dbname
    |
    mysql_port
    |
    output_database
    |
    pidfile
    ;

subnet:
    subneta
    |
    subnetb
    ;

newline:
    TOKNEWLINE
    {
	LineNo++;
    }
    ;

subneta:
    TOKSUBNET IPADDR IPADDR
    {
	MonitorSubnet(inet_network($2), inet_network($3));
    }
    ;

subnetb:
    TOKSUBNET IPADDR TOKSLASH NUMBER
    {
	unsigned int Subnet, Counter, Mask;

	Mask = 1; Mask <<= 31;
	for (Counter = 0, Subnet = 0; Counter < $4; Counter++) {
	    Subnet >>= 1;
	    Subnet |= Mask;
	}

	MonitorSubnet(inet_network($2), Subnet);
    }
    ;

string:
    STRING
    {
	$1[strlen($1)-1] = '\0';
        $$ = $1+1;
    }
    ;

device:
    TOKDEV string
    {
	config.dev = $2;
    }
    ;

htdocs_dir:
    TOKHTDOCSDIR string
    {
	config.htdocs_dir = $2;
    }
    ;

log_dir:
    TOKLOGDIR string
    {
	config.log_dir = $2;
    }
    ;

filter:
    TOKFILTER string
    {
	config.filter = $2;
    }
    ;

meta_refresh:
    TOKMETAREFRESH NUMBER
    {
	config.meta_refresh = $2;
    }
    ;

skip_intervals:
    TOKSKIPINTERVALS NUMBER
    {
	config.skip_intervals = $2+1;
    }
    ;

graph_cutoff:
    TOKGRAPHCUTOFF NUMBER
    {
	config.graph_cutoff = $2*1024;
    }
    ;

promisc:
    TOKPROMISC STATE
    {
	config.promisc = $2;
    }
    ;

output_cdf:
    TOKOUTPUTCDF STATE
    {
	config.output_cdf = $2;
    }
    ;

recover_cdf:
    TOKRECOVERCDF STATE
    {
	config.recover_cdf = $2;
    }
    ;

graph:
    TOKGRAPH STATE
    {
	config.graph = $2;
    }
    ;

pgsql_connect_string:
    TOKPGSQLCONNECTSTRING string
    {
	config.pgsql_connect_string = $2;
    }
    ;

mysql_host:
    TOKMYSQLHOST string
    {
	config.mysql_host = $2;
    }
    ;

mysql_user:
    TOKMYSQLUSER string
    {
	config.mysql_user = $2;
    }
    ;

mysql_pass:
    TOKMYSQLPASS string
    {
	config.mysql_pass = $2;
    }
    ;

mysql_dbname:
    TOKMYSQLDBNAME string
    {
	config.mysql_dbname = $2;
    }
    ;

mysql_port:
    TOKMYSQLPORT NUMBER
    {
	config.mysql_port = $2;
    }
    ;

output_database:
    TOKOUTPUTDATABASE TOKNONE
    {
	config.output_database = DB_NONE;
    }
    |
    TOKOUTPUTDATABASE TOKPGSQL
    {
	config.output_database = DB_PGSQL;
    }
    |
    TOKOUTPUTDATABASE TOKMYSQL
    {
	config.output_database = DB_MYSQL;
    }
    ;

pidfile:
    TOKPIDFILE string
    {
	config.pidfile = $2;
    }
    ;

sensor_id:
    TOKSENSORID string
    {
	config.sensor_id = $2;
    }
    ;
