// Copyright 2017 Qihoo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:// www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef SRC_NODE_ZP_SYNC_CONN_H_
#define SRC_NODE_ZP_SYNC_CONN_H_

#include <string>
#include "pink/include/pb_conn.h"
#include "pink/include/server_thread.h"
#include "src/node/client.pb.h"

class ZPSyncConn: public pink::PbConn  {
 public:
  ZPSyncConn(int fd, std::string ip_port, pink::ServerThread *thread);
  virtual ~ZPSyncConn();
  virtual int DealMessage();

 private:
  client::SyncRequest request_;
  void DebugReceive(const client::CmdRequest &crequest) const;
};

class ZPSyncConnHandle : public pink::ServerHandle  {
 public:
  ZPSyncConnHandle() {}
  virtual ~ZPSyncConnHandle() {}

  virtual void CronHandle() const override;
  using pink::ServerHandle::AccessHandle;
  virtual bool AccessHandle(std::string& ip) const override {
    return true;
  }
};

class ZPSyncConnFactory : public pink::ConnFactory  {
 public:
  virtual pink::PinkConn* NewPinkConn(
      int connfd,
      const std::string &ip_port,
      pink::ServerThread *server_thread,
      void* worker_private_data) const {
    return new ZPSyncConn(connfd, ip_port, server_thread);
  }
};

#endif  // SRC_NODE_ZP_SYNC_CONN_H_
