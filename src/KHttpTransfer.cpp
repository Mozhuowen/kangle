/*
 * KHttpTransfer.cpp
 *
 *  Created on: 2010-5-4
 *      Author: keengo
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

#include "http.h"
#include "KHttpTransfer.h"
#include "malloc_debug.h"
#include "KCacheStream.h"
#include "KGzip.h"
#include "KContentTransfer.h"
#include "KSelector.h"
#include "KFilterContext.h"
#include "KSubRequest.h"
KHttpTransfer::KHttpTransfer(KHttpRequest *rq, KHttpObject *obj) {
	init(rq, obj);
}
KHttpTransfer::KHttpTransfer() {
	init(NULL, NULL);
}
void KHttpTransfer::init(KHttpRequest *rq, KHttpObject *obj) {
	isHeadSend = false;
	this->rq = rq;
	this->obj = obj;
	wst = NULL;
	wstDelete = false;
	bufferSize = conf.buffer;
	responseChecked = false;
	if (rq) {
		sr = rq->sr;
		workModel = rq->workModel;
	} else {
		sr = NULL;
		workModel = 0;
	}
}
KHttpTransfer::~KHttpTransfer() {
	if(wstDelete && wst){
		delete wst;
	}
}
void KHttpTransfer::swapBuffer() {
	if (!TEST(obj->index.flags,ANSW_NO_CACHE)) {
		set_buffer_obj(buffer,obj);
	}
}
KWStream *KHttpTransfer::getWStream()
{
	if(wst){
		return wst;
	}
	wst = makeWriteStream(rq,obj,this,wstDelete);
	assert(wst);
	return wst;
}
StreamState KHttpTransfer::sendBuffer(bool isLast)
{
	StreamState result = STREAM_WRITE_SUCCESS;
	INT64 start = 0;
	INT64 send_len = 0;
	if(!isHeadSend){
		if (sendHead(isLast,start,send_len) != STREAM_WRITE_SUCCESS) {
			return STREAM_WRITE_FAILED;
		}
		if(rq->status_code == STATUS_CONTENT_PARTIAL 
			&& obj->data->status_code == STATUS_OK 
#ifdef ENABLE_TF_EXCHANGE
			&& rq->tf==NULL
#endif
			){
			kassert(isLast);
			result = buffer.send(st,start,send_len);
			buffer.clean();
			return result;
		}
	}
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf==NULL||conf.tmpfile==2) {
#endif
		if(st==rq && !TEST(rq->flags,RQ_SYNC) && rq->buffer.getLen()==0 
#ifdef ENABLE_TF_EXCHANGE
			&& rq->tf==NULL
#endif
			){
			//ֱ�ӽ�������
			buffer.swap(&rq->buffer);
			result = STREAM_WRITE_SUCCESS;
		} else {
			result = buffer.send(st);
			buffer.clean();
		}
#ifdef ENABLE_TF_EXCHANGE
	}
#endif
	return result;
}
StreamState KHttpTransfer::write_all(const char *str, int len) {
	if (len<=0) {
		return STREAM_WRITE_SUCCESS;
	}
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf==NULL || conf.tmpfile==2) {
		//tmpfileΪ2��ʾ˫�ػ���
#endif
		if (!TEST(rq->filter_flags,RF_NO_BUFFER)) {
			//���û������Ϊ�����壬�򻺳嵽buffer
			buffer.write_all(str, len);
			if (buffer.getLen() < bufferSize) {
				return STREAM_WRITE_SUCCESS;
			}
			return sendBuffer(false);
		}
#ifdef ENABLE_TF_EXCHANGE
	}
#endif
	//�����tempfile,��˴������壬��tempfile����
	if (!isHeadSend) {
		INT64 start = 0;
		INT64 send_len = 0;
		if (sendHead(false,start,send_len) != STREAM_WRITE_SUCCESS) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	return st->write_all(str,len);
}
StreamState KHttpTransfer::flush() {
	if (sendBuffer(false)==STREAM_WRITE_FAILED) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return STREAM_WRITE_FAILED;
	}
	return KWUpStream::flush();
}
StreamState KHttpTransfer::write_end() {
	if (preventWriteEnd) {
		return STREAM_WRITE_SUCCESS;
	}
	if (sendBuffer(true)==STREAM_WRITE_FAILED) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return STREAM_WRITE_FAILED;
	}
	return KWUpStream::write_end();
}
bool KHttpTransfer::support_sendfile()
{
	if (sendBuffer(false)==STREAM_WRITE_FAILED) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return false;
	}
	return st == static_cast<KWStream *>(rq);
}
StreamState KHttpTransfer::sendHead(bool haveAllData,INT64 &start,INT64 &send_len) {
	StreamState result = STREAM_WRITE_SUCCESS;
	isHeadSend = true;
	INT64 content_len = -1;
	cache_layer = true;
	if (haveAllData) {
		CLR(obj->index.flags,ANSW_CHUNKED);
		SET(obj->index.flags,ANSW_HAS_CONTENT_LENGTH);
		content_len = buffer.getLen();
#ifdef ENABLE_TF_EXCHANGE
		rq->closeTempFile();
#endif
	} else {
		CLR(rq->flags,RQ_HAVE_RANGE);
		if (TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
			content_len = obj->index.content_length;
			if (content_len > conf.max_cache_size) {
				cache_layer = false;
			}
		}
	}
	if (rq->needFilter()) {
		/*
		 ����Ƿ���Ҫ�������ݱ任�㣬���Ҫ���򳤶�δ֪
		 */
		content_len = -1;
		cache_layer = true;
	}
	gzip_layer = false;	
	
	if (TEST(workModel,WORK_MODEL_INTERNAL) && !TEST(workModel,WORK_MODEL_REPLACE)) {
		//����header
		//*
		if (!TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
			KBuffer b;
			KHttpHeader *header = obj->data->headers;
			while (header) {
				//ֻ����Set-Cookieͷ
				if (strcasecmp(header->attr,"Set-Cookie")==0 
					|| strcasecmp(header->attr,"Set-Cookie2")==0) {
					attach_av_pair_to_buff(header->attr, header->val, &b);
				}
				header = header->next;
			}
			buff *exsitHeader = rq->send_ctx.header;
			buff *tmpHeader;
			while (exsitHeader) {
				b.write_all(exsitHeader->data,exsitHeader->used);			
				tmpHeader = exsitHeader;
				exsitHeader = exsitHeader->next;
				free(tmpHeader->data);
				free(tmpHeader);
			}
			rq->send_ctx.header = NULL;
			rq->addSendHeader(&b);
		}
		//*/
	} else {
		if (check_need_gzip(rq, obj)) {
			if (
	#ifdef ENABLE_TF_EXCHANGE
				(rq->tf && !haveAllData) || 
	#endif
				buffer.getLen() >= conf.min_gzip_length) {
				SET(rq->flags,RQ_TE_GZIP);
				SET(obj->index.flags,FLAG_GZIP);
				content_len = -1;
				gzip_layer = true;
				cache_layer = true;
			}
		}
		KBuffer hdr_buffer;
		build_obj_header(rq, obj, hdr_buffer, content_len,start,send_len);
		if (TEST(rq->flags,RQ_SYNC)) {
			result = hdr_buffer.send(rq);
			if (result != STREAM_WRITE_SUCCESS) {
				return result;
			}
		} else {
			rq->addSendHeader(&hdr_buffer);
		}
	}
	loadStream();
	
	return result;
}
bool KHttpTransfer::loadStream() {
	assert(st==NULL && rq);
	if (sr) {
		st = sr->ctx->st;
	} else {
		st = rq;
	}
	/*
	 ���µ��Ͽ�ʼ����
	 �����������ٷ���.
	 */
	autoDelete = false;
	/*
	 ����Ƿ���chunk��
	 */
	if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && TEST(rq->flags,RQ_TE_CHUNKED)) {
		KWStream *st2 = new KChunked(st, autoDelete);
		//debug("����chunked��=%p,up=%p\n",st2,st);
		if (st2) {
			st = st2;
			autoDelete = true;
		} else {
			return false;
		}
	}
	//����Ƿ����cache��
	KCacheStream *st_cache = NULL;
	if (cache_layer && !TEST(obj->index.flags,ANSW_NO_CACHE)) {
		//debug("����cache��,up=%p\n",st);
		st_cache = new KCacheStream(st, autoDelete);
		if (st_cache) {
			st_cache->init(obj);
			st = st_cache;
			autoDelete = true;
		}
	}
	//����Ƿ�Ҫ����gzipѹ����
	//if(TEST(rq->flags,RQ_TE_GZIP)){
	if (gzip_layer) {
		KGzipCompress *st_gzip = new KGzipCompress(st, autoDelete);
		//debug("����gzipѹ����=%p,up=%p\n",st_gzip,st);
		if (st_gzip) {
			st = st_gzip;
			autoDelete = true;
		} else {
			return false;
		}
	}
	//���ݱ任��
	if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && rq->needFilter()) {
		//debug("�������ݱ任��\n");
		KContentTransfer *st_content = new KContentTransfer(st, autoDelete);
		if (st_content) {
			st_content->init(rq);
			st = st_content;
			autoDelete = true;
		} else {
			return false;
		}
		if (rq->of_ctx) {
			KWStream *filter_st_head = rq->of_ctx->getFilterStreamHead();
			if (filter_st_head) {				
				KWUpStream *filter_st_end = rq->of_ctx->getFilterStreamEnd();
				assert(filter_st_end);
				if (filter_st_end) {
					filter_st_end->connect(st,autoDelete);
					//st ��rq->of_ctx��������autoDeleteΪfalse
					autoDelete = false;
					st = filter_st_head;
				}
			}
		}
	}
	//�������
	return true;
}
