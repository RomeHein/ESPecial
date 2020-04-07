# ESP32 pin control server

This project aim to provide a server that can expose digital pins easily.
Features:
- Exposes digital pins to a REST API
- Provide web interface full vanilla js. No internet connexion required.
<div>
    <img src="images/controls.png" width="200">
    <img src="images/controls-edit.png" width="200">
    <img src="images/settings.png" width="200">
</div>

- Telegram bot: Access and control your esp32 from outside your home. No domotic server required! No port exposed to the outside world (way more secure than exposing your router ports)
<div>
    <img src="images/telegram.png" width="200">
</div>

- Wifi manager: gives a way to easily set your esp32 to your network.
- Update: web server OTA. Directly in the web interface.
- Provides info on the tft LCD screen.
- Provides manual control over the pins thanks to the two buttons provided by the TTGO chip.

## Work in progress:
- MQTT
- Documentation

## Wish list
- Makefile/bash script: It would be great to have all dependencies easily compiled to the project.
- Auto update: from remote server (why not a .ini build on this repo?)

## Getting Started

This code works well with [this](https://www.aliexpress.com/item/33048962331.html?spm=a2g0o.productlist.0.0.71ee316cmQo1JA&algo_pvid=6aadca0f-7463-41bf-8277-010dbd421b34&algo_expid=6aadca0f-7463-41bf-8277-010dbd421b34-6&btsid=0b0a0ae215834054133566008e89a2&ws_ab_test=searchweb0_0,searchweb201602_,searchweb201603_) type of chip from TTGO, but can easily be adapted to any other esp32 chip.

If your board uses a usb-c type port, you should be able to detect your board by installing this driver:
https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers

### Prerequisites

Before installing anything you'll need your esp32 to be ready. This involve having installed an additional Arduino board manager. The process is quite easy and can be found in the following link:
https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md

If you want to use vscode while coding (I strongly recommand it) follow this nice tutorial: https://medium.com/home-wireless/use-visual-studio-code-for-arduino-2d0cf4c1760b


### Installing

This code also has 6 dependencies which need to be added to your libraries:
- TFT_eSPI: (install via library manager) control your tft screen easily. Don't forget to choose the right setup in the User_setup_select.h of the library folder.
- ArduinoJSON v6 (install via library manager) Handle json in a very effective way.
- PubSubClient: (install via library manager)  MQTT handling. It's a very robust pubsub client, perfect for iot projects.
- Button2: (install via library manager) A easy way to handle buttons events
- WifiManager: https://github.com/tzapu/WiFiManager/tree/development
This library will allow you to easily set your board to your wifi. You'll need to get the development branch to have esp32 support.
- UniversalTelegramBot: https://github.com/RomeHein/Universal-Arduino-Telegram-Bot/tree/editMessage. Telegram api for arduino. This is a fork from the main repo. I've made a pull request but it's not yet accepted. So you'll need that fork to make this program works correctly.



## Contributing

All kind of contributions are welcome. I'm very new to arduino world, so don't hesitate to give any advices

## Authors

* **Romain Cayzac** - *Initial work*

## License

This project is licensed under the GNU GPLv3 License - see the [LICENSE.md](LICENSE.md) file for details
