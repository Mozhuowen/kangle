/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kanglesoft.com/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef GLOBAL_H_asdfkjl23kj4234
#define GLOBAL_H_asdfkjl23kj4234
#ifdef HAVE_CONFIG_H
#ifndef _WIN32
#include "config.h"
#endif
#endif
#ifndef _WIN32
#define INT64  long long
#else
#define HAVE_SOCKLEN_T 1
#define ENABLE_DETECT_WORKER_LOCK    1
#endif
/////////[42]
#define PROGRAM_NAME     "kangle"
/////////[43]
#ifndef VERSION
#define VERSION         "3.2.8"
#endif
#define VER_ID   VERSION
#ifndef MAX
#define MAX(a,b)  ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b)  ((a)>(b)?(b):(a))
#endif
#define	 GC_SLEEP_TIME	10

//#define  NBUFF_SIZE     16384
//���ܳ���8K,ajpЭ�����İ�Ϊ8k
#define  NBUFF_SIZE     8192

#define  JUMP_ALLOW     0
#define  JUMP_DENY      1
#define  JUMP_TABLE     2
#define  JUMP_SERVER    3
#define  JUMP_WBACK     4
#define  JUMP_VHS       5
#define  JUMP_FCGI      6
#define  JUMP_CONTINUE  7
#define  JUMP_PROXY     8
#define  RESPONSE_TABLE 9
#define  JUMP_CGI       10
#define  JUMP_API       11
#define  JUMP_DEFAULT   12
#define  JUMP_CMD       13
#define  JUMP_DROP       16
#define  JUMP_UNKNOW    100

#define  REQUEST          0
#define  RESPONSE         1
#define  REQUEST_RESPONSE 2
/***********************************/
#define WORK_MODEL_KA       (1)
#define WORK_MODEL_MANAGE   (1<<1)
#define WORK_MODEL_SSL      (1<<2)
#define WORK_MODEL_INTERNAL (1<<3)
#define WORK_MODEL_PER_IP   (1<<4)
#define WORK_MODEL_REPLACE  (1<<5)
#define WORK_MODEL_RQOK     (1<<6)
#ifdef  ENABLE_TPROXY
#define WORK_MODEL_TPROXY   (1<<7)
#endif
#define WORK_MODEL_PORTMAP  (1<<8)
/************************************/

#ifdef _WIN32
#define HAVE_SSTREAM 1
#define _LARGE_FILE  1
#endif


#define ENABLE_WRITE_BACK  1
#define ENABLE_LIMIT_SPEED 1
#define CONTENT_FILTER  1

#ifndef NDEBUG
#if !defined(_WIN32) && !defined(OPENBSD)
//#define MALLOCDEBUG   1
#endif
#if !defined(OPENBSD)
//#define DEAD_LOCK   1
#endif
#endif
#define PID_FILE   "/var/kangle.pid"

#include <assert.h>

#ifndef _WIN32
#include "environment.h"
#else
#pragma warning(disable: 4290 4996)
#endif
#define forever() for(;;)

#define IF_FREE(p) {if ( p ) xfree(p);p=NULL;}
#define IF_STRDUP(d,s) {if ( d ) xfree(d); d = xstrdup(s);}

#if !defined(ABS)
#define ABS(x)  ((x)>0?(x):(-(x)))
#endif
#define ROUND(x,b) ((x)%(b)?(((x)/(b)+1)*(b)):(x))

#define CHUNK_SIZE (512)

#define ROUND_CHUNKS(s) ((((s) / CHUNK_SIZE) + 1) * CHUNK_SIZE)

#define NET_HASH_SIZE (1024)
#define NET_HASH_MASK (NET_HASH_SIZE-1)

#define AND_PUT  1
#define AND_USE  2
///////////////////////////////////////////////////////////////////
/**
* obj->index.flags �����־λ
*/
#define FLAG_DEAD               1      /* ����������ȴ���� */
#define FLAG_URL_FREE           (1<<1) /* ����е�urlҪ�������� */
#define FLAG_IN_MEM             (1<<2) /* ����������ڴ��� */
#define FLAG_IN_DISK            (1<<3) /* ������ڴ����� */
#define FLAG_GZIP               (1<<4) /* gzipѹ���� */
#define FLAG_NO_BODY            (1<<5) /* �����body */
#define OBJ_MUST_REVALIDATE     (1<<6) /* must-revalidate */
#define OBJ_IS_STATIC2          (1<<7) /* ��̬��2 */
#define OBJ_IS_READY            (1<<8) /* ���׼������ */
//////////////////////////////////////////////////////////////////
#define ANSW_HAS_EXPIRES        (1<<9) /* ��Ӧ��Expireͷ */
#define ANSW_NO_CACHE           (1<<10) /* ��Ӧ���� */
#define ANSW_HAS_MAX_AGE        (1<<11) /* ��Ӧ��Max-Ageͷ */
#define ANSW_LAST_MODIFIED      (1<<12) /* �����Last-Modified */
#define ANSW_HAS_CONTENT_LENGTH (1<<13) /* ��Content-Lengthͷ */
#define ANSW_CLOSE              (1<<14) /* �������ӹر� */
#define ANSW_HAS_CONTENT_RANGE  (1<<15) /* ��Content-Range */
#define ANSW_CHUNKED            (1<<16) /* ��Ӧ��chunk���� */
/////////////////////////////////////////////////////////////////////
#define FLAG_RQ_INTERNAL        (1<<17) /* �ڲ������������� */
#define FLAG_RQ_GZIP            (1<<18) /* gzip������������� */
/////////////////////////////////////////////////////////////////////
#define FLAG_NO_DISK_CACHE      (1<<19) /* ���ô��̻��� */
#define FLAG_NO_NEED_CACHE      ANSW_NO_CACHE /*@deprecate ���軺�� */
#define FLAG_NEED_GZIP          (1<<20)  /* ��Ҫgzipѹ�� */
#define FLAG_NEED_CACHE         (1<<21)  /* ��Ҫ����(����Ҫ,���ͬʱ����ANSW_NO_CACHE,��no-cacheΪ˭) */
/////////////////////////////////////////////////////////////////////
/*
 * GLOBAL_KEY_CHECKED �͡�USER_KEY_CHECKED��ϵͳ�Ĵ�����ԡ�
 * ����ֻҪ��һ�������ˣ�����������FLAG_FILTER_DENY����deny��
 * @deprecated
 */
#define GLOBAL_KEY_CHECKED      (1<<22)
#define USER_KEY_CHECKED        (1<<23)
/////////////////////////////////////////////////////////////////////
#define FLAG_FILTER_DENY        (1<<24) /* �����˾ܾ� */
#define ANSW_LOCAL_SERVER       (1<<25) /* ����Ӧ��(fastcgi��) */
#define ANSW_XSENDFILE          (1<<26) /* x-accel-redirect */
/////////////////////////////////////////////////////////////////////
/////////[44]
#define OBJ_INDEX_UPDATE         (1<<30)
#define OBJ_NOT_OK               (1<<31)
/*
#define ST_GZIP              (1<<22)
#define ST_CACHE             (1<<23)
#define ST_CHUNK             (1<<24)
#define ST_SPLIMIT           (1<<25)
#define ST_CONTENT           (1<<26)
*/

#define STATUS_OK               200
#define STATUS_CREATED          201
#define STATUS_NO_CONTENT       204
#define STATUS_CONTENT_PARTIAL  206
#define STATUS_MULTI_STATUS     207
#define STATUS_MOVED            301
#define STATUS_FOUND            302
#define STATUS_NOT_MODIFIED     304
#define STATUS_TEMPORARY_REDIRECT 307
#define STATUS_BAD_REQUEST      400
#define STATUS_UNAUTH           401
#define STATUS_FORBIDEN         403
#define STATUS_NOT_FOUND        404
#define STATUS_METH_NOT_ALLOWED 405
#define STATUS_CONFLICT         409
#define STATUS_PRECONDITION     412
#define STATUS_SERVER_ERROR     500
#define STATUS_NOT_IMPLEMENT    501
#define STATUS_BAD_GATEWAY      502
#define STATUS_SERVICE_UNAVAILABLE  503
#define STATUS_GATEWAY_TIMEOUT  504



/**
* �����־λ
* rq->flags
*/
#define RQ_QUEUED              1
#define RQ_HAS_IF_MOD_SINCE    (1<<1)
#define RQ_VH_QUERIED          (1<<2)
//#define RQ_REPLACE_IP          (1<<2)
#define RQ_HAS_NO_CACHE        (1<<3)
#define RQ_NET_BIG_OBJECT      (1<<4)
#define RQ_BIG_OBJECT_CTX      (1<<5)
#define RQ_INPUT_CHUNKED       (1<<6)
#define RQ_SYNC                (1<<7)
#define RQ_HAS_ONLY_IF_CACHED  (1<<8)
#define RQ_HAS_AUTHORIZATION   (1<<9)
#define RQ_HAS_PROXY_AUTHORIZATION (1<<10)
#define RQ_HAS_KEEP_CONNECTION (1<<11)
#define RQ_URL_RANGED          (1<<12)
#define RQ_IS_REWRITED         (1<<13)
#define RQ_IS_BAD              (1<<14)
#define RQ_URL_ENCODE          (1<<15)
#define RQ_OBJ_VERIFIED        (1<<16)
#define RQ_IF_RANGE            (1<<17)
#define RQ_HAVE_RANGE          (1<<18)
#define RQ_TE_CHUNKED          (1<<19)
#define RQ_TE_GZIP             (1<<20)
#define RQ_HAS_SEND_HEADER     (1<<21)
#define RQ_URL_VARIED          (1<<22)
#define RQ_POST_UPLOAD         (1<<23)
#define RQ_CONNECTION_CLOSE    (1<<24)
#define RQ_OBJ_STORED          (1<<25)
#define RQ_HAVE_EXPECT         (1<<26)
#define RQ_HIT_CACHE           (1<<27)
#define RQ_TEMPFILE_HANDLED    (1<<28)
#define RQ_HAS_GZIP            (1<<29)
#define RQ_IS_ERROR_PAGE       (1<<30)
#define RQ_UPSTREAM_ERROR      (1<<31)



///////////////////////////////////////////////
//����rq->filter_flag
///////////////////////////////////////////////
#define  RF_TPROXY_UPSTREAM  (1)
#ifdef ENABLE_TPROXY
#define  RF_TPROXY_TRUST_DNS (1<<1)
#endif
#define  RF_PROXY_RAW_URL    (1<<2) /* �������ʱ��raw_url */
#define  RF_DOUBLE_CACHE_EXPIRE (1<<3)
#define  RF_UPSTREAM_NOKA    (1<<4)
#define  RF_IGNORE_ERROR     (1<<5)
#define  RF_MSERVER_NOSWITCH (1<<6)
#define  RQ_NO_EXTEND        (1<<7)
#define  RF_X_REAL_IP        (1<<8)
#define  RF_NO_X_FORWARDED_FOR  (1<<9)
#define  RQ_ONLY_V4          (1<<10)
#define  RQ_ONLY_V6          (1<<11)
#define  RF_NO_CACHE         (1<<12)
#define  RF_VIA              (1<<13)
#define  RF_X_CACHE          (1<<14)
#define  RF_NO_BUFFER        (1<<15)
#define  RF_NO_DISK_CACHE    (1<<16)
#define  MOD_NO_LOG          (1<<17)
#define  RQ_SEND_AUTH        (1<<18)
#define  RQ_SEND_PROXY_AUTH  (1<<19)
#define  RQ_CGI              (1<<20)
#define  RF_FOLLOWLINK_ALL   (1<<21)
#define  RF_FOLLOWLINK_OWN   (1<<22)
#define  RF_NO_X_SENDFILE    (1<<23)
#define  RF_CACHE_NO_LENGTH  (1<<24)
#define  RQ_URL_QS           (1<<25)
#define  RQ_FULL_PATH_INFO   (1<<26)
#define  RF_AGE              (1<<27)
#define  RQ_SWAP_OLD_OBJ     (1<<28)
#define  RQ_RESPONSE_DENY    (1<<29)
#define  RQF_CC_PASS         (1<<30)
#define  RQF_CC_HIT          (1<<31)

/**
* ������ж�
*/
#define KGL_LIST_KA               0
#define KGL_LIST_RW               1
#define KGL_LIST_CONNECT          2
#define KGL_LIST_SYNC             3
#define KGL_LIST_BLOCK            4
#define KGL_LIST_NONE             5


#define ANSW_SIZE  (4*1024)
#define SET(a,b)   ((a)|=(b))
#define CLR(a,b)   ((a)&=~(b))
#define TEST(a,b)  ((a)&(b))

/**
* urlЭ��
*/
#define PROTO_NONE  0
#define PROTO_HTTP  1
#define PROTO_FTP   2
#define PROTO_OTHER 4
#define PROTO_HTTPS 8

#define PROTO_IPV6  (1<<7)

#define        IS_SPACE(a)     isspace((unsigned char)a)
#define        IS_DIGIT(a)     isdigit((unsigned char)a)


#define  CONNECT_TIME_OUT    20
#if defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD)
#define BSD_OS 1
#endif
#define PROGRAM_NO_QUIT               0
#define PROGRAM_QUIT_IMMEDIATE        1
#define PROGRAM_QUIT_CLOSE_CONNECTION 2
#ifdef _WIN32
#define WHM_MODULE                 1
#endif

#define ENABLE_MULTI_TABLE         1
#define ENABLE_DIGEST_AUTH         1
#define ENABLE_MULTI_SERVER        1

/////////[45]
#ifdef ENABLE_DISK_CACHE
//����sqlite��������
#define ENABLE_DB_DISK_INDEX       1
#define ENABLE_SQLITE_DISK_INDEX   1
#endif
#ifndef HTTP_PROXY
#define WHM_MODULE                 1
#define ENABLE_VH_QUEUE            1
#define ENABLE_TF_EXCHANGE         1
//#define ENABLE_BASED_IP_VH         1
#define ENABLE_SUBDIR_PROXY        1
#endif
#define ENABLE_INPUT_FILTER        1
#define ENABLE_FORCE_CACHE         1
#define ENABLE_REQUEST_QUEUE       1
#define ENABLE_USER_ACCESS         1
//#define ENABLE_SSL_VERIFY_CLIENT   1
#define ENABLE_VH_RUN_AS           1
#define ENABLE_MANY_VH             1
#ifdef ENABLE_VH_RS_LIMIT
#define ENABLE_VH_LOG_FILE         1
#endif
#define ENABLE_SUB_VIRTUALHOST     1
#define ENABLE_BASED_PORT_VH       1
/////////[46]


//#define ENABLE_PIPE_LOG            1
#if defined(ENABLE_FORCE_CACHE) || defined(ENABLE_STATIC_URL)
#define ENABLE_STATIC_ENGINE       1
#endif
#define DEFAULT_COOKIE_STICK_NAME  "kangle_runat"
#define VARY_URL_KEY               1
enum Proto_t {
	Proto_http, Proto_fcgi, Proto_ajp,Proto_uwsgi,Proto_scgi,Proto_hmux
};
/**
* HASH_SIZE ֻ��Ϊ2��n�η�,Ҫ���ö�hash,��Ҫ����MULTI_HASHΪ1
*/
#define	HASH_SIZE	(1024)
#define	HASH_MASK	(HASH_SIZE-1)
#define MULTI_HASH      1
#ifdef LINUX
#define ENABLE_SENDFILE      1
#endif
#endif

