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

#include "global.h"

#include<time.h>

#include <sstream>

#include<map>
#include<string>
#include<list>
#include<stdarg.h>

//#include	"modules.h"
#include	"utils.h"
#include	"http.h"
#include	"cache.h"
#include	"log.h"
#include 	"KHttpManage.h"
#include 	"KSelectorManager.h"
#include 	"lib.h"
#include	"malloc_debug.h"
#include "KHttpObjectHash.h"
#include "KThreadPool.h"
#include "KVirtualHostManage.h"
#include "KSequence.h"
#include	"md5.h"
#include "KFastcgiUtils.h"
#include "KFastcgiFetchObject.h"
#include "KApiFetchObject.h"
#include "KObjectList.h"
#include "KHttpProxyFetchObject.h"
#include "KPoolableSocketContainer.h"
#include "lang.h"
#include "time_utils.h"
#include "KPipeMessageFetchObject.h"
#include "KHttpDigestAuth.h"
#include "KHttpBasicAuth.h"
#include "KSubRequest.h"
/////////[11]
using namespace std;

void free_url(KUrl *url) {
	url->destroy();
}
inline bool in_stop_cache(KHttpRequest *rq) {
	if (TEST(rq->filter_flags,RF_NO_CACHE))
		return true;
	if (rq->meth == METH_GET || rq->meth == METH_HEAD) {
		return false;
	}
	return true;
}
/*
 check if the keep-alive client has next request data.
 return 0=close 1=yes 2=continue 3=no next request but continue
 */
inline int checkHaveNextRequest(KHttpRequest *rq) {
	INT64 bodyLen = rq->parser.bodyLen;
	bodyLen -= rq->content_length;
	if (bodyLen > 0) {
		memcpy(rq->readBuf, rq->parser.body + rq->content_length, (int)bodyLen);
		rq->clean();
		rq->init();
		rq->hot = rq->readBuf + bodyLen;
		return rq->parser.parse(rq->readBuf, (int)bodyLen, rq);
	}
	return HTTP_PARSE_NO_NEXT_BUT_CONTINUE;
}
/*
�����Ѿ�����������һ������״̬
*/
void nextRequest(KHttpRequest *rq,bool switchThread)
{
	if (switchThread) {
		rq->handler = handleStartRequest;
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_NEXT);
		//rq->selector->addSocket(rq,STAGE_OP_NEXT);		
	} else {
		handleStartRequest(rq,0);
	}
}
void handleEndSubRequest(KSelectable *st ,int got)
{
	KHttpRequest *rq = (KHttpRequest *)st;
	rq->endSubRequest();
}
/*
��������
�������������������ڵ㣬һ�������̣߳�����һ����stage2����stage1ʱ���õ�
�����������п����ж���߳�ͬʱ���õġ�
*/
void stageEndRequest(KHttpRequest *rq)
{
#ifndef _WIN32	
#ifndef NDEBUG
	assert(!rq->server->isBlock());
#endif
#endif
/////////[12]
	if (!TEST(rq->flags,RQ_CONNECTION_CLOSE) && rq->sr) {
		//���������
		rq->handler = handleEndSubRequest;
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_NEXT);
		return;
	}
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		//����ʱ�ļ�����ʱ������ʱ�ļ�
		rq->tf->switchRead();
		kassert(!rq->tf->isWrite());
		rq->handler = handleRequestTempFileWrite;
		if(!limitWrite(rq,STAGE_OP_TF_WRITE)){
			rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_TF_WRITE);
		}
		return;
	}
#endif
	CLR(rq->workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE);
	//KContext *context = &rq->ctx;
	int status  = HTTP_PARSE_FAILED;
	//http pipe line ѭ��
	//freeContextObject(rq);
	rq->ctx->clean_obj(rq);
	log_access(rq);
	//context->rq = rq;
	if (!TEST(rq->flags,RQ_CONNECTION_CLOSE) && TEST(rq->flags,RQ_HAS_KEEP_CONNECTION)) {
		//���http pipe line���鿴�Ƿ�������Ҫ����
		status = checkHaveNextRequest(rq);
		if(status == HTTP_PARSE_SUCCESS){
			nextRequest(rq,true);
			return;
		}
	}
	/*
	����һ��״̬��ͬ������ͬ�Ĵ���
	*/
	if (status == HTTP_PARSE_FAILED || rq->list == KGL_LIST_NONE) {
		//selectorManager.closeRequest(rq);
#ifdef KSOCKET_SSL
		if (TEST(rq->workModel,WORK_MODEL_SSL)) {
			stageSSLShutdown(rq);
			return;
		}
#endif
		delete rq;
		return;
	}
	if (status != HTTP_PARSE_CONTINUE) {
		rq->clean();
		rq->init();
	}
	rq->handler = handleRequestRead;
#ifdef KSOCKET_SSL
	if (TEST(rq->workModel,WORK_MODEL_SSL)) {
		//ssl��Ϊ��һ��SSL_read�п���ֻ����һ�������ݣ�����Ҫ��������ֱ��û�����ݷ��ء�
		//��֪��bug��ssl�޷�ʹ�ó����ӳ�ʱ��ֻ��ʹ�ó�ʱ
		handleRequestRead(rq,0);
		return;
	}
#endif
	rq->selector->addRequest(rq,KGL_LIST_KA,STAGE_OP_READ);
}

void log_access(KHttpRequest *rq) {
	/*
	if (TEST(rq->workModel ,WORK_MODEL_MANAGE) || rq->isBad()) {
		return;
	}
	*/
	if (rq->isBad()) {
		return;
	}
	INT64 sended_length = rq->send_ctx.getSendedBodyLen();
	if (!TEST(rq->flags,RQ_BIG_OBJECT_CTX)) {
		rq->addFlow(sended_length,(TEST(rq->flags,RQ_HIT_CACHE)>0?FLOW_UPDATE_TOTAL|FLOW_UPDATE_CACHE:FLOW_UPDATE_TOTAL));
	}
	KStringBuf l(512);
	KLogElement *s = &accessLogger;
#ifndef HTTP_PROXY
	if (rq->svh) {
/////////[13]
#ifdef ENABLE_VH_LOG_FILE
		if (rq->svh->vh->logger) {
			s = rq->svh->vh->logger;
		}
#endif
	}
#endif
	if (s->place == LOG_NONE) {
		return;
	}
	char *referer = NULL;
	const char *user_agent = NULL;
	char tmp[64];
	int default_port = 80;
	if (TEST(rq->workModel,WORK_MODEL_SSL)) {
		default_port = 443;
	}
	if (rq->client_ip) {
		l << rq->client_ip;
	} else {
		rq->server->get_remote_ip(tmp,64);
		l << tmp ;
	}
	l.WSTR(":");
	l << rq->server->get_remote_port();
	l.WSTR(" - ");
	if (rq->auth && rq->auth->getUser()) {
		l << rq->auth->getUser();
	} else {
		l.WSTR("-");
	}
	l.WSTR(" ");
	timeLock.Lock();
	l.write_all((char *)cachedLogTime,28);
	timeLock.Unlock();
	l.WSTR(" \"");
	l << rq->getMethod();
	l.WSTR(" ");
	l << (TEST(rq->workModel,WORK_MODEL_SSL) ? "https://" : "http://");
	KUrl *url;
	if (rq->sr) {
		url = rq->url;
		referer = rq->sr->url->getUrl();
		user_agent = "-sub request-";
	} else {
		url = &rq->raw_url;
		referer = (char *)rq->parser.getHttpValue("Referer");
		user_agent = rq->parser.getHttpValue("User-Agent");
	}
	l << url->host;
	if (url->port != default_port) {
		l << ":" << url->port;
	}
	l << url->path;
	if (url->param) {
		l << "?" << url->param;
	}	
	l.WSTR(" HTTP/1.1\" ");
	l << rq->status_code << " " ;
#ifdef _WIN32
    const char *formatString="%I64d";
#else
    const char *formatString = "%lld";
#endif
	sprintf(tmp,formatString,sended_length);
	l << tmp;
#ifndef NDEBUG
	const char *range = rq->parser.getHttpValue("Range");
	if (range) {
	       l << " \"" << range << "\"" ;
	}
#endif
	//s->log(formatString,rq->send_ctx.send_size);
	l.WSTR(" \"");
	if(referer){
		l << referer;
	} else {
		l.WSTR("-");
	}
	l.WSTR("\" \"");
	if(user_agent){
		l << user_agent;
	} else {
		l.WSTR("-");
	}
	//*
	l.WSTR("\"[");
#ifndef NDEBUG
	l << "F" << (unsigned)rq->flags << "f" << (unsigned)rq->filter_flags;
#endif
	if(TEST(rq->flags,RQ_HIT_CACHE)){
		l.WSTR("C");
	}	
	if(TEST(rq->flags,RQ_OBJ_STORED)){
		l.WSTR("S");
	}
	if(TEST(rq->flags,RQ_OBJ_VERIFIED)){
		l.WSTR("V");
	}
	if(TEST(rq->flags,RQ_TE_GZIP)){
		l.WSTR("Z");
	}
	if(TEST(rq->flags,RQ_TE_CHUNKED)){
		l.WSTR("K");
	}

	if (TEST(rq->flags,RQ_HAS_KEEP_CONNECTION|RQ_CONNECTION_CLOSE) == RQ_HAS_KEEP_CONNECTION) {
		l.WSTR("L");
	}
	if (rq->sr) {
		free(referer);
	}
	/////////[14]
	//if (TEST(rq->flags,RQ_TIME_OUT)) {
	//	l << "T";
	//}
	if (TEST(rq->flags,RQ_QUEUED)) {
		l.WSTR("t");
		INT64 t = kgl_current_msec - rq->request_msec;
		l << t;
	}
	if (rq->mark!=0) {
		l.WSTR("m");
		l << (int)rq->mark;
	}
	l.WSTR("]\n");
	//*/
	s->startLog();
	s->write(l.getString(),l.getSize());
	s->endLog(false);
}
inline int checkRequest(KHttpRequest *rq)
{
	int jumpType = kaccess[REQUEST].check(rq, NULL);
	if (jumpType == JUMP_DENY) {
		return jumpType;
	}
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	if (rq->svh) {
		return rq->svh->vh->checkRequest(rq);
	}
#endif
#endif
	return jumpType;

}
void handleConnectMethod(KHttpRequest *rq)//https����
{
/////////[15]
	send_error(rq,NULL,STATUS_METH_NOT_ALLOWED,"The requested method CONNECT is not allowed");
/////////[16]
}
void stageHttpManageLogin(KHttpRequest *rq)
{
	if (rq->auth) {
		delete rq->auth;
		rq->auth = NULL;
	}
#ifdef ENABLE_DIGEST_AUTH
	if (conf.auth_type == AUTH_DIGEST) {
		KHttpDigestAuth *auth = new KHttpDigestAuth();
		auth->init(rq, PROGRAM_NAME);
		rq->auth = auth;
	} else
#endif
		rq->auth = new KHttpBasicAuth(PROGRAM_NAME);
	const char path_split_str[2]={PATH_SPLIT_CHAR,0};
	rq->buffer << "<html><body>Please set the admin user and password in the file: <font color='red'>"
		<< "kangle_installed_path" << path_split_str << "etc" << path_split_str 
		<< "config.xml</font> like this:"
		"<font color=red><pre>"
		"&lt;admin user='admin' password='kangle' admin_ips='127.0.0.1|*'/&gt;"
		"</pre></font>\n"
		"The default admin user is admin, password is kangle</body></html>";

	send_auth(rq,&rq->buffer);
}
void stageHttpManage(KHttpRequest *rq)
{
	rq->releaseVirtualHost();
	conf.admin_lock.Lock();
	if (!checkManageLogin(rq)) {
		conf.admin_lock.Unlock();
		char ips[MAXIPLEN];
		rq->server->get_remote_ip(ips,sizeof(ips));
		klog(KLOG_WARNING, "[ADMIN_FAILED]%s:%d %s\n",
				ips,
				rq->server->get_remote_port(), rq->raw_url.path);
		stageHttpManageLogin(rq);
		return;
	}
	conf.admin_lock.Unlock();
	/*
	if(getUrlValue("url_password")=="1" && rq->raw_url.param){
		xfree(rq->raw_url.param);
		rq->raw_url.param = xstrdup("***url_password***");
	}
	*/
	char ips[MAXIPLEN];
	rq->server->get_remote_ip(ips,sizeof(ips));
	klog(KLOG_NOTICE, "[ADMIN_SUCCESS]%s:%d %s%s%s\n",
			ips,
			rq->server->get_remote_port(), rq->raw_url.path,
			(rq->raw_url.param?"?":""),(rq->raw_url.param?rq->raw_url.param:""));
	if(strstr(rq->url->path,".whm") 
		|| strcmp(rq->url->path,"/logo.gif")==0
		|| strcmp(rq->url->path,"/kangle.css")==0){
		//�����whm�ģ�ֱ����whm,����post���ݴ���
		CLR(rq->flags,RQ_HAS_AUTHORIZATION);
		assert(rq->fetchObj==NULL && rq->svh==NULL);
		rq->svh = conf.sysHost->getFirstSubVirtualHost();
		if(rq->svh){
			rq->svh->vh->addRef();
			async_http_start(rq);
			return;
		}
		assert(false);
	}
	rq->fetchObj = new KHttpManage;
	processRequest(rq);
}
//��ʼһ��http����
void handleStartRequest(KSelectable *st,int got)
{
	KHttpRequest *rq = (KHttpRequest *)st;
	rq->beginRequest();
	//if (!TEST(rq->flags,RQ_HAS_KEEP_CONNECTION) || conf.keep_alive == 0){
		//SET(rq->flags,RQ_CONNECTION_CLOSE);
	//}
	if (TEST(rq->flags,RQ_IS_BAD)) {
		//say_bad_request("Bad request format.\n", "", ERR_BAD_URL, rq);
		send_error(rq, NULL, STATUS_BAD_REQUEST, "Bad url");
		return;
		//goto done;
	}
	if (rq->meth == METH_CONNECT) {
		//klog(KLOG_ERR,"CONNECT method not support\n");
		//rq->ctx->last_status = false;
		///stageEndRequest(rq);
		handleConnectMethod(rq);
		return;
	}
	if (rq->isBad()) {
		send_error(rq, NULL, STATUS_BAD_REQUEST, "Bad request format.");
		return;
	}
	if (TEST(rq->workModel,WORK_MODEL_MANAGE)) {
		stageHttpManage(rq);
	} else {
		async_http_start(rq);
	}
}
/*
����һ��http����,async��ͷ�ĺ�������ֵ��һ������˼��
����ֵ
false,�첽����,��ʱcontext��������߳̽ӹܡ�
true,ͬ������,contextû���������߳̽ӹ�
*/
bool async_http_start(KHttpRequest *rq)
{
	KContext *context = rq->ctx;
	bool result = true;
	assert(rq->fetchObj==NULL);
	//��������
	//char *split_url = rq->url->param;
	//rq->buffer << "test xxxx**************xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
	//return send_http(rq,NULL,"200 OK",NULL,&rq->buffer);
	if (!TEST(rq->workModel, WORK_MODEL_MANAGE|WORK_MODEL_INTERNAL) 
		&& checkRequest(rq) == JUMP_DENY) {
		goto done;
	}
	/////////[17]
	if (TEST(rq->flags,RQ_HAVE_EXPECT)) {
		rq->server->write_all("HTTP/1.1 100 Continue\r\n\r\n");
		CLR(rq->flags,RQ_HAVE_EXPECT);
	}
	//only if cached
	if (TEST(rq->flags, RQ_HAS_ONLY_IF_CACHED)) {
		context->obj = findHttpObject(rq, AND_USE, &context->new_object);
		if (!context->obj) {
			result = send_error(rq, context->obj, 404, "Not in cache");
			goto done;
		}
		result = asyncSendCache(rq);
		goto done;
	}
	//end purge or only if cached
	if (in_stop_cache(rq)) {
		context->obj = new KHttpObject(rq);
		context->new_object = 1;
		if (!context->obj) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			//context->last_status = false;
			goto done;
		}
		SET(context->obj->index.flags,FLAG_DEAD);
	} else {
		context->obj = findHttpObject(rq, AND_PUT | AND_USE, &context->new_object);
		//		msg = "net";
		if (!context->obj) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			goto done;
		}
	}
	if (context->new_object) { //It is a new object	
		if (rq->meth != METH_GET) {
			SET(context->obj->index.flags,FLAG_DEAD);
		}
		processNotCacheRequest(rq);
		result = false;
	} else {
		//useCache = true;
		result = asyncSendCache(rq);
	}
	done:
	return result;
}

/*
�ڲ�����һ��http����һ����SSI���ã�һ����api��execUrl���á�
ͬ����ʽ��
*/
bool processHttpRequest(KHttpRequest *rq) {
	//KContext context;
	//memset(&context,0,sizeof(KContext));
	//context.rq = rq;
	return async_http_start(rq);
}


