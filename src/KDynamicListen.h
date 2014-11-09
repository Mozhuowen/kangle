#ifndef KDYNAMICLISTEN_H
#define KDYNAMICLISTEN_H
#include <map>
#include "KServer.h"
#include "do_config.h"
/*
* �����˿ڹ���
* ����virtualhost��<bind>!ip:port</bind>
*/
class KListenKey
{
public:

	bool operator < (const KListenKey &a) const
	{
		int ret = strcmp(ip.c_str(),a.ip.c_str());
		if (ret < 0) {
			return true;
		}
		if (ret > 0) {
			return false;
		}
		if (port < a.port) {
			return true;
		}
		if (port > a.port) {
			return false;
		}
		return ipv4 < a.ipv4;
	}
	std::string ip;
	int port;
	bool ipv4;
	bool ssl;
};
/**
* ��virtualHostManager��������
*/
class KDynamicListen
{
public:
	KDynamicListen()
	{
		failedTries = 0;
	}
	void add(const char *listen,KVirtualHost *vh);
	void remove(const char *listen,KVirtualHost *vh);
	bool add(KListenHost *lh,bool start);
	void addStaticVirtualHost(KVirtualHost *vh);
	void removeStaticVirtualHost(KVirtualHost *vh);
	void flush(const char *listen);
	void flush();
	void delayStart();
	void getListenHtml(std::stringstream &s);
	void clear();
	void close();
	std::map<KListenKey,KServer *> listens;
private:	
	void parseListen(const char *listen,std::list<KListenKey> &lk);
	void parseListen(KListenHost *lh,std::list<KListenKey> &lk);
	bool initListen(const KListenKey &lk,KServer *server);
	KListenKey getListenKey(KListenHost *lh,bool ipv4);
	int failedTries; 
};
extern KDynamicListen dlisten;
#endif
