// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "engine/initd.h"

#include <sstream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "logging.h"
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
namespace dos {

InitdImpl::InitdImpl():tasks_(NULL), mutex_(), workers_(NULL),
  proc_mgr_(NULL){
  tasks_ = new std::map<std::string, Process>();
  workers_ = new ::baidu::common::ThreadPool(4);
  proc_mgr_ = new ProcessMgr();
}

InitdImpl::~InitdImpl(){
  delete workers_;
  delete tasks_;
}

void InitdImpl::Fork(RpcController*,
                     const ForkRequest* request,
                     ForkResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::map<std::string, Process>::iterator it = tasks_->find(request->process().name());
  if (it != tasks_->end()) {
    response->set_status(kRpcNameExist);
    done->Run();
    return;
  }

  bool ok = Launch(request->process());
  if (!ok) {
      response->set_status(kRpcError);
  }else { 
      response->set_status(kRpcOk);
  }
  done->Run();
}

void InitdImpl::Status(RpcController*,
                       const StatusRequest* request,
                       StatusResponse* response,
                       Closure* done) {
  response->set_status(kRpcOk);
  done->Run();
}

bool InitdImpl::Launch(const Process& process) {
  mutex_.AssertHeld(); 
  bool ok = proc_mgr_->Exec(process);
  if (!ok) {
    LOG(WARNING, "fail to fork process name %s", process.name().c_str());
    return false;
  }else {
    LOG(INFO, "fork process %s  successfully", process.name().c_str());
  }
  tasks_->insert(std::make_pair(process.name(), process));
  workers_->DelayTask(1000, boost::bind(&InitdImpl::CheckStatus, this,process.name()));
  return true;
}

void InitdImpl::CheckStatus(const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::map<std::string, Process>::iterator it = tasks_->find(name);
  if (it == tasks_->end()) {
    LOG(WARNING, "fail to find task with name %s", name.c_str());
    return;
  }
  dos::Process p;
  bool ok = proc_mgr_->Wait(name, &p);
  if (!ok) {
    LOG(WARNING, "fail to wait task %s", name.c_str());
    // clean task in proc_mgr;
    proc_mgr_->Kill(name, 9);
    return;
  }
  it->second.CopyFrom(p);
  if (p.running()) {
    workers_->DelayTask(2000, boost::bind(&InitdImpl::CheckStatus, this, name));
  }else { 
    LOG(INFO, "task with name %s is dead", name.c_str());
  }
}

void InitdImpl::Wait(RpcController* controller,
                     const WaitRequest* request,
                     WaitResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  for (int i = 0; i < request->names_size(); i++) {
    std::map<std::string, Process>::iterator it = tasks_->find(request->names(i));
    if (it == tasks_->end()) {
      LOG(WARNING, "task with name %s does not exist in initd", request->names(i).c_str());
      continue;
    }
    Process* p = response->add_processes();
    p->CopyFrom(it->second);
  }
  response->set_status(kRpcOk);
  done->Run();
}

void InitdImpl::Kill(RpcController* controller,
                     const KillRequest* request,
                     KillResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::set<std::string> will_be_removed;
  for (int i = 0; i < request->names_size(); i++) {
    std::string name = request->names(i);
    std::map<std::string, Process>::iterator it = tasks_->find(name);
    if (it == tasks_->end()) {
      LOG(WARNING, "task with name %s does not exist in initd", name.c_str());
      continue;
    }
    bool ok = proc_mgr_->Kill(name, request->signal());
    if (ok) {
      LOG(INFO, "kill task with name %s successfully", name.c_str());
      will_be_removed.insert(name);
      continue;
    }
    LOG(WARNING, "killing task with name %s fails", name.c_str());
  }
  response->set_status(kRpcOk);
  done->Run();
  std::set<std::string>::iterator it = will_be_removed.begin();
  for (; it != will_be_removed.end(); ++it) {
    tasks_->erase(*it);
  }
}

}// namespace dos