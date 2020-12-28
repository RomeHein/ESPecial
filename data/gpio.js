/*global settings, health, gpios, slaves, availableGpios, automations, versionsList, isSettingsMenuActivated, delay, request, displayNotification, closeAnySettings, createSpinner, createGpioControlRow, createI2cSlaveControlRow, createAutomationRow*/
// The edit panel for setting gpios
const openGpioSetting = (element) => {
    closeAnySettings();
    const gpio = gpios.find((gpio) => gpio.pin === +element.id.split("-")[1]);
    const row = document.getElementById("rowGpio-" + gpio.pin);
    if (!row.classList.value.includes("open")) {
        row.appendChild(createEditGpioPanel(gpio));
        row.classList.add("open");
        document.getElementById(`setGpioSave-${gpio.pin}`).checked = gpio.save;
        document.getElementById(`setGpioInvertState-${gpio.pin}`).checked = gpio.invert;
    }
};

const createPinOptions = (gpio) => {
    const usedGpiosPin = gpios.reduce((prev, _gpio) => {
        prev[_gpio.pin] = 1;
        return prev;
    }, {});
    const mode = gpio.mode || 1;
    const pinOptions = availableGpios.reduce((prev, availableGpio, availableGpioPin) => {
        if ((!usedGpiosPin[availableGpioPin] || availableGpioPin === +gpio.pin) && 
        (((+mode === 2 || +mode === -1 || +mode === -2) && !availableGpio.inputOnly) 
        || (+mode === -3 && availableGpio.adc) 
        || (+mode === -4 && availableGpio.touch) 
        || (+mode === -5 && availableGpio.dac) 
        || +mode === 1 || +mode === 4 || +mode === 5 || +mode === 8 || +mode === 9)) {
            prev += `<option value=${availableGpioPin} ${+gpio.pin === availableGpioPin ? "selected" : ""}>${availableGpioPin}</option>`;
        }
        return prev;
    },``);
    return {usedGpiosPin, pinOptions};
};

const createEditGpioPanel = (gpio) => {
    if (!gpio) {
        gpio = {
            pin: "new"
        };
    }
    const {usedGpiosPin, pinOptions} = createPinOptions(gpio);
    
    const sclPinOptions = availableGpios.reduce((prev,_, availableGpioPin) => {
        if ((!usedGpiosPin[availableGpioPin] && +availableGpioPin !== +gpio.sclpin) || +availableGpioPin === +gpio.sclpin) {
            prev += `<option ${+availableGpioPin === +gpio.sclpin ? "selected" : ""} value=${availableGpioPin}>${availableGpioPin}</option>`;
        }
        return prev;
    }, []);
    const channelOptions = [...Array(settings.general.maxChannels).keys()]
        .reduce((prev, channelNumber) => {
            return prev += `<option ${+gpio.channel === +channelNumber ? "selected" : ""} value=${channelNumber}>${channelNumber}</option>`;
        }
            , `<option ${+gpio.channel === -1 ? "selected" : ""} value=-1>-1</option>`);
    let child = document.createElement("div");
    child.classList.add("set");
    child.innerHTML = `<div class="set-inputs">
            <div class="row">
                <label for="setGpioLabel-${gpio.pin}">Label:</label>
                <input id="setGpioLabel-${gpio.pin}" type="text" name="label" value="${gpio.label || ""}" placeholder="Controller's title">
            </div>
            <div class="row">
                <label for="setGpioMode-${gpio.pin}">I/O mode:</label>
                <select onchange="updateGpioOptions(this)" id="setGpioMode-${gpio.pin}" name="mode">
                    <option ${+gpio.mode === 1 ? "selected" : ""} value=1>INPUT</option>
                    <option ${+gpio.mode === 4 ? "selected" : ""} value=4>PULLUP</option>
                    <option ${+gpio.mode === 5 ? "selected" : ""} value=5>INPUT PULLUP</option>
                    <option ${+gpio.mode === 8 ? "selected" : ""} value=8>PULLDOWN</option>
                    <option ${+gpio.mode === 9 ? "selected" : ""} value=9>INPUT PULLDOWN</option>
                    <option ${+gpio.mode === 2 ? "selected" : ""} value=2>OUTPUT (digital)</option>
                    <option ${+gpio.mode === -1 ? "selected" : ""} value=-1>PWM (ledc)</option>
                    <option ${+gpio.mode === -2 ? "selected" : ""} value=-2>I2C</option>
                    <option ${+gpio.mode === -3 ? "selected" : ""} value=-3>ADC (analog read)</option>
                    <option ${+gpio.mode === -4 ? "selected" : ""} value=-4>Touch</option>
                    <option ${+gpio.mode === -5 ? "selected" : ""} value=-5>DAC</option>
                </select>
            </div>
            <div class="row">
                <label for="setGpioPin-${gpio.pin}">Pin:</label>
                <select id="setGpioPin-${gpio.pin}" name="pin">${pinOptions}</select>
            </div>
            <div id="led-options" class="${+gpio.mode !== -1 ? "hidden" : ""}">
                <div class="row">
                    <label for="setGpioFrequency-${gpio.pin}">Frequency:</label>
                    <input id="setGpioFrequency-${gpio.pin}" type="text" name="frequency" value="${gpio.frequency || ""}" placeholder="Default to 50Hz if empty">
                </div>
                <div class="row">
                    <label for="setLedResolution-${gpio.pin}">Resolution:</label>
                    <input id="setLedResolution-${gpio.pin}" type="text" name="resolution" value="${gpio.resolution || ""}" placeholder="Default to 16bits if empty">
                </div>
                <div class="row">
                    <label for="setGpioChannel-${gpio.pin}">Channel:</label>
                    <select id="setGpioChannel-${gpio.pin}" name="channel" value="${gpio.channel}" placeholder="Default to 0">${channelOptions}</select>
                </div>
            </div>
            <div id="i2c-options" class="${+gpio.mode !== -2 ? "hidden" : ""}">
                <div class="row">
                    <label for="setGpioSclPin-${gpio.pin}">SCL pin:</label>
                    <select id="setGpioSclPin-${gpio.pin}" type="text" name="sclpin">${sclPinOptions}</select>
                </div>
                <div class="row">
                    <label for="setI2cFrequency-${gpio.pin}">Frequency:</label>
                    <input id="setI2cFrequency-${gpio.pin}" type="text" name="frequency" value="${gpio.frequency || ""}" placeholder="Default to 50Hz if empty">
                </div>
            </div>
            <div id="adc-options" class="${+gpio.mode !== -3 ? "hidden" : ""}">
                <div class="row info">
                    As ESPecial is using Wifi, ADC2 can't be use. As a result GPIOs 0, 2, 4, 12 - 15 and 25 - 27 won't be available for this mode. Depending on your ESP32 board, other GPIOs that should be available for analog read might not be usable. Read <a href='https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html'>this</a> for more info.
                </div>
                <div class="row">
                    <label for="setAdcResolution-${gpio.pin}">Resolution:</label>
                    <input id="setAdcResolution-${gpio.pin}" type="text" name="resolution" value="${gpio.resolution || ""}" placeholder="Default to 12 bits if empty">
                </div>
            </div>
            <div class="row ${gpio.mode < 0 ? "hidden" : ""}" id="setGpioInvertStateRow">
                <div class="switch">
                    <label for="setGpioInvertState-${gpio.pin}">Invert state:</label>
                    <input id="setGpioInvertState-${gpio.pin}" type="checkbox" class="switch-input" value="${gpio.invert}">
                    <label class="slider" for="setGpioInvertState-${gpio.pin}"></label>
                </div>
            </div>
            <div class="row ${+gpio.mode !== -1 && +gpio.mode !== 2 && +gpio.mode !== -5 ? "hidden" : ""}" id="setGpioSaveRow">
                <div class="switch">
                    <label for="setGpioSave-${gpio.pin}">Save state:</label>
                    <input id="setGpioSave-${gpio.pin}" type="checkbox" class="switch-input" value="${gpio.save}">
                    <label class="slider" for="setGpioSave-${gpio.pin}"></label>
                </div>
            </div>
        </div>
        <div class="set-buttons">
            <a onclick="closeAnySettings()" id="cancelGpio-${gpio.pin}" class="btn cancel">cancel</a>
            ${gpio.pin === "new" ? "" : `<a onclick="deleteGpio(this)" id="deleteGpio-${gpio.pin}" class="btn delete">delete</a>`}
            <a onclick="saveGpioSetting(this)" id="saveGpio-${gpio.pin}" class="btn save">save</a>
        </div>`;
    return child;
};

const updateGpioOptions = (element) => {
    const pin = +element.id.split("-")[1] || "new";
    const option = +document.getElementById(`setGpioMode-${pin}`).value;
    const selectedMode = document.getElementById(`setGpioMode-${pin || "new"}`).value;
    // Led mode
    if (option === -1) {
        document.getElementById("setGpioSaveRow").classList.remove("hidden");
        document.getElementById("led-options").classList.remove("hidden");
        document.getElementById("setGpioInvertStateRow").classList.add("hidden");
        document.getElementById("i2c-options").classList.add("hidden");
        document.getElementById("adc-options").classList.add("hidden");
    // I2C mode
    } else if (option === -2) {
        document.getElementById("setGpioSaveRow").classList.add("hidden");
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("setGpioInvertStateRow").classList.add("hidden");
        document.getElementById("i2c-options").classList.remove("hidden");
        document.getElementById("adc-options").classList.add("hidden");
    // ADC mode
    }  else if (option === -3) {
        document.getElementById("setGpioSaveRow").classList.add("hidden");
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("setGpioInvertStateRow").classList.add("hidden");
        document.getElementById("i2c-options").classList.add("hidden");
        document.getElementById("adc-options").classList.remove("hidden");
    // DAC mode
    }  else if (option === -5) {
        document.getElementById("setGpioSaveRow").classList.remove("hidden");
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("setGpioInvertStateRow").classList.add("hidden");
        document.getElementById("i2c-options").classList.add("hidden");
        document.getElementById("adc-options").classList.add("hidden");
    } else {
        if (option === 2) {
            document.getElementById("setGpioSaveRow").classList.remove("hidden");
        } else {
            document.getElementById("setGpioSaveRow").classList.add("hidden");
        }
        document.getElementById("setGpioInvertStateRow").classList.remove("hidden");
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("i2c-options").classList.add("hidden");
        document.getElementById("adc-options").classList.add("hidden");
    }
    // Update available pin for selected mode
    document.getElementById(`setGpioPin-${pin}`).innerHTML = createPinOptions({mode: selectedMode}).pinOptions;
};
const switchGpioState = async (element) => {
    try {
        const gpio = gpios.find((gpio) => gpio.pin === +element.id.split("-")[1]);
        element.classList.add("disable");
        if (gpio.mode > 0) {
            const isOn = element.classList.value.includes("on");
            await fetch(`${window.location.href}gpio/value?pin=${gpio.pin}&value=${(isOn && !gpio.invert) || (!isOn && gpio.invert) ? 0 : 1}`);
            element.classList.remove(isOn ? "on" : "off");
            element.classList.add(isOn ? "off" : "on");
            element.innerText = (isOn ? "off" : "on");
        // if PWM of DAC, send value directly
        } else if (gpio.mode === -1 || gpio.mode === -5) {
            await fetch(`${window.location.href}gpio/value?pin=${gpio.pin}&value=${element.value}`);
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
    element.classList.remove("disable");
};
const addGpio = () => {
    closeAnySettings();
    const topBar = document.getElementById("gpio-header-bar");
    if (!topBar.classList.value.includes("open")) {
        topBar.appendChild(createEditGpioPanel());
        topBar.classList.add("open");
    }
};
const saveGpioSetting = async (element) => {
    const gpioPin = element.id.split("-")[1];
    const isNew = (gpioPin === "new");
    let req = { settings: {} };
    const newPin = document.getElementById(`setGpioPin-${gpioPin}`).value;
    req.settings.label = document.getElementById(`setGpioLabel-${gpioPin}`).value;
    req.settings.mode = document.getElementById(`setGpioMode-${gpioPin}`).value;
    req.settings.sclpin = document.getElementById(`setGpioSclPin-${gpioPin}`).value;
    if (+req.settings.mode === -1) {
        req.settings.frequency = document.getElementById(`setGpioFrequency-${gpioPin}`).value;
        req.settings.resolution = document.getElementById(`setLedResolution-${gpioPin}`).value;
    } else if (+req.settings.mode === -2) {
        req.settings.frequency = document.getElementById(`setI2cFrequency-${gpioPin}`).value;
    } else if (+req.settings.mode === -3) {
        req.settings.resolution = document.getElementById(`setAdcResolution-${gpioPin}`).value;
    }
    req.settings.channel = document.getElementById(`setGpioChannel-${gpioPin}`).value;
    req.settings.save = document.getElementById(`setGpioSave-${gpioPin}`).checked;
    req.settings.invert = document.getElementById(`setGpioInvertState-${gpioPin}`).checked;
    if (newPin && +newPin !== +gpioPin) {
        req.settings.pin = +newPin;
    }
    if (!isNew) {
        req.pin = gpioPin;
    }
    try {
        if (!req.settings.mode || !req.settings.label) {
            throw new Error("Parameters missing, please fill all the inputs");
        }
        const newSetting = await request("gpio",req,isNew);
        let column = document.getElementById("gpios");
        if (isNew) {
            gpios.push(newSetting);
            column.insertBefore(createGpioControlRow(newSetting), column.firstChild);
            closeAnySettings();
        } else {
            gpios = gpios.map((oldGpio) => (+oldGpio.pin === +gpioPin) ? { ...newSetting } : oldGpio);
            let oldRow = document.getElementById("rowGpio-" + gpioPin);
            column.replaceChild(createGpioControlRow(newSetting), oldRow);
        }
        await displayNotification("Gpio saved", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const deleteGpio = async (element) => {
    const gpioPin = element.id.split("-")[1];
    try {
        await fetch(`${window.location.href}gpio?pin=${gpioPin}`, { method: "DELETE" });
        gpios = gpios.filter((gpio) => +gpio.pin !== +gpioPin);
        slaves = slaves.filter((slave) => +slave.mPin !== +gpioPin);
        closeAnySettings();
        document.getElementById("rowGpio-" + gpioPin).remove();
        await displayNotification("Gpio removed", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};

// I2C
const closeScan = (element) => {
    const gpioPin = element.id.split("-")[1];
    document.getElementById(`scanResults-${gpioPin}`).remove();
};
const closeI2cSlaveSettings = (id) => {
    document.getElementById(`i2cSlaveSettings-${id}`).remove();
};
const openI2cSlaveSettings = (element) => {
    const infos = element.id.split("-");
    const gpioPin = infos[1];
    const id = (infos.length === 3 ? element.id.split("-")[2] : "new");
    let label = "";
    let commands = "";
    let octetToRequest = 0;
    let save = 0;
    // Find the right slave attached to gpioPin
    let slave = slaves.find((s) => +s.id === id);
    if (slave) {
        label = slave.label;
        commands = slave.commands.join(",");
        octetToRequest = slave.octetRequest;
        save = slave.save;
    }
    const row = slave ? document.getElementById(`rowSlave-${id}`) : document.getElementById(`scanResult-${gpioPin}`);
    let child = document.createElement("div");
    child.innerHTML = `<div class="column slave-settings set" id="i2cSlaveSettings-${id}">
        <div class="set-inputs">
            <div class="row">
                <label for="setI2cSlaveLabel-${id}">Slave label:</label>
                <input id="setI2cSlaveLabel-${id}" type="text" name="label" value="${label}" placeholder="Controller's title">
            </div>
            <div class="row">
                <label for="setI2cSlaveCommands-${id}">Command:</label>
                <input id="setI2cSlaveCommands-${id}" type="text" name="register" value="${commands}" placeholder="Slave's command">
            </div>
            <div class="row">
                <label for="setI2cSlaveOctet-${id}">Request octet number:</label>
                <input id="setI2cSlaveOctet-${id}" type="number" name="octet" value="${octetToRequest}" placeholder="Slave's command">
            </div>
            <div class="row ${octetToRequest ? "hidden" : ""}">
                <div class="switch">
                    <label for="setI2cSlaveSave-${id}">Save:</label>
                    <input id="setI2cSlaveSave-${id}" type="checkbox" class="switch-input" value="${save}">
                    <label class="slider" for="setI2cSlaveSave-${id}"></label>
                </div>
            </div>
        </div>
        <div class="btn-container">
            <a onclick="closeI2cSlaveSettings(${id})" id="closeI2cSlaveSettings-${id}" class="btn cancel">close</a>
            <a onclick="deleteI2cSlave(this)" id="deleteI2cSlaveSettings-${id}" class="btn delete ${slave ? "" : "hidden"}">delete</a>
            <a onclick="saveI2cSlaveSettings(this)" id="saveI2cSlaveSettings-${id}" class="btn on">${slave ? "edit" : "add"}</a>
        </div>
    </div>`;
    row.appendChild(child.firstChild);
};
const updateI2cSlaveOptions = (element) => {
    const selectType = +element.value;
    if (selectType === 1) {
        document.getElementById("lcd-slave-options").classList.remove("hidden");
    } else {
        document.getElementById("lcd-slave-options").classList.add("hidden");
    }
};
const saveI2cSlaveSettings = async (element) => {
    const id = element.id.split("-")[1];
    const isNew = (id === "new");
    let req = { settings: {} };
    if (isNew) {
        req.settings.address = +element.parentElement.parentElement.parentElement.firstChild.textContent;
        req.settings.mPin = +element.parentElement.parentElement.parentElement.id.split("-")[1];
    } else {
        req.id = id;
    }
    req.settings.label = document.getElementById(`setI2cSlaveLabel-${id}`).value;
    req.settings.commands = document.getElementById(`setI2cSlaveCommands-${id}`).value.split(",");
    req.settings.octetRequest = +document.getElementById(`setI2cSlaveOctet-${id}`).value;
    req.settings.save = +document.getElementById(`setI2cSlaveSave-${id}`).checked;
    try {
        if (!req.settings.label) {
            throw new Error("Parameters missing, please fill at least label and type inputs.");
        }
        const newSetting = await request("slave",req,isNew);
        let row = document.getElementById(`rowGpio-${newSetting.mPin}`);
        if (isNew) {
            slaves.push(newSetting);
            row.insertBefore(createI2cSlaveControlRow(newSetting), row.lastChild);
            closeI2cSlaveSettings(id);
        } else {
            slaves = slaves.map((oldSlave) => (+oldSlave.id === +id) ? { ...newSetting } : oldSlave);
            let oldRow = document.getElementById("rowSlave-" + id);
            row.replaceChild(createI2cSlaveControlRow(newSetting), oldRow);
        }
        await displayNotification("Slave saved", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const deleteI2cSlave = async (element) => {
    const id = element.id.split("-")[1];
    try {
        await fetch(`${window.location.href}slave?id=${id}`, { method: "DELETE" });
        let row = document.getElementById("rowSlave-" + id);
        closeI2cSlaveSettings(id);
        row.remove();
        await displayNotification("Slave removed", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const createScanResult = (gpioPin, scanResults) => {
    let child = document.createElement("div");
    const headRow = `<div class="row" id="scanResultHead-${gpioPin}">Adresses:
            <div class="btn-container">
                <a onclick="closeScan(this)" id="closeScan-${gpioPin}" class="btn delete">x</a>
            </div>
    </div>`;
    let rows = `<div class="row" id="scanResult-${gpioPin}">No device attached</div>`;
    if (scanResults) {
        rows = scanResults.reduce((prev, scanResult) => {
            return prev + `<div class="row" id="scanResult-${gpioPin}">${scanResult}
                <div class="btn-container">
                        <a onclick="openI2cSlaveSettings(this)" id="editI2c-${gpioPin}" class="btn edit">set</a>
                    </div>
                </div>`;
        }, "");
    }
    child.innerHTML = `<div class="column scan-results" id="scanResults-${gpioPin}">${headRow}${rows}</div>`;
    return child.firstChild;
};
const scan = async (element) => {
    const gpioPin = element.id.split("-")[1];
    try {
        const res = await fetch(`${window.location.href}gpio/scan?pin=${gpioPin}`);
        const addresses = await res.json();
        const gpioRow = document.getElementById(`rowGpio-${gpioPin}`);
        gpioRow.appendChild(createScanResult(gpioPin, addresses));

    } catch (err) {
        await displayNotification(err, "error");
    }
};