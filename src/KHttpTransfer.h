/*
 * KHttpTransfer.h
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

#ifndef KHTTPTRANSFER_H_
#define KHTTPTRANSFER_H_
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KDeChunked.h"
#include "KGzip.h"
#include "KSendable.h"
#include "KChunked.h"
/*
 * This class use to transfer data to client
 * It support compress(use gzip) and chunk transfer encoding.

 ��������:
 ����Դ(localfetch) -->   <--����Դ(pushģʽ)
 chunked����(�л���û��)-->
 degzip(���ݽ�ѹ��,��������ݹ��˱任)-->
 (KHttpTransfer::write_all������Ҳ����httpͷ)
 ���ݹ���/�任 KHttpRequest��checkFilter -->
 gzipѹ��-->
 ��������-->
 chunked����-->
 ���ٷ���-->
 socket
 */
class KHttpTransfer: public KWUpStream {
public:
	KHttpTransfer(KHttpRequest *rq, KHttpObject *obj);
	KHttpTransfer();
	virtual ~KHttpTransfer();
	bool sendUnknowHeader(char *attr, char *val);
	void init(KHttpRequest *rq, KHttpObject *obj);
	bool support_sendfile();
	/*
	 * send actual data to client. cann't head data or chunked head
	 */
	StreamState write_all(const char *str, int len);
	/*
	 * д���������������û�з���(���ܴ���buffer��)����Ҫ�����������ݡ�
	 */
	StreamState write_end();
	StreamState flush();
	friend class KDeChunked;
	friend class KGzip;
	/*
		�õ�һ���ܵ�д�������ܻ���ǰ�����dzip,unchunked
	*/
	KWStream *getWStream();
	void setBufferSize(unsigned bufferSize)
	{
		this->bufferSize = bufferSize;
	}
public:
	KHttpRequest *rq;
	KHttpObject *obj;
	KSubRequest *sr;
private:
	bool loadStream();
	/*
	 * �����ķ���http body���ݸ��û���then str can save direct.no need copy.
	 * �������Ҫ��������Ҳ��������͸�buffer���档
	 * chunked���ݻ�������任���������write_all�����ݷ����û���
	 */
	//bool realSendBody(char *str, int len, bool directSave = false);
	//bool checkDeChunkFirst();
	bool gzip_layer;
	bool cache_layer;
	StreamState sendHead(bool haveAllData,INT64 &start,INT64 &send_len);
	StreamState sendBuffer(bool isLast);
	void swapBuffer();
	bool isHeadSend;
	KWStream *wst;
	bool wstDelete;
	u_short workModel;
	KBuffer buffer;
	unsigned bufferSize;
	bool responseChecked;
};

#endif /* KHTTPTRANSFER_H_ */