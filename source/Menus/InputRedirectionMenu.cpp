#include "InputRedirectionMenu.hpp"

#define NUM_TOTAL_MENU_OPTIONS (sizeof(pollable_options)/sizeof(pollable_options[0]))

struct InputRedirectionMenuOptionInfo {
	const std::string base_name;
	const std::string false_name;
	const bool is_selectable;
	const InputRedirectionMenuOutAction out_action;
};

// Shows the action that will happen when selected, like the other on/off options do
// (base_name is used while it is on, false_name while it is off).
static const InputRedirectionMenuOptionInfo toggle_enabled_option = {
.base_name = "Disable Input Redirection", .false_name = "Enable Input Redirection", .is_selectable = true,
.out_action = IR_MENU_TOGGLE_ENABLED};

// The current address is appended to this name when drawing it
static const InputRedirectionMenuOptionInfo ip_address_option = {
.base_name = "IP Address", .false_name = "", .is_selectable = true,
.out_action = IR_MENU_IP_ADDRESS};

static const InputRedirectionMenuOptionInfo* pollable_options[] = {
&toggle_enabled_option,
&ip_address_option,
};

InputRedirectionMenu::InputRedirectionMenu(TextRectanglePool* text_rectangle_pool) : OptionSelectionMenu(){
	this->options_indexes = new int[NUM_TOTAL_MENU_OPTIONS];
	this->initialize(text_rectangle_pool);
	this->num_enabled_options = 0;
}

InputRedirectionMenu::~InputRedirectionMenu() {
	delete []this->options_indexes;
}

void InputRedirectionMenu::class_setup() {
	this->num_options_per_screen = 5;
	this->min_elements_text_scaling_factor = num_options_per_screen + 2;
	this->width_factor_menu = 16;
	this->width_divisor_menu = 9;
	this->base_height_factor_menu = 12;
	this->base_height_divisor_menu = 6;
	this->min_text_size = 0.3f;
	this->max_width_slack = 1.1f;
	this->menu_color = sf::Color(30, 30, 60, 192);
	this->title = "Input Redirection";
	this->show_back_x = true;
	this->show_x = false;
	this->show_title = true;
}

void InputRedirectionMenu::insert_data() {
	this->num_enabled_options = 0;
	for(size_t i = 0; i < NUM_TOTAL_MENU_OPTIONS; i++) {
		this->options_indexes[this->num_enabled_options] = (int)i;
		this->num_enabled_options++;
	}
	this->prepare_options();
}

void InputRedirectionMenu::reset_output_option() {
	this->selected_index = InputRedirectionMenuOutAction::IR_MENU_NO_ACTION;
}

void InputRedirectionMenu::set_output_option(int index, int action) {
	if(index == BACK_X_OUTPUT_OPTION)
		this->selected_index = IR_MENU_BACK;
	else
		this->selected_index = pollable_options[this->options_indexes[index]]->out_action;
}

bool InputRedirectionMenu::is_option_selectable(int index, int action) {
	return pollable_options[this->options_indexes[index]]->is_selectable;
}

size_t InputRedirectionMenu::get_num_options() {
	return this->num_enabled_options;
}

std::string InputRedirectionMenu::get_string_option(int index, int action) {
	if(action == FALSE_ACTION)
		return pollable_options[this->options_indexes[index]]->false_name;
	return pollable_options[this->options_indexes[index]]->base_name;
}

void InputRedirectionMenu::prepare(float menu_scaling_factor, int view_size_x, int view_size_y, bool enabled, const std::string &ip) {
	int num_pages = this->get_num_pages();
	if(this->future_data.page >= num_pages)
		this->future_data.page = num_pages - 1;
	int start = this->future_data.page * this->num_options_per_screen;
	for(int i = 0; i < this->num_options_per_screen + 1; i++) {
		int index = (i * this->single_option_multiplier) + this->elements_start_id;
		if(!this->future_enabled_labels[index])
			continue;
		int real_index = start + i;
		int option_index = this->options_indexes[real_index];
		switch(pollable_options[option_index]->out_action) {
			case IR_MENU_TOGGLE_ENABLED:
				this->labels[index]->setText(this->setTextOptionBool(real_index, enabled));
				break;
			case IR_MENU_IP_ADDRESS:
				this->labels[index]->setText(this->setTextOptionString(real_index, ip.empty() ? "Not set" : ip));
				break;
			default:
				break;
		}
	}

	this->base_prepare(menu_scaling_factor, view_size_x, view_size_y);
}
