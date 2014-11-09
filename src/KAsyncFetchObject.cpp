#include "KAsyncFetchObject.h"
#include "http.h"
#include "KSelector.h"
#include "KAsyncWorker.h"
#ifdef _WIN32
#include "KIOCPSelector.h"
#endif
#include "KSingleAcserver.h"
#include "KCdnContainer.h"
#include "KSimulateRequest.h"
/////////[393]

static KTHREAD_FUNCTION asyncDnsCallBack(void *data)
{
	KHttpRequest *rq = (KHttpRequest *)data;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	if (kgl_current_msec - rq->active_msec > conf.time_out * 1000) {
		fo->connectCallBack(rq,NULL);
		KTHREAD_RETURN;
	}
	const char *ip = rq->bind_ip;
	KUrl *url = (TEST(rq->filter_flags,RF_PROXY_RAW_URL)?&rq->raw_url:rq->url);
	const char *host = url->host;
	u_short port = url->port;
	bool isIp = false;
	const char *ssl = NULL;
	int life_time = 2;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	char mip[MAXIPLEN];
	if (TEST(rq->workModel,WORK_MODEL_TPROXY) && TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
		if (TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)) {
			if (ip==NULL) {
				ip = rq->getClientIp();
			}
		}
		sockaddr_i s_sockaddr;
		socklen_t addr_len = sizeof(sockaddr_i);
		::getsockname(rq->c->socket->get_socket(), (struct sockaddr *) &s_sockaddr, &addr_len);
		KSocket::make_ip(&s_sockaddr, mip, MAXIPLEN);
		host = mip;
#ifdef KSOCKET_IPV6
		if (s_sockaddr.v4.sin_family == PF_INET6) {
			port = ntohs(s_sockaddr.v6.sin6_port);
		} else
#endif
		port = ntohs(s_sockaddr.v4.sin_port);
		isIp = true;
	}
#endif
#endif
	if (TEST(url->flags,KGL_URL_SSL)) {
		ssl = "s";
	}
#ifdef ENABLE_SIMULATE_HTTP
	/* ģ��������Ҫ�滻host��port */
	if (TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
		KSimulateSocket *ss = static_cast<KSimulateSocket *>(rq->c->socket);
		if (ss->host && *ss->host) {
			host = ss->host;
			if (ss->port>0) {
				port = ss->port;
			}
			life_time = ss->life_time;
		}
	}
#endif
	KRedirect *sa = cdnContainer.refsRedirect(ip,host,port,ssl,life_time,Proto_http,isIp);
	if (sa==NULL) {
		fo->connectCallBack(rq,NULL);
	} else {
		sa->connect(rq);
		sa->release();
	}
	KTHREAD_RETURN;
}
void bufferUpStreamReadBodyResult(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	bufCount = 1;
	int len;
	buf[0].iov_base = (char *)fo->getBodyBuffer(rq,len);
	buf[0].iov_len = len;
	return;
}
void resultUpStreamReadBodyResult(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadBody(rq,got);
}
void resultUpstreamReadPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadPost(rq,got);
}
void bufferUpstreamReadPost(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	int len;
	buf[0].iov_base = (char *)fo->getPostWBuffer(rq,len);
	buf[0].iov_len = len;
	bufCount = 1;
}
void resultUpstreamSendPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleSendPost(rq,got);
}
void bufferUpstreamSendPost(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->getPostRBuffer(rq,buf,bufCount);
}
void resultUpstreamSendHead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleSendHead(rq,got);
}
void bufferUpstreamSendHead(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->buffer.getRBuffer(buf,bufCount);
}
void resultUpstreamReadHead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadHead(rq,got);
}
void bufferUpstreamReadHead(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	int len;
	buf[0].iov_base = (char *)fo->getHeadRBuffer(rq,len);
	buf[0].iov_len = len;
	bufCount = 1;
	return;
}
void resultUpstreamConnectResult(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleConnectResult(rq,got);
}
void resultUpstreamSpdySendHeader(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleSpdySendHead(rq,got);
}
void KAsyncFetchObject::handleConnectResult(KHttpRequest *rq,int got)
{
	if (got==-1) {
		handleConnectError(rq,STATUS_GATEWAY_TIMEOUT,"connect to remote host time out");
		return;
	}
/////////[394]
	sendHead(rq);
}
void KAsyncFetchObject::retryOpen(KHttpRequest *rq)
{
	if (client) {
		if (!TEST(rq->flags,RQ_NET_BIG_OBJECT)) {
			rq->c->removeSocket();
		}
		client->removeSocket();
/////////[395]
		client->gc(-1);
		client = NULL;
/////////[396]
	}
	hot = header;
	open(rq);
}
void KAsyncFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	tryCount++;
	if (brd==NULL) {
		asyncDnsCallBack(rq);
		return;
		/*
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
		if (TEST(rq->workModel,WORK_MODEL_TPROXY) && TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
			asyncDnsCallBack(rq);
			return;
		}
#endif
#endif
		//�첽����
		rq->c->selector->removeRequest(rq);
		conf.dnsWorker->start(rq,asyncDnsCallBack);
		*/
		return;
	}
	if (!brd->rd->enable) {
		handleError(rq,STATUS_SERVICE_UNAVAILABLE,"extend is disable");
		return ;
	}
	badStage = BadStage_Connect;
	brd->rd->connect(rq);
}
void KAsyncFetchObject::connectCallBack(KHttpRequest *rq,KUpstreamSelectable *client,bool half_connection)
{
	this->client = client;
	if (client && client->selector==NULL) {
		client->selector = rq->c->selector;
	}
	if(this->client==NULL || this->client->socket->get_socket()==INVALID_SOCKET){
		if (client) {
			client->isBad(BadStage_Connect);
		}
		if (tryCount<=conf.errorTryCount) {
			//connect try again
			retryOpen(rq);
			return;
		}
		handleError(rq,STATUS_GATEWAY_TIMEOUT,"Cann't connect to remote host");
		return;
	}
	if (!TEST(rq->flags,RQ_NET_BIG_OBJECT)) {
		rq->c->removeSocket();
	}
	if (half_connection) {		
		client->connect(rq,resultUpstreamConnectResult);
	} else {
		sendHead(rq);
	}
}
void KAsyncFetchObject::sendHead(KHttpRequest *rq)
{
	//�����Է��ͽ׶�
	badStage = BadStage_TrySend;
	if (buffer.getLen()==0) {
		/////////[397]
		buildHead(rq);
	}
	unsigned len = buffer.startRead();
	if (len==0) {
		handleError(rq,STATUS_SERVER_ERROR,"cann't build head");
		return;
	}
	client->socket->setdelay();
	/////////[398]
	client->upstream_write(rq,resultUpstreamSendHead,bufferUpstreamSendHead);
}
void KAsyncFetchObject::continueReadBody(KHttpRequest *rq)
{
	//����Ƿ�Ҫ������body
	if (!checkContinueReadBody(rq)) {
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	if (!TEST(rq->flags,RQ_NET_BIG_OBJECT)) {
		rq->c->removeSocket();
	}
	client->upstream_read(rq,resultUpStreamReadBodyResult,bufferUpStreamReadBodyResult);
}
//������body����֮�󣬴ӻ�������body���ݲ�����
void KAsyncFetchObject::readBody(KHttpRequest *rq)
{
	int bodyLen;
	for (;;) {
		//�Ի�������body����
		char *body = nextBody(rq,bodyLen);
		if (body==NULL) {
			//������û����
			break;
		}
		//�����յ��˵�body����
		if (!pushHttpBody(rq,body,bodyLen)) {
			return;
		}
	}
	//������body
	continueReadBody(rq);
}
//�������body����
void KAsyncFetchObject::handleReadBody(KHttpRequest *rq,int got)
{
	assert(header);
	if (got<=0) {
		/////////[399]
		//assert(rq->send_ctx.body==NULL);
		/* ��bodyʧ��,����δ֪���ȣ����û������cache_no_length,�򲻻��� */
		lifeTime = -1;
		if (!TEST(rq->filter_flags,RF_CACHE_NO_LENGTH)
			|| TEST(rq->ctx->obj->index.flags,(ANSW_CHUNKED|ANSW_HAS_CONTENT_LENGTH))) { 
			SET(rq->ctx->obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
		}
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	//����body
	parseBody(rq,header,got);
	//�������������body����
	readBody(rq);
}
void KAsyncFetchObject::handleSpdySendHead(KHttpRequest *rq,int got)
{
	if (got<0) {
		handleConnectError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server");
		return;
	}
	assert(got==0);
	//���ͳɹ������뷢�ͳɹ��׶�,�˽׶ν�ֹ����
	badStage = BadStage_SendSuccess;
	sendHeadSuccess(rq);
}
void KAsyncFetchObject::sendHeadSuccess(KHttpRequest *rq)
{
	buffer.destroy();
	if (rq->left_read>0) {
		//handle post data
		if (rq->pre_post_length>0) {
			/* ����Ԥ���� */
#if 0
			if (client->spdy_ctx) {
				/*
				* �����spdy����Ҫ����post����
				* ������ݿ鷢��һ��fin��־.
				*/
				buffer.write_all(rq->parser.body,rq->pre_post_length);
				rq->left_read -= rq->pre_post_length;
				rq->pre_post_length = 0;
				sendPost(rq);
				return;
			}
#endif
			//send pre load post data
			client->upstream_write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
			return;
		}
		//read post data
		readPost(rq);
		return;
	}
	//����ͷ�ɹ�,��post���ݴ���.
	startReadHead(rq);
}
void KAsyncFetchObject::startReadHead(KHttpRequest *rq)
{
	client->socket->setnodelay();
	client->upstream_read(rq,resultUpstreamReadHead,bufferUpstreamReadHead);
}
void KAsyncFetchObject::handleSendHead(KHttpRequest *rq,int got)
{
	if (got<=0) {
		handleConnectError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server");
		return;
	}
	//���ͳɹ������뷢�ͳɹ��׶�,�˽׶ν�ֹ����
	badStage = BadStage_SendSuccess;
	if(buffer.readSuccess(got)){
		//continue send head
		client->upstream_write(rq,resultUpstreamSendHead,bufferUpstreamSendHead);
		return;
	}
	sendHeadSuccess(rq);
}
void KAsyncFetchObject::handleSendPost(KHttpRequest *rq,int got)
{
	if (got<=0) {
		handleConnectError(rq,STATUS_SERVER_ERROR,"cann't send post data to remote server");
		return;
	}
	assert(rq->left_read>=0);
	bool continueSendPost = false;
	if (rq->pre_post_length>0) {
		rq->left_read -= got;
		assert(got<=rq->parser.bodyLen);
		rq->parser.body += got;
		rq->parser.bodyLen -= got;
		rq->pre_post_length -= got;
		continueSendPost = rq->pre_post_length>0;
	} else {
		continueSendPost = buffer.readSuccess(got);
		if (!continueSendPost) {
			//����buffer,׼����һ��post
			buffer.destroy();
		}
	}
	if (continueSendPost) {
		client->upstream_write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
	} else {
		//try to read post
		if (rq->left_read==0) {
			startReadHead(rq);
			return;
		}
		readPost(rq);
	}
}
void KAsyncFetchObject::handleReadPost(KHttpRequest *rq,int got)
{
	if (got<=0) {
		stageEndRequest(rq);
		return;
	}
#ifdef ENABLE_INPUT_FILTER
	if (rq->hasInputFilter()) {
		int len;
		char *buf = getPostWBuffer(rq,len);
		if (JUMP_DENY==rq->if_ctx->check(buf,got,rq->left_read<=got)) {
			denyInputFilter(rq);
			return;
		}
	}
#endif
	rq->left_read-=got;
	buffer.writeSuccess(got);
	sendPost(rq);
}
void KAsyncFetchObject::readPost(KHttpRequest *rq)
{
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		int got = 0;
		char *tbuf = getPostWBuffer(rq,got);
		got = rq->tf->readBuffer(tbuf,got);
		if (got<=0) {
			handleError(rq,STATUS_SERVER_ERROR,"cann't read post data from temp file");
			return;
		}
		rq->left_read-=got;
		buffer.writeSuccess(got);
		sendPost(rq);
		return;
	}
#endif
#ifdef ENABLE_SIMULATE_HTTP
	if (TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
		KSimulateSocket *ss = static_cast<KSimulateSocket *>(rq->c->socket);
		int got = 0;
		char *tbuf = getPostWBuffer(rq,got);
		got = ss->post(ss->arg,tbuf,got);
		if (got<=0) {
			handleError(rq,STATUS_SERVER_ERROR,"cann't read post data from temp file");
			return;
		}
		rq->left_read-=got;
		buffer.writeSuccess(got);
		sendPost(rq);
		return;
	}
#endif
	//���û����ʱ�ļ���������Ϊ�Ѿ����˿ͻ���post���ݣ��޷����ã���������ͽ�ֹ��������
	tryCount = -1;
	client->removeSocket();
	rq->c->read(rq,resultUpstreamReadPost,bufferUpstreamReadPost);
}
void KAsyncFetchObject::sendPost(KHttpRequest *rq)
{	
	//����post
	buildPost(rq);
	buffer.startRead();
	if (!TEST(rq->flags,RQ_NET_BIG_OBJECT)) {
		rq->c->removeSocket();
	}
	client->upstream_write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
}
void KAsyncFetchObject::handleReadHead(KHttpRequest *rq,int got)
{
	char *buf = hot;
	if (got<=0) {
		handleConnectError(rq,STATUS_GATEWAY_TIMEOUT,"cann't recv head from remote server");
		return;
	}
	assert(hot);
	hot += got;
	switch(parseHead(rq,buf,got)){
		case Parse_Success:
			client->isGood();
			handleUpstreamRecvedHead(rq);
			break;
		case Parse_Failed:
			handleError(rq,STATUS_GATEWAY_TIMEOUT,"cann't parse upstream protocol");
			break;
		case Parse_Continue:
			client->upstream_read(rq,resultUpstreamReadHead,bufferUpstreamReadHead);
			break;
	}
	
}
void KAsyncFetchObject::handleConnectError(KHttpRequest *rq,int error,const char *msg)
{
	char ips[MAXIPLEN];
	client->socket->get_remote_ip(ips,sizeof(ips));
	klog(KLOG_INFO,"rq = %p connect to %s:%d error code=%d,msg=[%s],try count=%d,last errno=%d %s,socket=%d(%s) (%d %d)\n",
		(KSelectable *)rq,
		ips,
		client->socket->get_remote_port(),
		error,
		msg,
		tryCount,
		errno,
		strerror(errno),
		client->socket->get_socket(),
		(client->isNew()?"new":"pool"),
#ifndef NDEBUG
		(rq->c->socket->shutdownFlag?1:0),
		(client->socket->shutdownFlag?1:0)
#else
		2,2
#endif
		);
	assert(client);
	lifeTime = -1;
	client->isBad(badStage);
	SET(rq->flags,RQ_UPSTREAM_ERROR);
	if (badStage != BadStage_SendSuccess &&
		tryCount>=0 &&
		tryCount<=conf.errorTryCount) {
		//try again
		retryOpen(rq);
		return;
	}
	if (rq->ctx->lastModified>0 && TEST(rq->filter_flags,RF_IGNORE_ERROR)) {
		rq->ctx->obj->data->status_code = STATUS_NOT_MODIFIED;
		handleUpstreamRecvedHead(rq);
		return;
	}
	handleError(rq,error,msg);
}
