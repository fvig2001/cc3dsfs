#include "ControlServer.hpp"

#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ctl {

const char* const REQUEST_HEADER = "CC3DSFS-UPDATE-SETTINGS 1";

// ---------------------------------------------------------------------------
// Sockets (a few lines of platform differences)
// ---------------------------------------------------------------------------

#ifdef _WIN32
typedef SOCKET socket_t;
static const socket_t INVALID_SOCK = INVALID_SOCKET;
static void close_socket(socket_t s) { closesocket(s); }
static void init_sockets() {
	static bool done = false;
	if(!done) {
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);
		done = true;
	}
}
static bool set_non_blocking(socket_t s) {
	u_long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode) == 0;
}
#else
typedef int socket_t;
static const socket_t INVALID_SOCK = -1;
static void close_socket(socket_t s) { ::close(s); }
static void init_sockets() {}
static bool set_non_blocking(socket_t s) {
	int flags = fcntl(s, F_GETFL, 0);
	return (flags >= 0) && (fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0);
}
#endif

static sockaddr_in loopback_address(uint16_t port) {
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	return addr;
}

// ---------------------------------------------------------------------------
// Text format
// ---------------------------------------------------------------------------

bool is_valid_key_name(const std::string &key) {
	if(key.empty() || (key.size() > 64))
		return false;
	for(char c : key) {
		const bool ok = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_');
		if(!ok)
			return false;
	}
	return true;
}

bool build_request(const std::vector<std::string> &assignments, std::string &request, std::string &error) {
	if(assignments.empty()) {
		error = "No settings were given";
		return false;
	}
	if(assignments.size() > MAX_SETTINGS_PER_REQUEST) {
		error = "Too many settings in one request (maximum " + std::to_string(MAX_SETTINGS_PER_REQUEST) + ")";
		return false;
	}
	request = std::string(REQUEST_HEADER) + "\n";
	for(const std::string &assignment : assignments) {
		const size_t eq = assignment.find('=');
		if(eq == std::string::npos) {
			error = "'" + assignment + "' is not in the form name=value";
			return false;
		}
		const std::string key = assignment.substr(0, eq);
		const std::string value = assignment.substr(eq + 1);
		if(!is_valid_key_name(key)) {
			error = "'" + key + "' is not a valid setting name";
			return false;
		}
		if((value.find('\n') != std::string::npos) || (value.find('\r') != std::string::npos)) {
			error = "The value of '" + key + "' can't contain line breaks";
			return false;
		}
		if(value.size() > MAX_VALUE_LENGTH) {
			error = "The value of '" + key + "' is too long";
			return false;
		}
		request += key + "=" + value + "\n";
	}
	if(request.size() > MAX_DATAGRAM_SIZE) {
		error = "The request is too large";
		return false;
	}
	return true;
}

bool parse_request(const std::string &request, std::vector<Setting> &settings, std::string &error) {
	settings.clear();
	size_t pos = 0;
	int line_number = 0;
	bool header_seen = false;
	while(pos <= request.size()) {
		size_t end = request.find('\n', pos);
		if(end == std::string::npos)
			end = request.size();
		std::string line = request.substr(pos, end - pos);
		pos = end + 1;
		line_number++;
		if(!line.empty() && (line.back() == '\r'))
			line.pop_back();
		if(!header_seen) {
			if(line != REQUEST_HEADER) {
				error = "Not a cc3dsfs settings request";
				return false;
			}
			header_seen = true;
			continue;
		}
		if(line.empty())
			continue;
		const size_t eq = line.find('=');
		if((eq == std::string::npos) || (!is_valid_key_name(line.substr(0, eq)))) {
			error = "Line " + std::to_string(line_number) + " is not in the form name=value";
			return false;
		}
		if(line.size() - eq - 1 > MAX_VALUE_LENGTH) {
			error = "Line " + std::to_string(line_number) + " has a value which is too long";
			return false;
		}
		if(settings.size() >= MAX_SETTINGS_PER_REQUEST) {
			error = "Too many settings in one request (maximum " + std::to_string(MAX_SETTINGS_PER_REQUEST) + ")";
			return false;
		}
		settings.push_back({line.substr(0, eq), line.substr(eq + 1)});
	}
	if(!header_seen) {
		error = "Not a cc3dsfs settings request";
		return false;
	}
	if(settings.empty()) {
		error = "No settings were given";
		return false;
	}
	return true;
}

std::string format_response(const std::vector<Result> &results) {
	std::string out;
	int applied = 0, failed = 0;
	for(const Result &result : results) {
		if(result.ok) {
			out += "OK " + result.key + "\n";
			applied++;
		}
		else {
			out += "ERROR " + result.key + ": " + result.message + "\n";
			failed++;
		}
	}
	out += "DONE applied=" + std::to_string(applied) + " failed=" + std::to_string(failed) + "\n";
	return out;
}

int count_failures(const std::string &response) {
	const std::string marker = "DONE applied=";
	const size_t done_pos = response.rfind(marker);
	if(done_pos == std::string::npos)
		return -1;
	const size_t failed_pos = response.find(" failed=", done_pos);
	if(failed_pos == std::string::npos)
		return -1;
	try {
		return std::stoi(response.substr(failed_pos + 8));
	}
	catch(...) {
		return -1;
	}
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

struct Server::Impl {
	socket_t sock = INVALID_SOCK;
};

Server::Server() : impl(new Impl) {
}

Server::~Server() {
	this->stop();
	delete this->impl;
}

bool Server::start(uint16_t port, std::string &error) {
	this->stop();
	init_sockets();
	socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(s == INVALID_SOCK) {
		error = "Couldn't create the socket";
		return false;
	}
#ifdef _WIN32
	// Without this, another program could bind the same port and steal the requests
	int exclusive = 1;
	setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
#endif
	sockaddr_in addr = loopback_address(port);
	if(bind(s, (const sockaddr*)&addr, sizeof(addr)) != 0) {
		close_socket(s);
		error = "Port " + std::to_string(port) + " is not available (is another cc3dsfs already running?)";
		return false;
	}
	if(!set_non_blocking(s)) {
		close_socket(s);
		error = "Couldn't make the socket non-blocking";
		return false;
	}
	this->impl->sock = s;
	return true;
}

void Server::stop() {
	if(this->impl->sock != INVALID_SOCK)
		close_socket(this->impl->sock);
	this->impl->sock = INVALID_SOCK;
}

bool Server::is_running() const {
	return this->impl->sock != INVALID_SOCK;
}

int Server::poll(const Handler &handler) {
	if(!this->is_running())
		return 0;
	int handled = 0;
	// A limit per call, so a flood of requests can't stall the frame
	for(int i = 0; i < 8; i++) {
		char buffer[MAX_DATAGRAM_SIZE + 1];
		sockaddr_in from;
		socklen_t from_length = sizeof(from);
		int received = (int)recvfrom(this->impl->sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &from_length);
		if(received < 0)
			break; // Nothing (more) to read. (Windows can also report an earlier failed reply here: ignored)
		std::vector<Result> results;
		if((size_t)received > MAX_DATAGRAM_SIZE) {
			results.push_back({"request", false, "The request is too large"});
		}
		else {
			std::vector<Setting> settings;
			std::string error;
			if(parse_request(std::string(buffer, (size_t)received), settings, error))
				results = handler(settings);
			else
				results.push_back({"request", false, error});
		}
		const std::string reply = format_response(results);
		sendto(this->impl->sock, reply.c_str(), (int)reply.size(), 0, (const sockaddr*)&from, from_length);
		handled++;
	}
	return handled;
}

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------

int send_update(uint16_t port, const std::vector<std::string> &assignments, int timeout_ms, std::string &output) {
	if(port == 0) {
		output = "The control port is disabled (port 0)\n";
		return CLIENT_BAD_USAGE;
	}
	std::string request, error;
	if(!build_request(assignments, request, error)) {
		output = error + "\n";
		return CLIENT_BAD_USAGE;
	}
	init_sockets();
	socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(s == INVALID_SOCK) {
		output = "Couldn't create the socket\n";
		return CLIENT_BAD_USAGE;
	}
	// Connecting a UDP socket makes the system report "nobody is listening" right away
	// instead of leaving us to wait for the whole timeout.
	sockaddr_in addr = loopback_address(port);
	if(connect(s, (const sockaddr*)&addr, sizeof(addr)) != 0) {
		close_socket(s);
		output = "Couldn't reach the local control port\n";
		return CLIENT_BAD_USAGE;
	}
	const std::string not_running = "No answer from cc3dsfs on port " + std::to_string(port) +
		". It is either not running, or was started with a different --control_port, or has the control port disabled.\n";
	if(send(s, request.c_str(), (int)request.size(), 0) < 0) {
		close_socket(s);
		output = not_running;
		return CLIENT_NOT_RUNNING;
	}
	fd_set readable;
	FD_ZERO(&readable);
	FD_SET(s, &readable);
	timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;
	const int ready = select((int)s + 1, &readable, NULL, NULL, &timeout);
	if(ready <= 0) {
		close_socket(s);
		output = not_running;
		return CLIENT_NOT_RUNNING;
	}
	std::vector<char> buffer(65536);
	const int received = (int)recv(s, buffer.data(), (int)buffer.size(), 0);
	close_socket(s);
	if(received < 0) {
		output = not_running;
		return CLIENT_NOT_RUNNING; // Normally "connection refused": nothing is listening
	}
	output = std::string(buffer.data(), (size_t)received);
	const int failures = count_failures(output);
	return (failures == 0) ? CLIENT_OK : CLIENT_SOME_FAILED;
}

}
