/*
 * KVirtualProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include <list>
#include "KVirtualHostProcess.h"
#include "KAsyncFetchObject.h"
#include "lang.h"
/////////[234]
using namespace std;
//��������ʧ�ܵ�����Ĵ���
void handleVProcessPower(VProcessPowerParam *vpp,std::list<KHttpRequest *> &queue,bool success,KPoolableSocket *socket,bool half_connect)
{
	if (vpp->rq) {
		//����е�һ�����󣬰��״����Ӹ���
/////////[235]
		if (success){
			if (socket==NULL) {
				half_connect = false;
				socket = vpp->process->connect(vpp->rq,vpp->rd,half_connect);
			}
		} else {
			assert(socket==NULL);
			//socket cann't be NULL.but is socket is not NULL.we also can handle it.
			if (socket) {
				delete socket;
				socket = NULL;
			}
		}
		assert(vpp->rq && vpp->process && vpp->rd);
		static_cast<KAsyncFetchObject *>(vpp->rq->fetchObj)->connectCallBack(vpp->rq,socket,half_connect);
	} else {
		//û�е�һ�����󣬰��״�����ɾ��
		if (socket) {
			delete socket;
			socket = NULL;
		}
	}
	list<KHttpRequest *>::iterator it;
	for (it = queue.begin(); it!=queue.end(); it++) {
		if (success) {
			socket = vpp->process->connect((*it),vpp->rd,half_connect);
		}
		static_cast<KAsyncFetchObject *>((*it)->fetchObj)->connectCallBack((*it),socket,half_connect);
	}
}
//�������̹����߳�
FUNC_TYPE FUNC_CALL VProcessPowerWorker(void *param)
{
	VProcessPowerParam *vpp = (VProcessPowerParam *)param;
	assert(vpp);
	KVirtualHostProcess *process = vpp->process;
	assert(process);
	bool success;
	KVirtualHost *vh = vpp->vh;
	if (vh==NULL && vpp->rq) {
		vh = vpp->rq->svh->vh;
	}
	KPoolableSocket *socket = process->poweron(vh,vpp->rd,success);

	if (socket) {
		//������һ�����ӣ�Ŀǰ���ȶ�.
		delete socket;
		socket = NULL;
	}
	std::list<KHttpRequest *> queue;
	process->lock.Lock();
	queue.swap(process->queue);
	if (success) {
		process->status = VProcess_Poweron;
	} else {
		process->status = VProcess_Poweroff;
	}
	process->lock.Unlock();
	handleVProcessPower(vpp,queue,success,socket,false);
	delete vpp;
	process->release();
	KTHREAD_RETURN;
}
void getProcessInfo(const USER_T &user,const std::string &name,KProcess *process,KPoolableSocketContainer *ps,std::stringstream &s)
{
	//time_t totalTime = time(NULL) - process->startTime;
	s << "<tr>";
	s << "<td>[<a href='/process_kill?name=" << name << "&user=" << user;
	s << "&pid=" << process->getProcessId() << "'>" << klang["kill"] << "</a>]</td>";
	s << "<td>" << user << "</td>";
	s << "<td >" << process->getProcessId() << "</td>";
/////////[236]
	s << "<td>" << (ps->getRef() - 1) << "</td>";
	s << "<td>" << ps->getSize() << "</td>";
	s << "<td>" << (kgl_current_sec - process->getPowerOnTime()) << "</td>";
	s << "</tr>\n";

}
void KVirtualHostProcess::handleRequest(KHttpRequest *rq,KExtendProgram *rd)
{
	bool isHalf;
	if (status == VProcess_Poweron) {
		KPoolableSocket *socket = connect(rq,rd,isHalf);
		static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq,socket,isHalf);
		return;
	}
	lock.Lock();
	switch (status) {
	case VProcess_Poweroff:
	case VProcess_Close:
		{
			rq->selector->removeRequest(rq);
			//gc->queue.push_back(rq);
			VProcessPowerParam *param = new VProcessPowerParam;
			addRef();
			param->vh = rq->svh->vh;
			param->rq = rq;
			param->process = this;
			param->rd = rd;
			status = VProcess_Inprogress;
			if(!m_thread.start(param,VProcessPowerWorker)){
				std::list<KHttpRequest *> tq;
				tq.swap(this->queue);
				lock.Unlock();
				handleVProcessPower(param,tq,false,NULL,false);
				delete param;
				this->release();
				return;
			}
			break;
		}
	case VProcess_Inprogress:
		{
			rq->selector->removeRequest(rq);
			queue.push_back(rq);
			break;
		}
	case VProcess_Poweron:
		{
			lock.Unlock();
			KPoolableSocket *socket = connect(rq,rd,isHalf);
			static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq,socket,isHalf);
			return;
		}
	}
	lock.Unlock();
}
