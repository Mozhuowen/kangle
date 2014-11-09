/*
 * KPoolableStreamContainer.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KPOOLABLESTREAMCONTAINER_H_
#define KPOOLABLESTREAMCONTAINER_H_
#include <list>
#include "global.h"
#include "KPoolableSocket.h"
#include "KString.h"
#include "KMutex.h"
#include "KCountable.h"
#include "time_utils.h"
/*
 * ���ӳ�������
 */
class KPoolableSocketContainer: public KCountableEx {
public:
	KPoolableSocketContainer();
	virtual ~KPoolableSocketContainer();
	KPoolableSocket *getPoolSocket();
	/*
	��������
	close,�Ƿ�ر�
	lifeTime ����ʱ��
	*/
	virtual void gcSocket(KPoolableSocket *st,int lifeTime);
	void bind(KPoolableSocket *st);
	void unbind(KPoolableSocket *st);
	int getLifeTime() {
		return lifeTime;
	}
	/*
	 * �������ӳ�ʱʱ��
	 */
	void setLifeTime(int lifeTime);
	/*
	 * ����ˢ��ɾ����������
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * �����������
	 */
	void clean();
	/*
	 * �õ�������
	 */
	unsigned getSize() {
		lock.Lock();
		unsigned size = pools.size();	
		lock.Unlock();
		return size;
	}
	//isBad,isGood���ڼ���������
	virtual void isBad(KPoolableSocket *st,BadStage stage)
	{
	}
	virtual void isGood(KPoolableSocket *st)
	{
	}
#ifdef HTTP_PROXY
	virtual void buildHttpAuth(KWStream &s)
	{
	}
#endif
protected:
	/*
	 * �����������������
	 */
	void putPoolSocket(KPoolableSocket *st);
	/*
		֪ͨ�¼�.
		ev = 0 �ر�
		ev = 1 ����pool
	*/
	//virtual void noticeEvent(int ev,KPoolableSocket *st)
	//{

	//}

	KPoolableSocket *internalGetPoolSocket();
	int lifeTime;
	std::list<KPoolableSocket *> pools;
	KMutex lock;
private:
	void refreshPool(std::list<KPoolableSocket *> *pools)
	{
		std::list<KPoolableSocket *>::iterator it2;
		for (it2 = pools->end(); it2 != pools->begin();) {
			it2--;
			if ((*it2)->expireTime <= kgl_current_msec) {
				assert((*it2)->container == NULL);
				delete (*it2);
				it2 = pools->erase(it2);
			} else {
				break;
			}
		}
	}

};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
