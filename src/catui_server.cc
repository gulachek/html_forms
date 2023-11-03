#include <node.h>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace demo {

static int unix_send_fd(int sock, int fd_to_send);
static int unix_recv_fd(int sock);

using v8::FunctionCallbackInfo;

using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Value;

void UnixRecvFd(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();

  // TODO argument validation

  v8::MaybeLocal<v8::Int32> maybeInt =
      args[0]->ToInt32(isolate->GetCurrentContext());

  int sock = maybeInt.ToLocalChecked()->Value();
  int fd = unix_recv_fd(sock);
  args.GetReturnValue().Set(v8::Int32::New(isolate, fd));
}

void UnixSendFd(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();

  // TODO argument validation

  v8::MaybeLocal<v8::Int32> maybeSock =
      args[0]->ToInt32(isolate->GetCurrentContext());

  v8::MaybeLocal<v8::Int32> maybeFd =
      args[1]->ToInt32(isolate->GetCurrentContext());

  int sock = maybeSock.ToLocalChecked()->Value();
  int fd = maybeFd.ToLocalChecked()->Value();

  int ret = unix_send_fd(sock, fd);

  args.GetReturnValue().Set(v8::Int32::New(isolate, ret));
}

void Initialize(Local<Object> exports) {
  NODE_SET_METHOD(exports, "unixRecvFd", UnixRecvFd);
  NODE_SET_METHOD(exports, "unixSendFd", UnixSendFd);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

int unix_send_fd(int sock, int fd_to_send) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(struct msghdr));
  char buf[CMSG_SPACE(sizeof(int))];
  struct iovec io = {};
  io.iov_base = (void *)".";
  io.iov_len = 1;

  msg.msg_iov = &io;
  msg.msg_iovlen = 1;

  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *((int *)CMSG_DATA(cmsg)) = fd_to_send;
  msg.msg_controllen = cmsg->cmsg_len;

  return sendmsg(sock, &msg, 0);
}

/* fd if successful, -1 if not */
int unix_recv_fd(int sock) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(struct msghdr));

  char m_buf[256];
  struct iovec io = {.iov_base = m_buf, .iov_len = sizeof(m_buf)};
  msg.msg_iov = &io;
  msg.msg_iovlen = 1;

  char c_buf[256];
  msg.msg_control = c_buf;
  msg.msg_controllen = sizeof(c_buf);

  ssize_t nread = recvmsg(sock, &msg, 0);
  if (nread == -1) {
    perror("recvmsg");
    return -1;
  }

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

  if (cmsg->cmsg_type != SCM_RIGHTS) {
    fprintf(stderr, "recvmsg had cmsg_type %d instead of SCM_RIGHTS\n",
            cmsg->cmsg_type);
    return -1;
  }

  if (cmsg->cmsg_level != SOL_SOCKET) {
    fprintf(stderr, "recvmsg had cmsg_level %d instead of SOL_SOCKET\n",
            cmsg->cmsg_level);
    return -1;
  }

  if (cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
    fprintf(stderr,
            "recvmsg had cmsg_len %u instead of CMSG_LEN(sizeof(int)) "
            "(%lu)\n",
            cmsg->cmsg_len, CMSG_LEN(sizeof(int)));
    return -1;
  }

  return *((int *)CMSG_DATA(cmsg));
}

} // namespace demo
