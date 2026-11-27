#ifndef __INPUTREDIRECTIONIPMENU_HPP
#define __INPUTREDIRECTIONIPMENU_HPP

#include "OptionSelectionMenu.hpp"
#include <chrono>
#include <string>

#include "TextRectangle.hpp"
#include "sfml_gfx_structs.hpp"
#include "display_structs.hpp"

enum InputRedirectionIPMenuOutAction{
	IR_IP_MENU_NO_ACTION,
	IR_IP_MENU_BACK,
	IR_IP_MENU_OCTET_CHANGE, // Use selected_octet and selected_delta
	IR_IP_MENU_CHANGE_STEP,  // Handled inside the menu: never reported as selected_index
};

// Edits the 3DS IPv4 address one number (octet) at a time, using < and > buttons,
// so it works with a mouse, keyboard or controller alike.
class InputRedirectionIPMenu : public OptionSelectionMenu {
public:
	InputRedirectionIPMenu(TextRectanglePool* text_pool);
	~InputRedirectionIPMenu();
	void prepare(float scaling_factor, int view_size_x, int view_size_y, const std::string &ip);
	void insert_data();
	InputRedirectionIPMenuOutAction selected_index = InputRedirectionIPMenuOutAction::IR_IP_MENU_NO_ACTION;
	// Only meaningful when selected_index is IR_IP_MENU_OCTET_CHANGE
	int selected_octet = 0;  // 0 to 3
	int selected_delta = 0;  // Positive or negative amount to add to that octet
	void reset_output_option();
protected:
	bool is_option_inc_dec(int index);
	void set_output_option(int index, int action);
	size_t get_num_options();
	std::string get_string_option(int index, int action);
	void class_setup();
private:
	int *options_indexes;
	size_t num_enabled_options;
	int step_index = 1; // Which of the allowed step sizes is selected (starts at 10)
	int get_step() const;
};
#endif
