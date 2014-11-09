#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KUpstreamFetchObject.h"
#include "KSocketBuffer.h"
#include "KUpstreamSelectable.h"
#include "KHttpObject.h"
#include "KSelector.h"
#include "KSelectorManager.h"
enum Parse_Result
{
	Parse_Failed,
	Parse_Success,
	Parse_Continue
};
/**
* �첽������չ������֧���첽���õ���չ�Ӹ���̳�
*/
class KAsyncFetchObject : public KFetchObject
{
public:
	KAsyncFetchObject()
	{
		client = NULL;
		header = NULL;
		hot = NULL;
		current_size = 0;
		badStage = BadStage_Connect;
		//Ĭ�϶�����
		lifeTime = -1;
		tryCount = 0;
	}
	void close(KHttpRequest *rq)
	{
		if (client) {
			if (TEST(rq->filter_flags,RF_UPSTREAM_NOKA) 
				|| (rq->ctx->obj && TEST(rq->ctx->obj->index.flags,ANSW_CLOSE))) {
				lifeTime = -1;
			}
			client->removeSocket();
			client->gc(lifeTime);
			client = NULL;
		}
		KFetchObject::close(rq);

	}
	virtual ~KAsyncFetchObject()
	{
		assert(client==NULL);	
		if (client) {
			client->gc(-1);
		}
		if (header) {
			free(header);
		}
	}
	//�����е���ɣ������������ڱ�ʶ�����ӻ�����
	virtual void expectDone()
	{
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		return true;
	}
#endif
	void open(KHttpRequest *rq);
	void sendHead(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);

	void handleReadBody(KHttpRequest *rq,int got);
	void handleReadHead(KHttpRequest *rq,int got);
	void handleSendHead(KHttpRequest *rq,int got);
	void handleSpdySendHead(KHttpRequest *rq,int got);
	void handleSendPost(KHttpRequest *rq,int got);
	void handleReadPost(KHttpRequest *rq,int got);
	
	//�õ�post�����壬���͵�upstream
	void getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int &bufCount)
	{		

		if (rq->pre_post_length > 0) {
			assert(rq->left_read>0);
			bufCount = 1;
			buf[0].iov_len= rq->pre_post_length;
			buf[0].iov_base = rq->parser.body;
			return;
		}
		buffer.getRBuffer(buf,bufCount);
	}
	//�õ�postд���壬��client����post����
	char *getPostWBuffer(KHttpRequest *rq,int &len)
	{
		assert(rq->parser.bodyLen==0);
		assert(rq->left_read>0);
		char *buf = buffer.getWBuffer(len);
		len = (int)(MIN((INT64)len,rq->left_read));
		assert(len>0);
		return buf;
	}
	//�õ�head���壬��upstream��head
	char *getHeadRBuffer(KHttpRequest *rq,int &len)
	{
		if (header==NULL) {
			len = current_size = NBUFF_SIZE;
			header = (char *)malloc(current_size);
			hot = header;
			return hot;
		}
		assert(hot);
		unsigned used = hot - header;
		assert(used<=current_size);
		if (used>=current_size) {
			int new_size = current_size * 2;
			char *n = (char *)malloc(2 * current_size);
			memcpy(n,header,current_size);
			adjustBuffer(n - header);
			free(header);
			header = n;
			hot = header + current_size;
			current_size = new_size;
		}
		len = current_size - used;
		return hot;
	}
	//�õ�body����,��upstream��
	char *getBodyBuffer(KHttpRequest *rq,int &len)
	{
		//��body��ʱ���û�header�Ļ���
		len = current_size;
		assert(len>0);
		return header;
	}
	KConnectionSelectable *getSelectable()
	{
		return client;
	}
	KClientSocket *getSocket()
	{
		return client->socket;
	}
	void connectCallBack(KHttpRequest *rq,KUpstreamSelectable *client,bool half_connection = true);
	void handleConnectError(KHttpRequest *rq,int error,const char *msg);
	void handleConnectResult(KHttpRequest *rq,int got);
	KSocketBuffer buffer;
	KUpstreamSelectable *client;
	BadStage badStage;
protected:
	void sendHeadSuccess(KHttpRequest *rq);
	//header���·����ʱҪ���µ���ƫ����
	virtual void adjustBuffer(INT64 offset)
	{
	}
	int lifeTime;
	char *header;
	char *hot;
	unsigned current_size;
	//��������ͷ��buffer�С�
	virtual void buildHead(KHttpRequest *rq) = 0;
	//����head
	virtual Parse_Result parseHead(KHttpRequest *rq,char *data,int len) = 0;
	//����post���ݵ�buffer�С�
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	//����Ƿ�Ҫ������body,һ�㳤������Ҫ��
	//���������content-length���øú���
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
	}
	//��ȡbody����,���� 
	virtual char *nextBody(KHttpRequest *rq,int &len) = 0;
	//����body
	virtual Parse_Result parseBody(KHttpRequest *rq,char *data,int len) = 0;
private:
	void continueReadBody(KHttpRequest *rq);
	void readPost(KHttpRequest *rq);
	void sendPost(KHttpRequest *rq);
	int tryCount;
	void retryOpen(KHttpRequest *rq);
	void startReadHead(KHttpRequest *rq);
};
/////////[154]
#endif
