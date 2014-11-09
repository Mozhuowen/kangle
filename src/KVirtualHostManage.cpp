/*
 * Kconf.gvm->cpp
 *
 *  Created on: 2010-4-19
 *      Author: keengo
 *
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

#include "do_config.h"
#include "KVirtualHostManage.h"
#include "KLineFile.h"
#include "malloc_debug.h"
#include "lib.h"
#include "lang.h"
#include "KHttpServerParser.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "KTempleteVirtualHost.h"
#include "directory.h"
#include "KHtAccess.h"
#include "KConfigBuilder.h"
#include "KDynamicListen.h"

using namespace std;

std::string convertInt(int type)
{
	std::stringstream s;
	s << type;
	return s.str();
}
KVirtualHostManage::KVirtualHostManage() {
	curInstanceId = 1;
	/////////[245]
}
KVirtualHostManage::~KVirtualHostManage() {
	std::map<std::string, KVirtualHost *>::iterator it4;
	for (it4 = avh.begin(); it4 != avh.end(); it4++) {
		(*it4).second->destroy();
	}
	std::map<std::string, KGTempleteVirtualHost *>::iterator it;
	for (it=gtvhs.begin();it!=gtvhs.end();it++) {
		delete (*it).second;
	}
	gtvhs.clear();
	avh.clear();
}
void KVirtualHostManage::copy(KVirtualHostManage *vm)
{
	lock.Lock();
	globalVh.swap(&vm->globalVh);
	avh.swap(vm->avh);
	gtvhs.swap(vm->gtvhs);
	std::map<KListenKey,KServer *>::iterator it;
	for (it=dlisten.listens.begin();it!=dlisten.listens.end();it++) {
		(*it).second->unbindAllVirtualHost();
	}
	KServer::defaultVhc.clear();
	internalBindAllVirtualHost();
	lock.Unlock();
}
#if 0
void KVirtualHostManage::load(bool firstLoad) {
#ifndef HTTP_PROXY
	KHttpServerParser parser;
	string configFile = conf.path;
	configFile += VH_CONFIG_FILE;
	cur_config_vh_db = false;
	parser.parse(configFile);
	configFile = conf.path + "etc/vh.d/";
	cur_config_vh_db = true;
	list_dir(configFile.c_str(),virtualhost_config_handle,(void *)&parser);	
	/////////[246]
	if (vhd.isLoad()) {
		string errMsg;
		if (!vhd.loadVirtualHost(errMsg)) {
			klog(KLOG_ERR, "Cann't load VirtualHost[%s]\n", errMsg.c_str());
			//db server failed.then skip delete exsit vh
			//lock.Unlock();
			cur_config_vh_db = false;
			return;
		}
	}
	cur_config_vh_db = false;
#endif
}
#endif
void KVirtualHostManage::getAllGroupTemplete(std::list<std::string> &vhs)
{
	lock.Lock();
	std::map<std::string, KGTempleteVirtualHost *>::iterator it;
	for (it = gtvhs.begin(); it != gtvhs.end(); it++) {
		vhs.push_back((*it).first);
	}
	lock.Unlock();
}
bool KVirtualHostManage::getAllTempleteVh(const char *groupTemplete,std::list<std::string> &vhs) {
	if(groupTemplete==NULL){
		return false;
	}
	bool result = false;
	lock.Lock();
	std::map<std::string, KGTempleteVirtualHost *>::iterator it;
	it = gtvhs.find(groupTemplete);
	if(it!=gtvhs.end()){
		result = true;
		(*it).second->getAllTemplete(vhs);
	}
	lock.Unlock();
	return result;
}
void KVirtualHostManage::getAllVh(std::list<std::string> &vhs,bool status,bool onlydb) {
	std::stringstream s;
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it = avh.begin(); it != avh.end(); it++) {
		if(!onlydb || (*it).second->db){
			if(status){
				s.str("");
				s << (*it).first << " " << (*it).second->status;
				vhs.push_back(s.str());
			}else{
				vhs.push_back((*it).first);
			}
		}
	}
	lock.Unlock();
}
void KVirtualHostManage::getAutoName(std::string &name) {
	std::stringstream s;
	instancelock.Lock();
	s << program_start_time << "_" << curInstanceId++;
	instancelock.Unlock();
	s.str().swap(name);
}
int KVirtualHostManage::getNextInstanceId() {
	instancelock.Lock();
	int nextId = curInstanceId++;
	instancelock.Unlock();
	return nextId;
}
bool KVirtualHostManage::removeTempleteVirtualHost(std::string name)
{
	bool result = false;
	lock.Lock();
	int index = name.find_first_of(':');
	string subname;
	if(index>=0){
		subname = name.substr(index+1);
		name = name.substr(0,index);
	}
	map<std::string, KGTempleteVirtualHost *>::iterator it;
	it = gtvhs.find(name);
	if (it != gtvhs.end()) {
		KGTempleteVirtualHost *gtvh = (*it).second;
		if (gtvh) {
			result = gtvh->del(subname.c_str());
		}
		if (gtvh->isEmpty()) {
			gtvhs.erase(it);
			delete gtvh;
		}
	}
	lock.Unlock();
	return result;
}
KTempleteVirtualHost *KVirtualHostManage::refsTempleteVirtualHost(std::string name) {
	KTempleteVirtualHost *tvh = NULL;
	lock.Lock();
	int index = name.find_first_of(':');
	string subname;
	if(index>=0){
		subname = name.substr(index+1);
		name = name.substr(0,index);
	}
	map<std::string, KGTempleteVirtualHost *>::iterator it;
	it = gtvhs.find(name);
	if (it != gtvhs.end()) {
		KGTempleteVirtualHost *gtvh = (*it).second;
		if(gtvh){
			tvh = gtvh->findTemplete(subname.c_str(),false);
		}
	}
	lock.Unlock();
	return tvh;
}
bool KVirtualHostManage::updateTempleteVirtualHost(KTempleteVirtualHost *tvh) {
	KTempleteVirtualHost *ov = NULL;
	lock.Lock();
	string name = tvh->name;
	int index = name.find_first_of(':');
	string subname;
	if(index>=0){
		subname = name.substr(index+1);
		name = name.substr(0,index);
	}
	map<std::string, KGTempleteVirtualHost *>::iterator it;
	KGTempleteVirtualHost *gtvh = NULL;
	it = gtvhs.find(name);
	if (it != gtvhs.end()) {
		gtvh = (*it).second;
		if (gtvh) {
			ov = gtvh->findTemplete(subname.c_str(),true);
		}
	}
	if(gtvh == NULL){
		gtvh = new KGTempleteVirtualHost;
		gtvhs.insert(pair<std::string,KGTempleteVirtualHost *>(name,gtvh));
	}
	gtvh->add(subname.c_str(),tvh);
	lock.Unlock();
	if (ov) {
		ov->destroy();
	}
	return true;
}

KVirtualHost *KVirtualHostManage::refsVirtualHostByName(std::string name) {
	KVirtualHost *vh = NULL;
	lock.Lock();
	map<std::string, KVirtualHost *>::iterator it = avh.find(name);
	if (it != avh.end()) {
		vh = (*it).second;
		vh->addRef();
	}
	lock.Unlock();
	return vh;
}
void KVirtualHostManage::getMenuHtml(std::stringstream &s,KVirtualHost *v,std::stringstream &url,int t)
{
	KBaseVirtualHost *vh = &globalVh;
	if(v){
		vh = v;
		url << "name=" << v->name << "&";
	}
	s << "<html><head>"
			<< "<LINK href=/kangle.css type='text/css' rel=stylesheet></head>";
	s << "<body><table border=0><tr><td>";
	s << "[<a href='/vhlist'>" << klang["LANG_VHS"] << "</a>]";
	if (t) {
		s << " ==> [<a href='/vhlist?t=1&id=0'>" << klang["all_tvh"] << "</a>]";
		if (v) {
			url << "t=" << t << "&";
		}
	}
	if (v) {
		s << " ==> " << v->name;	
	}
	s << "</td></tr></table><br>";

	s << "<table width='100%'><tr><td align=left>";
	if (v) {
		s << "[<a href='/vhlist?" << url.str() << "&id=0'>" << klang["detail"] << "</a>] ";
	} else {
		s << "[<a href='/vhlist?id=0'>" << klang["all_vh"] << "</a>] ";// [<a href='/vhlist?t=1&id=0'>" << klang["all_tvh"];
	}	
	s << "[<a href='/vhlist?" << url.str() << "id=1'>" << klang["index"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=2'>" << klang["map_extend"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=3'>" << klang["error_page"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=5'>" << klang["alias"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=8'>" << klang["mime_type"]
			<< "</a>] ";
#ifndef HTTP_PROXY
	if(t==0 && v && v->user_access.size()>0){
		s << "[<a href='/vhlist?" << url.str() << "id=6'>" << klang["lang_requestAccess"] << "</a>]";
		s << "[<a href='/vhlist?" << url.str() << "id=7'>" << klang["lang_responseAccess"] << "</a>]";
	}
#endif
	s << "</td><td align=right>";
	//s << "[<a href=\"javascript:if(confirm('really reload')){ window.location='/reload_vh';}\">" << klang["reload_vh"] << "</a>]";
	s << "</td></tr></table>";
	s << "<hr>";
}
void KVirtualHostManage::getHtml(std::stringstream &s,std::string name, int id,KUrlValue &attribute) {
	int t = atoi(attribute.get("t").c_str());
	stringstream url;
	KBaseVirtualHost *vh = &globalVh;
	KVirtualHost *v = NULL;


	//url << "name=" << name;
	if (name.size() > 0) {
		if (t) {
			v = refsTempleteVirtualHost(name);
		} else {
			v = refsVirtualHostByName(name);
		}
		if (v) {
			vh = v;			
		} else {
			name = "";
		}
	}
	getMenuHtml(s,v,url,t);
	url << "id=" << id;
	if (id==0 && name.size()==0) {
		lock.Lock();
		getAllVhHtml(s,t);
		lock.Unlock();
	} else {
		vh->lock.Lock();
		if (id == 0) {
			getVhDetail(s, (KVirtualHost *) vh,true,t);
		} else if (id == 1) {
			vh->getIndexHtml(url.str(), s);
		} else if (id == 2) {
			vh->getRedirectHtml(url.str(), s);
		} else if (id == 3) {
			vh->getErrorPageHtml(url.str(), s);
		} else if (id == 4) {
			if (v) {
				v->destroy();
			}
			v = refsTempleteVirtualHost(attribute.get("templete").c_str());
			getVhDetail(s, v,false,t);
		} else if (id == 5) {
			vh->getAliasHtml(url.str(), s);
		} else if (id==6) {
			if(v){
				s << v->access[0].htmlAccess(name.c_str());
			}
		} else if (id==7) {
			if(v){
				s << v->access[1].htmlAccess(name.c_str());
			}
		} else if (id==8) {
			vh->getMimeTypeHtml(url.str(),s);
		}
		vh->lock.Unlock();
	}
	if (v) {
		v->destroy();
	}
	s << endTag();
	s << "</body></html>";
}
bool KVirtualHostManage::vhBaseAction(KUrlValue &attribute, std::string &errMsg) {
	string action = attribute["action"];
	string name = attribute["name"];
	int t = atoi(attribute["t"].c_str());
	//string host = attribute["host"];
	KBaseVirtualHost *bvh = &globalVh;
	KVirtualHost *v = NULL;
	bool result = false;
	bool reinherit = true;
	std::map<std::string,std::string> vhdata;
	bool skip_warning = false;
	if (name.size() > 0) {
		if (t) {
			v = refsTempleteVirtualHost(name);
		} else {
			v = refsVirtualHostByName(name);
		}
		bvh = v;
	}
	if (v && v->db) {
		vhdata["vhost"] = name;
	}
	if (action == "vh_add") {
		if (v) {
			errMsg = "name: " ;
			errMsg += name;
			errMsg += " is used!";
		} else {
			KTempleteVirtualHost *tvh = refsTempleteVirtualHost(attribute["templete"]);			
			result = vhAction(NULL, tvh, attribute, errMsg);
			if (tvh) {
				tvh->destroy();
			}
		}
		reinherit = false;
	} else {
		if (name.size() > 0 && v == NULL) {
			errMsg = "cann't find vh";
			return false;
		}
		if (action == "vh_delete") {
			if (v) {
				reinherit = false;
				if (t) {
					result = removeTempleteVirtualHost(name);
				} else {
					removeVirtualHost(v);
					result = true;
				}
				if (v->db || v->ext) {
					errMsg = "Warning! The virtualhost is managed by external file(extend file or database),it will not save to vh.xml file.";
					result = false;
				}
			}	
		} else if (action == "vh_edit") {
			reinherit = false;
			KTempleteVirtualHost *tvh = refsTempleteVirtualHost(attribute["templete"]);
			result = vhAction(v, tvh, attribute, errMsg);
			if (tvh) {
				tvh->destroy();
			}
		} else if (action == "indexadd") {
			attribute["id"] = "1";
			result = bvh->addIndexFile(attribute["index"],atoi(attribute["index_id"].c_str()));
			if (result && v && v->db) {
				vhdata["name"] = attribute["index"];
				vhdata["value"] = attribute["index_id"];
				vhdata["type"] = convertInt(VH_INFO_INDEX);
				skip_warning = vhd.addInfo(vhdata,errMsg,true);
			}
		} else if (action == "indexdelete") {
			attribute["id"] = "1";
			result = bvh->delIndexFile(attribute["index"]);
			if (result && v && v->db) {
				vhdata["name"] = attribute["index"];
				vhdata["type"] = convertInt(VH_INFO_INDEX);
				skip_warning = vhd.delInfo(vhdata,errMsg,true);
			}
		} else if (action == "redirectadd") {
			attribute["id"] = "2";
			bool file_ext = false;
			if (attribute["type"] == "file_ext") {
				file_ext = true;
			}
			bool confirmFile = false;
			if(attribute["confirm_file"] == "1"){
				confirmFile = true;
			}
			result = bvh->addRedirect(file_ext, attribute["value"],
					attribute["extend"], attribute["allow_method"],
					confirmFile,attribute["params"]);
			if (result && v && v->db) {
				std::stringstream value;
				value << (file_ext?1:0) << "," << attribute["value"];
				vhdata["name"] = value.str();
				value.str("");
				value << (confirmFile?1:0) << "," << attribute["extend"] << "," << attribute["allow_method"];
				vhdata["value"] = value.str();
				vhdata["type"] = convertInt(VH_INFO_MAP);
				skip_warning = vhd.addInfo(vhdata,errMsg,true);
			}
		} else if (action == "redirectdelete") {
			attribute["id"] = "2";
			bool file_ext = false;
			if (attribute["type"] == "file_ext") {
				file_ext = true;
			}
			result = bvh->delRedirect(file_ext, attribute["value"]);
			if (result && v && v->db) {
				std::stringstream value;
				value << (file_ext?1:0) << "," << attribute["value"];
				vhdata["name"] = value.str();
				vhdata["type"] = convertInt(VH_INFO_MAP);
				skip_warning = vhd.delInfo(vhdata,errMsg,true);
			}
		} else if (action == "errorpageadd") {
			attribute["id"] = "3";
			result = bvh->addErrorPage(atoi(attribute["code"].c_str()),
					attribute["url"]);
			if (result && v && v->db) {
				vhdata["name"] = attribute["code"];
				vhdata["value"] = attribute["url"];
				vhdata["type"] = convertInt(VH_INFO_ERROR_PAGE);
				skip_warning = vhd.addInfo(vhdata,errMsg,true);
			}
		} else if (action == "errorpagedelete") {
			attribute["id"] = "3";
			result = bvh->delErrorPage(atoi(attribute["code"].c_str()));
			if (result && v && v->db) {
				vhdata["name"] = attribute["code"];
				vhdata["type"] = convertInt(VH_INFO_ERROR_PAGE);
				skip_warning = vhd.delInfo(vhdata,errMsg,true);
			}
		} else if (action == "aliasadd") {
			attribute["id"] = "5";
			bool internal = false;
			if(attribute["internal"]=="1" || attribute["internal"]=="on"){
				internal = true;
			}
			result = bvh->addAlias(attribute["path"],
				attribute["to"], 
				(v?v->doc_root.c_str():conf.path.c_str()),
				internal,
				atoi(attribute["index"].c_str()), 
				errMsg);
			if (result && v && v->db) {
				vhdata["name"] = attribute["path"];
				std::stringstream value;
				value << attribute["to"] << "," << (internal?1:0) << "," << atoi(attribute["index"].c_str());
				vhdata["value"] = value.str();
				vhdata["type"] = convertInt(VH_INFO_ALIAS);
				skip_warning = vhd.addInfo(vhdata,errMsg,true);
			}
		} else if (action == "aliasdelete") {
			attribute["id"] = "5";
			result = bvh->delAlias(attribute["path"].c_str());
			if (result && v && v->db) {
				vhdata["name"] = attribute["path"];
				vhdata["type"] = convertInt(VH_INFO_ALIAS);
				skip_warning = vhd.delInfo(vhdata,errMsg,true);
			}
		} else if (action == "mimetypeadd") {
			attribute["id"] = "8";
			bool gzip = attribute["gzip"]=="1";
			int max_age = atoi(attribute["max_age"].c_str());
			bvh->addMimeType(attribute["ext"].c_str(),attribute["type"].c_str(),gzip,max_age);
			result = true;
			reinherit = false;
			if (result && v && v->db) {
				vhdata["name"] = attribute["ext"];
				std::stringstream value;
				value << attribute["type"] << "," << (gzip?1:0) << "," << max_age;
				vhdata["value"] = value.str();
				vhdata["type"] = convertInt(VH_INFO_MIME);
				skip_warning = vhd.addInfo(vhdata,errMsg,true);
			}
		} else if (action == "mimetypedelete") {
			attribute["id"] = "8";
			result = bvh->delMimeType(attribute["ext"].c_str());
			reinherit = false;
			if (result && v && v->db) {
				vhdata["name"] = attribute["ext"];
				vhdata["type"] = convertInt(VH_INFO_MIME);
				skip_warning = vhd.delInfo(vhdata,errMsg,true);
			}
		} else {
			errMsg = "action [" + action + "] is error";
		}
	}
	if (t) {
		reinherit = false;
	}
	if (v) {
		if (v->db || v->ext) {
			if (!skip_warning) {
				errMsg = "Warning! The virtualhost is managed by external file(extend file or database),it will not save to vh.xml file.";
				result = false;
			}
		}
		if (reinherit) {
			inheritVirtualHost(v, true);
		}
		v->destroy();
	} else if (reinherit) {
		inheriteAll();
	}
	return result;
}
void KVirtualHostManage::inheritVirtualHost(KVirtualHost *vh, bool clearFlag) {
	if (vh->isTemplete()) {
		return;
	}
	globalVh.lock.Lock();
	globalVh.inheriTo(vh, clearFlag);
	globalVh.lock.Unlock();
}
void KVirtualHostManage::inheriteAll() {
	std::map<string, KVirtualHost *>::iterator it;
	lock.Lock();
	globalVh.lock.Lock();
	for (it = avh.begin(); it != avh.end(); it++) {
		globalVh.inheriTo((*it).second, true);
	}
	globalVh.lock.Unlock();
	lock.Unlock();
}
bool KVirtualHostManage::vhAction(KVirtualHost *ov,KTempleteVirtualHost *tm,
		KUrlValue &attribute, std::string &errMsg) {
	attribute["from_web_console"] = 1;
	KAttributeHelper ah(attribute.get());
	bool isTemplate = atoi(attribute["t"].c_str())>0;
	KTempleteVirtualHost *tvh = NULL;
	KVirtualHost *vh ;
	if (isTemplate) {
		tvh = new KTempleteVirtualHost;
		tvh->initEvents = attribute["init_event"];
		tvh->destroyEvents = attribute["destroy_event"];
		tvh->updateEvents = attribute["update_event"];
		vh = tvh;
	} else {
		vh = new KVirtualHost;
	}
#ifndef HTTP_PROXY
	if(!KHttpServerParser::buildVirtualHost(&ah,vh,&conf.gvm->globalVh,tm,ov)){
		//todo:error
	}
#endif
	if (vh->name.size()==0) {
		errMsg = "name cann't be empty";
		delete vh;
		return false;
	}
	KLineFile lf;
	lf.init(attribute["host"].c_str());
	for (;;) {
		bool addFlag = true;
		char *line = lf.readLine();
		if (line == NULL) {
			break;
		}
		char *dir = strchr(line, '|');
		if (dir) {
			*dir = '\0';
			dir++;
		}	
		if (*line=='*' && strcmp(line,"*")!=0) {
			line++;
		}
		std::list<KSubVirtualHost *>::iterator it;
		u_short port = 0;
		char *p = strchr(line,':');
		if (p) {
			port = atoi(p+1);
			*p = '\0';
		}
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
			if (strcasecmp((*it)->host, line) == 0) {
				delete (*it);
				vh->hosts.erase(it);
				break;
			}
		}
		if (addFlag) {
			KSubVirtualHost *svh = new KSubVirtualHost(vh);
			svh->host = xstrdup(line);
			svh->setDocRoot(vh->doc_root.c_str(), dir);
			vh->hosts.push_back(svh);
		}
		//hosts.push_back(svh);
	}
#ifdef ENABLE_BASED_PORT_VH
	lf.init(attribute["bind"].c_str());
	for (;;) {
		bool addFlag = true;
		char *line = lf.readLine();
		if (line == NULL) {
			break;
		}
		if (*line=='@' || *line=='#' || *line=='!') {
			vh->binds.push_back(line);
			continue;
		}
		u_short port = atoi(line);
		std::list<u_short>::iterator it;
		for (it = vh->ports.begin(); it != vh->ports.end(); it++) {
			if (port == (*it)) {
				vh->ports.erase(it);
				break;
			}
		}
		if (addFlag) {
			vh->ports.push_back(atoi(line));
		}
	}
#endif
	if (ov) {
		vh->db = ov->db;
		vh->ext = ov->ext;
	} else {
		vh->db = false;
		vh->ext = false;
	}
	bool result;
	if (tvh) {
		result = updateTempleteVirtualHost(tvh);
	} else {
#if 0
        if (ov) {                
#ifdef ENABLE_VH_RUN_AS
            if (vh->caculateNeedKillProcess(ov)) {
                   conf.gam->killAllProcess(ov);
            }
#endif
		}
#endif
		lock.Lock();
		if (ov) {
            internalRemoveVirtualHost(ov);
        }
        result = internalAddVirtualHost(vh,ov);
		if (ov) {
			flushListen(ov);
		}
		lock.Unlock();
	}
	//if (ov && (ov->db || ov->ext)) {
	//	errMsg = "Warning! The virtualhost is managed by external file(extend file or database),it will not save to vh.xml file.";
	//	return false;
	//}
	return result;
}
bool KVirtualHostManage::saveConfig(std::string &errMsg) {
	return KConfigBuilder::saveConfig();
	/*
	build(s);
	s << "\r\n" << CONFIG_FILE_SIGN;
	KFile fp ;
	if (!fp.open(tmpfile.c_str(),fileWrite)) {
		fprintf(stderr, "cann't open configfile[%s] for write\n", tmpfile.c_str());
		errMsg = "cann't open vh.xml for write";
		return false;
	}
	if (conf.worker>1) {
		need_reboot_flag = true;
	}
	bool result = false;
	if (s.str().size() == fp.write(s.str().c_str(), s.str().size())) {
		result = true;
	}
	fp.close();
	if (!result) {
		errMsg = "cann't write sign string\n";
		fprintf(stderr,"cann't write sign string\n");
		return false;
	}
	unlink(lstfile.c_str());
	rename(file.c_str(),lstfile.c_str());
	rename(tmpfile.c_str(),file.c_str());
	if (conf.mergeFiles.size()>0) {
                //remove the merge config files.
                std::list<std::string>::iterator it;
                for(it=conf.mergeFiles.begin();it!=conf.mergeFiles.end();it++){
                        unlink((*it).c_str());
                }
                conf.mergeFiles.clear();
		KConfigBuilder::saveConfig();
        }
	return true;
	*/
}
void KVirtualHostManage::build(stringstream &s) {
	//s << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	lock.Lock();
	s << "<vhs ";
	globalVh.buildBaseXML(s);
	s << "</vhs>\n";
	std::map<std::string, KGTempleteVirtualHost *>::iterator it2;
	for (it2=gtvhs.begin();it2!=gtvhs.end();it2++) {
		std::map<string, KTempleteVirtualHost *>::iterator it3;
		for(it3=(*it2).second->tvhs.begin();it3!=(*it2).second->tvhs.end();it3++){
			if ((*it3).second->ext || (*it3).second->db) {
				continue;
			}
			s << "<vh_templete ";
			(*it3).second->buildXML(s);
			s << "</vh_templete>\n";
		}
	}
	std::map<string, KVirtualHost *>::iterator it;
	s << "\t<!--vh start-->\n";
	for (it = avh.begin(); it != avh.end(); it++) {
		if ((*it).second->ext || (*it).second->db) {
			continue;
		}
		s << "<vh ";
		(*it).second->buildXML(s);
		s << "</vh>\n";
	}
	s << "\t<!--vh end-->\n";
	lock.Unlock();
}
bool KVirtualHostManage::updateVirtualHost(KVirtualHost *vh,KVirtualHost *ov)
{
#ifdef ENABLE_VH_RUN_AS
	bool needKillProcess = false;
#endif
	lock.Lock();
	if (ov && this==conf.gvm) {
#ifdef ENABLE_VH_RUN_AS
		needKillProcess = vh->caculateNeedKillProcess(ov);
#endif
		internalRemoveVirtualHost(ov);
	}
	if (vh->name.size() == 0) {
		getAutoName(vh->name);
	}
	bool result = internalAddVirtualHost(vh,ov);
#ifdef ENABLE_VH_RUN_AS
	if (needKillProcess) {
		for(size_t i=0;i<ov->apps.size();i++){
			conf.gam->killCmdProcess(ov->apps[i]);
		}
		//conf.gam->killCmdProcess(ov->getUser());
	}
#endif
	if (ov && this==conf.gvm) {
		flushListen(ov);
	}
	lock.Unlock();
	return result;
}
bool KVirtualHostManage::updateVirtualHost(KVirtualHost *vh) {

	KVirtualHost *ov = refsVirtualHostByName(vh->name);
	bool result = updateVirtualHost(vh,ov);
	if (ov) {
		ov->destroy();
	}
	return result;
}
/*
 * ������������
 */
bool KVirtualHostManage::addVirtualHost(KVirtualHost *vh) {
	if (vh->name.size() == 0) {
		getAutoName(vh->name);
	}
	lock.Lock();
	bool result = internalAddVirtualHost(vh,NULL);
	lock.Unlock();
	return result;
}
/*
 * ɾ����������
 */
bool KVirtualHostManage::removeVirtualHost(KVirtualHost *vh) {
	lock.Lock();
	bool result = internalRemoveVirtualHost(vh);
#ifdef ENABLE_VH_RUN_AS
	for(size_t i=0;i<vh->apps.size();i++){
		conf.gam->killCmdProcess(vh->apps[i]);
	}
#endif
	flushListen(vh);
	lock.Unlock();
	return result;
}
bool KVirtualHostManage::internalAddVirtualHost(KVirtualHost *vh,KVirtualHost *ov) {
#ifdef ENABLE_USER_ACCESS
	vh->loadAccess(ov);
#endif
	std::map<std::string, KVirtualHost *>::iterator it;
	it = avh.find(vh->name);
	if (it != avh.end()) {
		klog(KLOG_ERR,"Cann't add VirtualHost [%s] name duplicate.\n",vh->name.c_str());
		return false;
	}
	if (this==conf.gvm) {
		internalBindVirtualHost(vh);
	}
	avh.insert(pair<string, KVirtualHost *> (vh->name, vh));
	vh->addRef();
	return true;
}
void KVirtualHostManage::bindVirtualHost(KServer *server)
{
	std::map<std::string, KVirtualHost *>::iterator it;
	for(it=avh.begin();it!=avh.end();it++){
		server->addVirtualHost((*it).second);
	}
}
void KVirtualHostManage::addAllVirtualHost()
{
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for(it=avh.begin();it!=avh.end();it++){
		dlisten.addStaticVirtualHost((*it).second);
	}
	dlisten.delayStart();
	lock.Unlock();
}
bool KVirtualHostManage::internalRemoveVirtualHost(KVirtualHost *vh,
		bool removeIndex) {
	if (removeIndex) {
		std::map<std::string, KVirtualHost *>::iterator it;
		it = avh.find(vh->name);
		if (it == avh.end()) {
			lock.Unlock();
			return false;
		}
		avh.erase(it);
	}
	dlisten.removeStaticVirtualHost(vh);
	KServer::removeDefaultVirtualHost(vh);
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string>::iterator it2;
	for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
		const char *bind = (*it2).c_str();
		if (*bind=='!') {
			dlisten.remove(bind+1,vh);
		}
	}
#endif
	vh->destroy();
	return true;
}
/*
 * ������������������rq�ϡ�
 */
query_vh_result KVirtualHostManage::queryVirtualHost(KHttpRequest *rq,const char *site) {
	SET(rq->flags,RQ_VH_QUERIED);
	query_vh_result result = query_vh_host_not_found;
	lock.Lock();
	if (rq->ls->vhc) {
		result = rq->ls->vhc->parseVirtualHost(rq,site);
	}
	if (result == query_vh_host_not_found) {
		result = KServer::parseVirtualHost(rq,site);
	}
	lock.Unlock();
	return result;
}

void KVirtualHostManage::getVhDetail(std::stringstream &s, KVirtualHost *vh,bool edit,int t) {
	//	string host;
	string action = "vh_add";
	if (edit) {
		action = "vh_edit";
	}
	string name;
	if (vh) {
		name = vh->name;
	}
	s << "<form name='frm' action='/vhbase?action=" << action << "&t=" << t;
	if (vh) {
		if(!edit) {
			//vh is a templete
			s << "&templete=" << vh->name;
		} else if(vh->tvh){
			s << "&templete=" << vh->tvh->name;
		}
	}
	s << "' method='post'>";
#ifdef ENABLE_VH_RUN_AS
	s << "<input name='add_dir' type='hidden' value='" << (vh ? vh->add_dir
			: "") << "'>";
#endif
	s << "<table border=1>";
	s << "<tr><td>" << LANG_NAME << "</td>";
	s << "<td>";
	s << "<input name='name' value='" ;
	if (edit) {
		if (vh) {
			s << vh->name.c_str();
		}
		s << "' readonly";
	} else {
		s << "'";
	}
	s << ">";	
	s << klang["status"] << ":<input name='status' size=3 value='" << (vh?vh->status:0) << "'>";
	s << "</td></tr>";
	s << "<tr><td>" << klang["doc_root"] << "</td><td><input name='doc_root' size=30 value='"
			<< (vh ? vh->orig_doc_root : "") << "'>";
#ifndef _WIN32
#ifdef ENABLE_VH_RUN_AS
	//s << "<input name='chroot' type='checkbox' value='1'"
	//		<< ((vh && vh->chroot) ? "checked" : "") << ">" << klang["chroot"];
#endif
#endif
#ifdef ENABLE_BASED_PORT_VH
	s << "<tr><td>" << klang["bind"] << "</td>";
	s << "<td><textarea name='bind' rows='3' cols='25'>";
	if (vh && edit) {
		list<u_short>::iterator it;
		for (it = vh->ports.begin(); it != vh->ports.end(); it++) {
			if ((*it) == 0) {
				s << "*\n";
			} else {
				s << (*it) << "\n";
			}
		}
		list<string>::iterator it2;
		for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
			s << (*it2) << "\n";
		}
	} else {
		s << "*";
	}
	s << "</textarea></td></tr>\n";
#endif
	s << "<tr><td>" << klang["vh_host"] << "</td>";
	s << "<td><textarea name='host' rows='4' cols='25'>";
	if (vh && edit) {
		list<KSubVirtualHost *>::iterator it;
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
			if ((*it)->fromTemplete) {
				continue;
			}
			if (*(*it)->host=='.') {
				s << "*";
			}
			s << (*it)->host;
			if (strcmp((*it)->dir, "/") != 0) {
				s << "|" << (*it)->dir;
			}
			s << "\n";
		}
	}
	s << "</textarea></td></tr>\n";

	s << "</td></tr>\n";
	s << "<tr><td>" << klang["inherit"]
			<< "</td><td><input name='inherit' type='radio' value='1' ";
	if (vh == NULL || vh->inherit) {
		s << "checked";
	}
	s << ">" << klang["inherit"]
			<< "<input name='inherit' type='radio' value='0' ";
	if (vh && !vh->inherit) {
		s << "checked";
	}
	s << ">" << klang["no_inherit"]
			<< "<input name='inherit' type='radio' value='2'>";
	s << klang["no_inherit2"] << "</td></tr>\n";
#ifdef ENABLE_VH_RUN_AS
	s << "<tr><td>" << LANG_RUN_USER << "</td><td>" << LANG_USER
			<< ":<input name='user' value='";
	s << (vh ? vh->user : "") << "' autocomplete='off' size=10> ";
#ifdef _WIN32
	s << LANG_PASS;
#else
	s << klang["group"];
#endif
	s << ":<input name='group' "
#ifdef _WIN32
			<< "type='password' "
#endif
			<< "value='"
#ifndef _WIN32
			<< (vh ? vh->group : "")
#endif
			<< "' autocomplete='off' size=10><td></tr>\n";
#endif
#ifdef ENABLE_VH_LOG_FILE
	s << "<tr><td>" << klang["log_file"]
			<< "</td><td><input name='log_file' value='" << (vh ? vh->logFile
			: "") << "'></td></tr>\n";
	s << "<tr><td>" << klang["log_mkdir"]
			<< "</td><td><input name='log_mkdir' type='radio' value='on' ";
	bool mkdirFlag = false;
	if (vh && vh->logger && vh->logger->mkdirFlag) {
		mkdirFlag = true;
	}
	if (mkdirFlag) {
		s << "checked";
	}
	s << ">" << LANG_ON << "<input name='log_mkdir' type='radio' value='off' ";
	if (!mkdirFlag) {
		s << "checked";
	}
	s << ">" << LANG_OFF << "</td></tr>\n";

	//log_handle
	s << "<tr><td>" << klang["log_handle"]
			<< "</td><td><input name='log_handle' type='radio' value='on' ";
	bool log_handle = true;
	if (vh && vh->logger && !vh->logger->log_handle) {
		log_handle = false;
	}
	if (log_handle) {
		s << "checked";
	}
	s << ">" << LANG_ON << "<input name='log_handle' type='radio' value='off' ";
	if (!log_handle) {
		s << "checked";
	}
	s << ">" << LANG_OFF << "</td></tr>\n";


	string rotateTime;
	if (vh && vh->logger) {
		vh->logger->getRotateTime(rotateTime);
	}
	s << "<tr><td>" << LANG_LOG_ROTATE_TIME
			<< "</td><td><input name='log_rotate_time' value='" << rotateTime
			<< "'></td></tr>\n";
	s << "<td>" << klang["log_rotate_size"]
			<< "</td><td><input name='log_rotate_size' value='" << (vh
			&& vh->logger ? vh->logger->rotateSize : 0) << "'></td></tr>\n";
	s << "<td>" << klang["logs_day"]
			<< "</td><td><input name='logs_day' value='" << (vh
			&& vh->logger ? vh->logger->logs_day : 0) << "'></td></tr>\n";
	s << "<td>" << klang["logs_size"]
			<< "</td><td><input name='logs_size' value='" << (vh
			&& vh->logger ? vh->logger->logs_size : 0) << "'></td></tr>\n";
#endif
	s << "<tr><td>" << klang["option"]
			<< "</td><td>";
	s << "<input name='browse' type='checkbox' value='on'"
			<< ((vh && vh->browse) ? "checked" : "") << ">" << klang["browse"];
	s << "<input name='concat' type='checkbox' value='1'"
			<< ((vh && vh->concat) ? "checked" : "") << ">" << klang["concat"];

#ifdef ENABLE_VH_FLOW
	s << "<input name='fflow' type='checkbox' value='1'"
			<< ((vh && vh->fflow) ? "checked" : "") << ">" << klang["flow"];
#endif
	s << "</td></tr>\n";
#ifdef ENABLE_USER_ACCESS
	s << "<tr><td>" << klang["access_file"]
			<< "</td><td><input name='access' value='" << (vh ? vh->user_access
			: "") << "'></td></tr>\n";
#endif
	s << "<tr><td>" << klang["htaccess"]
			<< "</td><td><input name='htaccess' value='" << (vh ? vh->htaccess
			: "") << "'></td></tr>\n";
	s <<  "<tr><td>" << klang["app_count"] << "</td><td>";
	s << "<input name='app' value='" << (vh ? vh->app : 1) << "' size='4'>";
	s << "<input type='checkbox' name='ip_hash' value='1' " ;
	if(vh && vh->ip_hash){
		s << "checked";
	}
	s << ">" << klang["ip_hash"] << "</td></tr>";
	s << "<tr><td>" << klang["app_share"] << "</td><td>";
	s << "<input type='radio' name='app_share' value='0' ";
	if(vh && vh->app_share==0){
		s << "checked";
	}
	s << "/>" << klang["app_share0"];
	s << "<input type='radio' name='app_share' value='1' ";
	if(vh==NULL || vh->app_share==1){
		s << "checked";
	}
	s << "/>" << klang["app_share1"];
	s << "<input type='radio' name='app_share' value='2' ";
	if(vh && vh->app_share==2){
		s << "checked";
	}
	s << "/>" << klang["app_share2"];
	s << "</td></tr>\n";
#ifdef ENABLE_VH_RS_LIMIT
	s << "<tr><td>" << klang["connect"]
			<< "</td><td><input name='max_connect' value='";
	s << (vh ? vh->max_connect : 0) << "'></td></tr>\n";
	s << "<tr><td>" << LANG_LIMIT_SPEED
			<< "</td><td><input name='speed_limit' value='";
	s << (vh ? vh->speed_limit : 0) << "'></td></tr>\n";
#endif
#ifdef ENABLE_VH_QUEUE
	s << "<tr><td>" << klang["max_worker"]
			<< "</td><td><input name='max_worker' value='";
	s << (vh ? vh->max_worker : 0) << "'></td></tr>\n";

	s << "<tr><td>" << klang["max_queue"]
			<< "</td><td><input name='max_queue' value='";
	s << (vh ? vh->max_queue : 0) << "'></td></tr>\n";
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	s << "<tr><td>" << klang["cert_file"] << "</td><td>";
	s << "<input name='certificate' value='";
	if(vh){
		s << vh->certfile;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>" << klang["private_file"] << "</td><td>";
	s << "<input name='certificate_key' value='";
	if(vh){
		s << vh->keyfile;
	}
	s << "'></td></tr>\n";
#endif
	s << "<tr><td>envs</td><td>";
	s << "<input name='envs' size=32 value=\"";
	if (vh) {
		vh->buildEnv(s);
	}
	s << "\"></td></tr>\n";
	if(t || (vh && vh->isTemplete() && edit)){
		KTempleteVirtualHost *tvh = NULL;
		if(vh){
			tvh = static_cast<KTempleteVirtualHost *>(vh);
		}
		s << "<tr><td>init event:</td>";
		s << "<td><input name='init_event' size=32 value='" << (tvh?tvh->initEvents:"") << "'></td></tr>";
		s << "<tr><td>destroy event:</td>";
		s << "<td><input name='destroy_event' size=32 value='" << (tvh?tvh->destroyEvents:"") << "'></td></tr>";
		s << "<tr><td>update event:</td>";
		s << "<td><input name='update_event' size=32 value='" << (tvh?tvh->updateEvents:"") << "'></td></tr>";

	}
	s << "</table>\n";
	s << "<input type=submit value='" << LANG_SUBMIT << "'>";
	s << "</form>";
}
void KVirtualHostManage::getVhIndex(std::stringstream &s,KVirtualHost *vh,int id,int t,u_short default_http_port)
{
		vh->lock.Lock();
		s << "<tr id='tr" << id << "' style='background-color: #ffffff' onmouseover=\"setbgcolor('tr";
		s << id << "','#bbbbbb')\" onmouseout=\"setbgcolor('tr" << id << "','#ffffff')\">";
		s
				<< "<td>";
		s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/vhbase?";
		s << "name=" << vh->name << "&action=vh_delete&t=" << t << "';}\">"
			<< LANG_DELETE << "</a>]";
		if (t) {
			s << "[<a href='/vhlist?id=4&templete=" << vh->name << "'>" << klang["new_vh"] << "</a>]";
		}
		s << "</td>";
		s << "<td ";
		if(vh->status!=0){
			s << "bgcolor='#bbbbbb' title='" << vh->status << "'";
		}
		s << ">";
		s << "<a href='/vhlist?id=0&name=" << vh->name << "&t=" << t << "'>"
				<< vh->name << "</td>";
		s << "<td >";
		/*
		 ȡ�ñ����������󶨵Ķ˿�
		 */
		u_short bind_port = 0;
#ifdef ENABLE_BASED_PORT_VH
		list<u_short>::iterator it_port;
		it_port = vh->ports.begin();
		if (it_port != vh->ports.end()) {
			bind_port = (*it_port);
		}
#endif
		if (bind_port == 0) {
			bind_port = default_http_port;
		}
		list<KSubVirtualHost *>::iterator it2;
		for (it2 = vh->hosts.begin(); it2
				!= vh->hosts.end(); it2++) {
			if (it2 != vh->hosts.begin()) {
				s << "<br>";
			}
			if (!(*it2)->allSuccess) {
				s << "FAILED ";
			}
			bool href = true;
			if ( *(*it2)->host=='.'
				|| strcasecmp((*it2)->host, "*") == 0 
				|| strcasecmp((*it2)->host,"default") == 0) {
				href = false;
			}
			if (href) {
				s << "<a href='http://" << (*it2)->host;
				if (bind_port != 80) {
					s << ":" << bind_port;
				}
				s << "/' target=_blank>";
			}
			if (*(*it2)->host=='.') {
				s << "*";
			}
			s << (*it2)->host;
			if (href) {
				s << "</a>";
			}
			if (strcmp((*it2)->dir, "/") != 0) {
				s << "|" << (*it2)->dir;
			}

		}
		s << "</td>";
		s << "<td ><div title='" << vh->doc_root << "'>"
				<< vh->orig_doc_root << "</div></td>";
#ifdef ENABLE_VH_RUN_AS
		s << "<td >" << (vh->user.size() > 0 ? vh->user
				: "&nbsp;");
#ifndef _WIN32
		if (vh->group.size() > 0) {
			s << ":" << vh->group;
		}
#endif
		s << "</td>";
#endif
		s << "<td >" << (vh->inherit ? LANG_ON : LANG_OFF) << "</td>";
#ifdef ENABLE_VH_LOG_FILE
		s << "<td >"
				<< (vh->logFile.size() > 0 ? vh->logFile
						: "&nbsp;") << "</td>";
#endif
		s << "<td >" << (vh->browse ? LANG_ON : LANG_OFF) << "</td>";
#ifdef ENABLE_USER_ACCESS
		s << "<td >"
				<< (vh->user_access.size() > 0 ? vh->user_access
						: "&nbsp;") << "</td>";
#endif
#ifdef ENABLE_VH_RS_LIMIT
		s << "<td >" << vh->getConnectCount() << "/"
				<< vh->max_connect << "</td>";
		s << "<td >" ;
#ifdef ENABLE_VH_FLOW
		s << vh->getSpeed(false) << "/";
#endif
		s << vh->speed_limit << "</td>";
#endif
		/////////[247]
#ifdef ENABLE_VH_QUEUE
		s << "<td >";
		if (vh->queue) {
			s << vh->queue->getWorkerCount() << "/" << vh->queue->getMaxWorker();
		}
		s << "</td>";
		s << "<td >";
		if (vh->queue) {
			s << vh->queue->getQueueSize() << "/" << vh->queue->getMaxQueue();
		}
		s << "</td>";
#endif
		//*
		s << "<td >";
		if (vh->tvh) {
			s << "<a href='/vhlist?id=0&name=" << vh->tvh->name << "&t=1'>" << vh->tvh->name << "</a>";
		}else{
			s << "&nbsp;";
		}
		s << "</td>";
		//*/
		s << "</tr>\n";
		vh->lock.Unlock();
}
/////////[248]
void KVirtualHostManage::getAllVhHtml(std::stringstream &s,int t) {
	map<string, KVirtualHost *>::iterator it;
	s << "<script language='javascript'>\r\n"
		"	function setbgcolor(id,color){"
		"		document.getElementById(id).style.backgroundColor = color;"
		"	}"
		"</script>\r\n";
	s << "[<a href='/vhlist?id=4&t=" << t << "'>" << (t?klang["new_tvh"]:klang["new_vh"]) << "</a>] ";
	if (!t) {
		s << avh.size();
	}
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< LANG_NAME << "</td>";
	s << "<td>" << klang["vh_host"] << "</td><td>" << klang["doc_root"]
			<< "</td>";
#ifdef ENABLE_VH_RUN_AS
	s << "<td>" << LANG_RUN_USER << "</td>";
#endif
	s << "<td>" << klang["inherit"] << "</td>";
#ifdef ENABLE_VH_LOG_FILE
	s << "<td>" << klang["log_file"] << "</td>";
#endif
	s << "<td>" << klang["browse"] << "</td>";
#ifdef ENABLE_USER_ACCESS
	s << "<td>" << klang["access_file"] << "</td>";
#endif
#ifdef ENABLE_VH_RS_LIMIT
	s << "<td>" << klang["connect"] << "/" << klang["limit"] << "</td>";
	s << "<td>" << klang["speed"] << "/" << klang["limit"] << "</td>";
#endif
	/////////[249]
#ifdef ENABLE_VH_QUEUE
	s << "<td>" << klang["worker"] << "</td>";
	s << "<td>" << klang["queue"]  << "</td>";
#endif
	s << "<td>" << klang["templete"] << "</td>";
	s << "</tr>";
	/*
	 ȡ��ϵͳ�ɹ�������http�˿�
	 */
	u_short default_http_port = 80;
	/*
	std::vector<KServer *>::iterator it_server;
	for (it_server = servers.begin(); it_server != servers.end(); it_server++) {
		if ((*it_server)->model == 0) {
			default_http_port = (*it_server)->server.get_self_port();
			if (default_http_port == 80) {
				break;
			}
		}
	}
	if (default_http_port == 0) {
		default_http_port = 80;
	}
	*/
	int id=0;
	if(t==1){
		std::map<std::string, KGTempleteVirtualHost *>::iterator it2;
		for(it2=gtvhs.begin();it2!=gtvhs.end();it2++){
			std::map<std::string,KTempleteVirtualHost *>::iterator it3;
			for(it3=(*it2).second->tvhs.begin();it3!=(*it2).second->tvhs.end();it3++){
				getVhIndex(s,(*it3).second,id,t,default_http_port);
				id++;
			}
		}
	} else {
		for (it = avh.begin(); it != avh.end(); it++,id++) {
			getVhIndex(s,(*it).second,id,t,default_http_port);
		}
	}
	s << "</table>";
	s << "[<a href='/vhlist?id=4&t=" << t << "'>" << (t?klang["new_tvh"]:klang["new_vh"])  << "</a>]";
}
/*
void KVirtualHostManage::updateAllVirtualHost() {
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it = avh.begin(); it != avh.end(); it++) {
		if ((*it).second->ext) {
			continue;
		}
		(*it).second->updatedFlag = false;
	}
	lock.Unlock();
}
void KVirtualHostManage::checkAllVirtualHost() {
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it, it_next;
	for (it = avh.begin(); it != avh.end();) {
		if (!(*it).second->updatedFlag) {
#ifdef ENABLE_VH_RUN_AS
			for(size_t i=0;i<(*it).second->apps.size();i++){
				conf.gam->killCmdProcess((*it).second->apps[i]);
			}
#endif
			internalRemoveVirtualHost((*it).second, false);
			it_next = it;
			it++;
			avh.erase(it_next);
		} else {
			it++;
		}
	}
	lock.Unlock();
}
*/
void KVirtualHostManage::startStaticListen(std::vector<KListenHost *> &services,bool start)
{
	lock.Lock();
	std::map<KListenKey,KServer *>::iterator it;
	for (it=dlisten.listens.begin();it!=dlisten.listens.end();it++) {
		(*it).second->dynamic = true;
	}
	for (size_t i=0;i<services.size();i++) {
		//��ֹ����ʱ��̫��������ȫ��������Ϊ�ҵ���
		setActive();
		dlisten.add(services[i],start);
	}
	dlisten.flush();
	lock.Unlock();
	return;
}
bool KVirtualHostManage::startService(KListenHost *service, bool start)
{
	lock.Lock();
	bool result = dlisten.add(service,start);
	lock.Unlock();
	return result;
}
void KVirtualHostManage::flushListen(KVirtualHost *vh)
{
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string>::iterator it2;
	for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
		const char *bind = (*it2).c_str();
		if (*bind=='!') {
			dlisten.flush(bind+1);
		}
	}
#endif
}
int KVirtualHostManage::getCount()
{
	lock.Lock();
	int count = avh.size();
	lock.Unlock();
	return count;
}
void KVirtualHostManage::getListenHtml(std::stringstream &s)
{
	lock.Lock();
	dlisten.getListenHtml(s);
	lock.Unlock();
}
void KVirtualHostManage::internalBindVirtualHost(KVirtualHost *vh)
{
	dlisten.addStaticVirtualHost(vh);
	KServer::addDefaultVirtualHost(vh);
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string>::iterator it2;
	for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
		const char *bind = (*it2).c_str();
		if (*bind=='!') {
			dlisten.add(bind+1,vh);
		}
	}
#endif
}
void KVirtualHostManage::internalBindAllVirtualHost()
{
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it=avh.begin();it!=avh.end();it++) {
		internalBindVirtualHost((*it).second);
	}
	dlisten.flush();
}
