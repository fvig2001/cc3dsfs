// Tests for the external settings control channel (no SFML needed).
// Build: g++ -std=c++17 -Iinclude tests/ctl_test.cpp source/ControlServer.cpp source/InputRedirection.cpp -o ctl_test -pthread
#include "ControlServer.hpp"
#include "InputRedirection.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace ctl;

static int failures = 0;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while(0)

static const uint16_t TEST_PORT = 47999;

// Runs a client in a thread while this thread plays the part of the main loop polling the server
static int run_client(Server &server, const std::vector<std::string> &args, std::string &output, const Server::Handler &handler, int timeout_ms = 2000) {
	std::atomic<bool> done(false);
	int code = -1;
	std::thread client([&]() { code = send_update(TEST_PORT, args, timeout_ms, output); done = true; });
	auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while(!done && std::chrono::steady_clock::now() < limit) {
		server.poll(handler);
		usleep(2000);
	}
	client.join();
	return code;
}

static std::vector<Result> accept_all(const std::vector<Setting> &settings) {
	std::vector<Result> out;
	for(const Setting &s : settings) out.push_back({s.key, true, ""});
	return out;
}

static void test_key_names() {
	CHECK(is_valid_key_name("bot_fullscreen"));
	CHECK(is_valid_key_name("ir_ip"));
	CHECK(!is_valid_key_name(""));
	CHECK(!is_valid_key_name("a b"));
	CHECK(!is_valid_key_name("a=b"));
	CHECK(!is_valid_key_name("a\nb"));
	CHECK(!is_valid_key_name(std::string(65, 'a')));
}

static void test_request_roundtrip() {
	std::string req, err;
	CHECK(build_request({"bot_fullscreen=0", "bot_have_titlebar=1", "ir_ip=192.168.1.5"}, req, err));
	std::vector<Setting> parsed;
	CHECK(parse_request(req, parsed, err));
	CHECK(parsed.size() == 3);
	CHECK(parsed[0].key == "bot_fullscreen" && parsed[0].value == "0");
	CHECK(parsed[2].key == "ir_ip" && parsed[2].value == "192.168.1.5");
	// A value may contain '=' and spaces, and be empty
	CHECK(build_request({"audio_device=My Device=2", "ir_ip="}, req, err));
	CHECK(parse_request(req, parsed, err));
	CHECK(parsed[0].value == "My Device=2");
	CHECK(parsed[1].key == "ir_ip" && parsed[1].value == "");
	// CRLF is tolerated (scripts on Windows)
	CHECK(parse_request(std::string(REQUEST_HEADER) + "\r\nsplit=1\r\n", parsed, err) && parsed[0].value == "1");
}

static void test_bad_requests() {
	std::string req, err;
	CHECK(!build_request({}, req, err));
	CHECK(!build_request({"novalue"}, req, err));
	CHECK(!build_request({"=5"}, req, err));
	CHECK(!build_request({"bad name=5"}, req, err));
	CHECK(!build_request({"a=line\nbreak"}, req, err));
	CHECK(!build_request({"a=" + std::string(MAX_VALUE_LENGTH + 1, 'x')}, req, err));
	std::vector<std::string> many(MAX_SETTINGS_PER_REQUEST + 1, "a=1");
	CHECK(!build_request(many, req, err));
	std::vector<Setting> parsed;
	CHECK(!parse_request("", parsed, err));
	CHECK(!parse_request("hello\nsplit=1\n", parsed, err));                 // wrong header
	CHECK(!parse_request(std::string(REQUEST_HEADER) + "\n", parsed, err)); // nothing to do
	CHECK(!parse_request(std::string(REQUEST_HEADER) + "\nsplit\n", parsed, err));
	CHECK(!parse_request(std::string(REQUEST_HEADER) + "\nsplit=1\nbroken line\n", parsed, err)); // one bad line rejects all
}

static void test_response_format() {
	std::string r = format_response({{"a", true, ""}, {"b", false, "unknown setting"}});
	CHECK(r == "OK a\nERROR b: unknown setting\nDONE applied=1 failed=1\n");
	CHECK(count_failures(r) == 1);
	CHECK(count_failures(format_response({{"a", true, ""}})) == 0);
	CHECK(count_failures("garbage") == -1);
	CHECK(count_failures("DONE applied=1") == -1);
}

static void test_end_to_end() {
	Server server; std::string err;
	CHECK(server.start(TEST_PORT, err));
	std::vector<Setting> seen;
	std::string out;
	int code = run_client(server, {"bot_fullscreen=0", "bot_have_titlebar=1"}, out, [&](const std::vector<Setting> &s) { seen = s; return accept_all(s); });
	CHECK(code == CLIENT_OK);
	CHECK(seen.size() == 2 && seen[0].key == "bot_fullscreen" && seen[1].value == "1");
	CHECK(out == "OK bot_fullscreen\nOK bot_have_titlebar\nDONE applied=2 failed=0\n");

	// The handler refusing some settings gives a non-zero exit code and says which
	code = run_client(server, {"split=1", "nonsense=1"}, out, [&](const std::vector<Setting> &s) {
		std::vector<Result> r; for(auto &x : s) r.push_back(x.key == "split" ? Result{x.key, true, ""} : Result{x.key, false, "unknown setting"}); return r; });
	CHECK(code == CLIENT_SOME_FAILED);
	CHECK(out.find("ERROR nonsense: unknown setting") != std::string::npos);
	CHECK(out.find("OK split") != std::string::npos);
	server.stop();
}

static void test_not_running() {
	std::string out;
	auto start = std::chrono::steady_clock::now();
	int code = send_update(TEST_PORT, {"split=1"}, 3000, out);
	double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	CHECK(code == CLIENT_NOT_RUNNING);
	CHECK(out.find("not running") != std::string::npos);
	CHECK(elapsed < 1.5); // "connection refused" is noticed at once, we don't sit through the 3 s timeout
	CHECK(send_update(0, {"split=1"}, 100, out) == CLIENT_BAD_USAGE);
	CHECK(send_update(TEST_PORT, {"oops"}, 100, out) == CLIENT_BAD_USAGE);
}

static void test_port_in_use_and_restart() {
	Server a, b; std::string err;
	CHECK(a.start(TEST_PORT, err));
	CHECK(!b.start(TEST_PORT, err));            // a second instance can't take the port
	CHECK(err.find("not available") != std::string::npos);
	CHECK(!b.is_running());
	CHECK(b.poll(accept_all) == 0);             // polling a server which isn't running is harmless
	a.stop();
	CHECK(b.start(TEST_PORT, err));             // and the port is free again after stopping
	b.stop();
}

static void test_garbage_gets_error_reply() {
	Server server; std::string err;
	CHECK(server.start(TEST_PORT, err));
	auto exchange = [&](const std::string &payload) {
		std::string reply;
		std::atomic<bool> done(false);
		std::thread t([&]() {
			int s = socket(AF_INET, SOCK_DGRAM, 0);
			sockaddr_in addr; memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET; addr.sin_port = htons(TEST_PORT); addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			timeval tv{2, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
			sendto(s, payload.data(), payload.size(), 0, (sockaddr*)&addr, sizeof(addr));
			char buf[8192]; int n = (int)recv(s, buf, sizeof(buf), 0);
			if(n > 0) reply.assign(buf, n);
			close(s); done = true; });
		auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(4);
		while(!done && std::chrono::steady_clock::now() < limit) { server.poll(accept_all); usleep(2000); }
		t.join();
		return reply;
	};
	std::string r = exchange("GET / HTTP/1.1\r\n\r\n");
	CHECK(r.find("ERROR request: Not a cc3dsfs settings request") != std::string::npos);
	CHECK(count_failures(r) == 1);
	r = exchange(std::string(MAX_DATAGRAM_SIZE + 500, 'a'));
	CHECK(r.find("ERROR request: The request is too large") != std::string::npos);
	r = exchange(std::string(REQUEST_HEADER) + "\nsplit=1\n");
	CHECK(r == "OK split\nDONE applied=1 failed=0\n"); // and it still works afterwards
	server.stop();
}

// The settings names used by "--update_settings ir_*" are checked in the same way as the config file
static void test_ir_settings_and_hosts() {
	ir::Config c;
	CHECK(ir::apply_config_setting(c, "ip", "192.168.85.200") == ir::SettingResult::APPLIED && c.ip == "192.168.85.200");
	CHECK(ir::apply_config_setting(c, "enabled", "1") == ir::SettingResult::APPLIED && c.enabled);
	CHECK(ir::apply_config_setting(c, "enabled", "0") == ir::SettingResult::APPLIED && !c.enabled);
	CHECK(ir::apply_config_setting(c, "enabled", "yes") == ir::SettingResult::BAD_VALUE);
	CHECK(ir::apply_config_setting(c, "invert_y", "1") == ir::SettingResult::APPLIED && c.invert_y);
	CHECK(ir::apply_config_setting(c, "deadzone", "0.3") == ir::SettingResult::APPLIED && c.deadzone > 0.29f && c.deadzone < 0.31f);
	CHECK(ir::apply_config_setting(c, "deadzone", "abc") == ir::SettingResult::BAD_VALUE);
	CHECK(ir::apply_config_setting(c, "bind_home", "button:8") == ir::SettingResult::APPLIED && c.bindings[ir::SLOT_HOME].index == 8);
	CHECK(ir::apply_config_setting(c, "bind_home", "button:99") == ir::SettingResult::BAD_VALUE);
	CHECK(c.bindings[ir::SLOT_HOME].index == 8); // a bad value leaves the setting as it was
	CHECK(ir::apply_config_setting(c, "bind_nothing", "button:1") == ir::SettingResult::UNKNOWN_KEY);
	CHECK(ir::apply_config_setting(c, "nothing", "1") == ir::SettingResult::UNKNOWN_KEY);

	CHECK(ir::is_valid_host(""));
	CHECK(ir::is_valid_host("192.168.1.50"));
	CHECK(ir::is_valid_host("my-3ds.local"));
	CHECK(ir::is_valid_host("3ds"));
	CHECK(!ir::is_valid_host("192.168.1"));      // looks numeric, isn't a full address
	CHECK(!ir::is_valid_host("999.1.1.1"));
	CHECK(!ir::is_valid_host("1.2.3.4.5"));
	CHECK(!ir::is_valid_host("bad host"));
	CHECK(!ir::is_valid_host("a;b"));
	CHECK(!ir::is_valid_host("a\nb"));
	CHECK(!ir::is_valid_host(".local"));
	CHECK(!ir::is_valid_host(std::string(300, 'a')));
}

int main() {
	test_key_names(); test_request_roundtrip(); test_bad_requests(); test_response_format();
	test_not_running(); test_port_in_use_and_restart(); test_end_to_end(); test_garbage_gets_error_reply();
	test_ir_settings_and_hosts();
	if(failures == 0) printf("ALL CONTROL TESTS PASSED\n"); else printf("%d FAILURE(S)\n", failures);
	return failures ? 1 : 0;
}
