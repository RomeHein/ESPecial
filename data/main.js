var settings = {};
var health = {};
var gpios = [];
var slaves = [];
var availableGpios = [];
var automations = [];
var versionsList = [];
var isSettingsMenuActivated = false;
const delay = ((ms) => new Promise((resolve) => setTimeout(resolve, ms)));
const request = async (uri, body, post) => {
    const resp = await fetch(`${window.location.href}${uri}`, {
        method: post ? "POST" : "PUT",
        headers: {
            "Accept": "application/json",
            "Content-Type": "application/json"
        },
        body: JSON.stringify(body)
    });
    return resp.json();
};
const displayNotification = async (message, type) => {
    const blocker = document.getElementById("blocker");
    blocker.classList.add("hidden");
    const notifier = document.getElementById("notifier");
    const text = document.getElementById("notifier-text");
    notifier.classList.remove("hidden");
    notifier.classList.add(type);
    text.innerText = message;
    await delay(2000);
    notifier.classList.add("hidden");
    notifier.classList.remove(type);
};
const closeAnySettings = () => {
    document.querySelectorAll(".open").forEach((row) => {
        row.classList.remove("open");
        row.removeChild(row.lastChild);
    });
};
const createSpinner = () => {
    let spinner = document.createElement("div");
    spinner.classList.add("spinner");
    spinner.innerHTML = "<div class=\"lds-ring\"><div></div><div></div><div></div><div></div></div>";
    return spinner;
};
// GPIO
const createGpioControlRow = (gpio) => {
    let child = document.createElement("div");
    const digitalState = (gpio.state && !gpio.invert) || (!gpio.state && gpio.invert);
    let additionnalButton = `<a onclick="switchGpioState(this)" id="stateGpio-${gpio.pin}" class="btn ${digitalState ? "on" : "off"} ${+gpio.mode !== 2 ? "input-mode" : ""}">${digitalState ? "on" : "off"}</a>`;
    if (+gpio.mode === -1) {
        additionnalButton = `<input type="number" onchange="switchGpioState(this)" id="stateGpio-${gpio.pin}" value="${gpio.state}">`;
    } else if (+gpio.mode === -2) {
        additionnalButton = `<a onclick="scan(this)" id="i2cScan-${gpio.pin}" class="btn on">scan</a>`;
    }
    child.innerHTML = `<div class="row" id="rowGpio-${gpio.pin}">
        <div class="label"> ${gpio.label}</div>
        <div class="btn-container">
            <a onclick="openGpioSetting(this)" id="editGpio-${gpio.pin}" class="btn edit">edit</a>${additionnalButton}
        </div>
    </div>`;
    return child.firstChild;
};

const createI2cSlaveControlRow = (slave) => {
    let child = document.createElement("div");
    let actionButton = `<a onclick="sendI2cSlaveCommand(this,true)" id="setI2cSlaveData-${slave.id}" class="btn on">set</a>`;
    if (slave.octetRequest) {
        actionButton = `<a onclick="sendI2cSlaveCommand(this)" id="getI2cSlaveData-${slave.id}" class="btn on">get</a>`;
    }
    child.innerHTML = `<div class="row slave" id="rowSlave-${slave.id}">
        <div class="label"> ${slave.label}</div>
        <div class="btn-container">
            <a onclick="openI2cSlaveSettings(this)" id="editI2cSlave-${slave.mPin}-${slave.id}" class="btn edit">edit</a>${actionButton}
            <input id="i2cSlaveData-${slave.id}" type="text">
        </div>
    </div>`;
    return child.firstChild;
};
const createAutomationRow = (automation) => {
    let child = document.createElement("div");
    child.innerHTML = `<div class="row" id="rowAutomation-${automation.id}">
        <div class="label"> ${automation.label}</div>
        <div class="btn-container">
            <a onclick="openAutomationSetting(this)" id="editAutomation-${automation.id}" class="btn edit">edit</a>
            <a onclick="runAutomation(this)" id="runAutomation-${automation.id}" class="btn on">run</a>
        </div>
    </div>`;
    return child.firstChild;
};
const restart = async () => {
    try {
        const blocker = document.getElementById("blocker");
        const res = await fetch(window.location.href + "restart");
        blocker.classList.add("hidden");
    } catch (err) {
        blocker.classList.add("hidden");
        await displayNotification(err, "error");
    }
};
const switchIndicatorState = (indicatorId, stateCode) => {
    if (+stateCode === 1) {
        document.getElementById(indicatorId).classList.add("ok");
        document.getElementById(indicatorId).classList.remove("error");
    } else if (+stateCode === 0) {
        document.getElementById(indicatorId).classList.remove("ok");
        document.getElementById(indicatorId).classList.remove("error");
    } else {
        document.getElementById(indicatorId).classList.remove("ok");
        document.getElementById(indicatorId).classList.add("error");
    }
};
const fetchServicesHealth = async () => {
    try {
        const res = await fetch(window.location.href + "health");
        health = await res.json();
        switchIndicatorState("telegram-indicator", health.telegram);
        switchIndicatorState("mqtt-indicator", health.mqtt);
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const reloadFirmwareVersionsList = async () => {
    try {
        await fetch(window.location.href + "firmware/list");
    } catch (err) {
        await displayNotification(err, "error");
    }
};

const fetchGpios = async () => {
    try {
        const res = await fetch(window.location.href + "gpios");
        const newGpios = await res.json();
        if (newGpios && newGpios.length) {
            gpios = newGpios;
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const fetchAvailableGpios = async () => {
    try {
        const res = await fetch(window.location.href + "gpios/available");
        availableGpios = await res.json();
    } catch (err) {
        await displayNotification(err, "error");
    }
};
// I2C
const fetchI2cSlaves = async () => {
    try {
        const res = await fetch(window.location.href + "slaves");
        const newSlaves = await res.json();
        if (newSlaves && newSlaves.length) {
            slaves = newSlaves;
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const sendI2cSlaveCommand = async (element, write) => {
    const id = element.id.split("-")[1];
    const inputElement = document.getElementById(`i2cSlaveData-${id}`);
    try {
        const res = await fetch(window.location.href + `slave/command?id=${id}` + (write && inputElement.value ? `&value=${+inputElement.value}` : ""));
        if (!write && res) {
            const data = await res.json();
            inputElement.value = data;
        }
        await displayNotification("Command sent", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};
// Automations
const fetchAutomations = async () => {
    try {
        const res = await fetch(window.location.href + "automations");
        const newAutomations = await res.json();
        if (newAutomations && newAutomations.length) {
            automations = newAutomations;
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
};
// Settings
const fetchSettings = async () => {
    try {
        const res = await fetch(window.location.href + "settings");
        const s = await res.json();
        if (s) {
            // Save settings
            settings = s;
            // Add them to the dom
            document.getElementById("telegram-active").checked = settings.telegram.active;
            document.getElementById("telegram-token").value = settings.telegram.token;
            document.getElementById("telegram-users").value = settings.telegram.users.filter(userId => userId !== 0).join(",");
            document.getElementById("mqtt-active").checked = settings.mqtt.active;
            document.getElementById("mqtt-fn").value = settings.mqtt.fn;
            document.getElementById("mqtt-host").value = settings.mqtt.host;
            document.getElementById("mqtt-port").value = settings.mqtt.port;
            document.getElementById("mqtt-user").value = settings.mqtt.user;
            document.getElementById("mqtt-password").value = settings.mqtt.password;
            document.getElementById("mqtt-topic").value = settings.mqtt.topic;
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
};
// Events
window.onload = async () => {
    fetchSettings();
    fetchAvailableGpios();
    fetchServicesHealth();
    reloadFirmwareVersionsList();
    await Promise.all([fetchGpios(), fetchAutomations(), fetchI2cSlaves()]);
    const containerG = document.getElementById("gpios");
    gpios.forEach((gpio) => {
        containerG.appendChild(createGpioControlRow(gpio));
    });
    const containerA = document.getElementById("automations");
    automations.forEach((automation) => {
        containerA.appendChild(createAutomationRow(automation));
    });
    slaves.forEach((slave) => {
        const gpioRow = document.getElementById(`rowGpio-${slave.mPin}`);
        if (gpioRow) {
            gpioRow.appendChild(createI2cSlaveControlRow(slave));
        }
    });
    document.getElementById("page-loader").remove();
};
if (!!window.EventSource) {
    var source = new EventSource("/events");

    source.addEventListener("firmwareList", async (e) => {
        versionsList = JSON.parse(e.data);
        const versionSelector = document.getElementById("select-firmware-version");
        for (let info of versionsList) {
            let option = document.createElement("div");
            option.innerHTML = `<option value="${info.version}">v${info.version}</option>`;
            versionSelector.appendChild(option.firstChild);
            if (info.version > settings.general.firmwareVersion) {
                await displayNotification(`"New firmware(${info.version}) available`, "success");
            }
        }
    }, false);

    source.addEventListener("firmwareDownloaded", (e) => {
        const versionSelector = document.getElementById("select-firmware-version");
        document.getElementById("blocker-title").innerText = `Installing firmware v${versionSelector.value}...`;
    }, false);

    source.addEventListener("firmwareUpdateError", async (e) => {
        document.getElementById("blocker-title").classList.add("hidden");
        await displayNotification(e.data, "error");
    }, false);

    // Allow Gpio buttons refresh
    source.addEventListener("pin", (e) => {
        const pin = e.data.split("-")[0];
        const state = e.data.split("-")[1];
        const gpioRow = document.getElementById(`stateGpio-${pin}`);
        if (gpioRow.type === "number") {
            gpioRow.value = state;
        } else {
            gpioRow.classList.remove(+state ? "off" : "on");
            gpioRow.classList.add(+state ? "on" : "off");
            gpioRow.textContent = +state ? "on" : "off";
        }
    }, false);

    // Allow Automation buttons refresh
    source.addEventListener("automation", (e) => {
        const automation = e.data.split("-")[0];
        const state = e.data.split("-")[1];
        const button = document.getElementById(`runAutomation-${automation}`);
        if (+state) {
            button.classList.add("disable");
            button.innerText = "running...";
        } else {
            button.classList.remove("disable");
            button.innerText = "run";
        }
        
    }, false);

    source.addEventListener("shouldRefresh", (e) => {
        location.reload();
    }, false);
}