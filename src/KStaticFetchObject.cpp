#include "KStaticFetchObject.h"
#include "http.h"
#include "KContentType.h"
#include "KVirtualHostManage.h"
/////////[28]
void handleAsyncRead(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	fo->handleAsyncReadBody(rq,got);
}
KTHREAD_FUNCTION simulateAsyncRead(void *param)
{
	KHttpRequest *rq = (KHttpRequest *)param;
        KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	fo->syncReadBody(rq);
	KTHREAD_RETURN;
}
void KStaticFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	assert(!rq->file->isDirectory());
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_HAS_CONTENT_LENGTH|ANSW_LOCAL_SERVER);
	if (rq->ctx->lastModified > 0 && rq->ctx->lastModified == rq->file->getLastModified()) {
		if (rq->ctx->mt==modified_if_modified) {
			obj->data->status_code = STATUS_NOT_MODIFIED;
			handleUpstreamRecvedHead(rq);
			return;
		}
	} else if (rq->ctx->mt == modified_if_range) {
		CLR(rq->flags,RQ_HAVE_RANGE);
	}
	assert(rq->file);
#ifdef _WIN32
	/////////[29]
#else
	const char *filename = rq->file->getName();
	if (filename) {
		fp.open(filename,fileRead,(ad?KFILE_ASYNC:0));
	}
#endif
	if (!fp.opened()) {
		handleError(rq,STATUS_NOT_FOUND,"file not found");
		return;
	}
	/////////[30]
	if (!rq->sr) {
#ifdef ENABLE_TF_EXCHANGE
		if (rq->tf) {
			//��̬����������ʱ�ļ�
			delete rq->tf;
			rq->tf = NULL;
		}
#endif
		//����content-type
		if (!stageContentType(rq,obj)) {
			handleError(rq,STATUS_FORBIDEN,"cann't find such content-type");
			return;
		}
		//����last-modified
		SET(obj->index.flags,ANSW_LAST_MODIFIED);
		obj->index.content_length = rq->file->fileSize;
		obj->index.last_modified = rq->file->getLastModified();
		char tmp_buf[42];
		mk1123time(obj->index.last_modified, tmp_buf, 41);
		obj->insertHttpHeader("Last-Modified",(const char *)tmp_buf);
	}
	if (TEST(rq->flags,RQ_HAVE_RANGE)) {
		//����������������
		SET(obj->index.flags,ANSW_HAS_CONTENT_RANGE);
		rq->ctx->content_range_length = rq->file->fileSize;
		INT64 content_length = rq->file->fileSize;
		if(!adjust_range(rq,rq->file->fileSize)){
			handleError(rq,416,"range error");
		    return;
		}
		/////////[31]
		if (!fp.seek(rq->range_from,seekBegin)) {
			handleError(rq,500,"cann't seek to right position");
			return ;
		}
		if (!TEST(rq->flags,RQ_URL_RANGED)) {
			KStringBuf b;
			char buf[INT2STRING_LEN];
			b.WSTR("bytes ");
			b << int2string(rq->range_from, buf) << "-" ;
			b << int2string(rq->range_to, buf) << "/" ;
			b << int2string(content_length, buf);
			obj->insertHttpHeader2(xstrdup("Content-Range"),b.stealString());
			obj->index.content_length = rq->file->fileSize;
			obj->data->status_code = STATUS_CONTENT_PARTIAL;
		}
	} else {
		//����status_code
		if (obj->data->status_code==0) {
			obj->data->status_code = STATUS_OK;
		}
	}
	//rq->buffer << "1234";
	//֪ͨhttpͷ�Ѿ��������
	handleUpstreamRecvedHead(rq);
}
void KStaticFetchObject::readBody(KHttpRequest *rq)
{
	assert(rq->file);
	assert(fp.opened());
#if 0
	if (!fp.opened()) {
		stage_rdata_end(rq,STREAM_WRITE_FAILED);
		return ;
	}
#endif
#ifdef _WIN32
	if (ad) {
		asyncReadBody(rq);
		return;
	}
#else
	//simulate async
	if (conf.async_io) {
		rq->selector->removeSocket(rq);
		conf.ioWorker->start(rq,simulateAsyncRead);
		return;
	}
#endif
	syncReadBody(rq);
}
void KStaticFetchObject::syncReadBody(KHttpRequest *rq)
{
	assert(rq->ctx->st);
	//if (rq->ctx->st->support_sendfile()) {
		//todo: sendfile;
		//printf("support sendfile\n");
	//}
	char buf[8192];
	int len;
	do {
		len = (int) MIN(rq->file->fileSize,(INT64)sizeof(buf));
		if (len <= 0) {
			stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
			return ;
		}	
		len = fp.read(buf, len);
		if(len<=0){
			stage_rdata_end(rq,STREAM_WRITE_FAILED);
			return ;
		}	
		rq->file->fileSize -= len;
	} while(pushHttpBody(rq,buf,len));
}
//�첽io�����
void KStaticFetchObject::handleAsyncReadBody(KHttpRequest *rq,int got)
{
	if (got<=0) {
		stage_rdata_end(rq,STREAM_WRITE_FAILED);
		return;
	}
	rq->file->fileSize -= got;
	/////////[32]
	if (!pushHttpBody(rq,ad->buf,got)) {
		return;
	}
	asyncReadBody(rq);
	return;
}
//�첽io��
void KStaticFetchObject::asyncReadBody(KHttpRequest *rq)
{
	int len = (int) MIN(rq->file->fileSize,(INT64)sizeof(ad->buf));
	if (len <= 0) {
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	/////////[33]
	rq->handler = handleAsyncRead;
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_ASYNC_READ);
	return;
}