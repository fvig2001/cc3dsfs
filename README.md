# cc3dsfs

cc3dsfs is a multi-platform capture and display program for [3dscapture's](https://3dscapture.com/) N3DSXL, 3DS and DS capture boards written in C++.
The main goal is to offer the ability to use the Capture Card with a TV, via fullscreen mode.

IS Nitro Emulator (newer revisions), IS Nitro Capture and IS TWL Capture support is also present. Though results may vary (and the amount of video delay may be significantly higher based on the cable used).

Optimize Old 3DS/2DS capture boards and Optimize New 3DS/2DS capture boards are also supported.

Partner CTR Capture devices are supported.

Finally, support for a really old Chameleon USB FX2 board is also present, used with Nisetro DS(i) capture boards.

## Features

- Performance-focused design, with low latency for both audio and video (measured to oscillate between 1 and 2 frames on 120hz displays).
- Option to split the screens in separate windows, to address them separately.
- Make your game run in fullscreen mode. If you own multiple displays, you can even use one per-window.
- Many builtin crop options for the screens.
- Many other settings, explained in [Controls](#Controls).
- Prebuilt executables available in the [Releases](https://github.com/Lorenzooone/cc3dsfs/releases/latest) page for ease of use.
- Various command line arguments to personalize how the software should behave. Use -h or --help as an argument to display the help message.

_Note: On 3DS, DS, GBA, GBC and GB games boot in scaled resolution mode by default. Holding START or SELECT while launching these games will boot in native resolution mode._

_Note: Make sure the 3DS audio is not set to Surround._

## Dependencies

cc3dsfs has three build dependencies: CMake, g++ and git.
Make sure all are installed.
On MacOS, [Homebrew](https://brew.sh/) can be used to install both CMake and git. An automatic popup should appear to install g++ at Compile time.

cc3dsfs has four library dependencies: [FTDI's D3XX driver](https://ftdichip.com/drivers/d3xx-drivers/) (on Windows), [FTDI's D2XX driver](https://ftdichip.com/drivers/d2xx-drivers/) (on Windows), [libusb](https://libusb.info/) and [SFML](https://www.sfml-dev.org/).
All of them should get downloaded automatically via CMake during the building process.

Linux users who wish to compile this, will also need to install the SFML dependencies. Different distributions will require slightly different processes.
Below, the command for Debian-based distributions, which also lists the required libraries.

```
sudo apt update
sudo apt install \
    g++ \
    git \
    cmake \
    libxrandr-dev \
    libxcursor-dev \
    libudev-dev \
    libflac-dev \
    libvorbis-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libdrm-dev \
    libgbm-dev \
    libfreetype-dev \
    libharfbuzz-dev \
    xorg-dev
```

Additionally, when compiling for a Raspberry Pi, install gpiod and libgpiod-dev.

On Windows, you may need to install the Visual C++ Redistributable set of libraries. They are available here: [Official Microsoft VC Redist Link](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-microsoft-visual-c-redistributable-version).

## Compilation

To compile the program, assuming CMake, git and g++ are installed on the system, this is the command which should be launched:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release ; cmake --build build --config Release
```

This will download FTD3XX, FTD2XX, libusb and SFML, which may take a while during the first execution of the command. Later runs should be much faster.
On MacOS, you may also be prompted to install the Apple Command Line Developer Tools first.

When compiling on a Raspberry Pi, to enable usage of GPIO, use:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DRASPBERRY_PI_COMPILATION=TRUE ; cmake --build build --config Release
```

### Docker Compilation

Alternatively, one may use Docker to compile the Linux version for its different architectures by running: `docker run --rm -it -v ${PWD}:/home/builder/cc3dsfs lorenzooone/cc3dsfs:<builder>`
The following builders are available: builder32, builder64, builderarm32, builderarm64, builderriscv64 and builderandroid (in beta).

## Controls

The software has a GUI which exposes all of the available settings. There are also various available keyboard shortcuts which allow quicker access to the options.
Most of the settings are explained in [Keyboard shortcuts](#Keyboard-shortcuts).

### Keyboard controls
- __Enter key__: Used to open the GUI when it is not shown, as well as to confirm the selection of an option. Holding it for 10 seconds will reset the application to its defaults.
- __Arrow keys__: Used to change the selected option in the GUI.

### Mouse controls
- __Right click__: Used to open the GUI when it is not shown. Holding it for 10 seconds will reset the application to its defaults.
- __Left click__: Used to confirm the selection of an option.

### Joystick controls
- __Option/Share buttons__: Used to open the GUI when it is not shown.
- __A/B/X/Y buttons__: Used to confirm the selection of an option. Holding B (X on a PS5 controller) for 10 seconds will reset the application to its defaults.
- __Dpad/Sitcks__: Used to change the selected option in the GUI.

_Note: Currently only tested using a PS5 controller._

### Keyboard shortcuts
- __O key__: Open/Close connection to the 3DS/DS.
- __F key__: Toggles Fullscreen mode on/off. Only guaranteed to work on the primary monitor. When using certain setups, it also works for multiple monitors.
- __S key__: Swaps between Split mode and Joint mode. In Split mode, each screen has its own window. In Joint mode, they are combined into a single one.
- __C key__: Cycles to the next Crop mode for the focused window. For 3DS, the currently supported cropping modes are: 3DS, 16:10 DS, scaled DS, native DS, scaled GBA, native GBA, scaled GB, native GB, scaled SNES, native SNES and native NES. For DS, the currently supported cropping modes are: DS, Top GBA and Bottom GBA. There are also game-specific cropping modes which can be enabled via the Video Settings.
- __-/0 key__: Decrements/Increments the scaling by 0.5x for the non-Fullscreen focused window. 1.0x is the minimum. 45.0x is the maximum.
- __Y/U key__: In Joint Fullscreen mode, increases the size of the top/bottom screen. Corresponds to the _Screens ratio_ Video Setting.
- __Z/X key__: Decreases/Increases the Menu Scaling multiplier by 0.1. 0.3 is the minimum. 10.0 is the maximum.
- __T key__: Moves the relative position of the bottom screen inside of the joint mode window clockwise. Corresponds to the _Screens Relative Positions_ Video Setting.
- __V key__: Toggles VSync on/off for the focused window. VSync prevents screen tearing. However, at low refresh rates (60 hz), it may significantly increase image delay.
- __A key__: Toggles Async on/off for the focused window. Having it off ensures all the available windows show the same frame. If you're having framerate issues, turning this setting on/off could help.
- __B key__: Toggles Blur on/off for the focused window. This is only noticeable at scales of 1.5x or greater.
- __6 key__: In Joint mode, changes the position of the smaller screen relative to the bigger one between 0, 1/2 Max and Max. Accessible via either the _Offset Settings_, or the _Screen Offset_ Video Settings.
- __7 key__: In Joint Fullscreen mode, changes the distance from the upper screen between 0, 1/2 Max and Max. Accessible via the _Offset Settings_ Video Setting.
- __4 key__: In Fullscreen mode, changes the X distance from the border between 0, 1/2 Max and Max. Accessible via the _Offset Settings_ Video Setting.
- __5 key__: In Fullscreen mode, changes the Y distance from the border between 0, 1/2 Max and Max. Accessible via the _Offset Settings_ Video Setting.
- __8/9 key__: Rotates the screen(s) of the focused window 90 degrees counterclockwise/clockwise. Accessible via the _Rotation Settings_ Video Setting.
- __H/J key__: Rotates the top screen of the focused window 90 degrees counterclockwise/clockwise. Accessible via either the _Rotation Settings_, or the _Screen Rotation_ Video Settings.
- __K/L key__: Rotates the bottom screen of the focused window 90 degrees counterclockwise/clockwise. Accessible via either the _Rotation Settings_, or the _Screen Rotation_ Video Settings.
- __R key__: Toggles extra padding on/off. It can be used to account for windows with rounded corners.
- __2/3 key__: Cycles through the available Pixel Aspect Ratios (PAR) for the top/bottom screen. Useful for VC games and for Chrono Trigger DS.
- __M key__: Toggles Audio mute on/off.
- __,/. key__: Decrements/Increments the volume by 5 units. 0 is the minimum. 200 is the maximum.
- __N key__: Restarts the audio output. Useful if the audio stops working after changing OS settings.
- __F1 - F4 keys__: Loads from layouts 1 through 4 respectively. To access more profiles, the GUI must be used.
- __F5 - F8 keys__: Saves to layouts 1 through 4 respectively. To access more profiles, the GUI must be used.
- __Esc key__: Properly quits the program.

_Note: The volume is independent of the actual volume level set with the physical slider on the console._

## Input redirection (3DS)

cc3dsfs can send controller and touch input to a 3DS running [InputRedirection](https://github.com/TuxSH/InputRedirection), using the same 20-byte UDP protocol (port 4950) as [InputRedirectionClient](https://github.com/TuxSH/InputRedirectionClient-Qt). The logic is a port of [jojo8356/InputRedirectionClient](https://github.com/jojo8356/InputRedirectionClient).

- **Menu:** *Main menu > Input Redirection* has *Enable/Disable Input Redirection* and *IP Address*. The address is set one number at a time with the `<` and `>` buttons; the *Change by* option switches between steps of 1, 10 and 100 (numbers wrap around, so `<` on 0 gives 255). Every change is applied immediately and saved to `input_redirection.cfg`, so there is nothing to confirm. The remaining settings (button bindings, inversions, touch presets, deadzone, touch offset) are still edited in that file.
- Or start with `--ir_ip <3DS IP>`, or set `enabled=1` and `ip=` in `input_redirection.cfg` (same folder as the other configs).
- **Touch:** while no menu is open, left click (or touch) on the bottom screen is sent to the 3DS as a stylus touch. This accounts for rotation, scaling, pixel aspect ratio and cropping. Left click/touch on the top screen, right click, Enter and the controller still work as before. While touch redirection is on, a touch on the bottom screen does not trigger the long-press that opens the menu; long-press the top screen instead.
- **Reconnection:** the 3DS side is UDP, so it is never really "connected". If the socket or address fails (network change, 3DS rebooting, hostname not resolving yet), the socket is rebuilt and the host re-resolved, retrying every 0.5s up to 5s. A notification shows when it starts failing and when it recovers. Note that a "sending" status means packets are leaving this computer, not that the 3DS received them. The capture card side uses the existing *Periodic connection try* option, which is now on by default.
- **Settings** (all in `input_redirection.cfg`): `invert_y`, `invert_ab`, `invert_xy`, 19 bindings (`bind_a`, `bind_b`, ..., `bind_touch1`, `bind_touch2`) written as `button:N`, `axis+:N`, `axis-:N` or `none`, touch presets (`touch1_x/y`, `touch2_x/y`), `deadzone`, `touch_offset_x/y`, and which axes are the sticks. Axis numbers follow SFML's order: 0=X, 1=Y, 2=Z, 3=R, 4=U, 5=V, 6=PovX, 7=PovY. Use `--list_joysticks` to see what SFML detects.

## Changing settings while running (`--update_settings`)

Another program or script can change settings of a cc3dsfs which is already running:

```
cc3dsfs --update_settings bot_fullscreen=0 bot_have_titlebar=1
cc3dsfs --update_settings ir_ip=192.168.1.50 ir_enabled=1
```

The running instance answers with what happened to each setting, and the command exits with `0` if everything was applied, `1` if some settings were refused, `2` if no cc3dsfs answered (not running, or a different `--control_port`) and `3` for a mistake in the command itself.

```
$ cc3dsfs --update_settings bot_fullscreen=0 nonsense=1
OK bot_fullscreen
ERROR nonsense: unknown setting
DONE applied=1 failed=1
```

**Which settings:** the same names and values as in the layout `.cfg` files (the files saved by the *Save* option of the menu are a good reference).
- Per window settings start with `top_`, `bot_` or `joint_`, followed by the name: `bot_fullscreen`, `top_have_titlebar`, `joint_scale`, `bot_vsync`, `top_rot`, ...
- Global ones: `split`, `requested_3d`, `interleaved_3d`, `do_ratio_cycling`, `fixed_output_framerate`, `target_fixed_output_framerate`, and the audio ones: `volume`, `mute`, `audio_mode`, `audio_output_type`, `audio_output_device_name`, ...
- Input Redirection settings start with `ir_` followed by the name used in `input_redirection.cfg`: `ir_ip`, `ir_enabled`, `ir_touch_enabled`, `ir_invert_y`, `ir_invert_ab`, `ir_invert_xy`, `ir_deadzone`, `ir_bind_home`, ... `ir_ip` takes an IPv4 address or a hostname, and an empty value clears it. Bad addresses such as `192.168.1` or `999.1.1.1` are refused.
- Settings about the capture hardware (battery, scan lists, capture formats, ...) are refused: they are only loaded from files.

**What happens:** window settings are applied the same way as when a layout is loaded, so the windows are set up again only if something really changed. Changes to layout settings are not written to any layout file (use the *Save* option for that). `ir_` changes are saved to `input_redirection.cfg` right away, like the changes made from the menu. Settings in the same command are independent: if one is refused, the others are still applied.

**How it works:** the running cc3dsfs listens on UDP port `47951` of the local computer (`127.0.0.1` only, nothing from the network can reach it). Use `--control_port N` to pick another port (also for the `--update_settings` command), or `--control_port 0` to turn the feature off. Any program running on the same computer can send settings, so keep that in mind on a shared machine.

A script doesn't need to call the executable. It can send one UDP datagram to `127.0.0.1:47951` with these lines, and read the reply in the format shown above:

```
CC3DSFS-UPDATE-SETTINGS 1
bot_fullscreen=0
ir_ip=192.168.1.50
```

```python
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(3)
s.sendto(b"CC3DSFS-UPDATE-SETTINGS 1\nbot_fullscreen=0\nbot_have_titlebar=1\n", ("127.0.0.1", 47951))
print(s.recv(65536).decode())
```

## Profiles/Layouts

When starting the program for the first time, a message indicating a load failure for the cc3dsfs.cfg file will be displayed, and the same will occur when attempting to load from any given layout file if it hasn't been saved to before. That is normal. These files must be created by the program first before they can be loaded from. The program saves its current configuration to the cc3dsfs.cfg file when the program is successfully closed, creating the file if it doesn't already exist, and loading from it every time at startup.

The current configuration can be saved to various extra profiles, creating the given file if it doesn't already exist. Changing settings after a layout is loaded will not automatically overwrite its file. To make the changes permanent, saving the profile again is required.

The name of profiles can be changed by altering the __name__ field in its file.

On Linux and MacOS, by default the profiles can be found at the "${HOME}/.config/cc3dsfs" folder. Or "/home/<user_name>/.config/cc3dsfs".

On Windows, the profiles can be found in the ".config/cc3dsfs" folder inside the directory in which the program runs from.

The CC3DSFS\_CFG\_DIR environment variable can be used to specify a different target folder for cc3dsfs to store its data.

## Notes
- On Linux, you may need to include the udev USB access rules. You can use the .rules files available in the repository's usb\_rules directory, or define your own. For ease of use, releases come bundled with a script to do it named install\_usb\_rules.sh. It may require elevated permissions to execute properly. You may get a permission error if the rules are not installed.
- At startup, the audio may be unstable. It should fix itself, if you give it enough time.
- If, at first, the connection to the 3DS/DS fails, reconnect the 3DS/DS and then try again. If that also doesn't work, try restarting the program. If that also doesn't work, try restarting the computer.
- USB Hubs can be the cause of connection issues. If you're having problems, try checking whether the 3DS/DS connects fine or not without any other devices connected.
- Current font in use: OFL Sorts Mill Goudy TT
- Enabling Slow Poll may slightly boost the FPS of the software, at the cost of an extremely slight decrease in frame latency, and slower reaction times of the software to key presses. Disabled by default (as when the FPS are greater than the CC's, it's not reccomended).
- On MacOS, you may get a notice about Apple being unable to check for malware, or the developer being unknown. To open the program regardless of that, check the [Official Guide for "Opening Applications from Unknown Developers" from Apple, for your version of MacOS](https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac).
- Certain devices (including Apple hardware running MacOS 15 or newer) may have issues displaying images at the refresh rate of the Nintendo DS/3DS consoles. To account for this, there are options to setup a fixed framerate output. They can be found under _Video Settings_ -> _Output Framerate Settings_. When making use of this fixed output framerate, it is suggested to turn VSync on to avoid artifacts. For the best results, make sure that the fixed framerate is a multiple of 60, and that the fixed framerate matches the display's framerate.
- Certain TVs/Monitors may add some audio delay for the purpose of video/lip syncing. If you're experiencing too much audio delay when using this software, try checking in the TV/Monitor settings whether you can reduce that added delay. One of the names used for that setting is "Lip Sync", or something along that line.
- MacOS does not allow running multiple instances of the same application, normally. If you want to run multiple instances of cc3dsfs on MacOS, open the terminal in the folder where cc3dsfs is and type `open -n cc3dsfs.app`.
- To reset to the default settings, besides the options shown in [Controls](#Controls), you can find the _Reset Settings_ option inside of the _Extra Settings_.
- There are multiple capture cards which use the same EZ-USB FX2LP board, creating a conflict. These are the Optimize Old 3DS/2DS capture cards and the Nisetro DS(i) capture cards. This means that when the user connects a device which uses the EZ-USB FX2LP board, they need to select one capture card among the possible ones. To avoid this extra step, the user can disable scanning for the conflicting capture cards they do not intend to use. The settings to do this are available under _Extra Settings_ -> _USB Conflict Resolution_.
### Loopy Capture Cards
- When using the new 2024 Loopy DS Capture Card on Windows, the default driver (FTD2XX) adds one extra frame of latency. To remove that, consider switching to WinUSB as the driver. To change driver, download a software to install drivers like [Zadig](https://zadig.akeo.ie/), select the device in question and select WinUSB. Then install the driver and wait for it to finish. The application will now use WinUSB, with better latency (the serial shown in the Status menu will have an l where there previously was a d).
### Optimize Capture Cards
- To properly use the Optimize capture cards with cc3dsfs, a serial key is needed. This can be added via the _Optimize 3DS Settings_, under the _Add New Serial Key_ option. You can use CTRL+V in the textbox to copy the serial key, or insert it manually. To obtain the key, get your device ID (it can be copied from the _Optimize 3DS Settings_ menu) and type it in the [Official site](https://optimize.ath.cx/productkey_en.html). Using a keyboard to do this is suggested (but not required). The software will read the serial key from the EEPROM on Optimize New 3DS Capture Cards (v1).
- Keys are saved under the keys folder inside the cc3dsfs config folder.
- When using an Optimize Old 3DS Capture Card or a Nisetro DS(i) Capture Card on Windows, make sure either the Cypress USB drivers are installed, or the Optimize Old 3DS drivers are installed. Releases come with a folder which has a .bat file to install the needed drivers.
- When using an Optimize New 3DS Capture Card on Windows, make sure the Optimize New 3DS drivers are installed. Releases come with a folder which has a .bat file to install the needed drivers.
- Depending on the 2DS Hardware revision (starting with 2DSs from late 2014 onwards), Optimize Old 2DS Capture cards may need to use a special (older) firmware. This can be selected when first connecting to the capture card. RGB888 mode works normally. RGB565 does not have sound. For these 2DS capture cards, initial synchronization must also be done upon the first connection. To do this, upon connecting the capture card, go to Optimize 3DS Settings -> Synch Settings, and change the parameters until an image is displayed.
- Windows 11 may block the driver of the Optimize Old/New 3DS/2DS Capture Card on the basis of being unable to "verify the digital signature". To make the device work again with cc3dsfs, it is possible to install the Cypress USB drivers. Releases come with a folder which has a .bat file to install the needed drivers.
- Inside the _Optimize 3DS Settings_ menu, there is an option to downgrade Optimize New 3DS (v1) Capture Cards to v2 (by removing the EEPROM configuration). This makes the capture card compatible with more recent releases of certain software. However, it makes it so: 1) the capture card is not compatible anymore with older releases of software that expect a specific USB ID; 2) it is not possible to grab the serial key of the capture card from the EEPROM. In general, the downgrade to v2 is not advised, though an option for it is included.
### IS Nitro/TWL devices
- When using IS Nitro Emulator, IS Nitro Capture or IS TWL Capture devices on Windows, cc3dsfs is compatible with both the official driver and WinUSB, with the latter being useful if you don't have access to the official driver. To install and use WinUSB, plug in your device, download a software to install drivers like [Zadig](https://zadig.akeo.ie/), select the device in question and select WinUSB. Then install the driver and wait for it to finish. The devices should now work properly with this application.
### Partner CTR Capture
- When using Partner CTR Capture devices on Windows, cc3dsfs is compatible with both the official driver and libusb-win32 (or alternatives), with the latter being useful if you don't have access to the official driver. To install and use libusb-win32 (or alternatives), plug in your device, download a software to install drivers like [Zadig](https://zadig.akeo.ie/), select the device in question and select libusb-win32 (or alternatives). Then install the driver and wait for it to finish. The devices should now work properly with this application.
### Input Redirect Feature
- Supports input redirect using controller connected to the device (Should disable controller input on the application itself)
- Supports input redirect using mouse/touch
- To use input redirect, you need to update the config file found at .config/cc3dsfs/input_redirection.cfg and update the ip address and settings
### Aggressive Reconnect
- This fork makes the application reconnect a little more aggressively. Like it will reconnect after a disconnect within 5 seconds

Used Claude to port Input Redirect Features due to laziness.



