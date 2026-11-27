// Standalone tests for the input redirection core (no SFML needed).
// Build: g++ -std=c++17 -Iinclude tests/ir_test.cpp source/InputRedirection.cpp -o ir_test -pthread
#include "InputRedirection.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

using namespace ir;

static int failures = 0;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while(0)
#define CHECK_EQ(a, b) do { unsigned long long _a = (a), _b = (b); if(_a != _b) { printf("FAIL line %d: %s == %s (0x%llX vs 0x%llX)\n", __LINE__, #a, #b, _a, _b); failures++; } } while(0)

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// ---- Ports of the Rust tests ----
static void test_default_frame() {
	Config cfg;
	uint8_t f[FRAME_SIZE];
	encode_frame(f, RawState(), TouchState(), 0, cfg);
	CHECK_EQ(rd32(f + 0), 0xFFF);
	CHECK_EQ(rd32(f + 4), 0x2000000);
	CHECK_EQ(rd32(f + 8), 0x7FF7FF);
	CHECK_EQ(rd32(f + 12), 0x80800081);
	CHECK_EQ(rd32(f + 16), 0);
}
static void test_a_clears_bit0() {
	Config cfg; cfg.invert_ab = false; // Explicit: don't depend on whatever the default is
	RawState raw; raw.active[SLOT_A] = true;
	CHECK_EQ(map_buttons(raw, cfg), 0xFFF & ~1u);
}
static void test_ab_swap() {
	Config cfg; cfg.invert_ab = true; RawState raw; raw.active[SLOT_A] = true;
	CHECK_EQ(map_buttons(raw, cfg), 0xFFF & ~(1u << 1));
}
static void test_xy_swap() {
	Config cfg; cfg.invert_xy = true; RawState raw; raw.active[SLOT_X] = true;
	CHECK_EQ(map_buttons(raw, cfg), 0xFFF & ~(1u << 11));
}
static void test_ir_buttons() {
	RawState raw; raw.active[SLOT_ZR] = true; raw.active[SLOT_ZL] = true;
	CHECK_EQ(map_ir_buttons(raw), 0x6);
}
static void test_touch_encoding() {
	uint32_t t = encode_touch(160, 120, true);
	CHECK(t & (1u << 24));
	CHECK_EQ(t & 0xFFF, (0xFFFu * 160) / 320);
	CHECK_EQ((t >> 12) & 0xFFF, (0xFFFu * 120) / 240);
	CHECK_EQ(encode_touch(5, 5, false), 0x2000000);
	CHECK_EQ(encode_touch(9999, 9999, true) & 0xFFF, 0xFFF); // Clamped like the source
}
static void test_circle_pad_full_right() {
	uint32_t r = encode_circle_pad(1.0f, 0.0f, false);
	CHECK_EQ(r & 0xFFF, 0xDD0);
	CHECK_EQ((r >> 12) & 0xFFF, 0x800);
}
static void test_cpp_ir() {
	uint32_t r = encode_cpp(0, 0, false, 0x6);
	CHECK_EQ(r & 0xFF, 0x81);
	CHECK_EQ((r >> 8) & 0xFF, 0x6);
}

// ---- Tests for what is new / differs ----
static void test_stick_up_is_high_y() {
	// SFML reports "up" as a negative Y. The 3DS wants up to be above center (0x800).
	Config cfg; JoystickSnapshot snap; snap.any_connected = true;
	snap.axes[1] = -1.0f;
	RawState raw = resolve_joystick(snap, cfg);
	CHECK(raw.ly > 0.9f);
	uint32_t r = encode_circle_pad(raw.lx, raw.ly, cfg.invert_y);
	CHECK(((r >> 12) & 0xFFF) > 0x800);
	// ...and the invert Y option flips it
	r = encode_circle_pad(raw.lx, raw.ly, true);
	CHECK(((r >> 12) & 0xFFF) < 0x800);
}
static void test_deadzone() {
	Config cfg; cfg.deadzone = 0.2f; JoystickSnapshot snap; snap.any_connected = true;
	snap.axes[0] = 0.1f; snap.axes[1] = 0.1f; // inside
	RawState raw = resolve_joystick(snap, cfg);
	CHECK(raw.lx == 0.0f && raw.ly == 0.0f);
	snap.axes[0] = 1.0f; snap.axes[1] = 0.0f; // full deflection must still reach full range
	raw = resolve_joystick(snap, cfg);
	CHECK(raw.lx > 0.999f);
}
static void test_no_joystick() {
	Config cfg; JoystickSnapshot snap; // not connected, buttons/axes zero
	RawState raw = resolve_joystick(snap, cfg);
	CHECK_EQ(map_buttons(raw, cfg), 0xFFF);
}
static void test_default_bindings() {
	Config cfg; JoystickSnapshot snap; snap.any_connected = true;
	snap.buttons[0] = true;      // A
	snap.axes[7] = 1.0f;         // PovY +  -> Up
	snap.axes[2] = 1.0f;         // Z  -> ZL
	RawState raw = resolve_joystick(snap, cfg);
	CHECK(raw.active[SLOT_A] && raw.active[SLOT_UP] && raw.active[SLOT_ZL]);
	CHECK(!raw.active[SLOT_B] && !raw.active[SLOT_DOWN] && !raw.active[SLOT_ZR]);
	snap.axes[2] = -1.0f;        // Trigger at rest on Linux reads -100
	raw = resolve_joystick(snap, cfg);
	CHECK(!raw.active[SLOT_ZL]);
}
static void test_touch_offset_and_presets() {
	Config cfg; cfg.touch_offset_x = 10; cfg.touch_offset_y = -5;
	cfg.touch1_x = 200; cfg.touch1_y = 100;
	cfg.bindings[SLOT_TOUCH1] = PhysicalInput{PhysicalInput::BUTTON, 9};
	uint8_t f[FRAME_SIZE];
	TouchState pointer{true, 100, 100};
	encode_frame(f, RawState(), pointer, 0, cfg);
	CHECK_EQ(rd32(f + 4), encode_touch(110, 95, true));
	RawState raw; raw.active[SLOT_TOUCH1] = true;
	encode_frame(f, raw, pointer, 0, cfg);
	CHECK_EQ(rd32(f + 4), encode_touch(200, 100, true)); // preset wins, offset not applied
}
static void test_config_roundtrip() {
	Config a;
	a.enabled = true; a.ip = "192.168.1.50"; a.invert_y = true; a.invert_ab = true; a.invert_xy = true;
	a.deadzone = 0.3f; a.touch_offset_x = -7; a.touch_offset_y = 12; a.touch1_x = 33; a.touch2_y = 44;
	a.right_x_axis = 3; a.right_y_axis = 4;
	a.bindings[SLOT_HOME] = PhysicalInput{PhysicalInput::BUTTON, 8};
	a.bindings[SLOT_A] = PhysicalInput{PhysicalInput::AXIS_NEG, 5};
	a.bindings[SLOT_B] = PhysicalInput();
	CHECK_EQ(save_config("/tmp/ir_test.cfg", a), 0);
	Config b; b.bindings.fill(PhysicalInput());
	CHECK_EQ(load_config("/tmp/ir_test.cfg", b), 0);
	CHECK(b.enabled && b.ip == "192.168.1.50" && b.invert_y && b.invert_ab && b.invert_xy);
	CHECK(b.deadzone > 0.299f && b.deadzone < 0.301f);
	CHECK(b.touch_offset_x == -7 && b.touch_offset_y == 12 && b.touch1_x == 33 && b.touch2_y == 44);
	CHECK(b.right_x_axis == 3 && b.right_y_axis == 4);
	for(int i = 0; i < SLOT_COUNT; i++)
		CHECK(a.bindings[i] == b.bindings[i]);
	CHECK(load_config("/tmp/does_not_exist.cfg", b) != 0);
}
static void test_config_garbage() {
	FILE *f = fopen("/tmp/ir_garbage.cfg", "w");
	fputs("deadzone=abc\nbind_a=button:99\nbind_b=axis+:notanumber\nnoequalsign\ntouch1_x=99999\nip=1.2.3.4\r\n", f);
	fclose(f);
	Config c;
	CHECK_EQ(load_config("/tmp/ir_garbage.cfg", c), 0);
	CHECK(c.bindings[SLOT_A] == Config().bindings[SLOT_A]); // Out of range index rejected, default kept
	CHECK(c.bindings[SLOT_B] == Config().bindings[SLOT_B]);
	CHECK_EQ(c.touch1_x, 320);                              // Clamped
	CHECK(c.ip == "1.2.3.4");                               // CRLF tolerated
}

// ---- IPv4 helpers and autosave (used by the Input Redirection menus) ----
static void test_parse_ipv4_valid() {
	int o[4] = {-1, -1, -1, -1};
	CHECK(parse_ipv4("192.168.85.200", o));
	CHECK(o[0] == 192 && o[1] == 168 && o[2] == 85 && o[3] == 200);
	CHECK(parse_ipv4("0.0.0.0", o) && o[0] == 0 && o[3] == 0);
	CHECK(parse_ipv4("255.255.255.255", o) && o[0] == 255 && o[3] == 255);
	CHECK(parse_ipv4("010.001.000.007", o) && o[0] == 10 && o[1] == 1 && o[3] == 7); // 3-digit zero padding is accepted
}
static void test_parse_ipv4_invalid() {
	const char *bad[] = {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.999", "1.2.3.", ".1.2.3", "1..2.3",
		"a.b.c.d", "1.2.3.4 ", " 1.2.3.4", "1.2.3.4\n", "1,2,3,4", "my-3ds.local", "1.2.3.-4", "1234.1.1.1", "192.168.1.1/24"};
	for(const char *text : bad) {
		int o[4] = {7, 7, 7, 7};
		bool ok = parse_ipv4(text, o);
		if(ok) printf("  accepted bad ip: '%s'\n", text);
		CHECK(!ok);
		CHECK(o[0] == 7 && o[1] == 7 && o[2] == 7 && o[3] == 7); // untouched on failure
	}
}
static void test_format_and_step() {
	int o[4] = {192, 168, 85, 200};
	CHECK(format_ipv4(o) == "192.168.85.200");
	int clamp[4] = {-5, 300, 0, 255};
	CHECK(format_ipv4(clamp) == "0.255.0.255");
	CHECK_EQ(step_octet(250, 10), 4);     // wraps up
	CHECK_EQ(step_octet(0, -1), 255);     // wraps down
	CHECK_EQ(step_octet(5, -10), 251);
	CHECK_EQ(step_octet(100, 100), 200);
	CHECK_EQ(step_octet(200, 100), 44);
	CHECK_EQ(step_octet(0, 0), 0);
	// Round trip through parse/format
	int r[4]; CHECK(parse_ipv4(format_ipv4(o), r)); CHECK(r[0] == 192 && r[3] == 200);
}
static void test_adjust_ipv4_octet() {
	CHECK(adjust_ipv4_octet("", 0, 10) == "10.0.0.0");                    // starting from nothing
	CHECK(adjust_ipv4_octet("192.168.85.200", 3, 100) == "192.168.85.44"); // wraps past 255
	CHECK(adjust_ipv4_octet("1.2.0.4", 2, -1) == "1.2.255.4");             // wraps below 0
	CHECK(adjust_ipv4_octet("my-3ds.local", 1, 1) == "0.1.0.0");           // hostname: starts from zeros
	CHECK(adjust_ipv4_octet("1.2.3.4", 4, 5) == "1.2.3.4");                // bad octet index: unchanged
	CHECK(adjust_ipv4_octet("1.2.3.4", -1, 5) == "1.2.3.4");
	CHECK(adjust_ipv4_octet("10.0.0.1", 0, 0) == "10.0.0.1");
	// Other octets are never touched
	CHECK(adjust_ipv4_octet("11.22.33.44", 1, 1) == "11.23.33.44");
}
// What a user does in the menus: type an address with the < > buttons, toggle it on. Each change is saved.
static void test_menu_flow_autosaves_each_step() {
	remove("/tmp/ir_flow.cfg");
	Manager m;
	m.set_autosave_path("/tmp/ir_flow.cfg");
	// 192.168.85.200 using step 10/1/100 presses, like the IP editor does
	struct { int octet; int delta; int times; } presses[] = {
		{0, 100, 1}, {0, 10, 9}, {0, 1, 2},     // 100 + 90 + 2 = 192
		{1, 100, 1}, {1, 10, 6}, {1, 1, 8},     // 168
		{2, 10, 8}, {2, 1, 5},                  // 85
		{3, 100, 2},                             // 200
	};
	for(auto &p : presses) for(int i = 0; i < p.times; i++) {
		Config c = m.get_config();
		c.ip = adjust_ipv4_octet(c.ip, p.octet, p.delta);
		CHECK(m.update_config_and_save(c));
	}
	CHECK(m.get_config().ip == "192.168.85.200");
	Config c = m.get_config(); c.enabled = !c.enabled; CHECK(m.update_config_and_save(c)); // the toggle row
	Config fromdisk; CHECK_EQ(load_config("/tmp/ir_flow.cfg", fromdisk), 0);
	CHECK(fromdisk.ip == "192.168.85.200" && fromdisk.enabled);
	// And it survives a "restart": a new manager loading that file starts out the same
	Manager restarted; CHECK_EQ(restarted.load("/tmp/ir_flow.cfg"), 0);
	CHECK(restarted.get_config().ip == "192.168.85.200" && restarted.get_config().enabled);
}
static void test_autosave() {
	remove("/tmp/ir_autosave.cfg");
	Manager m; // not started: only the config handling is tested
	Config c; c.enabled = true; c.ip = "10.0.0.5";
	CHECK(m.update_config_and_save(c)); // no autosave path yet: applies, writes nothing
	FILE *f = fopen("/tmp/ir_autosave.cfg", "r"); CHECK(f == NULL); if(f) fclose(f);
	m.set_autosave_path("/tmp/ir_autosave.cfg");
	c.ip = "192.168.85.200"; c.enabled = false;
	CHECK(m.update_config_and_save(c));
	Config loaded; CHECK_EQ(load_config("/tmp/ir_autosave.cfg", loaded), 0);
	CHECK(loaded.ip == "192.168.85.200" && !loaded.enabled);
	// A later change overwrites it, so the file always matches the latest settings
	c.enabled = true; c.ip = "172.16.0.9";
	CHECK(m.update_config_and_save(c));
	Config again; CHECK_EQ(load_config("/tmp/ir_autosave.cfg", again), 0);
	CHECK(again.ip == "172.16.0.9" && again.enabled);
	CHECK(m.get_config().ip == "172.16.0.9");
	// Other settings aren't lost when the menu only changes ip/enabled
	Config custom = m.get_config(); custom.invert_y = true; custom.bindings[SLOT_HOME] = PhysicalInput{PhysicalInput::BUTTON, 9};
	CHECK(m.update_config_and_save(custom));
	custom.ip = "1.2.3.4"; CHECK(m.update_config_and_save(custom));
	Config check; CHECK_EQ(load_config("/tmp/ir_autosave.cfg", check), 0);
	CHECK(check.invert_y && check.bindings[SLOT_HOME] == custom.bindings[SLOT_HOME] && check.ip == "1.2.3.4");
	// Unwritable location: reports failure but still applies the config
	m.set_autosave_path("/nonexistent_dir_xyz/ir.cfg");
	c.ip = "9.9.9.9";
	CHECK(!m.update_config_and_save(c));
	CHECK(m.get_config().ip == "9.9.9.9");
}

// ---- Live network tests (loopback, port 4950) ----
static int open_receiver() {
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in a; memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET; a.sin_port = htons(UDP_PORT); a.sin_addr.s_addr = inet_addr("127.0.0.1");
	int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if(bind(s, (sockaddr*)&a, sizeof(a)) != 0) { perror("bind"); return -1; }
	return s;
}
static bool recv_frame(int s, uint8_t *buf, int timeout_ms) {
	timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	return recv(s, buf, FRAME_SIZE, 0) == FRAME_SIZE;
}
static void drain(int s) {
	uint8_t b[64]; timeval tv{0, 1000};
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	while(recv(s, b, sizeof(b), 0) > 0) {}
}

static void test_manager_send_and_touch() {
	int rx = open_receiver(); CHECK(rx >= 0); if(rx < 0) return;
	Manager m;
	Config cfg; cfg.enabled = true; cfg.ip = "127.0.0.1";
	m.set_config(cfg); m.start();
	uint8_t f[FRAME_SIZE];
	CHECK(recv_frame(rx, f, 1000));
	CHECK_EQ(rd32(f + 0), 0xFFF);
	m.set_touch(160, 120);
	bool saw_touch = false;
	for(int i = 0; i < 10 && !saw_touch; i++)
		if(recv_frame(rx, f, 500) && (rd32(f + 4) & (1u << 24))) saw_touch = true;
	CHECK(saw_touch);
	CHECK(m.is_touch_redirect_active());
	m.release_touch();
	bool released = false;
	for(int i = 0; i < 10 && !released; i++)
		if(recv_frame(rx, f, 500) && rd32(f + 4) == 0x2000000) released = true;
	CHECK(released);
	m.stop();
	close(rx);
}

static void test_manager_reconnect() {
	int rx = open_receiver(); CHECK(rx >= 0); if(rx < 0) return;
	Manager m;
	Config cfg; cfg.enabled = true; cfg.ip = "no-such-host.invalid"; // Unresolvable: sender can't open
	m.set_config(cfg); m.start();
	Status st; std::string txt;
	usleep(300 * 1000);
	CHECK(m.get_status() == Status::RETRYING);
	CHECK(m.consume_status_change(st, txt) && st == Status::RETRYING);
	CHECK(!m.consume_status_change(st, txt)); // Reported only once
	uint8_t f[FRAME_SIZE];
	drain(rx);
	CHECK(!recv_frame(rx, f, 200)); // Nothing is being sent yet
	// "Device comes back": address becomes valid. The manager should pick it up on its own.
	cfg.ip = "127.0.0.1"; m.set_config(cfg);
	CHECK(recv_frame(rx, f, 2000));
	CHECK(m.get_status() == Status::SENDING);
	// Disabling releases the 3DS inputs (neutral frame) and goes quiet
	m.set_touch(10, 10);
	usleep(150 * 1000);
	cfg.enabled = false; m.set_config(cfg);
	usleep(200 * 1000);
	drain(rx);
	CHECK(!recv_frame(rx, f, 300));
	CHECK(m.get_status() == Status::DISABLED);
	m.stop();
	close(rx);
}

static void test_manager_no_ip() {
	Manager m; Config cfg; cfg.enabled = true; m.set_config(cfg); m.start();
	usleep(150 * 1000);
	CHECK(m.get_status() == Status::NO_HOST_SET);
	m.stop();
}

int main() {
	test_default_frame(); test_a_clears_bit0(); test_ab_swap(); test_xy_swap(); test_ir_buttons();
	test_touch_encoding(); test_circle_pad_full_right(); test_cpp_ir();
	test_stick_up_is_high_y(); test_deadzone(); test_no_joystick(); test_default_bindings();
	test_touch_offset_and_presets(); test_config_roundtrip(); test_config_garbage();
	test_parse_ipv4_valid(); test_parse_ipv4_invalid(); test_format_and_step(); test_adjust_ipv4_octet(); test_menu_flow_autosaves_each_step(); test_autosave();
	test_manager_send_and_touch(); test_manager_reconnect(); test_manager_no_ip();
	if(failures == 0) printf("ALL TESTS PASSED\n");
	else printf("%d FAILURE(S)\n", failures);
	return failures ? 1 : 0;
}
