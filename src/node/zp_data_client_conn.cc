#include "zp_data_client_conn.h"

#include <vector>
#include <algorithm>
#include <glog/logging.h>

#include "zp_data_worker_thread.h"
#include "zp_data_server.h"

extern ZPDataServer* zp_data_server;

////// ZPDataClientConn //////
ZPDataClientConn::ZPDataClientConn(int fd, std::string ip_port, pink::Thread* thread) :
  PbConn(fd, ip_port) {
  self_thread_ = dynamic_cast<ZPDataWorkerThread*>(thread);
}

ZPDataClientConn::~ZPDataClientConn() {
}

// Msg is  [ length (int32) | pb_msg (length bytes) ]
int ZPDataClientConn::DealMessage() {
  self_thread_->PlusThreadQuerynum();
  request_.ParseFromArray(rbuf_ + cur_pos_ - header_len_, header_len_);

  // TODO test only
  switch (request_.type()) {
    case client::Type::SET: {
      DLOG(INFO) << "Receive Set cmd";
      break;
    }
    case client::Type::GET: {
      DLOG(INFO) << "Receive Get cmd";
      break;
    }
    case client::Type::SYNC: {
      DLOG(INFO) << "Receive Sync cmd";
      break;
    }
  }

  Cmd* cmd = self_thread_->GetCmd(static_cast<int>(request_.type()));
  if (cmd == NULL) {
    LOG(ERROR) << "unsupported type: " << (int)request_.type();
    return -1;
  }

  Status s = cmd->Init(&request_);
  if (!s.ok()) {
    LOG(ERROR) << "command Init failed, " << s.ToString();
  }

  set_is_reply(true);
  std::string raw_msg(rbuf_ + cur_pos_ - header_len_ - 4, header_len_ + 4);

  Partition* partition = NULL;
  if (request_.type() ==  client::Type::SYNC) {
    partition = zp_data_server->GetPartitionById(request_.sync().partition_id());
  } else {
    partition = zp_data_server->GetPartition(cmd->key());
  }

  // TODO maybe we can use global status or just return error
  if (partition == NULL) {
    response_.set_type(request_.type());
    switch (request_.type()) {
      case client::Type::SET: {
        response_.mutable_set()->set_code(client::StatusCode::kError);
        response_.mutable_set()->set_msg("no partition");
        break;
      }
      case client::Type::GET: {
        response_.mutable_get()->set_code(client::StatusCode::kError);
        response_.mutable_get()->set_msg("no partition");
        break;
      }
      case client::Type::SYNC: {
        response_.mutable_sync()->set_code(client::StatusCode::kError);
        response_.mutable_sync()->set_msg("no partition");
        break;
      }
    }
    LOG(ERROR) << "Error partition";
    res_ = &response_;
    return 0;
  }

  partition->DoCommand(cmd, request_, response_, raw_msg);

  res_ = &response_;
  return 0;
}

