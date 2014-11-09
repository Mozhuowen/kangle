#ifndef KREQUESTLIST_H
#define KREQUESTLIST_H
class KHttpRequest;
/**
* ����˫���б���
*/
class RequestList {
public:
	RequestList();
	KHttpRequest *getHead();
	KHttpRequest *getEnd();
	void pushBack(KHttpRequest *rq);
	void pushFront(KHttpRequest *rq);
	KHttpRequest *popBack();
	KHttpRequest *popHead();
	void clear() {
		head = end = NULL;
	}
	KHttpRequest *remove(KHttpRequest *rq);
private:
	KHttpRequest *head;
	KHttpRequest *end;
};

#endif
