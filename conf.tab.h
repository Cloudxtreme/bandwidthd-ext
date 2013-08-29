/* A Bison parser, made by GNU Bison 2.5.1.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2012 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     TOKJUNK = 258,
     TOKSUBNET = 259,
     TOKDEV = 260,
     TOKSLASH = 261,
     TOKSKIPINTERVALS = 262,
     TOKGRAPHCUTOFF = 263,
     TOKPROMISC = 264,
     TOKOUTPUTCDF = 265,
     TOKRECOVERCDF = 266,
     TOKGRAPH = 267,
     TOKNEWLINE = 268,
     TOKFILTER = 269,
     TOKMETAREFRESH = 270,
     TOKPGSQLCONNECTSTRING = 271,
     TOKSENSORID = 272,
     TOKHTDOCSDIR = 273,
     TOKLOGDIR = 274,
     TOKMYSQLHOST = 275,
     TOKMYSQLUSER = 276,
     TOKMYSQLPASS = 277,
     TOKMYSQLDBNAME = 278,
     TOKMYSQLPORT = 279,
     TOKNONE = 280,
     TOKPGSQL = 281,
     TOKMYSQL = 282,
     TOKOUTPUTDATABASE = 283,
     TOKPIDFILE = 284,
     IPADDR = 285,
     NUMBER = 286,
     STRING = 287,
     STATE = 288
   };
#endif
/* Tokens.  */
#define TOKJUNK 258
#define TOKSUBNET 259
#define TOKDEV 260
#define TOKSLASH 261
#define TOKSKIPINTERVALS 262
#define TOKGRAPHCUTOFF 263
#define TOKPROMISC 264
#define TOKOUTPUTCDF 265
#define TOKRECOVERCDF 266
#define TOKGRAPH 267
#define TOKNEWLINE 268
#define TOKFILTER 269
#define TOKMETAREFRESH 270
#define TOKPGSQLCONNECTSTRING 271
#define TOKSENSORID 272
#define TOKHTDOCSDIR 273
#define TOKLOGDIR 274
#define TOKMYSQLHOST 275
#define TOKMYSQLUSER 276
#define TOKMYSQLPASS 277
#define TOKMYSQLDBNAME 278
#define TOKMYSQLPORT 279
#define TOKNONE 280
#define TOKPGSQL 281
#define TOKMYSQL 282
#define TOKOUTPUTDATABASE 283
#define TOKPIDFILE 284
#define IPADDR 285
#define NUMBER 286
#define STRING 287
#define STATE 288




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2072 of yacc.c  */
#line 40 "conf.y"

    int number;
    char *string;



/* Line 2072 of yacc.c  */
#line 123 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE bdconfig_lval;


