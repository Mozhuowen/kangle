/*
 * KPoolableSocketContainer.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KPoolableSocketContainer.h"
#include "log.h"
#include "time_utils.h"
#include "KSelectorManager.h"
using namespace std;

KPoolableSocketContainer::KPoolableSocketContainer() {
	lifeTime = 0;
}
KPoolableSocketContainer::~KPoolableSocketContainer() {
	clean();
}

void KPoolableSocketContainer::unbind(KUpstreamSelectable *st) {	
	release();
}
void KPoolableSocketContainer::gcSocket(KUpstreamSelectable *st,int lifeTime) {
	if (this->lifeTime <= 0) {
		debug("sorry the lifeTime is zero.we must close it\n");
		//noticeEvent(0, st);
		st->destroy();
		return;
	}
	if (lifeTime<0) {
		debug("the poolableSocket have error,we close it\n");
		//noticeEvent(0, st);
		st->destroy();
		return;
	}
	if (lifeTime == 0 || lifeTime>this->lifeTime) {
		lifeTime = this->lifeTime;
	}
	st->expireTime = kgl_current_msec + (lifeTime * 1000);
	putPoolSocket(st);
}
void KPoolableSocketContainer::putPoolSocket(KUpstreamSelectable *st)
{
	lock.Lock();
	assert(st->container);
	st->container = NULL;
	pools.push_front(st);
	//debug("success put it to container[%p],size=[%d]\n", container,
	//		container->pools.size());
	lock.Unlock();
	unbind(st);
}
KUpstreamSelectable *KPoolableSocketContainer::internalGetPoolSocket() {

	list<KUpstreamSelectable *>::iterator it;
	list<KUpstreamSelectable *> *pools = &this->pools;
	/*
	 * ������ܵ���refresh�����ｫ������������ӡ�
	 */
	for (;;) {
		if (pools->size() == 0) {
			return NULL;
		}
		it = --pools->end();
		if (kgl_current_msec < (*it)->expireTime) {
			break;
		}
		//printf("���ӹ�����!\n");
		assert((*it)->container == NULL);
		(*it)->destroy();
		pools->erase(it);
	}	
	it = pools->begin();
	assert(it!=pools->end());
	KUpstreamSelectable *socket = (*it);
	pools->erase(it);
	bind(socket);
	return socket;	
}
void KPoolableSocketContainer::bind(KUpstreamSelectable *st) {
	assert(st->container==NULL);
	st->container = this;
	refsLock.Lock();
	refs++;
	refsLock.Unlock();
}
void KPoolableSocketContainer::setLifeTime(int lifeTime) {
	this->lifeTime = lifeTime;
	if (lifeTime <= 0) {
		clean();
	}
}
void KPoolableSocketContainer::refresh(time_t nowTime) {
	lock.Lock();
	refreshPool(&pools);
	lock.Unlock();
}
KUpstreamSelectable *KPoolableSocketContainer::getPoolSocket() {
	lock.Lock();
	KUpstreamSelectable *socket = internalGetPoolSocket();
	lock.Unlock();
	return socket;
}
void KPoolableSocketContainer::clean() {
	lock.Lock();
	std::list<KUpstreamSelectable *>::iterator it;
	for (it = pools.begin(); it != pools.end(); it++) {
		assert((*it)->container==NULL);
		(*it)->destroy();
	}
	pools.clear();
	lock.Unlock();
}

