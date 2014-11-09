#ifndef KTEMPFILE_H
#define KTEMPFILE_H
#include "global.h"
#include <stdio.h>
#include <string>
#include "forwin32.h"
#include "KSocketBuffer.h"
#include "KFile.h"
#ifdef ENABLE_TF_EXCHANGE
class KHttpRequest;
class KTempFile
{
public:
	KTempFile();
	~KTempFile();
	//��ʼ��,length���Ѿ����ȣ����Ϊ-1,�򳤶�δ֪
	void init(INT64 length);
	bool writeBuffer(KHttpRequest *rq,const char *buf,int len);
	char *writeBuffer(int &size);
	bool writeSuccess(KHttpRequest *rq,int got);
	void switchRead();
	//���¶�
	void resetRead();
	int readBuffer(char *buf,int size);
	char *readBuffer(int &size);
	//����true,�����������������
	bool readSuccess(int got);
	bool isWrite()
	{
		return writeModel;
	}
	bool checkLast(int got)
	{
		return total_size + got >= length;
	}
	INT64 getSize()
	{
		return total_size;
	}
private:
	bool openFile(KHttpRequest *rq);
	bool dumpOutBuffer();
	bool dumpInBuffer();
	KFile fp;
	KSocketBuffer buffer;
	INT64 length;
	//�ܳ���
	INT64 total_size;
	std::string file;
	bool writeModel;
};
//��post���ݵ���ʱ�ļ�
void stageReadTempFile(KHttpRequest *rq);
FUNC_TYPE FUNC_CALL clean_tempfile_thread(void *param);
#endif
#endif
