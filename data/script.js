var settings = {};
var health = {};
var gpios = [];
var slaves = [];
var availableGpios = [];
var automations = [];
var isSettingsMenuActivated = false;
const delay = (ms => new Promise(resolve => setTimeout(resolve, ms)));
// Restart ESP
const restart = async () => {
    try {
        const res = await fetch(window.location.href + 'restart');
        blocker.classList.add('hidden');
        location.reload();
    } catch (err) {
        blocker.classList.add('hidden');
        console.error(`Error: ${err}`);
    }
}
const switchIndicatorState = (indicatorId, stateCode) => {
    if (stateCode == 1) {
        document.getElementById(indicatorId).classList.add('ok');
        document.getElementById(indicatorId).classList.remove('error');
    } else if (stateCode == 0) {
        document.getElementById(indicatorId).classList.remove('ok');
        document.getElementById(indicatorId).classList.remove('error');
    } else {
        document.getElementById(indicatorId).classList.remove('ok');
        document.getElementById(indicatorId).classList.add('error');
    }
}
const fetchServicesHealth = async () => {
    try {
        const res = await fetch(window.location.href + 'health');
        health = await res.json();
        switchIndicatorState('telegram-indicator', health.telegram);
        switchIndicatorState('mqtt-indicator', health.mqtt);
    } catch (err) {
        console.error(`Error: ${err}`);
    }
}
// Update software
const fillUpdateInput = (element) => {
    const fileName = element.value.split('\\');
    document.getElementById('file-update-label').innerHTML = fileName[fileName.length-1];
    document.getElementById('submit-update-file').classList.remove('disable');
};
const submitUpdate = async (e) => {
    e.preventDefault();
    const blocker = document.getElementById('blocker');
    blocker.classList.remove('hidden');
    document.getElementById('blocker-title').innerText = 'Loading new software, please wait...';
    const form = document.getElementById('upload-form');
    const data = new FormData(form);
    try {
        const res = await fetch(window.location.href + 'update', {
            method: 'POST',
            body: data
        });
        document.getElementById('blocker-title').innerText = 'Restarting device, please wait...';
        await delay(2000);
        location.reload();
    } catch (err) {
        blocker.classList.add('hidden');
        console.error(`Error: ${err}`);
    }
};
// Telegram
const submitTelegram = async (e) => {
    e.preventDefault();
    const active = +document.getElementById(`telegram-active`).checked;
    const token = document.getElementById(`telegram-token`).value;
    const users = document.getElementById(`telegram-users`).value.split(',').map(id => +id);
    if (token != settings.telegram.token || active != settings.telegram.active || (JSON.stringify(users.sort()) !== JSON.stringify(settings.telegram.users.sort()))) {
        try {
            const res = await fetch(window.location.href + 'telegram', {
                method: 'POST',
                headers: { contentType: false, processData:false },
                body: JSON.stringify({token, active, users})
            });
            settings.telegram = {active, token};
        } catch (err) {
            console.error(`Error: ${err}`);
        }
    }
};
// MQTT
const submitMqtt = async (e) => {
    e.preventDefault();
    const active = document.getElementById(`mqtt-active`).checked;
    const fn = document.getElementById(`mqtt-fn`).value;
    const host = document.getElementById(`mqtt-host`).value;
    const port = document.getElementById(`mqtt-port`).value;
    const user = document.getElementById(`mqtt-user`).value;
    const password = document.getElementById(`mqtt-password`).value;
    const topic = document.getElementById(`mqtt-topic`).value;
    try {
        await fetch(window.location.href + 'mqtt', {
            method: 'POST',
            headers: { contentType: false, processData:false },
            body: JSON.stringify({active, fn, host, port, user, password, topic})
        });
        settings.mqtt = {active, fn, host, port, user, password, topic};
        await mqttConnect();

    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const mqttConnect = async () => {
    const loader = document.getElementById(`mqtt-retry-loader`);
    const retryButton = document.getElementById(`mqtt-retry`);
    const retryText = retryButton.firstElementChild;
    try {
        retryText.classList.add('hidden');
        loader.classList.remove('hidden');
        await fetch(window.location.href + 'mqtt/retry');
        while (health.mqtt === 0) {
            await fetchServicesHealth();
            await delay(1000); //avoid spaming esp32
        }
        if (health.mqtt == 1) {
            retryButton.classList.add('hidden');
        } else {
            retryButton.classList.remove('hidden');
        }
    } catch(err) {
        console.error(`Error: ${err}`);
    }
    loader.classList.add('hidden');
    retryText.classList.remove('hidden');
};
// Gpios
const fetchGpios = async () => {
    try {
        const res = await fetch(window.location.href + 'gpios');
        const newGpios = await res.json();
        if (newGpios && newGpios.length) {
            gpios = newGpios;
        }
    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const fetchAvailableGpios = async () => {
    try {
        const res = await fetch(window.location.href + 'gpios/available');
        availableGpios = await res.json();
    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const switchGpioState = async (element) => {
    try {
        const gpio = gpios.find(gpio => gpio.pin === +element.id.split('-')[1])
        if (gpio.mode>0) {
            const isOn = element.classList.value.includes('on');
            await fetch(`${window.location.href}gpio/value?pin=${gpio.pin}&value=${(isOn && !gpio.invert) || (!isOn && gpio.invert) ? 0 : 1}`);
            element.classList.remove(isOn ? 'on' : 'off');
            element.classList.add(isOn ? 'off' : 'on');
            element.innerText = (isOn ? 'off' : 'on');
        } else if (gpio.mode===-1) {
            await fetch(`${window.location.href}gpio/value?pin=${gpio.pin}&value=${element.value}`);
        }
    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const addGpio = () => {
    closeAnySettings();
    const topBar = document.getElementById('gpio-header-bar');
    if (!topBar.classList.value.includes('open')) {
        topBar.appendChild(createEditGpioPanel());
        topBar.classList.add('open');
    }
};
const deleteGpio = async (element) => {
    const gpioPin = element.id.split('-')[1];
    try {
        await fetch(`${window.location.href}gpio?pin=${gpioPin}`, {method: 'DELETE'});
        gpios = gpios.filter(gpio => gpio.pin != gpioPin)
        closeAnySettings();
        document.getElementById('rowGpio-' + gpioPin).remove();
    } catch (err) {
        console.error(err);
    }
};
// I2C
const fetchI2cSlaves = async () => {
    try {
        const res = await fetch(window.location.href + 'slaves');
        const newSlaves = await res.json();
        if (newSlaves && newSlaves.length) {
            slaves = newSlaves;
        }
    } catch (err) {
        console.error(`Error: ${err}`);
    }
}

const sendI2cSlaveCommand = async (element, write) => {
    const id = element.id.split('-')[1];
    const inputElement = document.getElementById(`i2cSlaveData-${id}`);
    try {
        const res = await fetch(window.location.href + `slave/command?id=${id}` + (write && inputElement.value ? `&value=${+inputElement.value}`: ''));
        if (!write && res) {
            const data = await res.json();
            inputElement.value = data;
        }
    } catch (err) {
        console.error(`Error: ${err}`);
    }
}

const deleteI2cSlave = async (element) => {
    const id = element.id.split('-')[1];
    try {
        await fetch(`${window.location.href}slave?id=${id}`, {method: 'DELETE'});
        await fetchAutomations();
        let row = document.getElementById('rowSlave-' + id);
        closeI2cSlaveSettings(id);
        row.remove();
    } catch (err) {
        console.error(err);
    }
}

// Automations
const fetchAutomations = async () => {
    try {
        const res = await fetch(window.location.href + 'automations');
        const newAutomations = await res.json();
        if (newAutomations && newAutomations.length) {
            automations = newAutomations;
        }
    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const runAutomation = async (element) => {
    const automationId = element.id.split('-')[1];
    try {
        await fetch(window.location.href + 'automation/run?id='+automationId);
    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const addAutomation = () => {
    closeAnySettings();
    const topBar = document.getElementById('automation-header-bar');
    if (!topBar.classList.value.includes('open')) {
        topBar.appendChild(createEditAutomationPanel());
        topBar.classList.add('open');
    }
};

const deleteAutomation = async (element) => {
    const automationId = element.id.split('-')[1];
    try {
        await fetch(`${window.location.href}automation?id=${automationId}`, {method: 'DELETE'});
        await fetchAutomations();
        let row = document.getElementById('rowAutomation-' + automationId);
        closeAnySettings();
        row.remove()
    } catch (err) {
        console.error(err);
    }
};

const scan = async (element) => {
    const gpioPin = element.id.split('-')[1];
    try {
        const res = await fetch(`${window.location.href}gpio/scan?pin=${gpioPin}`);
        const addresses = await res.json();
        const gpioRow = document.getElementById(`rowGpio-${gpioPin}`);
        gpioRow.appendChild(createScanResult(gpioPin, addresses));
        
    } catch (err) {
        console.error(err);
    }
}
// Settings
const fetchSettings = async () => {
    try {
        const res = await fetch(window.location.href + 'settings');
        const s = await res.json();
        if (s) {
            // Save settings
            settings = s;
            // Add them to the dom
            document.getElementById(`telegram-active`).checked = settings.telegram.active;
            document.getElementById(`telegram-token`).value = settings.telegram.token;
            document.getElementById(`telegram-users`).value = settings.telegram.users.filter(userId => userId != 0).join(',');
            document.getElementById(`mqtt-active`).checked = settings.mqtt.active;
            document.getElementById(`mqtt-fn`).value = settings.mqtt.fn;
            document.getElementById(`mqtt-host`).value = settings.mqtt.host;
            document.getElementById(`mqtt-port`).value = settings.mqtt.port;
            document.getElementById(`mqtt-user`).value = settings.mqtt.user;
            document.getElementById(`mqtt-password`).value = settings.mqtt.password;
            document.getElementById(`mqtt-topic`).value = settings.mqtt.topic;
        }
    } catch (err) {
        console.error(`Error: ${err}`);
    }
};
const saveGpioSetting = async (element) => {
    const gpioPin = element.id.split('-')[1];
    const isNew = (gpioPin === 'new');
    let req = { settings: {} };
    const newPin = document.getElementById(`setGpioPin-${gpioPin}`).value;
    req.settings.label = document.getElementById(`setGpioLabel-${gpioPin}`).value;
    req.settings.mode = document.getElementById(`setGpioMode-${gpioPin}`).value;
    req.settings.sclpin = document.getElementById(`setGpioSclPin-${gpioPin}`).value;
    if (req.settings.mode == -1) {
        req.settings.frequency  = document.getElementById(`setGpioFrequency-${gpioPin}`).value;
    } else if (req.settings.mode == -2) {
        req.settings.frequency  = document.getElementById(`setI2cFrequency-${gpioPin}`).value;
    }
    req.settings.resolution = document.getElementById(`setGpioResolution-${gpioPin}`).value;
    const channel = document.getElementById(`setGpioChannel-${gpioPin}`).value;
    req.settings.channel = document.getElementById(`setGpioChannel-${gpioPin}`).value;
    req.settings.save = document.getElementById(`setGpioSave-${gpioPin}`).checked;
    req.settings.invert = document.getElementById(`setGpioInvertState-${gpioPin}`).checked;
    if (newPin && newPin != gpioPin) {
        req.settings.pin = +newPin;
    }
    if (!isNew) {
        req.pin = gpioPin;
    }
    try {
        if (!req.settings.mode || !req.settings.label) {
            throw new Error('Parameters missing, please fill all the inputs');
        }
        const resp = await fetch(`${window.location.href}gpio`, {
            method: isNew ? 'POST' : 'PUT',
            headers: {
                'Accept': 'application/json',
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(req)
        });
        const newSetting = await resp.json();
        let column = document.getElementById('gpios');
        if (isNew) {
            gpios.push(newSetting);
            column.insertBefore(createGpioControlRow(newSetting), column.firstChild);
            closeAnySettings();
        } else {
            gpios = gpios.map(oldGpio => (oldGpio.pin == +gpioPin) ? { ...newSetting } : oldGpio);
            let oldRow = document.getElementById('rowGpio-' + gpioPin);
            column.replaceChild(createGpioControlRow(newSetting), oldRow);
        }
    } catch (err) {
        console.error(err);
    }
};
const saveAutomationSetting = async (element) => {
    const automationId = element.id.split('-')[1];
    const isNew = (automationId === 'new');
    let req = { settings: {} };
    if (!isNew) {
        req.id = automationId;
    }
    req.settings.label = document.getElementById(`setAutomationLabel-${automationId}`).value;
    req.settings.conditions = [...document.getElementById(`condition-editor-result`).childNodes].map(rowElement => {
        const id = +rowElement.id.split('-')[1];
        return [
            +document.getElementById(`addGpioCondition-${id}`).value,
            +document.getElementById(`addSignCondition-${id}`).value,
            +document.getElementById(`addValueCondition-${id}`).value.split(':').join(''),
            +document.getElementById(`addNextSignCondition-${id}`).value,
        ];
    })
    req.settings.actions = [...document.getElementById(`action-editor-result`).childNodes].map(rowElement => {
        const id = +rowElement.id.split('-')[1];
        const type = document.getElementById(`addTypeAction-${id}`).value;
        if (type == 5) {
            return [type, document.getElementById(`addHTTPMethod-${id}`).value,
                document.getElementById(`addHTTPAddress-${id}`).value,
                document.getElementById(`addHTTPBody-${id}`).value
            ];
        } else if (type == 6) {
            return [type, document.getElementById(`addAutomation-${id}`).value,'',''];
        } else {
            return [type, document.getElementById(`addValueAction-${id}`).value,
                document.getElementById(`addGpioAction-${id}`).value,
                document.getElementById(`addSignAction-${id}`).value
            ];
        }
    })
    req.settings.autoRun = document.getElementById(`setAutomationAutoRun-${automationId}`).checked;
    req.settings.debounceDelay = +document.getElementById(`setAutomationDebounceDelay-${automationId}`).value;
    req.settings.loopCount = +document.getElementById(`setAutomationLoopCount-${automationId}`).value;
    try {
        if (!req.settings.label) {
            throw new Error('Parameters missing, please fill at least label and type inputs.');
        }
        const resp = await fetch(`${window.location.href}automation`, {
            method: isNew ? 'POST' : 'PUT',
            headers: {
                'Accept': 'application/json',
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(req)
        });
        const newSetting = await resp.json();
        let column = document.getElementById('automations');
        if (isNew) {
            automations.push(newSetting);
            column.insertBefore(createAutomationRow(newSetting), column.firstChild);
            closeAnySettings();
        } else {
            automations = automations.map(oldAutomation => (oldAutomation.id == +automationId) ? {...newSetting} : oldAutomation);
            let oldRow = document.getElementById('rowAutomation-' + automationId);
            column.replaceChild(createAutomationRow(newSetting), oldRow);
        }
    } catch (err) {
        console.error(err);
    }
};
const openGpioSetting = (element) => {
    closeAnySettings();
    const gpio = gpios.find(gpio => gpio.pin === +element.id.split('-')[1]);
    const row = document.getElementById('rowGpio-' + gpio.pin);
    if (!row.classList.value.includes('open')) {
        row.appendChild(createEditGpioPanel(gpio));
        row.classList.add('open');
        document.getElementById(`setGpioSave-${gpio.pin}`).checked = gpio.save;
        document.getElementById(`setGpioInvertState-${gpio.pin}`).checked = gpio.invert;
    }
};
const openAutomationSetting = (element) => {
    closeAnySettings();
    const automation = automations.find(automation => automation.id === +element.id.split('-')[1]);
    const row = document.getElementById('rowAutomation-' + automation.id);
    if (!row.classList.value.includes('open')) {
        row.appendChild(createEditAutomationPanel(automation));
        // Fill conditions
        automation.conditions.forEach(condition => {
            // Check if the condition contains a valid math operator sign
            if (condition[1]) {
                addConditionEditor(condition);
            }
        })
        // Fill actions
        automation.actions.forEach(action => {
            // Check if the action contains a valid type
            if (action[0]) {
                addActionEditor(action);
            }
        })
        row.classList.add('open');
        document.getElementById(`setAutomationAutoRun-${automation.id}`).checked = automation.autoRun;
    }
};
const closeAnySettings = () => {
    document.querySelectorAll('.open').forEach(row => {
        row.classList.remove('open');
        row.removeChild(row.lastChild);
    });
};
// Element creation
const switchPage = () => {
    isSettingsMenuActivated = !isSettingsMenuActivated;
    if (isSettingsMenuActivated) {
        document.getElementById('go-to-settings-button').classList.add('hidden');
        document.getElementById('gpio-container').classList.add('hidden');
        document.getElementById('automation-container').classList.add('hidden');
        document.getElementById('home-button').classList.remove('hidden');
        document.getElementById('setting-container').classList.remove('hidden');
    } else {
        document.getElementById('go-to-settings-button').classList.remove('hidden');
        document.getElementById('gpio-container').classList.remove('hidden');
        document.getElementById('automation-container').classList.remove('hidden');
        document.getElementById('home-button').classList.add('hidden');
        document.getElementById('setting-container').classList.add('hidden');
    }
};
const createGpioControlRow = (gpio) => {
    let child = document.createElement('div');
    const digitalState = (gpio.state && !gpio.invert) || (!gpio.state && gpio.invert);
    let additionnalButton = `<a onclick='switchGpioState(this)' id='stateGpio-${gpio.pin}' class='btn ${digitalState ? 'on' : 'off'} ${gpio.mode != 2 ? 'input-mode' : ''}'>${digitalState ? 'on' : 'off'}</a>`;
    if (gpio.mode == -1) {
        additionnalButton = `<input type='number' onchange='switchGpioState(this)' id='stateGpio-${gpio.pin}' value='${gpio.state}'>`;
    } else if (gpio.mode == -2) {
        additionnalButton = `<a onclick='scan(this)' id='i2cScan-${gpio.pin}' class='btn on'>scan</a>`;
    }
    child.innerHTML = `<div class='row' id='rowGpio-${gpio.pin}'>
        <div class='label'> ${gpio.label}</div>
        <div class='btn-container'>
            <a onclick='openGpioSetting(this)' id='editGpio-${gpio.pin}' class='btn edit'>edit</a>${additionnalButton}
        </div>
    </div>`;
    return child.firstChild;
};

const createI2cSlaveControlRow = (slave) => {
    let child = document.createElement('div');        
    let actionButton = `<a onclick='sendI2cSlaveCommand(this,true)' id='setI2cSlaveData-${slave.id}' class='btn on'>set</a>`
    if (slave.octetRequest) {
        actionButton = `<a onclick='sendI2cSlaveCommand(this)' id='getI2cSlaveData-${slave.id}' class='btn on'>get</a>`
    }
    child.innerHTML = `<div class='row slave' id='rowSlave-${slave.id}'>
        <div class='label'> ${slave.label}</div>
        <div class='btn-container'>
            <a onclick='openI2cSlaveSettings(this)' id='editI2cSlave-${slave.mPin}-${slave.id}' class='btn edit'>edit</a>${actionButton}
            <input id='i2cSlaveData-${slave.id}' type='text'>
        </div>
    </div>`;
    return child.firstChild;
}

const saveI2cSlaveSettings = async (element) => {
    const id = element.id.split('-')[1];
    const isNew = (id === 'new');
    let req = { settings: {} };
    if (isNew) {
        req.settings.address = +element.parentElement.parentElement.parentElement.firstChild.textContent;
        req.settings.mPin = +element.parentElement.parentElement.parentElement.id.split('-')[1];
    } else {
        req.id = id;
    }
    req.settings.label = document.getElementById(`setI2cSlaveLabel-${id}`).value;
    req.settings.commands = document.getElementById(`setI2cSlaveCommands-${id}`).value.split(',');
    req.settings.octetRequest = +document.getElementById(`setI2cSlaveOctet-${id}`).value;
    req.settings.save = +document.getElementById(`setI2cSlaveSave-${id}`).checked;
    try {
        if (!req.settings.label) {
            throw new Error('Parameters missing, please fill at least label and type inputs.');
        }
        const resp = await fetch(`${window.location.href}slave`, {
            method: isNew ? 'POST' : 'PUT',                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
            headers: {
                'Accept': 'application/json',
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(req)
        });
        const newSetting = await resp.json();
        let row = document.getElementById(`rowGpio-${newSetting.mPin}`);
        if (isNew) {
            slaves.push(newSetting);
            row.insertBefore(createI2cSlaveControlRow(newSetting), row.lastChild);
            closeI2cSlaveSettings(id);
        } else {
            slaves = slaves.map(oldSlave => (oldSlave.id == +id) ? {...newSetting} : oldSlave);
            let oldRow = document.getElementById('rowSlave-' + id);
            row.replaceChild(createI2cSlaveControlRow(newSetting), oldRow);
        }
    } catch (err) {
        console.error(err);
    }
};

const openI2cSlaveSettings = (element) => {
    const infos = element.id.split('-');
    const gpioPin = infos[1];
    const id = (infos.length === 3 ? element.id.split('-')[2] : 'new');
    let label = '';
    let commands = "";
    let octetToRequest = 0;
    let save = 0
    // Find the right slave attached to gpioPin
    let slave = slaves.find(s => s.id == id);
    if (slave) {
        label = slave.label;
        commands = slave.commands.join(',');
        octetToRequest = slave.octetRequest;
        save = slave.save;
    }
    const row = slave ? document.getElementById(`rowSlave-${id}`) : document.getElementById(`scanResult-${gpioPin}`);
    let child = document.createElement('div');
    child.innerHTML = `<div class='column slave-settings set' id='i2cSlaveSettings-${id}'>
        <div class='set-inputs'>
            <div class='row'>
                <label for='setI2cSlaveLabel-${id}'>Slave label:</label>
                <input id='setI2cSlaveLabel-${id}' type='text' name='label' value='${label}' placeholder="Controller's title">
            </div>
            <div class='row'>
                <label for='setI2cSlaveCommands-${id}'>Command:</label>
                <input id='setI2cSlaveCommands-${id}' type='text' name='register' value='${commands}' placeholder="Slave's command">
            </div>
            <div class='row'>
                <label for='setI2cSlaveOctet-${id}'>Request octet number:</label>
                <input id='setI2cSlaveOctet-${id}' type='number' name='octet' value='${octetToRequest}' placeholder="Slave's command">
            </div>
            <div class='row ${octetToRequest ? 'hidden' : ''}'>
                <label for='setI2cSlaveSave-${id}'>Save:</label>
                <input id='setI2cSlaveSave-${id}' type='checkbox' name='save' value='${save}'>
            </div>
        </div>
        <div class='btn-container'>
            <a onclick='closeI2cSlaveSettings(${id})' id='closeI2cSlaveSettings-${id}' class='btn cancel'>close</a>
            <a onclick='deleteI2cSlave(this)' id='deleteI2cSlaveSettings-${id}' class='btn delete ${slave ? '':'hidden'}'>delete</a>
            <a onclick='saveI2cSlaveSettings(this)' id='saveI2cSlaveSettings-${id}' class='btn on'>${slave ? 'edit':'add'}</a>
        </div>
    </div>`;
    row.appendChild(child.firstChild);
}

const closeScan = (element) => {
    const gpioPin = element.id.split('-')[1];
    document.getElementById(`scanResults-${gpioPin}`).remove();
}

const closeI2cSlaveSettings = (id) => {
    document.getElementById(`i2cSlaveSettings-${id}`).remove();
}

const createScanResult = (gpioPin, scanResults) => {
    let child = document.createElement('div');
    const headRow = `<div class='row' id='scanResultHead-${gpioPin}'>Adresses:
            <div class='btn-container'>
                <a onclick='closeScan(this)' id='closeScan-${gpioPin}' class='btn delete'>x</a>
            </div>
    </div>`
    let rows = `<div class='row' id='scanResult-${gpioPin}'>No device attached</div>`
    if (scanResults) {
        rows = scanResults.reduce((prev, scanResult) => {
            return prev + `<div class='row' id='scanResult-${gpioPin}'>${scanResult}
                <div class='btn-container'>
                        <a onclick='openI2cSlaveSettings(this)' id='editI2c-${gpioPin}' class='btn edit'>set</a>
                    </div>
                </div>`
        },'')
    }
    child.innerHTML = `<div class='column scan-results' id='scanResults-${gpioPin}'>${headRow}${rows}</div>`;
    return child.firstChild;
}
const createAutomationRow = (automation) => {
    let child = document.createElement('div');
    child.innerHTML = `<div class='row' id='rowAutomation-${automation.id}'>
        <div class='label'> ${automation.label}</div>
        <div class='btn-container'>
            <a onclick='openAutomationSetting(this)' id='editAutomation-${automation.id}' class='btn edit'>edit</a>
            <a onclick='runAutomation(this)' id='runAutomation-${automation.id}' class='btn on'>run</a>
        </div>
    </div>`;
    return child.firstChild;
};
// The edit panel for setting gpios
const createEditGpioPanel = (gpio) => {
    if (!gpio) {
        gpio = {
            pin: 'new'
        };
    }
    let modeOptions = `<option ${gpio.mode == 1 ? 'selected' : ''} value=1>INPUT</option><option ${gpio.mode == 4 ? 'selected' : ''} value=4>PULLUP</option><option ${gpio.mode == 5 ? 'selected' : ''} value=5>INPUT PULLUP</option><option ${gpio.mode == 8 ? 'selected' : ''} value=8>PULLDOWN</option><option ${gpio.mode == 9 ? 'selected' : ''} value=9>INPUT PULLDOWN</option>`;
    const pinOptions = availableGpios.reduce((prev, availableGpio) => {
        if ((!gpios.find(_gpio => _gpio.pin == availableGpio.pin) && availableGpio.pin != gpio.pin) || availableGpio.pin == gpio.pin) {
            // Complete the mode select input while we are here...
            if (availableGpio.pin == gpio.pin && !availableGpio.inputOnly) {
                modeOptions += `<option ${gpio.mode == 2 ? 'selected' : ''} value=2>OUTPUT</option>`;
                modeOptions += `<option ${gpio.mode == -1 ? 'selected' : ''} value=-1>LED CONTROL</option>`;
                modeOptions += `<option ${gpio.mode == -2 ? 'selected' : ''} value=-2>I2C</option>`;
            }
            prev += `<option ${availableGpio.pin == gpio.pin ? 'selected' : ''} value=${availableGpio.pin}>${availableGpio.pin}</option>`;
        }
        return prev;
    },[]);
    const sclPinOptions = availableGpios.reduce((prev, availableGpio) => {
        if ((!gpios.find(_gpio => _gpio.pin == availableGpio.pin) && availableGpio.pin != gpio.sclpin) || availableGpio.pin == gpio.sclpin) {
            prev += `<option ${availableGpio.pin == gpio.sclpin ? 'selected' : ''} value=${availableGpio.pin}>${availableGpio.pin}</option>`;
        }
        return prev;
    },[]);
    const channelOptions = [...Array(settings.general.maxChannels).keys()]
    .reduce((prev,channelNumber) => {
        return prev +=`<option ${gpio.channel == channelNumber ? 'selected' : ''} value=${channelNumber}>${channelNumber}</option>`}
        ,`<option ${gpio.channel == -1 ? 'selected' : ''} value=-1>-1</option>`);
    let child = document.createElement('div');
    child.classList.add('set');
    child.innerHTML = `<div class='set-inputs'>
            <div class='row'>
                <label for='setGpioPin-${gpio.pin}'>Pin:</label>
                <select id='setGpioPin-${gpio.pin}' name='pin' onchange='updateModeOptions("${gpio.pin}")'>${pinOptions}</select>
            </div>
            <div class='row'>
                <label for='setGpioLabel-${gpio.pin}'>Label:</label>
                <input id='setGpioLabel-${gpio.pin}' type='text' name='label' value='${gpio.label || ''}' placeholder="Controller's title">
            </div>
            <div class='row'>
                <label for='setGpioMode-${gpio.pin}'>I/O mode:</label>
                <select onchange='updateGpioOptions(this)' id='setGpioMode-${gpio.pin}' name='mode'>${modeOptions}</select>
            </div>
            <div id='led-options' class='${gpio.mode != -1 ? 'hidden' : ''}'>
                <div class='row'>
                    <label for='setGpioFrequency-${gpio.pin}'>Frequency:</label>
                    <input id='setGpioFrequency-${gpio.pin}' type='text' name='frequency' value='${gpio.frequency || ''}' placeholder="Default to 50Hz if empty">
                </div>
                <div class='row'>
                    <label for='setGpioResolution-${gpio.pin}'>Resolution:</label>
                    <input id='setGpioResolution-${gpio.pin}' type='text' name='resolution' value='${gpio.resolution || ''}' placeholder="Default to 16bits if empty">
                </div>
                <div class='row'>
                    <label for='setGpioChannel-${gpio.pin}'>Channel:</label>
                    <select id='setGpioChannel-${gpio.pin}' name='channel' value='${gpio.channel}' placeholder="Default to 0">${channelOptions}</select>
                </div>
            </div>
            <div id='i2c-options' class='${gpio.mode != -2 ? 'hidden' : ''}'>
                <div class='row'>
                    <label for='setGpioSclPin-${gpio.pin}'>SCL pin:</label>
                    <select id='setGpioSclPin-${gpio.pin}' type='text' name='sclpin'>${sclPinOptions}</select>
                </div>
                <div class='row'>
                    <label for='setI2cFrequency-${gpio.pin}'>Frequency:</label>
                    <input id='setI2cFrequency-${gpio.pin}' type='text' name='frequency' value='${gpio.frequency || ''}' placeholder="Default to 50Hz if empty">
                </div>
            </div>
            <div class='row ${gpio.mode < 0 ? 'hidden' : ''}' id='setGpioInvertStateRow'>
                <label for='setGpioInvertState-${gpio.pin}'>Invert state:</label>
                <input type='checkbox' name='invert' id='setGpioInvertState-${gpio.pin}' value='${gpio.invert}'>
            </div>
            <div class='row ${gpio.mode != -1 && gpio.mode != 2 ? 'hidden' : ''}' id='setGpioSaveRow'>
                <label for='setGpioSave-${gpio.pin}'>Save state:</label>
                <input type='checkbox' name='save' id='setGpioSave-${gpio.pin}' value='${gpio.save}'>
            </div>
            </div>
        <div class='set-buttons'>
            <a onclick='closeAnySettings()' id='cancelGpio-${gpio.pin}' class='btn cancel'>cancel</a>
            ${gpio.pin === "new" ? '' : `<a onclick='deleteGpio(this)' id='deleteGpio-${gpio.pin}' class='btn delete'>delete</a>`}
            <a onclick='saveGpioSetting(this)' id='saveGpio-${gpio.pin}' class='btn save'>save</a>
        </div>`;
    return child;
};
const deleteRowEditor = (element) => {
    const isCondition = element.id.split('-')[0] === 'deleteCondition';
    const rowNumber = +element.id.split('-')[1] || 0;
    document.getElementById(`${isCondition ? 'condition' : 'action'}-${rowNumber}`).remove();
};
const addConditionEditor = (condition) => {
    let selectedGpio = 0;
    let selectedSign = 0;
    let selectedValue = 0;
    let selectedNextSign = 0;
    if (condition) {
        selectedGpio = condition[0];
        selectedSign = condition[1];
        selectedValue = condition[2];
        selectedNextSign = condition[3];
    }
    // If time type is selected, put the value in HH:MM format
    if (selectedGpio == -1) {
        selectedValue = `${Math.floor(selectedValue/100)}:${selectedValue%100}`;
    }

    let gpioConditionOptions = `<option value=-2 ${selectedGpio == -2 ? 'selected':''}>Weekday</option><option value=-1 ${selectedGpio == -1 ? 'selected':''}>Time</option>`
    gpioConditionOptions += gpios.reduce((acc,gpio) =>  acc+`<option value=${gpio.pin} ${selectedGpio == gpio.pin ? 'selected':''}>${gpio.label}</option>`,``);
    
    const conditionEditorElement = document.getElementById(`condition-editor-result`);
    const conditionNumber = '-' + conditionEditorElement.childElementCount;
    const rowElement = document.createElement('div');
    rowElement.id = `condition${conditionNumber}`;
    rowElement.classList.add('row');
    rowElement.innerHTML = `<select onchange='updateConditionValueType(this)' id='addGpioCondition${conditionNumber}' name='gpioCondition'>${gpioConditionOptions}</select>
                    <select id='addSignCondition${conditionNumber}' name='signCondition'>
                        <option value=1 ${selectedSign == 1 ? 'selected':''}>=</option>
                        <option value=2 ${selectedSign == 2 ? 'selected':''}>!=</option>
                        <option value=3 ${selectedSign == 3 ? 'selected':''}>></option>
                        <option value=4 ${selectedSign == 4 ? 'selected':''}><</option>
                    </select>
                    <input type='${selectedGpio==-1?'time':'number'}' id='addValueCondition${conditionNumber}' name='valueCondition' value='${selectedValue}' placeholder='value'>
                    <select id='addNextSignCondition${conditionNumber}' name='nextSignCondition'>
                        <option value=0 ${selectedNextSign == 0? 'selected':''}>none</option>
                        <option value=1 ${selectedNextSign == 1 ? 'selected':''}>AND</option>
                        <option value=2 ${selectedNextSign == 2 ? 'selected':''}>OR</option>
                        <option value=3 ${selectedNextSign == 3 ? 'selected':''}>XOR</option>
                    </select>
                    <a onclick='deleteRowEditor(this)' id='deleteCondition${conditionNumber}' class='btn delete'>x</a>`
    conditionEditorElement.appendChild(rowElement);
    // Update conditions left number
    document.getElementById(`condition-editor-title`).innerText = `Condition editor (${settings.general.maxConditions-conditionEditorElement.childElementCount})`;
};
const addActionEditor = (action) => {
    let selectedType = 1;
    let selectedValue = 0;
    let selectedPin = 0;
    let selectedSign = 1;
    selectedHttpMethod = 1;
    selectedHttpAddress = '';
    selectedHttpContent = '';
    if (action) {
        selectedType = action[0];
        selectedValue = action[1];
        selectedPin = action[2];
        selectedSign = action[3];
        selectedHttpMethod = action[1];
        selectedHttpAddress = action[2];
        selectedHttpContent = action[3];
    }
    let gpioActionOptions = gpios.reduce((acc,gpio) =>  acc+`<option value=${gpio.pin} ${selectedPin == gpio.pin ? 'selected':''}>${gpio.label}</option>`,``);
    let automationOptions = automations.reduce((acc,automation) =>  acc+`<option value=${automation.id} ${selectedValue == automation.id ? 'selected':''}>${automation.label}</option>`,``);
    const actionEditorElement = document.getElementById(`action-editor-result`);
    const actionNumber = '-' + actionEditorElement.childElementCount;
    const rowElement = document.createElement('div');
    rowElement.id = `action${actionNumber}`;
    rowElement.classList.add('row');
    rowElement.innerHTML = `<select onchange='updateActionType(this)' id='addTypeAction${actionNumber}' name='signAction'>
                        <option value=1 ${selectedType == 1 ? 'selected':''}>Set Gpio pin</option>
                        <option value=2 ${selectedType == 2 ? 'selected':''}>Send telegram message</option>
                        <option value=3 ${selectedType == 3 ? 'selected':''}>Delay</option>
                        <option value=4 ${selectedType == 4 ? 'selected':''}>Serial print</option>
                        <option value=5 ${selectedType == 5 ? 'selected':''}>HTTP</option>
                        <option value=6 ${selectedType == 6 ? 'selected':''}>Automation</option>
                    </select>
                    <select id='addHTTPMethod${actionNumber}' name='httpType' class='${selectedType == 5 ? '': 'hidden'}'>
                        <option value=1 ${selectedHttpMethod == 1 ? 'selected':''}>GET</option>
                        <option value=2 ${selectedHttpMethod == 2 ? 'selected':''}>POST</option>
                    </select>
                    <input id='addHTTPAddress${actionNumber}' name='httpAddress' class='${selectedType == 5 ? '': 'hidden'}' placeholder='http://www.placeholder.com/' value='${selectedHttpAddress}'>
                    <input id='addHTTPBody${actionNumber}' name='httpBody' class='${selectedType == 5 ? '': 'hidden'}' placeholder='Body in json format' value='${selectedHttpContent}'>
                    <select id='addGpioAction${actionNumber}' name='gpioAction' class='${selectedType == 1 ? '' : 'hidden'}'>${gpioActionOptions}</select>
                    <select id='addSignAction${actionNumber}' name='signAction' class='${selectedType == 1 ? '' : 'hidden'}'>
                        <option value=1 ${selectedSign == 1 ? 'selected':''}>=</option>
                        <option value=2 ${selectedSign == 2 ? 'selected':''}>+=</option>
                        <option value=3 ${selectedSign == 3 ? 'selected':''}>-=</option>
                        <option value=4 ${selectedSign == 4 ? 'selected':''}>*=</option>
                    </select>
                    <select id='addAutomation${actionNumber}' name='automation' class='${selectedType == 6 ? '': 'hidden'}'>${automationOptions}</select>
                    <input id='addValueAction${actionNumber}' name='valueAction' value='${selectedValue}' class='${selectedType == 5 || selectedType == 6 ? 'hidden': ''}' placeholder='value'>
                    <a onclick='deleteRowEditor(this)' id='deleteAction${actionNumber}' class='btn delete'>x</a>`
    actionEditorElement.appendChild(rowElement);
    // Update actions left number
    document.getElementById(`action-editor-title`).innerText = `Action editor (${settings.general.maxActions-actionEditorElement.childElementCount})`;
};
// The edit panel for setting gpios
const createEditAutomationPanel = (automation) => {
    if (!automation) {
        automation = {
            id: 'new',
            conditions: [],
            type: 1,
            loopCount: 0
        };
    }
    let nextAutomationOptions = automations.reduce((acc,a) => acc+`<option ${automation.nextAutomationId === a.id ? 'selected' : ''} value=${a.id}>${a.label}</option>`,``);
    let gpioOptions = gpios.reduce((acc,gpio) =>  acc+`<option ${automation.pinC === gpio.pin ? 'selected' : ''} value=${gpio.pin}>${gpio.label}</option>`,``);
    let gpioConditionOptions = gpios.reduce((acc,gpio) =>  acc+`<option value=${gpio.pin}>${gpio.label}</option>`,``);
    let child = document.createElement('div');
    child.classList.add('set');
    child.innerHTML = `<div class='set-inputs'>
            <div class='row ${automation.id === 'new' ? 'hidden' : ''}'>
                <label for='setAutomationLabel-${automation.id}'>Id: ${automation.id}</label>
            </div>
            <div class='row'>
                <label for='setAutomationLabel-${automation.id}'>Label:</label>
                <input id='setAutomationLabel-${automation.id}' type='text' name='label' value='${automation.label||''}' placeholder='Describe your automation'>
            </div>
            <div class='row'>
                <label for='setAutomationAutoRun-${automation.id}'>Event triggered:</label>
                <input type='checkbox' name='autorun' id='setAutomationAutoRun-${automation.id}' value='${automation.autoRun}'>
            </div>
            <div class='row'>
                <label for='setAutomationDebounceDelay-${automation.id}'>Debounce delay (ms):</label>
                <input id='setAutomationDebounceDelay-${automation.id}' type='number' name='debounceDelay' value='${automation.debounceDelay||''}' placeholder='Minimum time between each run'>
            </div>
            <div class='row'>
                <label for='setAutomationLoopCount-${automation.id}'>Repeat automation:</label>
                <input id='setAutomationLoopCount-${automation.id}' type='number' name='loopCount' value='${automation.loopCount||''}' placeholder='How many times the automation must be repeat'>
            </div>
            <div class='column'>
                <div id='condition-editor' class='row'>
                    <div id='condition-editor-title'>Condition editor (${settings.general.maxConditions})</div>
                    <a onclick='addConditionEditor()' id='addCondition-${automation.id}' class='btn save'>+</a>
                </div>
                <div id='condition-editor-result' class="column"></div>
            </div>
            <div class='column'>
                <div id='action-editor' class='row'>
                    <div id='action-editor-title'>Action editor (${settings.general.maxActions})</div>
                    <a onclick='addActionEditor()' id='addAction-${automation.id}' class='btn save'>+</a>
                </div>
                <div id='action-editor-result' class="column"></div>
            </div>
            </div>
        <div class='set-buttons'>
            <a onclick='closeAnySettings()' id='cancelAutomation-${automation.id}' class='btn cancel'>cancel</a>
            ${automation.id === "new" ? '' : `<a onclick='deleteAutomation(this)' id='deleteAutomation-${automation.id}' class='btn delete'>delete</a>`}
            <a onclick='saveAutomationSetting(this)' id='saveAutomation-${automation.id}' class='btn save'>save</a>
        </div>`;
    return child;
};
const createSpinner = () => {
    let spinner = document.createElement('div');
    spinner.classList.add('spinner');
    spinner.innerHTML = '<div class="lds-ring"><div></div><div></div><div></div><div></div></div>';
    return spinner;
};
    // Change the input of available mode for a given pin
    const updateModeOptions = (pin) => {
    const selectPin = document.getElementById(`setGpioPin-${pin || 'new'}`);
    const selectMode = document.getElementById(`setGpioMode-${pin || 'new'}`);

    const availableGpioInfos = availableGpios.find(gpio => gpio.pin == selectPin.value);
    if (availableGpioInfos.inputOnly && selectMode.childElementCount === 8) {
        selectMode.removeChild(selectMode.lastChild);
        selectMode.removeChild(selectMode.lastChild);
        selectMode.removeChild(selectMode.lastChild);
        document.getElementById(`led-options`).classList.add('hidden');
        document.getElementById(`setGpioSaveRow`).classList.add('hidden');
    } else if (!availableGpioInfos.inputOnly && selectMode.childElementCount === 5) {
        let outputOption = document.createElement('div');
        outputOption.innerHTML = `<option value=2>OUTPUT</option>`;
        let ledOption = document.createElement('div');
        ledOption.innerHTML = `<option value=-1>LED CONTROL</option>`;
        let i2coption = document.createElement('div');
        i2coption.innerHTML = `<option value=-2>I2C</option>`;
        document.getElementById(`setGpioSaveRow`).classList.remove('hidden');
        selectMode.appendChild(outputOption.firstChild);
        selectMode.appendChild(ledOption.firstChild);
        selectMode.appendChild(i2coption.firstChild);
    }
};
const updateGpioOptions = (element) => {
    const pin = +element.id.split('-')[1] || 'new';
    const option = document.getElementById(`setGpioMode-${pin}`).value;
    // Led mode
    if (option == -1) {
        document.getElementById(`led-options`).classList.remove('hidden');
        document.getElementById(`setGpioSaveRow`).classList.remove('hidden');
        document.getElementById(`setGpioInvertStateRow`).classList.add('hidden');
    // I2C mode
    } else if (option == -2) {
        document.getElementById(`setGpioSaveRow`).classList.add('hidden');
        document.getElementById(`led-options`).classList.add('hidden');
        document.getElementById(`setGpioInvertStateRow`).classList.add('hidden');
        document.getElementById(`i2c-options`).classList.remove('hidden');
    } else {
        if (option == 2) {
            document.getElementById(`setGpioSaveRow`).classList.remove('hidden');
        } else {
            document.getElementById(`setGpioSaveRow`).classList.add('hidden');
        }
        document.getElementById(`setGpioInvertStateRow`).classList.remove('hidden');
        document.getElementById(`led-options`).classList.add('hidden');
        document.getElementById(`i2c-options`).classList.add('hidden');
        
    }
}

const updateI2cSlaveOptions = (element) => {
    const selectType = element.value;
    if (selectType == 1) {
        document.getElementById(`lcd-slave-options`).classList.remove('hidden');
    } else {
        document.getElementById(`lcd-slave-options`).classList.add('hidden');
    }
}
const updateAutomationTypes = (id) => {
    const selectType = document.getElementById(`setAutomationType-${id || 'new'}`);
    if (+selectType.value !== 3) {
        document.getElementById(`setAutomationPinC-${id || 'new'}`).parentElement.classList.add('hidden');
        document.getElementById(`setAutomationPinValueC-${id || 'new'}`).parentElement.classList.add('hidden');
        document.getElementById(`setAutomationMessage-${id || 'new'}`).parentElement.classList.remove('hidden');
    } else {
        document.getElementById(`setAutomationPinC-${id || 'new'}`).parentElement.classList.remove('hidden');
        document.getElementById(`setAutomationPinValueC-${id || 'new'}`).parentElement.classList.remove('hidden');
        document.getElementById(`setAutomationMessage-${id || 'new'}`).parentElement.classList.add('hidden');
    }
};
const updateActionType = (element) => {
    const rowNumber = +element.id.split('-')[1];
    if (element.value == 1) {
        document.getElementById(`addGpioAction-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addSignAction-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addValueAction-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addHTTPMethod-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addHTTPAddress-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addHTTPBody-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addAutomation-${rowNumber}`).classList.add('hidden');
    } else if (element.value == 5){
        document.getElementById(`addHTTPMethod-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addHTTPAddress-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addHTTPBody-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addGpioAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addSignAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addValueAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addAutomation-${rowNumber}`).classList.add('hidden');
    } else if (element.value == 6){
        document.getElementById(`addHTTPMethod-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addHTTPAddress-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addHTTPBody-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addGpioAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addSignAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addValueAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addAutomation-${rowNumber}`).classList.remove('hidden');
    } else {
        document.getElementById(`addHTTPMethod-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addHTTPAddress-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addHTTPBody-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addGpioAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addSignAction-${rowNumber}`).classList.add('hidden');
        document.getElementById(`addValueAction-${rowNumber}`).classList.remove('hidden');
        document.getElementById(`addAutomation-${rowNumber}`).classList.add('hidden');
    }
};
const updateConditionValueType = (element) => {
    const rowNumber = +element.id.split('-')[1];
    const isHourCondition = (element.value==-1);
    if (isHourCondition) {
        document.getElementById(`addValueCondition-${rowNumber}`).type='time';
    } else {
        document.getElementById(`addValueCondition-${rowNumber}`).type='number';
    }
};
// Events
window.onload = async () => {
    fetchSettings();
    fetchAvailableGpios();
    fetchServicesHealth();
    await Promise.all([fetchGpios(), fetchAutomations(), fetchI2cSlaves()]);
    const containerG = document.getElementById('gpios');
    gpios.forEach(gpio => {
        containerG.appendChild(createGpioControlRow(gpio));
    });
    const containerA = document.getElementById('automations');
    automations.forEach(automation => {
        containerA.appendChild(createAutomationRow(automation));
    });
    slaves.forEach(slave => {
        const gpioRow = document.getElementById(`rowGpio-${slave.mPin}`);
        gpioRow.appendChild(createI2cSlaveControlRow(slave));
    });
    document.getElementById('page-loader').remove();
};