#ifndef KREADWRITEBUFFER_H
#define KREADWRITEBUFFER_H
#include "KBuffer.h"
#include "malloc_debug.h"
class KReadWriteBuffer
{
public:
	KReadWriteBuffer()
	{
		memset(this,0,sizeof(KReadWriteBuffer));
	}
	~KReadWriteBuffer()
	{
		while (head) {
			buff *next = head->next;
			xfree(head->data);
			xfree(head);
			head = next;
		}
	}
	inline KReadWriteBuffer & operator <<(const char *str)
	{
		write_all(str, (int)strlen(str));
		return *this;
	}
	inline KReadWriteBuffer & operator <<(const char c) {
		write_all(&c, 1);
		return *this;
	}
	inline KReadWriteBuffer & operator <<(const int value) {
		char buf[16];
		int len = snprintf(buf, 15, "%d", value);
		if (len <= 0) {
			return *this;
		}
		write_all(buf, len);
		return *this;
	}
	inline KReadWriteBuffer & operator <<(const INT64 value) {
		char buf[INT2STRING_LEN];
		int len = snprintf(buf, INT2STRING_LEN - 1, INT64_FORMAT, value);
		if (len <= 0) {
			return *this;
		}
		write_all(buf, len);
		return *this;
	}
	void getReadBuffer(LPWSABUF buffer,int &bufferCount);
	char *getReadBuffer(int &len);
	bool readSuccess(int got);
	void writeSuccess(int got);
	char *getWriteBuffer(int &len);
	inline void appendBuffer(buff *buf)
	{
		if (write_hot_buf==NULL) {
			head = buf;
		} else {
			write_hot_buf->next = buf;
		}
		buf->next = NULL;
		write_hot_buf = buf;
		totalLen += buf->used;
	}
	int read(char *buf,int len);
	void write_byte(int ch)
	{
		char temp[2];
		temp[0] = ch;
		write_all(temp,1);
	}
	void write_all(const char *buf, int len);
	inline void print()
	{
		buff *tmp = head;
		while(tmp){
			if(tmp->used>0){
				fwrite(tmp->data,1,tmp->used,stdout);
			}
			tmp = tmp->next;
		}
	}
	inline void destroy()
	{
		while (head) {
			buff *next = head->next;
			free(head->data);
			free(head);
			head = next;
		}
		memset(this,0,sizeof(KReadWriteBuffer));
	}
	inline void clean()
	{
		destroy();
	}
	unsigned getLen()
	{
		return totalLen;
	}
	buff *getHead()
	{
		return head;
	}
	buff *stealBuff()
	{
		buff *ret = head;
		memset(this,0,sizeof(*this));
		return ret;
	}
private:
	inline buff *newbuff()
	{
		buff *buf = (buff *)malloc(sizeof(buff));
		buf->flags = 0;
		buf->data = (char *)malloc(NBUFF_SIZE);
		buf->used = 0;
		buf->next = NULL;
		return buf;
	}
	buff *head;
	buff *write_hot_buf;
	char *read_hot;
	char *write_hot;
	int totalLen;
};
#endif
