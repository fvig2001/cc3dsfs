#ifndef __INPUTREDIRECTIONMENU_HPP
#define __INPUTREDIRECTIONMENU_HPP

#include "OptionSelectionMenu.hpp"
#include <chrono>
#include <string>

#include "TextRectangle.hpp"
#include "sfml_gfx_structs.hpp"
#include "display_structs.hpp"

enum InputRedirectionMenuOutAction{
	IR_MENU_NO_ACTION,
	IR_MENU_BACK,
	IR_MENU_TOGGLE_ENABLED,
	IR_MENU_IP_ADDRESS,
};

// Main Input Redirection menu: turn it on/off and open the IP address editor.
class InputRedirectionMenu : public OptionSelectionMenu {
public:
	InputRedirectionMenu(TextRectanglePool* text_pool);
	~InputRedirectionMenu();
	void prepare(float scaling_factor, int view_size_x, int view_size_y, bool enabled, const std::string &ip);
	void insert_data();
	InputRedirectionMenuOutAction selected_index = InputRedirectionMenuOutAction::IR_MENU_NO_ACTION;
	void reset_output_option();
protected:
	void set_output_option(int index, int action);
	bool is_option_selectable(int index, int action);
	size_t get_num_options();
	std::string get_string_option(int index, int action);
	void class_setup();
private:
	int *options_indexes;
	size_t num_enabled_options;
};
#endif
