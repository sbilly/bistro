#include <gflags/gflags.h>
#include <glog/logging.h>
#include <memory>

#include "bistro/bistro/if/gen-cpp2/BistroScheduler.h"
#include "bistro/bistro/utils/service_clients.h"
#include "bistro/bistro/utils/server_socket.h"
#include "bistro/bistro/worker/BistroWorkerHandler.h"
#include "folly/Memory.h"
#include "thrift/lib/cpp2/server/ThriftServer.h"

// TODO: It would be useful to periodically re-read this host:port from a
// file to ensure your scheduler can survive machine failures.
DEFINE_string(scheduler_host, "", "Scheduler's hostname.");
DEFINE_int32(scheduler_port, 0, "Scheduler's thrift port.");
DEFINE_string(worker_command, "", "Command to run for the worker.");

static const bool scheduler_host_validator = google::RegisterFlagValidator(
  &FLAGS_scheduler_host,
  [](const char* flagname, const std::string& value) { return !value.empty(); }
);

static const bool scheduler_port_validator = google::RegisterFlagValidator(
  &FLAGS_scheduler_port,
  [](const char* flagname, int32_t value) {return value > 0 && value < 65536; }
);

static const bool worker_command_validator = google::RegisterFlagValidator(
  &FLAGS_worker_command,
  [](const char* flagname, const std::string& value) { return !value.empty(); }
);

using namespace facebook::bistro;

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = 1;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  cpp2::ServiceAddress scheduler_addr;
  // DO: It would be faster & more robust to pre-resolve the hostname here.
  scheduler_addr.ip_or_host = FLAGS_scheduler_host;
  scheduler_addr.port = FLAGS_scheduler_port;

  auto my_socket_and_addr = getServerSocketAndAddress();
  auto server = std::make_shared<apache::thrift::ThriftServer>();
  auto handler = std::make_shared<BistroWorkerHandler>(
    [server, scheduler_addr](folly::EventBase* event_base) {
      return getAsyncClientForAddress<cpp2::BistroSchedulerAsyncClient>(
        event_base,
        scheduler_addr
      );
    },
    FLAGS_worker_command,
    my_socket_and_addr.second,  // Could change in the presence of proxies
    my_socket_and_addr.second.port  // Actual local port the worker has locked
  );

  server->useExistingSocket(std::move(my_socket_and_addr.first));
  server->setInterface(std::move(handler));
  server->serve();
}