%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "conf.tab.h"

%}

%%
[ \t]+      			/* Ignore */;
\n                               { return TOKNEWLINE; }
#.*\n        			 { return TOKNEWLINE; }
([0-9]{1,3}"."){3,3}([0-9]{1,3}) { bdconfig_lval.string=strdup(yytext); return IPADDR; }
[-0-9]+				 { bdconfig_lval.number=atoi(yytext); return NUMBER; }
\".*\"				 { bdconfig_lval.string=strdup(yytext); return STRING; }
\/				 { return TOKSLASH; }
true|false                       { bdconfig_lval.number=!strcmp(yytext,"true"); return STATE; }
subnet				 { return TOKSUBNET; }
dev				 { return TOKDEV; }
skip_intervals			 { return TOKSKIPINTERVALS; }
graph_cutoff			 { return TOKGRAPHCUTOFF; }
promiscuous			 { return TOKPROMISC; }
output_cdf			 { return TOKOUTPUTCDF; }		
recover_cdf			 { return TOKRECOVERCDF; }		
graph				 { return TOKGRAPH; }
filter				 { return TOKFILTER; }
meta_refresh			 { return TOKMETAREFRESH; }
pgsql_connect_string	         { return TOKPGSQLCONNECTSTRING; }
mysql_host			 { return TOKMYSQLHOST; }
mysql_user			 { return TOKMYSQLUSER; }
mysql_pass			 { return TOKMYSQLPASS; }
mysql_dbname			 { return TOKMYSQLDBNAME; }
mysql_port			 { return TOKMYSQLPORT; }
sensor_id			 { return TOKSENSORID; }
htdocs_dir			 { return TOKHTDOCSDIR; }
log_dir				 { return TOKLOGDIR; }
none				 { return TOKNONE; }
pgsql				 { return TOKPGSQL; }
mysql				 { return TOKMYSQL; }
output_database			 { return TOKOUTPUTDATABASE; }
pidfile				 { return TOKPIDFILE; }
.				 { return TOKJUNK; }
%%
