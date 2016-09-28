#include "zp_kv.h"

#include <glog/logging.h>
#include "zp_data_server.h"

#include "slash_string.h"
#include "nemo.h"

extern ZPDataServer *zp_data_server;

void InitClientCmdTable(std::unordered_map<int, Cmd*> *cmd_table) {
  //Kv
  ////SetCmd
  Cmd* setptr = new SetCmd(kCmdFlagsKv | kCmdFlagsWrite);
  cmd_table->insert(std::pair<int, Cmd*>(static_cast<int>(client::Type::SET), setptr));
  ////GetCmd
  Cmd* getptr = new GetCmd(kCmdFlagsKv | kCmdFlagsRead);
  cmd_table->insert(std::pair<int, Cmd*>(static_cast<int>(client::Type::GET), getptr));

  // Sync
  Cmd* syncptr = new SyncCmd(kCmdFlagsRead | kCmdFlagsAdmin | kCmdFlagsSuspend);
  cmd_table->insert(std::pair<int, Cmd*>(static_cast<int>(client::Type::SYNC), syncptr));
}

// We use static_cast instead of dynamic_cast, caz we know exactly the Derived class type.
Status SetCmd::Init(google::protobuf::Message *req) {
  client::CmdRequest* request = static_cast<client::CmdRequest*>(req);
  key_ = request->set().key();
  DLOG(INFO) << "SetCmd Init key(" << key_ << ") ok";

  return Status::OK();
}

void SetCmd::Do(google::protobuf::Message *req, google::protobuf::Message *res, void* partition, bool readonly) {
  client::CmdRequest* request = static_cast<client::CmdRequest*>(req);
  client::CmdResponse* response = static_cast<client::CmdResponse*>(res);
  Partition* ptr = static_cast<Partition*>(partition);

  client::CmdResponse_Set* set_res = response->mutable_set();
  response->set_type(client::Type::SET);

  if (readonly) {
    set_res->set_code(client::StatusCode::kError);
    set_res->set_msg("readonly mode");
    DLOG(INFO) << "readonly mode, failed Set key(" << key_ << ") at Partition: " << ptr->partition_id();
    return;
  }

  //LOG(ERROR) << "Test: BGSave";
  //ptr->Bgsave();

  //int32_t ttl;
  nemo::Status s = ptr->db()->Set(request->set().key(), request->set().value());
  //nemo::Status s = zp_data_server->db()->Set(request->set().key(), request->set().value());
  if (!s.ok()) {
    set_res->set_code(client::StatusCode::kError);
    set_res->set_msg(s.ToString());
    LOG(ERROR) << "command failed: Set, caz " << s.ToString();
  } else {
    set_res->set_code(client::StatusCode::kOk);
    DLOG(INFO) << "Set key(" << key_ << ") at Partition: " << ptr->partition_id() << " ok";
  }
}

Status GetCmd::Init(google::protobuf::Message *req) {
  client::CmdRequest* request = static_cast<client::CmdRequest*>(req);
  key_ = request->get().key();
  DLOG(INFO) << "GetCmd Init key(" << key_ << ") ok";

  return Status::OK();
}

void GetCmd::Do(google::protobuf::Message *req, google::protobuf::Message *res, void* partition, bool readonly) {
  client::CmdRequest* request = static_cast<client::CmdRequest*>(req);
  client::CmdResponse* response = static_cast<client::CmdResponse*>(res);
  Partition* ptr = static_cast<Partition*>(partition);

  client::CmdResponse_Get* get_res = response->mutable_get();
  response->set_type(client::Type::GET);

  std::string value;
  nemo::Status s = ptr->db()->Get(request->get().key(), &value);
  if (s.ok()) {
    get_res->set_code(client::StatusCode::kOk);
    get_res->set_value(value);
    DLOG(INFO) << "Get key(" << request->get().key() << ") at Partition " << ptr->partition_id() << " ok, value is (" << value << ")";
  } else if (s.IsNotFound()) {
    get_res->set_code(client::StatusCode::kNotFound);
    DLOG(INFO) << "Get key(" << request->get().key() << ") at Partition " << ptr->partition_id() << " not found!";
  } else {
    get_res->set_code(client::StatusCode::kError);
    get_res->set_msg(s.ToString());
    LOG(ERROR) << "command failed: Get at Partition " << ptr->partition_id() << ", caz " << s.ToString();
  }
}

// Sync between nodes
void SyncCmd::Do(google::protobuf::Message *req, google::protobuf::Message *res, void* partition, bool readonly) {
  client::CmdRequest* request = static_cast<client::CmdRequest*>(req);
  client::CmdResponse* response = static_cast<client::CmdResponse*>(res);
  Partition* ptr = static_cast<Partition*>(partition);

  client::CmdRequest_Sync sync_req = request->sync();

  slash::Status s;
  Node node(sync_req.node().ip(), sync_req.node().port());

  response->set_type(client::Type::SYNC);

  LOG(INFO) << "SyncCmd Partition:" << ptr->partition_id()
    << " a new node(" << node.ip << ":" << node.port << ") filenum " << sync_req.filenum() << ", offset " << sync_req.offset();
  s = ptr->AddBinlogSender(node, sync_req.filenum(), sync_req.offset());

  client::CmdResponse_Sync* sync_res = response->mutable_sync();
  if (s.ok()) {
    sync_res->set_code(client::StatusCode::kOk);
    DLOG(INFO) << "SyncCmd add node ok";
  } else if (s.IsIncomplete()) {
    sync_res->set_code(client::StatusCode::kWait);
    DLOG(INFO) << "SyncCmd add node incomplete";
  } else {
    sync_res->set_code(client::StatusCode::kError);
    sync_res->set_msg(s.ToString());
    LOG(ERROR) << "command failed: Sync, caz " << s.ToString();
  }
}
