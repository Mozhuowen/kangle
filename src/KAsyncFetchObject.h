#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KUpstreamFetchObject.h"
#include "KSocketBuffer.h"
#include "KPoolableSocket.h"
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
class KAsyncFetchObject : public KUpstreamFetchObject
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
			rq->selector->removeSocket(rq);
			client->destroy(lifeTime);
			client = NULL;
		}
		KFetchObject::close(rq);

	}
	virtual ~KAsyncFetchObject()
	{
		assert(client==NULL);	
		if (client) {
			client->destroy(-1);
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
	void handleSendPost(KHttpRequest *rq,int got);
	void handleReadPost(KHttpRequest *rq,int got);
	
	//�õ�post�����壬���͵�upstream
	void getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int &bufCount)
	{		
		int pre_loaded_body = (int)(MIN(rq->parser.bodyLen,rq->left_read));
		if (pre_loaded_body > 0) {
			assert(rq->left_read>0);
			bufCount = 1;
#ifdef _WIN32
			buf[0].len= pre_loaded_body;
			buf[0].buf = rq->parser.body;
#else
			buf[0].iov_len= pre_loaded_body;
			buf[0].iov_base = rq->parser.body;
#endif
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
	KClientSocket *getSocket()
	{
		return client;
	}
#ifdef _WIN32
	KSelectable *getBindData()
	{
		client->bindcpio_flag = true;
		return client;
	}
#endif
	SOCKET getSockfd()
	{
		if (client) {
			return client->get_socket();
		}
		return INVALID_SOCKET;
	}
	void connectCallBack(KHttpRequest *rq,KPoolableSocket *client,bool half_connection = true);
	void handleConnectError(KHttpRequest *rq,int error,const char *msg);
	void handleConnectResult(KHttpRequest *rq,int got);
	KSocketBuffer buffer;
	KPoolableSocket *client;
	BadStage badStage;
protected:
	//header���·����ʱҪ���µ���ƫ����
	virtual void adjustBuffer(INT64 offset)
	{
	}
	//reopen����
	virtual void reset()
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
	void readPost(KHttpRequest *rq,bool useEvent=true);
	void sendPost(KHttpRequest *rq);
	int tryCount;
	void retryOpen(KHttpRequest *rq);
};
/////////[116]
#endif
