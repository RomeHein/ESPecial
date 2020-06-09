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
const createEditGpioPanel = (gpio) => {
    if (!gpio) {
        gpio = {
            pin: "new"
        };
    }
    let modeOptions = `<option ${+gpio.mode === 1 ? "selected" : ""} value=1>INPUT</option><option ${+gpio.mode === 4 ? "selected" : ""} value=4>PULLUP</option><option ${+gpio.mode === 5 ? "selected" : ""} value=5>INPUT PULLUP</option><option ${+gpio.mode === 8 ? "selected" : ""} value=8>PULLDOWN</option><option ${+gpio.mode === 9 ? "selected" : ""} value=9>INPUT PULLDOWN</option>`;
    const pinOptions = availableGpios.reduce((prev, availableGpio) => {
        if ((!gpios.find((_gpio) => +_gpio.pin === +availableGpio.pin) && +availableGpio.pin !== +gpio.pin) || +availableGpio.pin === +gpio.pin) {
            // Complete the mode select input while we are here...
            if (+availableGpio.pin === +gpio.pin && !availableGpio.inputOnly) {
                modeOptions += `<option ${+gpio.mode === 2 ? "selected" : ""} value=2>OUTPUT</option>`;
                modeOptions += `<option ${+gpio.mode === -1 ? "selected" : ""} value=-1>LED CONTROL</option>`;
                modeOptions += `<option ${+gpio.mode === -2 ? "selected" : ""} value=-2>I2C</option>`;
            }
            prev += `<option ${+availableGpio.pin === +gpio.pin ? "selected" : ""} value=${availableGpio.pin}>${availableGpio.pin}</option>`;
        }
        return prev;
    }, []);
    const sclPinOptions = availableGpios.reduce((prev, availableGpio) => {
        if ((!gpios.find((_gpio) => +_gpio.pin === +availableGpio.pin) && +availableGpio.pin !== +gpio.sclpin) || +availableGpio.pin === +gpio.sclpin) {
            prev += `<option ${+availableGpio.pin === +gpio.sclpin ? "selected" : ""} value=${availableGpio.pin}>${availableGpio.pin}</option>`;
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
                <label for="setGpioPin-${gpio.pin}">Pin:</label>
                <select id="setGpioPin-${gpio.pin}" name="pin" onchange="updateModeOptions('${gpio.pin}')">${pinOptions}</select>
            </div>
            <div class="row">
                <label for="setGpioLabel-${gpio.pin}">Label:</label>
                <input id="setGpioLabel-${gpio.pin}" type="text" name="label" value="${gpio.label || ""}" placeholder="Controller's title">
            </div>
            <div class="row">
                <label for="setGpioMode-${gpio.pin}">I/O mode:</label>
                <select onchange="updateGpioOptions(this)" id="setGpioMode-${gpio.pin}" name="mode">${modeOptions}</select>
            </div>
            <div id="led-options" class="${+gpio.mode !== -1 ? "hidden" : ""}">
                <div class="row">
                    <label for="setGpioFrequency-${gpio.pin}">Frequency:</label>
                    <input id="setGpioFrequency-${gpio.pin}" type="text" name="frequency" value="${gpio.frequency || ""}" placeholder="Default to 50Hz if empty">
                </div>
                <div class="row">
                    <label for="setGpioResolution-${gpio.pin}">Resolution:</label>
                    <input id="setGpioResolution-${gpio.pin}" type="text" name="resolution" value="${gpio.resolution || ""}" placeholder="Default to 16bits if empty">
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
            <div class="row ${gpio.mode < 0 ? "hidden" : ""}" id="setGpioInvertStateRow">
                <label for="setGpioInvertState-${gpio.pin}">Invert state:</label>
                <input type="checkbox" name="invert" id="setGpioInvertState-${gpio.pin}" value="${gpio.invert}">
            </div>
            <div class="row ${+gpio.mode !== -1 && +gpio.mode !== 2 ? "hidden" : ""}" id="setGpioSaveRow">
                <label for="setGpioSave-${gpio.pin}">Save state:</label>
                <input type="checkbox" name="save" id="setGpioSave-${gpio.pin}" value="${gpio.save}">
            </div>
            </div>
        <div class="set-buttons">
            <a onclick="closeAnySettings()" id="cancelGpio-${gpio.pin}" class="btn cancel">cancel</a>
            ${gpio.pin === "new" ? "" : `<a onclick="deleteGpio(this)" id="deleteGpio-${gpio.pin}" class="btn delete">delete</a>`}
            <a onclick="saveGpioSetting(this)" id="saveGpio-${gpio.pin}" class="btn save">save</a>
        </div>`;
    return child;
};
// Change the input of available mode for a given pin
const updateModeOptions = (pin) => {
    const selectPin = document.getElementById(`setGpioPin-${pin || "new"}`);
    const selectMode = document.getElementById(`setGpioMode-${pin || "new"}`);

    const availableGpioInfos = availableGpios.find((gpio) => +gpio.pin === +selectPin.value);
    if (availableGpioInfos.inputOnly && selectMode.childElementCount === 8) {
        selectMode.removeChild(selectMode.lastChild);
        selectMode.removeChild(selectMode.lastChild);
        selectMode.removeChild(selectMode.lastChild);
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("setGpioSaveRow").classList.add("hidden");
    } else if (!availableGpioInfos.inputOnly && selectMode.childElementCount === 5) {
        let outputOption = document.createElement("div");
        outputOption.innerHTML = "<option value=2>OUTPUT</option>";
        let ledOption = document.createElement("div");
        ledOption.innerHTML = "<option value=-1>LED CONTROL</option>";
        let i2coption = document.createElement("div");
        i2coption.innerHTML = "<option value=-2>I2C</option>";
        document.getElementById("setGpioSaveRow").classList.remove("hidden");
        selectMode.appendChild(outputOption.firstChild);
        selectMode.appendChild(ledOption.firstChild);
        selectMode.appendChild(i2coption.firstChild);
    }
};
const updateGpioOptions = (element) => {
    const pin = +element.id.split("-")[1] || "new";
    const option = +document.getElementById(`setGpioMode-${pin}`).value;
    // Led mode
    if (option === -1) {
        document.getElementById("led-options").classList.remove("hidden");
        document.getElementById("setGpioSaveRow").classList.remove("hidden");
        document.getElementById("setGpioInvertStateRow").classList.add("hidden");
        // I2C mode
    } else if (option === -2) {
        document.getElementById("setGpioSaveRow").classList.add("hidden");
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("setGpioInvertStateRow").classList.add("hidden");
        document.getElementById("i2c-options").classList.remove("hidden");
    } else {
        if (option === 2) {
            document.getElementById("setGpioSaveRow").classList.remove("hidden");
        } else {
            document.getElementById("setGpioSaveRow").classList.add("hidden");
        }
        document.getElementById("setGpioInvertStateRow").classList.remove("hidden");
        document.getElementById("led-options").classList.add("hidden");
        document.getElementById("i2c-options").classList.add("hidden");

    }
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
        } else if (gpio.mode === -1) {
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
    } else if (+req.settings.mode === -2) {
        req.settings.frequency = document.getElementById(`setI2cFrequency-${gpioPin}`).value;
    }
    req.settings.resolution = document.getElementById(`setGpioResolution-${gpioPin}`).value;
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
                <label for="setI2cSlaveSave-${id}">Save:</label>
                <input id="setI2cSlaveSave-${id}" type="checkbox" name="save" value="${save}">
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