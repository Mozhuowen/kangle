/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef request_h_include
#define request_h_include
#include <string.h>
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "KSocket.h"
#include "KMutex.h"
#include "KAcserver.h"
#include "KHttpProtocolParserHook.h"
#include "KHttpProtocolParser.h"
#include "KHttpHeader.h"
#include "KBuffer.h"
#include "KSelectable.h"
#include "KDomainUser.h"
#include "KSendable.h"
#include "KString.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KUrl.h"
#include "KServer.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "KInputFilter.h"
#include "KFlowInfo.h"

#define READ_BUFF_SZ	8191

class KFetchObject;
class KSubVirtualHost;
class KSelector;
class KFilterHelper;
class KHttpObject;
class KAccess;
class KFilterKey;
class KRequestQueue;
class KBigObjectContext;
#define		REQUEST_EMPTY	0
#define		REQUEST_READY	1
#define 	MIN_SLEEP_TIME	4
#define FOLLOW_LINK_ALL  1
#define FOLLOW_LINK_OWN  2
#define FOLLOW_PATH_INFO 4
/////////[282]
#define AUTH_REQUEST_HEADER "Authorization"
#define AUTH_RESPONSE_HEADER "WWW-Authenticate"
#define AUTH_STATUS_CODE 401
/////////[283]

class KManageIP {
public:
	KMutex ip_lock;
	std::map<ip_addr, unsigned> ip_map;
};
/**
���������󣬿�ʼһ�����������request->beginSubRequest,���������ء�
��һ���ص���������һ��������
�ص�����ֻ�����һ�Ρ����崦����actionָ����
sub_request_free ָʾ����û��������ɣ����������ֻҪ�ͷ�����ڴ�������ɡ�
sub_request_pop  ָʾ�����������ء�������ز�����
*/
enum sub_request_action {
	sub_request_free,
	sub_request_pop
};
typedef void (* sub_request_call_back) (KHttpRequest *rq,void *data,sub_request_action action);
typedef void (* request_clean_call_back) (KHttpRequest *rq,void *data);
class KSubRequest;
class KContext;
class KOutputFilterContext;
class KCleanHook
{
public:
	request_clean_call_back callBack;
	void *data;
	KCleanHook *next;
};
class KHttpRequestData
{
public:
	int flags;
	int filter_flags;
	 /*
	 left_read use with content_length it mean that the post data left to deal.
	 */
	INT64 left_read;
	INT64 content_length;
	time_t if_modified_since;
	INT64 range_from;
	INT64 range_to;
	unsigned short status_code;
	unsigned short cookie_stick;
};
class KHttpRequest: public KHttpProtocolParserHook,public KStream,public KSelectable,public KHttpRequestData {
public:
	inline KHttpRequest()
	{
		stackSize = 0;
		list = KGL_LIST_NONE;
		//client=NULL;
		server = NULL;
		//as=NULL;
		fetchObj = NULL;
	//	charset = NULL;
		readBuf = NULL;
		readBuf = (char *) xmalloc(READ_BUFF_SZ + 1);
		current_size = READ_BUFF_SZ;
		svh = NULL;
		auth = NULL;
		url = NULL;
		//parser.strdup_flag = false;
		file = NULL;
		of_ctx = NULL;
		client_ip = NULL;
		bind_ip = NULL;
		ch = NULL;
		ch_connect = NULL;
#ifdef ENABLE_INPUT_FILTER
		if_ctx = NULL;
#endif
		/////////[284]
		ctx = new KContext;
		slh = NULL;
#ifdef ENABLE_TF_EXCHANGE
		tf = NULL;
#endif
		sr = NULL;
		meth = 0;
		fh = NULL;
		mark = 0;
#ifdef ENABLE_REQUEST_QUEUE
		queue = NULL;
#endif
	}
	inline ~KHttpRequest()
	{
		close(false);
		if (ls) {
			ls->release();
		}
#ifdef ENABLE_REQUEST_QUEUE
		assert(queue == NULL);
#endif
	}
	void close(bool reuse);
	void clean(bool keep_alive=true);
	void init();
	bool isBad();
	char *get_read_buf(int &size);
	char *get_write_buf(int &size);

#ifdef _WIN32
	void get_write_buf(LPWSABUF buffer,int &bufferCount);
#endif
	/*
	 ��post����ʱ�������,����Ҫֱ�ӵ���server->read�ˡ�
	 */
	int read(char *buf, int len);
	//����post body
	void resetPostBody();
	std::string getInfo();
	char *getUrl();
	void beginRequest();
	ReadState canRead(int aio_got=0);
	WriteState canWrite(int aio_got=0);
	SOCKET getSockfd() {
		return server->get_socket();
	}
	bool getPeerAddr(ip_addr *addr) {
		server->get_remote_addr(addr);
		return true;
	}
	int getFollowLink()
	{
		int follow_link = 0;
		if (conf.path_info) {
			follow_link|=FOLLOW_PATH_INFO;
		}
		if (TEST(filter_flags,RF_FOLLOWLINK_OWN)) {
			follow_link|=FOLLOW_LINK_OWN;
			return follow_link;
		}
		if (TEST(filter_flags,RF_FOLLOWLINK_ALL)) {
			follow_link|=FOLLOW_LINK_ALL;
		}
		return follow_link;
	}
	void endParse();
	bool closeConnection();
	void closeFetchObject(bool destroy=true);
	void resetFetchObject();

	void freeUrl();
	bool rewriteUrl(char *newUrl, int errorCode = 0,const char *prefix = NULL);
	char http_major;
	char http_minor;
	char meth;
	char state;
	char *hot;
	char *readBuf;
	size_t current_size;
	INT64 request_msec;
	const char *getState() {
		switch (state) {
		case STATE_IDLE:
			return "idle";
		case STATE_CONNECT:
			return "connect";
		case STATE_SEND:
			return "send";
		case STATE_RECV:
			return "recv";
		case STATE_QUEUE:
			return "queue";
		}
		return "unknow";
	}
	/*
	 * server��ԭʼsocket,
	 * һ�������һ���ģ������ssi�����ڲ�����ʱ�Ͳ�һ���ˡ�
	 */
	KClientSocket *server;
#ifdef ENABLE_TF_EXCHANGE
	//��ʱ�ļ�
	KTempFile *tf;
	void closeTempFile()
	{
		if (tf) {
			delete tf;
			tf = NULL;
		}
		SET(flags,RQ_TEMPFILE_HANDLED);
	}
#endif
	//�������
	KBuffer buffer;
	//����Դ
	KFetchObject *fetchObj;
	//�����ļ�ӳ��
	KFileName *file;
	//��������
	KSubVirtualHost *svh;
	void releaseVirtualHost();
	//������������
	KServer *ls;
	//������
	KSubRequest *sr;
	/*
	 * ԭʼurl
	 */
	KUrl raw_url;
	KUrl *url;
	//http��֤
	KHttpAuth *auth;
	KMutex urlLock;
	//����httpЭ�����
	KHttpProtocolParser parser;
	//�й�object������������
	KContext *ctx;
	//����������
	KSendContext send_ctx;
#ifdef ENABLE_INPUT_FILTER
	/************
	* �������
	*************/
	KInputFilterContext *if_ctx;
	KInputFilterContext *getInputFilterContext()
	{
		if (if_ctx == NULL && (content_length>0 || url->param)) {
			if_ctx = new KInputFilterContext(this);
		}
		return if_ctx;
	}
#endif
	/****************
	* �������
	*****************/
	KOutputFilterContext *of_ctx;
	KOutputFilterContext *getOutputFilterContext();
	void addFilter(KFilterHelper *chain);
	void addSendHeader(KBuffer *hbuf);
	inline bool needFilter() {
		return of_ctx!=NULL;
	}
	/////////[285]
	int parseHeader(const char *attr, char *val, bool isFirst);
	const char *getMethod();
	void getCharset(KHttpHeader *header);
	StreamState write_all(const char *buf, int len);

	//�첽���ã����������󣬷���ʱ���Ǵ�fetchObj->open��
	void beginSubRequest(KUrl *url,sub_request_call_back callBack,void *data);
	void endSubRequest();
	int checkFilter(KHttpObject *obj);	
	u_short workModel;
	unsigned char tmo_left;
	//��ʱʱ����(tmo+1)*conf.time_out
	unsigned char tmo;
	/*
	 * stackSizeָʾssiָʾ�ڲ�����������
	 *
	 */
	unsigned char stackSize;
	unsigned char list;
	unsigned char mark;
	KHttpRequest *prev;
	KHttpRequest *next;
	//����(����)
	KSpeedLimitHelper *slh;
	void addSpeedLimit(KSpeedLimit *sl)
	{
		KSpeedLimitHelper *helper = new KSpeedLimitHelper(sl);
		helper->next = slh;
		slh = helper;
	}
	INT64 getSendTime(int len)
	{
		INT64 sendTime = 0;
		KSpeedLimitHelper *helper = slh;
		INT64 current_time = kgl_current_msec;
		while (helper) {
			INT64 t = helper->sl->getSendTime(len,current_time);
			if (t>sendTime) {
				sendTime = t;
			}
			helper = helper->next;
		}
		return sendTime;
	}
	int getSleepTime(int len)
	{
		int sleepTime = 0;
		KSpeedLimitHelper *helper = slh;
		while (helper) {
			int t = helper->sl->getSleepTime(len);
			if (t>sleepTime) {
				sleepTime = t;
			}
			helper = helper->next;
		}
		return sleepTime;
	}
	//�ͻ���ʵip(�п��ܱ��滻)
	char *getClientIp()
	{
		if (client_ip) {
			return client_ip;
		}
		client_ip = (char *)malloc(MAXIPLEN);
		server->get_remote_ip(client_ip,MAXIPLEN);
		return client_ip;
	}
	char *client_ip;
	//��������ʱ���󶨵ı���ip
	char *bind_ip;
	//������,chΪ�����������ch_connectΪ���ӽ�������
	KCleanHook *ch;
	KCleanHook *ch_connect;
	void registerCleanHook(request_clean_call_back callBack,void *data)
	{
		KCleanHook *hook = new KCleanHook;
		hook->next = ch;
		hook->callBack = callBack;
		hook->data = data;
		ch = hook;
	}
	void registerConnectCleanHook(request_clean_call_back callBack,void *data)
	{
		KCleanHook *hook = new KCleanHook;
		hook->next = ch_connect;
		hook->callBack = callBack;
		hook->data = data;
		ch_connect = hook;
	}
	//����ͳ��
	KFlowInfoHelper *fh;
	void addFlow(INT64 flow,int flowFlag)
	{
		KFlowInfoHelper *helper = fh;
		while (helper) {
			helper->fi->addFlow(flow,flowFlag);
			helper = helper->next;
		}
	}
	void addFlowInfo(KFlowInfo *fi)
	{
		KFlowInfoHelper *helper = new KFlowInfoHelper(fi);
		helper->next = fh;
		fh = helper;
	}
/////////[286]
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue;
#endif
private:
	bool parseMeth(const char *src);
	bool parseConnectUrl(char *src);
	bool parseHttpVersion(char *ver);
};
/**
* ����˫���б���
*/
class RequestList {
public:
	RequestList();
	KHttpRequest *getHead();
	KHttpRequest *getEnd();
	void pushBack(KHttpRequest *rq);
	void pushFront(KHttpRequest *rq);
	KHttpRequest *popBack();
	KHttpRequest *popHead();
	void clear() {
		head = end = NULL;
	}
	KHttpRequest *remove(KHttpRequest *rq);
private:
	KHttpRequest *head;
	KHttpRequest *end;
};
struct RequestError
{
	int code;
	const char *msg;
	void set(int code,const char *msg)
	{
		this->code = code;
		this->msg = msg;
	}
};
inline u_short string_hash(const char *p, u_short res) {
        int i = 8;
        while(*p && i){
                --i;
                res *= *p;
                p++;
        }
        return res;
        /*
        if (p && *p) {
                //p = p + strlen(p) - 1;
                i = 8;
                while ((p >= str) && i) {
                        i--;
                        res += *p * *p;
                        p--;
                }
        }
        return res;
        */
}
/**
* ���뷢�����ݣ�����rq->buffer
*/
void stageWriteRequest(KHttpRequest *rq);
/**
* ���뷢�����ݣ�����ָ����buff
*/
void stageWriteRequest(KHttpRequest *rq,buff *buf,INT64 start,INT64 len);
#endif
