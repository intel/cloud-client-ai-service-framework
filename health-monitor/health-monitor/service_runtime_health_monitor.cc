// Copyright (C) 2020 Intel Corporation

#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <error.h>

#include <grpc++/grpc++.h>

#include "service_runtime_health_monitor.h"
#include "service_runtime_health_monitor.pb.h"
#include "service_runtime_health_monitor.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using service_runtime_health_monitor::Monitor;
using service_runtime_health_monitor::AgentStatus;
using service_runtime_health_monitor::Empty;
using service_runtime_health_monitor::PulseServerAddr;

std::string container_gateway_ip;

#define WATCHDOG_TIMEOUT 120

#define DOCKER_INSPECT_GATEWAY \
	"docker inspect service_runtime_container -f '{{.NetworkSettings.Gateway}}'"
#define NETSTATE_PULSE \
	"netstat -nplt | grep pulseaudio | grep -w tcp | awk '{print $4}' | cut -d ':' -f 2"

#define MAX_INTERVAL	64

static int get_pulse_server_port(char *port, int size)
{
	FILE *fp;
	int s;

	if (size <= 0)
		return -1;

	if ((fp = popen(NETSTATE_PULSE, "r")) == NULL) {
		log_error("popen get_pulse_server_port error: %d\n", errno);
		return -1;
	}
	while (fgets(port, size, fp) != NULL) {
		log_debug("fget: %s\n", port);
	};
	s = pclose(fp);

	if (WIFEXITED(s)) {
		s = WEXITSTATUS(s);
	} else if (WIFSIGNALED(s)) {
		s = WTERMSIG(s);
	} else if (WIFSTOPPED(s)) {
		s = WSTOPSIG(s);
	}

	if (s != 0)
		return -1;

	port[strcspn(port, "\n")] = 0;

	return 0;
}

static int get_container_gateway_addr(char *addr, int size)
{
	FILE *fp;
	int s;
	int interval = 1;

	if (size <= 0)
		return -1;
	for (;;) {
		if ((fp = popen(DOCKER_INSPECT_GATEWAY, "r")) == NULL) {
			log_error("popen docker error: %d\n", errno);
			goto sleep;
		}
		while (fgets(addr, size, fp) != NULL);
		s = pclose(fp);
		if (WIFEXITED(s)) {
			s = WEXITSTATUS(s);
		} else if (WIFSIGNALED(s)) {
			s = WTERMSIG(s);
		} else if (WIFSTOPPED(s)) {
			s = WSTOPSIG(s);
		}
		if (s == 0)
			break;
sleep:
		sleep(interval);
		if (interval < MAX_INTERVAL)
			interval *= 2;
	}

	addr[strcspn(addr, "\n")] = 0;
	log_info("%s\n", addr);

	return 0;
}

static int run_cmd(const char *cmd)
{
	int status;

	if ((status = system(cmd)) == -1) {
		log_error("fork failed cmd: %s\n", cmd);
		perror("system");
		return -1;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	} else {
		log_error("terminated abnormally cmd: %s\n", cmd);
		return -1;
	}
}

static int service_runtime_restart()
{
	int retry = 0;

	while (retry ++ < 3) {
		if (run_cmd("/opt/intel/service_runtime/service_runtime.sh restart") == 0)
			return 0;
		sleep(1);
	}

	return -1;
}

class ServiceRuntimeWatchdog
{
private:
	std::atomic<bool> timer_running;
	std::thread thread;
	std::condition_variable timer_reset_cond;
	std::mutex timer_reset_cond_mutex;

	void timer_thread()
	{
		std::chrono::seconds timeout =
			std::chrono::seconds(WATCHDOG_TIMEOUT);
		std::unique_lock<std::mutex> lock(timer_reset_cond_mutex);

		log_info("watchdog timer thread started\n");

		while (timer_running) {
			auto ret = timer_reset_cond.wait_for(lock, timeout) ;
			if (std::cv_status::timeout == ret) {
				// restart service_runtime
				log_info("watchdog timer timeout, "
					 "restart service_runtime\n");
				if (service_runtime_restart() != 0)
					log_error("cannot restart service_runtime\n");
				// TODO: add a counter to record the continuous
				// restart
			} else {
				log_debug("watchdog timer reset\n");
			}
		}
	}

public:
	ServiceRuntimeWatchdog()
	{
		timer_running = true;
		thread = std::thread(&ServiceRuntimeWatchdog::timer_thread,
				     this);
	}

	~ServiceRuntimeWatchdog()
	{
		std::unique_lock<std::mutex> lock(timer_reset_cond_mutex);
		timer_running = false;
		timer_reset_cond.notify_all();
		thread.join();
	}

	void feed()
	{
		std::unique_lock<std::mutex> lock(timer_reset_cond_mutex);
		timer_reset_cond.notify_all();
	}
};


class MonitorImpl final : public Monitor::Service
{
private:
	unsigned int unhealthy_count;
	ServiceRuntimeWatchdog wd;

	template <typename T>
	void log_status(const T& field, std::string field_name)
	{
		if (field.size() == 0)
			return;
		// TODO: for klocwork remove field_name
		//log_info("%s: %d\n", field_name.c_str(), field.size());
		for (const auto &dt: field)
			log_info("  %s\n", dt.c_str());
	}
public:
	MonitorImpl(): unhealthy_count(0) {};

	Status notify(ServerContext* context, const AgentStatus* status,
		      Empty* empty) override
	{
		wd.feed();

		log_status(status->dead_targets(), "dead_targets");
		log_status(status->new_dead_targets(), "new_dead_targets");
		log_status(status->dead_healthcheck_targets(),
			   "dead_healthcheck_targets");
		log_status(status->new_dead_healthcheck_targets(),
			   "new_dead_healthcheck_targets");

		if (status->new_dead_targets_size() > 0
		    || status->new_dead_healthcheck_targets_size() > 0) {
			unhealthy_count ++;
		} else {
			unhealthy_count = 0;
		}

		if (unhealthy_count != 0)
			log_info("unhealthy_count: %d\n", unhealthy_count);

		if (unhealthy_count >= 3) {
			log_info("restart service_runtime\n");
			if (service_runtime_restart() != 0)
				log_error("cannot restart service_runtime\n");
		}
		// TODO: if after restart container still is unhealthy should
		// stop to restart container?

		return Status::OK;
	}

	Status get_pulse_server_addr(ServerContext* context,
				     const Empty* empty,
				     PulseServerAddr* pulse_server_addr) override
	{
		char port_buffer[16];

		if (get_pulse_server_port(port_buffer,
					  sizeof(port_buffer)) != 0) {
			pulse_server_addr->set_status(-1);
			return Status::OK;
		}
		if (strlen(port_buffer) == 0) {
			pulse_server_addr->set_status(-1);
			return Status::OK;
		}

		pulse_server_addr->set_status(0);
		std::string s = "tcp:" + container_gateway_ip + ":" + port_buffer;
		log_info("pulse_server_addr=%s\n", s.c_str());
		pulse_server_addr->set_addr(s);

		return Status::OK;
	}
};

int run_server(const char *port)
{
	char addr_buffer[32];

	get_container_gateway_addr(addr_buffer, sizeof(addr_buffer));
	container_gateway_ip = addr_buffer;

	std::string server_address(container_gateway_ip);
	server_address += ":";
	server_address += port;

	MonitorImpl *service = new MonitorImpl;
	ServerBuilder *builder = new ServerBuilder;
	builder->SetMaxReceiveMessageSize(-1);
	builder->AddListeningPort(server_address,
				  grpc::InsecureServerCredentials());
	builder->RegisterService(service);
	std::unique_ptr<Server> server(builder->BuildAndStart());
	log_info("Servre listening on %s\n", server_address.c_str());
	server->Wait();

	delete builder;
	delete service;

	return 0;
}

int main(int argc, char **argv)
{
	const char *port;
	if (argc < 2)
		port = "8082";
	else
		port = argv[1];

	setvbuf(stdout, NULL, _IOLBF, 1024);
	setvbuf(stderr, NULL, _IONBF, 1024);

	run_server(port);

	return 0;
}
