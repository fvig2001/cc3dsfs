#include "InputRedirection.hpp"

#include <SFML/Window/Joystick.hpp>

#include <algorithm>
#include <cmath>

namespace ir {

// Merges all connected joysticks, like the source project does with gilrs
// ("all connected controllers merged"): buttons are OR'd, and for each axis
// the value with the largest magnitude wins.
JoystickSnapshot take_joystick_snapshot() {
	JoystickSnapshot snap;
	for(unsigned int i = 0; i < sf::Joystick::Count; i++) {
		if(!sf::Joystick::isConnected(i))
			continue;
		snap.any_connected = true;
		const unsigned int num_buttons = std::min<unsigned int>(sf::Joystick::getButtonCount(i), MAX_JOY_BUTTONS);
		for(unsigned int b = 0; b < num_buttons; b++)
			if(sf::Joystick::isButtonPressed(i, b))
				snap.buttons[b] = true;
		for(unsigned int a = 0; a < NUM_JOY_AXES; a++) {
			sf::Joystick::Axis axis = sf::Joystick::Axis((unsigned int)sf::Joystick::Axis::X + a);
			if(!sf::Joystick::hasAxis(i, axis))
				continue;
			// SFML reports -100..100
			float value = std::min(std::max(sf::Joystick::getAxisPosition(i, axis) / 100.0f, -1.0f), 1.0f);
			if(std::fabs(value) > std::fabs(snap.axes[a]))
				snap.axes[a] = value;
		}
	}
	return snap;
}

}
