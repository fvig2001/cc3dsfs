#include "InputRedirectionIPMenu.hpp"
#include "InputRedirection.hpp"

#define NUM_TOTAL_MENU_OPTIONS (sizeof(pollable_options)/sizeof(pollable_options[0]))

// How much a single < or > press changes a number. Cycled by the first option.
static const int step_sizes[] = {1, 10, 100};
#define NUM_STEP_SIZES ((int)(sizeof(step_sizes)/sizeof(step_sizes[0])))

struct InputRedirectionIPMenuOptionInfo {
	const std::string base_name;
	const bool is_inc;
	const std::string dec_str;
	const std::string inc_str;
	const int octet; // Which part of the address this changes, or -1
	const InputRedirectionIPMenuOutAction out_action;
};

static const InputRedirectionIPMenuOptionInfo step_option = {
.base_name = "Change by",
.is_inc = false, .dec_str = "", .inc_str = "", .octet = -1,
.out_action = IR_IP_MENU_CHANGE_STEP};

static const InputRedirectionIPMenuOptionInfo octet_1_option = {
.base_name = "Octet 1",
.is_inc = true, .dec_str = "<", .inc_str = ">", .octet = 0,
.out_action = IR_IP_MENU_OCTET_CHANGE};

static const InputRedirectionIPMenuOptionInfo octet_2_option = {
.base_name = "Octet 2",
.is_inc = true, .dec_str = "<", .inc_str = ">", .octet = 1,
.out_action = IR_IP_MENU_OCTET_CHANGE};

static const InputRedirectionIPMenuOptionInfo octet_3_option = {
.base_name = "Octet 3",
.is_inc = true, .dec_str = "<", .inc_str = ">", .octet = 2,
.out_action = IR_IP_MENU_OCTET_CHANGE};

static const InputRedirectionIPMenuOptionInfo octet_4_option = {
.base_name = "Octet 4",
.is_inc = true, .dec_str = "<", .inc_str = ">", .octet = 3,
.out_action = IR_IP_MENU_OCTET_CHANGE};

// Exactly one page (5 options), so the whole address is always visible
static const InputRedirectionIPMenuOptionInfo* pollable_options[] = {
&step_option,
&octet_1_option,
&octet_2_option,
&octet_3_option,
&octet_4_option,
};

InputRedirectionIPMenu::InputRedirectionIPMenu(TextRectanglePool* text_rectangle_pool) : OptionSelectionMenu(){
	this->options_indexes = new int[NUM_TOTAL_MENU_OPTIONS];
	this->initialize(text_rectangle_pool);
	this->num_enabled_options = 0;
}

InputRedirectionIPMenu::~InputRedirectionIPMenu() {
	delete []this->options_indexes;
}

void InputRedirectionIPMenu::class_setup() {
	this->num_options_per_screen = 5;
	this->min_elements_text_scaling_factor = num_options_per_screen + 2;
	this->width_factor_menu = 16;
	this->width_divisor_menu = 9;
	this->base_height_factor_menu = 12;
	this->base_height_divisor_menu = 6;
	this->min_text_size = 0.3f;
	this->max_width_slack = 1.1f;
	this->menu_color = sf::Color(30, 30, 60, 192);
	this->title = "3DS IP Address";
	this->show_back_x = true;
	this->show_x = false;
	this->show_title = true;
}

int InputRedirectionIPMenu::get_step() const {
	return step_sizes[this->step_index % NUM_STEP_SIZES];
}

void InputRedirectionIPMenu::insert_data() {
	this->num_enabled_options = 0;
	for(size_t i = 0; i < NUM_TOTAL_MENU_OPTIONS; i++) {
		this->options_indexes[this->num_enabled_options] = (int)i;
		this->num_enabled_options++;
	}
	this->prepare_options();
}

void InputRedirectionIPMenu::reset_output_option() {
	this->selected_index = InputRedirectionIPMenuOutAction::IR_IP_MENU_NO_ACTION;
}

void InputRedirectionIPMenu::set_output_option(int index, int action) {
	if(index == BACK_X_OUTPUT_OPTION) {
		this->selected_index = IR_IP_MENU_BACK;
		return;
	}
	const InputRedirectionIPMenuOptionInfo* option = pollable_options[this->options_indexes[index]];
	if(option->octet >= 0) {
		this->selected_octet = option->octet;
		this->selected_delta = (action == INC_ACTION) ? this->get_step() : -this->get_step();
		this->selected_index = IR_IP_MENU_OCTET_CHANGE;
	}
	else if(option->out_action == IR_IP_MENU_CHANGE_STEP) {
		this->step_index = (this->step_index + 1) % NUM_STEP_SIZES;
		this->selected_index = IR_IP_MENU_NO_ACTION;
	}
	else
		this->selected_index = option->out_action;
}

size_t InputRedirectionIPMenu::get_num_options() {
	return this->num_enabled_options;
}

bool InputRedirectionIPMenu::is_option_inc_dec(int index) {
	return pollable_options[this->options_indexes[index]]->is_inc;
}

std::string InputRedirectionIPMenu::get_string_option(int index, int action) {
	if((action == INC_ACTION) && this->is_option_inc_dec(index))
		return pollable_options[this->options_indexes[index]]->inc_str;
	if((action == DEC_ACTION) && this->is_option_inc_dec(index))
		return pollable_options[this->options_indexes[index]]->dec_str;
	return pollable_options[this->options_indexes[index]]->base_name;
}

void InputRedirectionIPMenu::prepare(float menu_scaling_factor, int view_size_x, int view_size_y, const std::string &ip) {
	// If the saved address isn't a plain number address (empty, or a hostname set
	// by hand in the config file) the numbers start from 0.
	int octets[4] = {0, 0, 0, 0};
	ir::parse_ipv4(ip, octets);

	int num_pages = this->get_num_pages();
	if(this->future_data.page >= num_pages)
		this->future_data.page = num_pages - 1;
	int start = this->future_data.page * this->num_options_per_screen;
	for(int i = 0; i < this->num_options_per_screen + 1; i++) {
		int index = (i * this->single_option_multiplier) + this->elements_start_id;
		if(!this->future_enabled_labels[index])
			continue;
		int real_index = start + i;
		const InputRedirectionIPMenuOptionInfo* option = pollable_options[this->options_indexes[real_index]];
		if(option->octet >= 0) {
			this->labels[index]->setText(this->setTextOptionInt(real_index, octets[option->octet]));
			// The buttons show how much they will change the number by
			this->labels[index + 1]->setText("-" + std::to_string(this->get_step()));
			this->labels[index + 2]->setText("+" + std::to_string(this->get_step()));
		}
		else if(option->out_action == IR_IP_MENU_CHANGE_STEP)
			this->labels[index]->setText(this->setTextOptionInt(real_index, this->get_step()));
	}

	this->base_prepare(menu_scaling_factor, view_size_x, view_size_y);
}
