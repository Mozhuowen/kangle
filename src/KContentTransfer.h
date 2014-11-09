#ifndef KCONTENTTRANSFER_H
#define KCONTENTTRANSFER_H
#include "KStream.h"
#include "KHttpRequest.h"

/*
���ݱ任
*/
class KContentTransfer : public KWUpStream
{
public:
	KContentTransfer(KWStream *st,bool autoDelete) : KWUpStream(st,autoDelete)
	{
		rq = NULL;
	}
	/*
	��������httpͷ�ٵ������init���������ʵ��������
	*/
	bool init(KHttpRequest *rq);
	StreamState write_all(const char *str,int len);
private:
	KHttpRequest *rq;
};
#endif
