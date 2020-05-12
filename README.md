# ESPecial

This project aim to provide an easy way to control your esp32. You no longer need programming skills to set complex tasks. This can be a very nice approach for small projects and for educational purposes.


Features:
- Automation: program actions that can be triggered via many different channels (api, mqtt, telegram bot or simply by pins events), without having to code. Control pins value, send telegram message, display messages on screen, send http request etc. No need to update the firmware, everything is dynamic.
- Exposes gpio and actions to a REST API: Set pin digital/analog value, mode (input/output), frequence, resolution, if you want to store its state in flash. Trigger automations.
- Provide web interface full vanilla js. No internet connexion required. Pins mode, actions, conditions, telegram, everything can be set via the interface.
<div>
    <img src="images/sample-home.png" width="200">
    <img src="images/edit-gpio-panel.png" width="200">
    <img src="images/add-automation-panel.png" width="200">
    <img src="images/settings.png" width="200">
</div>

- Telegram bot: Access and control your esp32 from outside your home. No domotic server required! No port exposed to the outside world (way more secure than exposing your router ports). Restrict the access with a user authorised list.
<div>
    <img src="images/telegram.png" width="200">
</div>

- Mqtt client: publish/subscribe pin state, actions.
- Wifi manager: gives a way to easily set your esp32 to your network.
- OTA: Update firmware from the web interface.

## Work in progress:
- I2C integration
- Add possibility to make http requests as a type of action.
- Web Interface: Give it some love, fix some issues, particulary on mqtt connection
## Wish list
- Unit and integration tests!! I'm really new in arduino and even c++ world, so I might need more time to work on that part.
- Find a suitable async web server. At the moment, the rest api is syncronous, one call at a time 😓, I'll make a branch with the ESPAsyncWebServer library, but I'm a bit concerned about the heap memory issue they keep having since 2018...
- Use websockets for the front. So that we don't need to refresh the page to get the updated pin states.
- Make this library compatible with other boards. For now it will only work with dual core boards, as the event listener for pin values is attached to the core 0. With some work, everything could be handle on one core, for instance by using interupts instead of the loop currenlty checking conditions. Another bottle neck is the UniversalTelegramBot library that seems to make long poll to Telegram api and therefor blocks the core process.
- Makefile/bash script: It would be great to have all dependencies easily compiled to the project.
- Auto update: from remote server (why not a .ini build on this repo?)
- reduce library dependencies: UniversalTelegramBot, WifiManager and ArduinoJson could be avoided. Leaving only PubSubClient. This would decrease drastically the size of the project on the esp32.
- Using light weight front library like Preact. This would enhancy greatly the coding experience...
## Getting Started

First, do yourself a nice gift, buy an ESP32 :) Any esp32 will work with this project. 

If your board uses a usb-c type port, you should be able to detect your board by installing this driver:
https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers

### Prerequisites

Before installing anything you'll need your esp32 to be ready. This involve having installed an additional Arduino board manager. The process is quite easy and can be found in the following link:
https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md

If you want to use vscode while coding (I strongly recommand it) follow this nice tutorial: https://medium.com/home-wireless/use-visual-studio-code-for-arduino-2d0cf4c1760b


### Installing

This code also has 4 dependencies which need to be added to your libraries:
- ArduinoJSON v6 (install via library manager) Handle json in a very effective way.
- PubSubClient: (install via library manager)  MQTT handling. It's a very robust pubsub client, perfect for iot projects.
- WifiManager: https://github.com/tzapu/WiFiManager/tree/development
This library will allow you to easily set your board to your wifi. You'll need to get the development branch to have esp32 support.
- UniversalTelegramBot: https://github.com/RomeHein/Universal-Arduino-Telegram-Bot/tree/editMessage. Telegram api for arduino. This is a fork from the main repo. I've made a pull request but it's not yet accepted. So you'll need that fork to make this program works correctly.

## Usage

### Wifi connection
You should first connect your esp32 to your local network. This is easily done thanks to the WifiManager library. Simply power on your device, and connect to the access point (AP) provided. Its name should be the one you provided in the variable called `APName` at the begining of the main.ino file, you can of course change it. 
Once connected to the AP, a configuration window should appears. From here you can connect to your local network (the one provided by your rooter, internet box etc).

### Web interface
The ESP32 should now display its local IP on the tft screen, you can also find the IP in the serial log of the esp32. Simply enter the address provided on your favorite browser and enter.
You are now on the page directly served by your ESP32! The interface is responsive and should be usable on your smartphone.
It should look like this:
<p align="center">
    <img src="images/initial-home-page.png" width="400">
</p>
You now need to add your first pin handler. This is done by simply clicking on the 'plus' button in the top right corner.
<p align="center">
    <img src="images/add-gpio-panel.png" width="400">
</p>
Choose the pin you want to control/listen from the list. This list will only display available pins, so you won't have doublons 👌.
You can switch from INPUT modes, to OUTPUT and Analog. When analog mode is selected, you can then parameter frequency pulse, resolution and channel.
The 'save state' checkbox will allow you to save a state in the flash memory of the controller, allowing you to get back your state even after a reboot.
Once you are done with the settings, press 'save' to add the new GPIO.
A new line should appear:
<p align="center">
    <img src="images/gpios-new-line.png" width="400">
</p>
You can now control the state of your pin by pressing the 'on/off' button if it's mode is set to OUTPU. If its mode is set to INPUT, it will only display its value. On digital mode, you'll be able to set the pulse value.
<br>

### Automation!

And now the best part: set actions that will triggered based on gpios state or telegram/mqtt/api events!
This will give you infinite possibilities for controlling your esp32, without having to code or download new firmware.

Click on the plus sign on the top right corner of the automation container. This panel should appear:
<p align="center">
    <img src="images/initial-automation-panel.png" width="400">
</p>
Here, you'll be able to set conditions based on gpios value. I'm sure there are tons of other possible types, but for now this is enough to cover a lot of use cases.
Automations are based on conditions. Keep in mind that in order to run, all conditions have to be true.
You can add up to 5 conditions per automations. This is to limit heap memory consumption.
Each condition can be linked to the previous one by AND/OR/XOR logic operators. If a condition has "none" operator defined, the next condition will be ignored.
For now, you have two types of conditions:

- Gpio value: the main loop will check for gpios change value every 50ms. If a gpio state has changed, the process will check all conditions of all event driven automations. When all conditions are fullfilled for a given automation, it will run it.
- Time: The main loop will also check all conditions of all event driven automations every minute. You can set time conditions based on hours or weekday.

⚠️ Important note:
This is important that you set a Gpio value condition to your automatisation, otherwise, the time checking loop will run you automation every minute, or every time a gpio state change.
Reason: I still can't figure a correct way to handle time events. If you want to participate to the project, that's a point I would need help.
 
Now we can set our first action. Simply click the add button in the action editor section.
You can choose between three types of actions:
- Set gpio value/pulse
- Send telegram message
- Send http/https request.
- Delay: note that this delay is an actual 'delay' function. Meaning that you'll block the process. Yes, automations run sequencially. The process maintains an automation queue where the oldest automation queued is played first. So don't go crazy on that `delay` option (meaning this should not be used as a timer 😉).

For telegram message and http request type, you can have access to pins value and system information by using the special syntax `${pinNumber}`or `${info}` directly in your text. 

⚠️ Important note: In order to keep the heap memory consumption low, fields are restreigned to 100 characters. This means that any sentence/http address/json longer that 100 char will be ignored. This limit can be increased by changing the variable `MAX_MESSAGE_TEXT_SIZE` in the `PreferenceHandler.h`


If you select the `event triggered` option, the action will be triggered whenever its conditions become true. This can be very handy if you want to send a Telegram notification when a gpio value changes.
You can simulate a `while` loop by setting the `repeat action` input. This loop apply to the whole actions set into the automation. Just keep in mind that if you leave it empty or set it to 0, the action won't trigger. So set it to `1` at least.

You can also specify the next action. This is very handy to easily program complex behaviours.

Sometimes complex behaviours can overlaps, that's why the debounce delay option specified the amount an automation has to wait before being played again.

### Use the rest API
Once controls and actions added to your panel, you'll be able to trigger them by hitting the rest API: 

```
http://your.ip.local.ip/gpio/pinNumber/value
```
And set its state `on` with:
```
http://your.ip.local.ip/gpio/pinNumber/value/1
```
or `off`
```
http://your.ip.local.ip/gpio/pinNumber/value/0
```

And simply send:
```
http://your.ip.local.ip/automationtion/automationId
```
to run a specific action. Note that all conditions you have specified for than action must be fullfilled in order to execute it.

### Telegram Bot
All this is cool, but what if you want to access/control your esp32 from outside your local network?
The easiest/safest way is [Telegram Bot](https://core.telegram.org/bots). You'll find on that [page](https://core.telegram.org/bots#6-botfather) how to create a Telegram Bot in 5 minutes.
Once created, and your bot token in good hands, just go back to your esp32 page and click the setting button. At the bottom of the page you should find the Telegram section, simply past your bot token here, and tick the active box.
Now say hello to your bot!
<p align="center">
    <img src="images/telegram.png" width="400">
</p>
When you first start the conversation with your bot, telegram will only display a `start` button. Tap it and if everything is ok, your bot should answer the available commands amongs your telegram id.
It's important to note that if you leave the authorised user list empty, your bot will answer to anyone. So be secure, and add at least one user id 😉

Now you're be able to control all pins in output mode by sending a `/out` or the automations you have set with `/auto`. The bot will answer a list of buttons corresponding to the list you've set on the web interface of the ESP32, sweet!

### MQTT client (advance)
MQTT is a really nice pubsub protocol. I really encourage you to integrate this feature in your home automation. This allows a two ways communication between your home bridge and your iot device (here the esp32) in a very lightweight way.
To configure it, you'll need to enter the address of your MQTT broker aswell as your username and password for this broker.
You can then set the topic in which the pin states will be published.
The esp32 will listen for all pins on that topic like this:

```
mqtt://your.broker.address:port/yourTopic/friendlyName/gpios/pinNumber
```

It will also publish any state update on the same logic.

## Contributing

All kind of contributions are welcome. I'm very new to arduino world, so don't hesitate to give any advices

## Authors

* **Romain Cayzac** - *Initial work*

## License

This project is licensed under the GNU GPLv3 License - see the [LICENSE.md](LICENSE.md) file for details
