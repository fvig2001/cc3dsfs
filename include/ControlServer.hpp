#ifndef __CONTROLSERVER_HPP
#define __CONTROLSERVER_HPP

// Lets another program change settings of a running cc3dsfs:
//   cc3dsfs --update_settings bot_fullscreen=0 bot_have_titlebar=1 ir_ip=192.168.1.50
//
// The running instance listens on a UDP port of the loopback interface (127.0.0.1 only,
// so nothing outside this computer can reach it). It is checked once per frame from the
// main loop, without blocking, so the settings are always changed from the same thread
// which already does it for menus and profile loading.
//
// Wire format, so that scripts can talk to it directly without calling the executable.
// Request: one UDP datagram of text lines:
//     CC3DSFS-UPDATE-SETTINGS 1
//     bot_fullscreen=0
//     ir_ip=192.168.1.50
// Reply: one line for each setting, then a summary:
//     OK bot_fullscreen
//     ERROR ir_ip: ...
//     DONE applied=1 failed=1
//
// This header does not depend on SFML.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ctl {

constexpr uint16_t DEFAULT_PORT = 47951;
constexpr size_t MAX_DATAGRAM_SIZE = 4096;
constexpr size_t MAX_SETTINGS_PER_REQUEST = 128;
constexpr size_t MAX_VALUE_LENGTH = 512;
extern const char* const REQUEST_HEADER;

struct Setting {
	std::string key;
	std::string value;
};

struct Result {
	std::string key;
	bool ok;
	std::string message; // Why it failed (empty if ok)
};

// Letters, digits and underscores only, up to 64 characters
bool is_valid_key_name(const std::string &key);

// Client side: "key=value" arguments -> request text
bool build_request(const std::vector<std::string> &assignments, std::string &request, std::string &error);
// Server side: request text -> settings. Rejects the whole request if any line is malformed.
bool parse_request(const std::string &request, std::vector<Setting> &settings, std::string &error);
std::string format_response(const std::vector<Result> &results);
// Number of failed settings in a reply, or -1 if the text isn't a valid reply
int count_failures(const std::string &response);

class Server {
public:
	Server();
	~Server();
	// Listens on 127.0.0.1:port. Returns false (and says why) e.g. if another instance already uses the port.
	bool start(uint16_t port, std::string &error);
	void stop();
	bool is_running() const;

	typedef std::function<std::vector<Result>(const std::vector<Setting>&)> Handler;
	// Handles the requests which have arrived, replying to each. Never blocks.
	// Returns how many requests were handled.
	int poll(const Handler &handler);
private:
	struct Impl;
	Impl *impl;
};

enum ClientExitCode {
	CLIENT_OK = 0,           // Everything was applied
	CLIENT_SOME_FAILED = 1,  // cc3dsfs answered, but refused some settings
	CLIENT_NOT_RUNNING = 2,  // No answer: not running, or a different port
	CLIENT_BAD_USAGE = 3     // The arguments or the socket were unusable
};

// Sends the settings to a running cc3dsfs and waits for its answer.
// output receives what should be shown to the user.
int send_update(uint16_t port, const std::vector<std::string> &assignments, int timeout_ms, std::string &output);

}

#endif
