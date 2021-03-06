/*
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
#include "KHttpProtocolParser.h"
#include "malloc_debug.h"
#include "log.h"
#include <assert.h>
#include <stdio.h>
KHttpProtocolParser::~KHttpProtocolParser() {
	destroy();
}
bool KHttpProtocolParser::insertHeader(KHttpHeader *new_t,bool tail)
{
	if (headers == NULL) {
		headers = last = new_t;
		return true;
	}
	if (tail) {
		assert(last);
		last->next = new_t;
		last = new_t;
	} else {
		new_t->next = headers;
		headers = new_t;
	}
	return true;
}
bool KHttpProtocolParser::insertHeader(const char *attr,int attr_len,const char *val,int val_len,bool tail)
{
	KHttpHeader *new_t = (struct KHttpHeader *) xmalloc(sizeof(KHttpHeader));
	if (new_t == NULL) {
		return false;
	}
	new_t->attr = xstrdup(attr);
	new_t->attr_len = attr_len;
	new_t->val = xstrdup(val);
	new_t->val_len = val_len;
	new_t->next = NULL;
	return insertHeader(new_t,tail);

}
bool KHttpProtocolParser::parseHeader(char *header, char *end,bool isFirst,
		KHttpProtocolParserHook *hook) {
	char *val;
	if (isFirst && hook && hook->proto==Proto_http) {
		val = strchr(header, ' ');
	} else {
		val = strchr(header, ':');
	}
	if (val == NULL) {
		return true;
		/*
		if (isFirst) {	
			//对于fastcgi协议，有可能会发送第一行HTTP/1.1 xxx xxx过来。
			//为了兼容性，要忽略第一行错误
			return true;
		}
		return false;
		*/
	}
	int attr_len = val - header;
	*val = 0;
	val++;
	while (*val && IS_SPACE((unsigned char)*val))
		val++;
	int ret = 1;
	int val_len = end - val;
	if (hook) {
		ret = hook->parseHeader(header, val,val_len,isFirst);
	}
	switch(ret){
		case PARSE_HEADER_FAILED:
			return false;
		case PARSE_HEADER_NO_INSERT:
			return true;
		case PARSE_HEADER_INSERT_BEGIN:
			return insertHeader(header,attr_len,val,val_len,false);
		default:
			return insertHeader(header,attr_len,val,val_len);
	}
}
int KHttpProtocolParser::parse(char *buf, int len,
		KHttpProtocolParserHook *hook) {
	assert(len-checked>0);
	if (checked == 0 && hook) {
		hook->startParse();
	}
	restart: char *start = buf + checked;
	int leftLen = len - checked;
	assert(leftLen>=0);
	if (leftLen <= 0) {
		return HTTP_PARSE_CONTINUE;
	}
	char *pn = (char *) memchr(start, '\n', leftLen);
	if (pn == NULL) {
		return HTTP_PARSE_CONTINUE;
	}
	if (start[0] == '\n' || start[0] == '\r') {
		checked = pn + 1 - buf;
		if (!started) {
			goto restart;
		}
		orig_body = body = pn + 1;
		bodyLen = len - checked;
		assert(bodyLen>=0);
		if (hook) {
			hook->endParse();
		}
		//printf("body[0]=%d,bodyLen=%d\n",body[0],bodyLen);
		return HTTP_PARSE_SUCCESS;
	}
	if (started) {
		/*
		 * 我们还要看看这个http域有没有换行，据rfc2616.
		 *        LWS            = [CRLF] 1*( SP | HT )
		 *        我们还要看看下一行第一个字符是否是空行。
		 */
		if (pn - buf == len - 1) {
			/*
			 * 如果\n是最后的字符,则要continue.
			 */
			return HTTP_PARSE_CONTINUE;
		}
		/*
		 * 如果下一行开头字符是SP或HT，则要并行处理。把\r和\n都换成SP
		 */
		if (pn[1] == ' ' || pn[1] == '\t') {
			*pn = ' ';
			char *pr = (char *) memchr(start, '\r', pn - start);
			if (pr) {
				*pr = ' ';
			}
			goto restart;
		}
	}
	checked = pn + 1 - buf;
	char *pr = (char *) memchr(start, '\r', pn - start);
	char *end;
	if (pr) {
		*pr = '\0';
		end = pr;
	} else {
		*pn = '\0';
		end = pn;
	}
	if (!parseHeader(start, end,!started, hook)) {
		//klog(KLOG_DEBUG,
		//		"httpparse:cann't parse header,checked=%d,start=[%s]\n",
		//		checked, start);
		return HTTP_PARSE_FAILED;
	}
	started = true;
	goto restart;
	return HTTP_PARSE_CONTINUE;
}
/////////[274]
