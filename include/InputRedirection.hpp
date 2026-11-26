#ifndef __INPUTREDIRECTION_HPP
#define __INPUTREDIRECTION_HPP

// 3DS InputRedirection client (https://github.com/TuxSH/InputRedirection).
// Port of the protocol/mapping logic of jojo8356/InputRedirectionClient (Rust).
// This header and InputRedirection.cpp deliberately do not depend on SFML,
// so the logic can be unit tested on its own. The SFML glue lives in
// InputRedirectionSFML.cpp.

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace ir {

constexpr uint16_t UDP_PORT = 4950;
constexpr int TOUCH_SCREEN_WIDTH = 320;
constexpr int TOUCH_SCREEN_HEIGHT = 240;
constexpr int FRAME_SIZE = 20;
constexpr int MAX_JOY_BUTTONS = 32;
constexpr int NUM_JOY_AXES = 8; // Same order as sf::Joystick::Axis: X, Y, Z, R, U, V, PovX, PovY

// The functions which can be bound. Same set as the source project (19).
enum Slot {
	SLOT_A, SLOT_B, SLOT_X, SLOT_Y,
	SLOT_UP, SLOT_DOWN, SLOT_LEFT, SLOT_RIGHT,
	SLOT_L, SLOT_R, SLOT_SELECT, SLOT_START,
	SLOT_ZL, SLOT_ZR,
	SLOT_HOME, SLOT_POWER, SLOT_POWER_LONG,
	SLOT_TOUCH1, SLOT_TOUCH2,
	SLOT_COUNT
};

const char* slot_name(Slot slot);

// What physical control triggers a slot. SFML has no standard gamepad
// layout, so a binding is either a button index or one direction of an axis.
struct PhysicalInput {
	enum Kind : uint8_t { NONE = 0, BUTTON = 1, AXIS_POS = 2, AXIS_NEG = 3 };
	Kind kind = NONE;
	uint8_t index = 0;
	bool operator==(const PhysicalInput &o) const { return (kind == o.kind) && (index == o.index); }
};

struct Config {
	bool enabled = false;
	std::string ip;
	bool touch_enabled = true;
	// Settings carried over from the source project
	bool invert_y = false;
	bool invert_ab = false;
	bool invert_xy = false;
	std::array<PhysicalInput, SLOT_COUNT> bindings;
	int touch1_x = 0, touch1_y = 0; // Preset positions triggered by SLOT_TOUCH1/2
	int touch2_x = 0, touch2_y = 0;
	// Additions (not in the source project, see notes in the README)
	float deadzone = 0.15f;         // Needed since SFML reports raw axis values
	int touch_offset_x = 0;         // Calibration, applied to pointer-based touch only
	int touch_offset_y = 0;
	// Which joystick axes are the sticks. Defaults assume the Linux Xbox layout.
	int left_x_axis = 0, left_y_axis = 1, right_x_axis = 4, right_y_axis = 5;

	Config();
	void set_defaults();
};

// Values for each 3DS function and stick, already resolved from the physical inputs.
// Sticks are in [-1, 1], with up/right being positive.
struct RawState {
	std::array<bool, SLOT_COUNT> active{};
	float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
};

struct TouchState {
	bool pressed = false;
	int x = 0, y = 0;
};

// What the joystick backend reports. Merged over all connected joysticks.
struct JoystickSnapshot {
	bool any_connected = false;
	std::array<bool, MAX_JOY_BUTTONS> buttons{};
	std::array<float, NUM_JOY_AXES> axes{}; // -1.0 to 1.0, SFML sign convention
};

// Implemented in InputRedirectionSFML.cpp. Merges every connected SFML joystick.
JoystickSnapshot take_joystick_snapshot();

// ---- Protocol / mapping (pure functions, unit tested) ----
RawState resolve_joystick(const JoystickSnapshot &snapshot, const Config &config);
float apply_deadzone(float &x, float &y, float deadzone);
uint32_t map_buttons(const RawState &raw, const Config &config);
uint32_t map_ir_buttons(const RawState &raw);
uint32_t map_interface_buttons(const RawState &raw);
uint32_t encode_circle_pad(float x, float y, bool invert_y);
uint32_t encode_cpp(float x, float y, bool invert_y, uint32_t ir_buttons);
uint32_t encode_touch(int x, int y, bool pressed);
void encode_frame(uint8_t out[FRAME_SIZE], const RawState &raw, const TouchState &pointer_touch, uint32_t ui_interface_bits, const Config &config);

// ---- Config I/O (key=value lines, same style as the rest of cc3dsfs) ----
// Returns 0 on success, non-zero if the file could not be read/written.
int load_config(const std::string &path, Config &config);
int save_config(const std::string &path, const Config &config);
std::string describe_input(const PhysicalInput &input);

// ---- Networking ----
class UdpSender {
public:
	UdpSender();
	~UdpSender();
	// Resolves the host and creates the socket. Re-resolves every time it is called.
	bool open(const std::string &host);
	bool is_open() const;
	bool send_frame(const uint8_t frame[FRAME_SIZE]);
	void close();
private:
	struct Impl;
	Impl *impl;
};

enum class Status {
	DISABLED,   // Turned off in the settings
	NO_HOST_SET, // Enabled, but no IP/host was set
	SENDING,    // Packets are leaving this machine. UDP gives no confirmation that the 3DS got them.
	RETRYING    // The socket/address failed. Trying to open it again.
};

// Owns the sender thread. Handles automatic reconnection to the 3DS.
class Manager {
public:
	Manager();
	~Manager();
	void start();
	void stop();

	Config get_config() const;
	void set_config(const Config &config);
	int load(const std::string &path);
	int save(const std::string &path) const;

	// Called from the thread(s) which pump the window events.
	void update_joystick(const JoystickSnapshot &snapshot);
	JoystickSnapshot get_joystick_snapshot() const;
	void get_last_frame(uint8_t out[FRAME_SIZE]) const; // Last frame built for sending (debugging)
	// Coordinates are in 3DS bottom screen pixels (0-319, 0-239).
	void set_touch(int x, int y);
	void release_touch();
	bool is_touch_redirect_active() const; // enabled && touch_enabled
	void set_interface_button(int bit, bool pressed);

	Status get_status() const;
	// Returns true (once) if the status changed since the last call, and fills in the new text.
	bool consume_status_change(Status &new_status, std::string &text);
	static std::string status_text(Status status, const std::string &ip);

private:
	void thread_main();
	void build_frame(uint8_t out[FRAME_SIZE], bool neutral);

	mutable std::mutex mtx;
	std::condition_variable cv;
	std::thread worker;
	bool running = false;
	Config config;
	JoystickSnapshot joy;
	TouchState touch;
	uint32_t ui_interface_bits = 0;
	uint8_t last_frame[FRAME_SIZE] = {};
	Status status = Status::DISABLED;
	Status last_reported_status = Status::DISABLED;
	std::atomic<bool> touch_redirect_active{false};
};

}

#endif
