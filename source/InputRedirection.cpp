#include "InputRedirection.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ir {

static const char* const slot_names[SLOT_COUNT] = {
	"a", "b", "x", "y",
	"down", "up", "left", "right",
	"l", "r", "select", "start",
	"zl", "zr",
	"home", "power", "power_long",
	"touch1", "touch2"
};

const char* slot_name(Slot slot) {
	if((slot < 0) || (slot >= SLOT_COUNT))
		return "";
	return slot_names[slot];
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

static PhysicalInput button_input(uint8_t index) {
	PhysicalInput p;
	p.kind = PhysicalInput::BUTTON;
	p.index = index;
	return p;
}

static PhysicalInput axis_input(bool positive, uint8_t index) {
	PhysicalInput p;
	p.kind = positive ? PhysicalInput::AXIS_POS : PhysicalInput::AXIS_NEG;
	p.index = index;
	return p;
}

Config::Config() {
	this->set_defaults();
}

void Config::set_defaults() {
	// Same defaults as the source project (A->A, B->B, ... Home/Power/Touch unbound),
	// expressed using the layout SFML reports for an Xbox-style pad on Linux.
	// Other controllers/OSes may differ; they can be rebound in the config file.
	this->bindings.fill(PhysicalInput());
	this->bindings[SLOT_A] = button_input(0);
	this->bindings[SLOT_B] = button_input(1);
	this->bindings[SLOT_X] = button_input(2);
	this->bindings[SLOT_Y] = button_input(3);
	this->bindings[SLOT_L] = button_input(4);
	this->bindings[SLOT_R] = button_input(5);
	this->bindings[SLOT_SELECT] = button_input(6);
	this->bindings[SLOT_START] = button_input(7);
	this->bindings[SLOT_RIGHT] = axis_input(true, 6);  // PovX
	this->bindings[SLOT_LEFT] = axis_input(false, 6);
	this->bindings[SLOT_UP] = axis_input(true, 7);     // PovY
	this->bindings[SLOT_DOWN] = axis_input(false, 7);
	this->bindings[SLOT_ZL] = axis_input(true, 2);     // Z (left trigger)
	this->bindings[SLOT_ZR] = axis_input(true, 3);     // R (right trigger)
	this->invert_y = false;
	this->invert_ab = true;
	this->invert_xy = true;
	this->touch1_x = this->touch1_y = this->touch2_x = this->touch2_y = 0;
	this->deadzone = 0.15f;
	this->touch_offset_x = this->touch_offset_y = 0;
	this->left_x_axis = 0;
	this->left_y_axis = 1;
	this->right_x_axis = 4;
	this->right_y_axis = 5;
	// enabled, ip and touch_enabled are deliberately left alone: they are not "mappings".
}

std::string describe_input(const PhysicalInput &input) {
	switch(input.kind) {
		case PhysicalInput::BUTTON:
			return "button:" + std::to_string((int)input.index);
		case PhysicalInput::AXIS_POS:
			return "axis+:" + std::to_string((int)input.index);
		case PhysicalInput::AXIS_NEG:
			return "axis-:" + std::to_string((int)input.index);
		default:
			return "none";
	}
}

static bool parse_input(const std::string &value, PhysicalInput &out) {
	if(value == "none") {
		out = PhysicalInput();
		return true;
	}
	size_t colon = value.find(':');
	if(colon == std::string::npos)
		return false;
	std::string kind = value.substr(0, colon);
	int index = 0;
	try {
		index = std::stoi(value.substr(colon + 1));
	}
	catch(...) {
		return false;
	}
	if((index < 0) || (index > 255))
		return false;
	if(kind == "button" && index < MAX_JOY_BUTTONS)
		out = button_input((uint8_t)index);
	else if(kind == "axis+" && index < NUM_JOY_AXES)
		out = axis_input(true, (uint8_t)index);
	else if(kind == "axis-" && index < NUM_JOY_AXES)
		out = axis_input(false, (uint8_t)index);
	else
		return false;
	return true;
}

static bool parse_int(const std::string &value, int &out, int min_v, int max_v) {
	try {
		int v = std::stoi(value);
		out = std::min(std::max(v, min_v), max_v);
		return true;
	}
	catch(...) {
		return false;
	}
}

static bool parse_float(const std::string &value, float &out, float min_v, float max_v) {
	try {
		float v = std::stof(value);
		if(std::isnan(v))
			return false;
		out = std::min(std::max(v, min_v), max_v);
		return true;
	}
	catch(...) {
		return false;
	}
}

int save_config(const std::string &path, const Config &config) {
	std::ofstream file(path);
	if(!file.good())
		return 1;
	file << "enabled=" << config.enabled << std::endl;
	file << "ip=" << config.ip << std::endl;
	file << "touch_enabled=" << config.touch_enabled << std::endl;
	file << "invert_y=" << config.invert_y << std::endl;
	file << "invert_ab=" << config.invert_ab << std::endl;
	file << "invert_xy=" << config.invert_xy << std::endl;
	file << "deadzone=" << config.deadzone << std::endl;
	file << "touch_offset_x=" << config.touch_offset_x << std::endl;
	file << "touch_offset_y=" << config.touch_offset_y << std::endl;
	file << "touch1_x=" << config.touch1_x << std::endl;
	file << "touch1_y=" << config.touch1_y << std::endl;
	file << "touch2_x=" << config.touch2_x << std::endl;
	file << "touch2_y=" << config.touch2_y << std::endl;
	file << "left_x_axis=" << config.left_x_axis << std::endl;
	file << "left_y_axis=" << config.left_y_axis << std::endl;
	file << "right_x_axis=" << config.right_x_axis << std::endl;
	file << "right_y_axis=" << config.right_y_axis << std::endl;
	for(int i = 0; i < SLOT_COUNT; i++)
		file << "bind_" << slot_names[i] << "=" << describe_input(config.bindings[i]) << std::endl;
	return file.good() ? 0 : 1;
}

int load_config(const std::string &path, Config &config) {
	std::ifstream file(path);
	if(!file.good())
		return 1;
	std::string line;
	while(std::getline(file, line)) {
		if(!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t eq = line.find('=');
		if(eq == std::string::npos)
			continue;
		std::string key = line.substr(0, eq);
		std::string value = line.substr(eq + 1);
		int i = 0;
		if(key == "enabled") { if(parse_int(value, i, 0, 1)) config.enabled = i; }
		else if(key == "ip") config.ip = value;
		else if(key == "touch_enabled") { if(parse_int(value, i, 0, 1)) config.touch_enabled = i; }
		else if(key == "invert_y") { if(parse_int(value, i, 0, 1)) config.invert_y = i; }
		else if(key == "invert_ab") { if(parse_int(value, i, 0, 1)) config.invert_ab = i; }
		else if(key == "invert_xy") { if(parse_int(value, i, 0, 1)) config.invert_xy = i; }
		else if(key == "deadzone") parse_float(value, config.deadzone, 0.0f, 0.95f);
		else if(key == "touch_offset_x") parse_int(value, config.touch_offset_x, -TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_WIDTH);
		else if(key == "touch_offset_y") parse_int(value, config.touch_offset_y, -TOUCH_SCREEN_HEIGHT, TOUCH_SCREEN_HEIGHT);
		else if(key == "touch1_x") parse_int(value, config.touch1_x, 0, TOUCH_SCREEN_WIDTH);
		else if(key == "touch1_y") parse_int(value, config.touch1_y, 0, TOUCH_SCREEN_HEIGHT);
		else if(key == "touch2_x") parse_int(value, config.touch2_x, 0, TOUCH_SCREEN_WIDTH);
		else if(key == "touch2_y") parse_int(value, config.touch2_y, 0, TOUCH_SCREEN_HEIGHT);
		else if(key == "left_x_axis") parse_int(value, config.left_x_axis, 0, NUM_JOY_AXES - 1);
		else if(key == "left_y_axis") parse_int(value, config.left_y_axis, 0, NUM_JOY_AXES - 1);
		else if(key == "right_x_axis") parse_int(value, config.right_x_axis, 0, NUM_JOY_AXES - 1);
		else if(key == "right_y_axis") parse_int(value, config.right_y_axis, 0, NUM_JOY_AXES - 1);
		else if(key.compare(0, 5, "bind_") == 0) {
			std::string name = key.substr(5);
			for(int s = 0; s < SLOT_COUNT; s++) {
				if(name == slot_names[s]) {
					PhysicalInput p;
					if(parse_input(value, p))
						config.bindings[s] = p;
					break;
				}
			}
		}
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Input resolution and protocol (direct port of domain/{input_mapper,protocol}.rs)
// ---------------------------------------------------------------------------

static bool input_active(const PhysicalInput &input, const JoystickSnapshot &snap) {
	switch(input.kind) {
		case PhysicalInput::BUTTON:
			return (input.index < MAX_JOY_BUTTONS) && snap.buttons[input.index];
		case PhysicalInput::AXIS_POS:
			return (input.index < NUM_JOY_AXES) && (snap.axes[input.index] > 0.5f);
		case PhysicalInput::AXIS_NEG:
			return (input.index < NUM_JOY_AXES) && (snap.axes[input.index] < -0.5f);
		default:
			return false;
	}
}

float apply_deadzone(float &x, float &y, float deadzone) {
	float mag = std::sqrt((x * x) + (y * y));
	if(mag <= deadzone || mag <= 0.0f) {
		x = 0.0f;
		y = 0.0f;
		return 0.0f;
	}
	float scaled = std::min((mag - deadzone) / (1.0f - deadzone), 1.0f);
	x = (x / mag) * scaled;
	y = (y / mag) * scaled;
	return scaled;
}

RawState resolve_joystick(const JoystickSnapshot &snapshot, const Config &config) {
	RawState raw;
	if(!snapshot.any_connected)
		return raw;
	for(int i = 0; i < SLOT_COUNT; i++)
		raw.active[i] = input_active(config.bindings[i], snapshot);
	// SFML: down/right are positive. The 3DS (and the source's protocol code) want up to be positive.
	raw.lx = snapshot.axes[std::min(std::max(config.left_x_axis, 0), NUM_JOY_AXES - 1)];
	raw.ly = -snapshot.axes[std::min(std::max(config.left_y_axis, 0), NUM_JOY_AXES - 1)];
	raw.rx = snapshot.axes[std::min(std::max(config.right_x_axis, 0), NUM_JOY_AXES - 1)];
	raw.ry = -snapshot.axes[std::min(std::max(config.right_y_axis, 0), NUM_JOY_AXES - 1)];
	apply_deadzone(raw.lx, raw.ly, config.deadzone);
	apply_deadzone(raw.rx, raw.ry, config.deadzone);
	return raw;
}

// Active-low: pressed = 0, released = 1.
// Bit layout: 0=A, 1=B, 2=Select, 3=Start, 4=Right, 5=Left, 6=Up, 7=Down, 8=R, 9=L, 10=X, 11=Y
uint32_t map_buttons(const RawState &raw, const Config &config) {
	uint32_t hid_pad = 0xFFF;
	const Slot a_src = config.invert_ab ? SLOT_B : SLOT_A;
	const Slot b_src = config.invert_ab ? SLOT_A : SLOT_B;
	const Slot x_src = config.invert_xy ? SLOT_Y : SLOT_X;
	const Slot y_src = config.invert_xy ? SLOT_X : SLOT_Y;
	const struct { Slot src; int bit; } table[] = {
		{a_src, 0}, {b_src, 1},
		{SLOT_SELECT, 2}, {SLOT_START, 3}, {SLOT_RIGHT, 4}, {SLOT_LEFT, 5},
		{SLOT_UP, 6}, {SLOT_DOWN, 7}, {SLOT_R, 8}, {SLOT_L, 9},
		{x_src, 10}, {y_src, 11}
	};
	for(const auto &entry : table)
		if(raw.active[entry.src])
			hid_pad &= ~(1u << entry.bit);
	return hid_pad;
}

uint32_t map_ir_buttons(const RawState &raw) {
	uint32_t state = 0;
	if(raw.active[SLOT_ZR])
		state |= 1 << 1;
	if(raw.active[SLOT_ZL])
		state |= 1 << 2;
	return state;
}

uint32_t map_interface_buttons(const RawState &raw) {
	uint32_t state = 0;
	if(raw.active[SLOT_HOME])
		state |= 1;
	if(raw.active[SLOT_POWER])
		state |= 2;
	if(raw.active[SLOT_POWER_LONG])
		state |= 4;
	return state;
}

static uint32_t clamp_bits(double raw, double original, uint32_t limit) {
	// Matches Rust's saturating `as u32` (negatives and NaN become 0).
	uint32_t v = (std::isnan(raw) || raw < 0.0) ? 0 : (raw >= 4294967295.0 ? 0xFFFFFFFFu : (uint32_t)raw);
	if(v >= limit)
		return (original < 0.0) ? 0x000 : limit;
	return v;
}

// X in bits 0-11, Y in bits 12-23. Center = 0x800.
uint32_t encode_circle_pad(float x, float y, bool invert_y) {
	const double CPAD_BOUND = 0x5D0;
	double lx = x;
	double ly = invert_y ? -(double)y : (double)y;
	if((lx == 0.0) && (ly == 0.0))
		return 0x7FF7FF;
	uint32_t ux = clamp_bits((lx * CPAD_BOUND) + 0x800, lx, 0xFFF);
	uint32_t uy = clamp_bits((ly * CPAD_BOUND) + 0x800, ly, 0xFFF);
	return (uy << 12) | ux;
}

// C-stick with 45 degree rotation ("thanks, Nintendo"), IR buttons and the 0x81 magic byte.
uint32_t encode_cpp(float x, float y, bool invert_y, uint32_t ir_buttons) {
	const double CPP_BOUND = 0x7F;
	const double SQRT1_2 = 0.70710678118654752440;
	double rx = x;
	double ry = invert_y ? -(double)y : (double)y;
	if((rx == 0.0) && (ry == 0.0) && (ir_buttons == 0))
		return 0x80800081;
	uint32_t ux = clamp_bits((SQRT1_2 * (rx + ry) * CPP_BOUND) + 0x80, rx, 0xFF);
	uint32_t uy = clamp_bits((SQRT1_2 * (ry - rx) * CPP_BOUND) + 0x80, ry, 0xFF);
	return (uy << 24) | (ux << 16) | (ir_buttons << 8) | 0x81;
}

// Bit 24 = pressed, bits 12-23 = Y (12-bit), bits 0-11 = X (12-bit).
uint32_t encode_touch(int x, int y, bool pressed) {
	if(!pressed)
		return 0x2000000;
	uint32_t cx = (uint32_t)std::min(std::max(x, 0), TOUCH_SCREEN_WIDTH);
	uint32_t cy = (uint32_t)std::min(std::max(y, 0), TOUCH_SCREEN_HEIGHT);
	uint32_t tx = (0xFFFu * cx) / TOUCH_SCREEN_WIDTH;
	uint32_t ty = (0xFFFu * cy) / TOUCH_SCREEN_HEIGHT;
	return (1u << 24) | (ty << 12) | tx;
}

static void put_le32(uint8_t *dst, uint32_t v) {
	dst[0] = (uint8_t)(v & 0xFF);
	dst[1] = (uint8_t)((v >> 8) & 0xFF);
	dst[2] = (uint8_t)((v >> 16) & 0xFF);
	dst[3] = (uint8_t)((v >> 24) & 0xFF);
}

void encode_frame(uint8_t out[FRAME_SIZE], const RawState &raw, const TouchState &pointer_touch, uint32_t ui_interface_bits, const Config &config) {
	TouchState touch = pointer_touch;
	touch.x += config.touch_offset_x;
	touch.y += config.touch_offset_y;
	// Gamepad-triggered presets win over the pointer, like in the source project.
	if(raw.active[SLOT_TOUCH1]) {
		touch = {true, config.touch1_x, config.touch1_y};
	}
	else if(raw.active[SLOT_TOUCH2]) {
		touch = {true, config.touch2_x, config.touch2_y};
	}
	uint32_t hid_pad = map_buttons(raw, config);
	uint32_t touch_screen = encode_touch(touch.x, touch.y, touch.pressed);
	uint32_t circle_pad = encode_circle_pad(raw.lx, raw.ly, config.invert_y);
	uint32_t cpp = encode_cpp(raw.rx, raw.ry, config.invert_y, map_ir_buttons(raw));
	uint32_t interface_buttons = map_interface_buttons(raw) | ui_interface_bits;
	put_le32(out + 0, hid_pad);
	put_le32(out + 4, touch_screen);
	put_le32(out + 8, circle_pad);
	put_le32(out + 12, cpp);
	put_le32(out + 16, interface_buttons);
}

// ---------------------------------------------------------------------------
// Networking
// ---------------------------------------------------------------------------

#ifdef _WIN32
typedef SOCKET socket_t;
static const socket_t INVALID_SOCK = INVALID_SOCKET;
static void close_socket(socket_t s) { closesocket(s); }
static void ensure_winsock() {
	static bool done = false;
	if(!done) {
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);
		done = true;
	}
}
#else
typedef int socket_t;
static const socket_t INVALID_SOCK = -1;
static void close_socket(socket_t s) { ::close(s); }
static void ensure_winsock() {}
#endif

struct UdpSender::Impl {
	socket_t sock = INVALID_SOCK;
	sockaddr_in addr;
};

UdpSender::UdpSender() : impl(new Impl) {
	memset(&this->impl->addr, 0, sizeof(this->impl->addr));
}

UdpSender::~UdpSender() {
	this->close();
	delete this->impl;
}

bool UdpSender::open(const std::string &host) {
	this->close();
	ensure_winsock();
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // The 3DS only does IPv4
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo *result = NULL;
	if((getaddrinfo(host.c_str(), NULL, &hints, &result) != 0) || (result == NULL))
		return false;
	memcpy(&this->impl->addr, result->ai_addr, sizeof(sockaddr_in));
	freeaddrinfo(result);
	this->impl->addr.sin_family = AF_INET;
	this->impl->addr.sin_port = htons(UDP_PORT);
	this->impl->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	return this->impl->sock != INVALID_SOCK;
}

bool UdpSender::is_open() const {
	return this->impl->sock != INVALID_SOCK;
}

bool UdpSender::send_frame(const uint8_t frame[FRAME_SIZE]) {
	if(!this->is_open())
		return false;
	int sent = (int)sendto(this->impl->sock, (const char*)frame, FRAME_SIZE, 0, (const sockaddr*)&this->impl->addr, sizeof(this->impl->addr));
	return sent == FRAME_SIZE;
}

void UdpSender::close() {
	if(this->impl->sock != INVALID_SOCK)
		close_socket(this->impl->sock);
	this->impl->sock = INVALID_SOCK;
}

// ---------------------------------------------------------------------------
// Manager: owns the 20 Hz sender thread and the reconnection logic
// ---------------------------------------------------------------------------

static const std::chrono::milliseconds TICK_PERIOD(50); // 20 Hz, same as the source project
static const int RETRY_MIN_MS = 500;
static const int RETRY_MAX_MS = 5000;

Manager::Manager() {
}

Manager::~Manager() {
	this->stop();
}

void Manager::start() {
	std::lock_guard<std::mutex> lock(this->mtx);
	if(this->running)
		return;
	this->running = true;
	this->worker = std::thread(&Manager::thread_main, this);
}

void Manager::stop() {
	{
		std::lock_guard<std::mutex> lock(this->mtx);
		if(!this->running)
			return;
		this->running = false;
	}
	this->cv.notify_all();
	if(this->worker.joinable())
		this->worker.join();
}

Config Manager::get_config() const {
	std::lock_guard<std::mutex> lock(this->mtx);
	return this->config;
}

void Manager::set_config(const Config &new_config) {
	std::lock_guard<std::mutex> lock(this->mtx);
	this->config = new_config;
	this->touch_redirect_active = new_config.enabled && new_config.touch_enabled;
	if(!this->touch_redirect_active)
		this->touch.pressed = false;
}

int Manager::load(const std::string &path) {
	Config loaded = this->get_config();
	int ret = load_config(path, loaded);
	if(ret == 0)
		this->set_config(loaded);
	return ret;
}

int Manager::save(const std::string &path) const {
	return save_config(path, this->get_config());
}

void Manager::update_joystick(const JoystickSnapshot &snapshot) {
	std::lock_guard<std::mutex> lock(this->mtx);
	this->joy = snapshot;
}

void Manager::get_last_frame(uint8_t out[FRAME_SIZE]) const {
	std::lock_guard<std::mutex> lock(this->mtx);
	memcpy(out, this->last_frame, FRAME_SIZE);
}

JoystickSnapshot Manager::get_joystick_snapshot() const {
	std::lock_guard<std::mutex> lock(this->mtx);
	return this->joy;
}

void Manager::set_touch(int x, int y) {
	std::lock_guard<std::mutex> lock(this->mtx);
	this->touch = {true, x, y};
}

void Manager::release_touch() {
	std::lock_guard<std::mutex> lock(this->mtx);
	this->touch.pressed = false;
}

bool Manager::is_touch_redirect_active() const {
	return this->touch_redirect_active;
}

void Manager::set_interface_button(int bit, bool pressed) {
	if((bit < 0) || (bit > 2))
		return;
	std::lock_guard<std::mutex> lock(this->mtx);
	if(pressed)
		this->ui_interface_bits |= (1u << bit);
	else
		this->ui_interface_bits &= ~(1u << bit);
}

Status Manager::get_status() const {
	std::lock_guard<std::mutex> lock(this->mtx);
	return this->status;
}

std::string Manager::status_text(Status status, const std::string &ip) {
	switch(status) {
		case Status::DISABLED:
			return "Input redirection: off";
		case Status::NO_HOST_SET:
			return "Input redirection: no 3DS address set";
		case Status::SENDING:
			return "Input redirection: sending to " + ip;
		case Status::RETRYING:
			return "Input redirection: can't reach " + ip + ", retrying...";
	}
	return "";
}

bool Manager::consume_status_change(Status &new_status, std::string &text) {
	std::lock_guard<std::mutex> lock(this->mtx);
	if(this->status == this->last_reported_status)
		return false;
	this->last_reported_status = this->status;
	new_status = this->status;
	text = status_text(this->status, this->config.ip);
	return true;
}

void Manager::build_frame(uint8_t out[FRAME_SIZE], bool neutral) {
	// Caller holds mtx
	if(neutral) {
		encode_frame(out, RawState(), TouchState(), 0, this->config);
		return;
	}
	RawState raw = resolve_joystick(this->joy, this->config);
	encode_frame(out, raw, this->touch, this->ui_interface_bits, this->config);
	memcpy(this->last_frame, out, FRAME_SIZE);
}

void Manager::thread_main() {
	UdpSender sender;
	std::string opened_host;
	bool was_sending = false;
	int backoff_ms = RETRY_MIN_MS;
	auto next_retry = std::chrono::steady_clock::now();

	std::unique_lock<std::mutex> lock(this->mtx);
	while(this->running) {
		const Config cfg = this->config;
		uint8_t frame[FRAME_SIZE];
		uint8_t neutral_frame[FRAME_SIZE];
		this->build_frame(frame, false);
		this->build_frame(neutral_frame, true);
		lock.unlock();

		Status new_status;
		if(!cfg.enabled)
			new_status = Status::DISABLED;
		else if(cfg.ip.empty())
			new_status = Status::NO_HOST_SET;
		else
			new_status = Status::SENDING;

		if(new_status == Status::SENDING) {
			if(sender.is_open() && (opened_host != cfg.ip)) {
				// Address changed in the settings: start over with the new one
				sender.close();
				backoff_ms = RETRY_MIN_MS;
				next_retry = std::chrono::steady_clock::now();
			}
			if(!sender.is_open()) {
				if(std::chrono::steady_clock::now() >= next_retry) {
					// (Re)resolves the host every time, so a changed DHCP lease behind a hostname is picked up.
					if(sender.open(cfg.ip)) {
						opened_host = cfg.ip;
					}
					else {
						next_retry = std::chrono::steady_clock::now() + std::chrono::milliseconds(backoff_ms);
						backoff_ms = std::min(backoff_ms * 2, RETRY_MAX_MS);
					}
				}
			}
			if(sender.is_open()) {
				if(sender.send_frame(frame)) {
					was_sending = true;
					backoff_ms = RETRY_MIN_MS;
				}
				else {
					// Network went away, interface changed, etc. Drop the socket and rebuild it.
					sender.close();
					next_retry = std::chrono::steady_clock::now() + std::chrono::milliseconds(backoff_ms);
					backoff_ms = std::min(backoff_ms * 2, RETRY_MAX_MS);
					new_status = Status::RETRYING;
				}
			}
			else
				new_status = Status::RETRYING;
		}
		else {
			if(was_sending && sender.is_open())
				sender.send_frame(neutral_frame); // Don't leave buttons held on the 3DS
			was_sending = false;
			sender.close();
			backoff_ms = RETRY_MIN_MS;
		}

		lock.lock();
		this->status = new_status;
		this->cv.wait_for(lock, TICK_PERIOD, [this]{ return !this->running; });
	}

	// Shutting down: release everything on the 3DS side
	if(was_sending && sender.is_open()) {
		uint8_t neutral_frame[FRAME_SIZE];
		this->build_frame(neutral_frame, true);
		lock.unlock();
		sender.send_frame(neutral_frame);
		lock.lock();
	}
	this->status = Status::DISABLED;
}

}
