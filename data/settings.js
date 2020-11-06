/*global settings, health, gpios, slaves, availableGpios, automations, versionsList, blocker, restart, isSettingsMenuActivated, delay, request, displayNotification, closeAnySettings, createSpinner, createGpioControlRow, createI2cSlaveControlRow, createAutomationRow, fetchServicesHealth*/
const switchPage = () => {
    isSettingsMenuActivated = !isSettingsMenuActivated;
    if (isSettingsMenuActivated) {
        document.getElementById("go-to-settings-button").classList.add("hidden");
        document.getElementById("gpio-container").classList.add("hidden");
        document.getElementById("automation-container").classList.add("hidden");
        document.getElementById("camera-container").classList.add("hidden");
        document.getElementById("home-button").classList.remove("hidden");
        document.getElementById("setting-container").classList.remove("hidden");
    } else {
        document.getElementById("go-to-settings-button").classList.remove("hidden");
        document.getElementById("gpio-container").classList.remove("hidden");
        document.getElementById("camera-container").classList.remove("hidden");
        document.getElementById("automation-container").classList.remove("hidden");
        document.getElementById("home-button").classList.add("hidden");
        document.getElementById("setting-container").classList.add("hidden");
    }
};
// Update software
const fillUpdateInput = (element) => {
    const fileName = element.value.split("\\");
    document.getElementById("firmware-file-label").innerHTML = fileName[fileName.length - 1];
    document.getElementById("submit-update-file").classList.remove("disable");
};
const selectFirmwareVersion = () => {
    const versiontSelector = document.getElementById("select-firmware-version");
    if (versiontSelector.value) {
        document.getElementById("submit-update-file").classList.remove("disable");
    } else {
        document.getElementById("submit-update-file").classList.add("disable");
    }
};
const submitUpdate = async () => {
    const blocker = document.getElementById("blocker");
    blocker.classList.remove("hidden");
    const versiontSelector = document.getElementById("select-firmware-version");
    if (versiontSelector.value) {
        // Reqest repo download
        try {
            await fetch(window.location.href + `update/version?v=${versiontSelector.value}`);
            document.getElementById("blocker-title").innerText = `Downloading firmware v${versiontSelector.value}, please wait...`;
        } catch (err) {
            blocker.classList.add("hidden");
            await displayNotification(err, "error");
        }
    } else {
        // Manual upload
        document.getElementById("blocker-title").innerText = "Loading new software, please wait...";
        const firmwareFile = document.getElementById("firmware-file");
        const data = new FormData();
        data.append("firmware", firmwareFile.files[0]);
        try {
            const res = await fetch(window.location.href + "update", {
                processData: false,
                contentType: false,
                method: "POST",
                body: data
            });
            document.getElementById("blocker-title").innerText = "Restarting device, please wait...";
        } catch (err) {
            blocker.classList.add("hidden");
            await displayNotification(err, "error");
        }
    }

};
// MQTT
const switchMqtt = (input) => {
    const mqttForm = document.getElementById("mqtt-settings");
    if (input.checked) {
        mqttForm.classList.remove("hidden");
    } else {
        mqttForm.classList.add("hidden");
    }
};

const mqttConnect = async () => {
    const loader = document.getElementById("mqtt-retry-loader");
    const retryButton = document.getElementById("mqtt-retry");
    const retryText = retryButton.firstElementChild;
    try {
        retryText.classList.add("hidden");
        loader.classList.remove("hidden");
        await fetch(window.location.href + "mqtt/retry");
        while (health.mqtt === 0) {
            await fetchServicesHealth();
            await delay(1000); //avoid spaming esp32
        }
        if (+health.mqtt === 1) {
            retryButton.classList.add("hidden");
            await displayNotification("Mqtt client connected", "success");
        } else {
            retryButton.classList.remove("hidden");
            await displayNotification("Could not connect Mqtt client", "error");
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
    loader.classList.add("hidden");
    retryText.classList.remove("hidden");
};

const saveMqttSettings = async (e) => {
    e.preventDefault();
    const active = document.getElementById("mqtt-active").checked;
    const fn = document.getElementById("mqtt-fn").value;
    const host = document.getElementById("mqtt-host").value;
    const port = document.getElementById("mqtt-port").value;
    const user = document.getElementById("mqtt-user").value;
    const password = document.getElementById("mqtt-password").value;
    const topic = document.getElementById("mqtt-topic").value;
    try {
        await request("mqtt",{ active, fn, host, port, user, password, topic },true);
        settings.mqtt = { active, fn, host, port, user, password, topic };
        await displayNotification("Mqtt parameters saved", "success");
        await mqttConnect();
    } catch (err) {
        await displayNotification(err, "error");
    }
};
// Telegram
const switchTelegram = (input) => {
    const telegramForm = document.getElementById("telegram-settings");
    if (input.checked) {
        telegramForm.classList.remove("hidden");
    } else {
        telegramForm.classList.add("hidden");
    }
}
const saveTelegramSettings = async () => {
    const active = +document.getElementById("telegram-active").checked;
    const token = document.getElementById("telegram-token").value;
    const users = document.getElementById("telegram-users").value.split(",").map((id) => +id);
    if (token !== settings.telegram.token || active !== +settings.telegram.active || (settings.telegram.users && JSON.stringify(users.sort()) !== JSON.stringify(settings.telegram.users.sort()))) {
        try {
            await request("telegram",{ active, token, users },true);
            settings.telegram = { active, token, users };
            await displayNotification("Telegram parameters saved", "success");
        } catch (err) {
            await displayNotification(err, "error");
        }
    }
};
// WIFI
const switchSta = (input) => {
    const staSettings = document.getElementById("sta-settings");
    if (input.checked) {
        staSettings.classList.remove("hidden");
    } else {
        staSettings.classList.add("hidden");
    }
}
const submitWifi= async (e) => {
    e.preventDefault();
    const dns = document.getElementById("wifi-dns").value;
    const apSsid = document.getElementById("wifi-ap-ssid").value;
    const apPsw = document.getElementById("wifi-ap-psw").value;
    const staEnable = +document.getElementById("wifi-sta-enable").checked;
    const staSsid = document.getElementById("wifi-sta-ssid").value;
    const staPsw = document.getElementById("wifi-sta-psw").value;
    if (dns !== +settings.wifi.dns || apSsid !== +settings.wifi.apSsid || apPsw !== +settings.wifi.apPsw || staEnable !== +settings.wifi.staEnable || staSsid !== +settings.wifi.staSsid || staPsw !== +settings.wifi.staPsw) {
        try {
            if (!apSsid) {
                throw new Error("Missing wifi ssid/password for AP mode");
            }
            if (staEnable && (!staSsid || !staPsw)) {
                throw new Error("Missing wifi ssid/password STA mode");
            }
            await request("wifi",{dns, apSsid, apPsw, staEnable, staSsid, staPsw },true);
            settings.wifi = {dns, apSsid, apPsw, staEnable, staSsid, staPsw };
            await displayNotification("Wifi parameters saved. ESP32 restarting", "success");
            await restart();
        } catch (err) {
            await displayNotification(err, "error");
        }
    }
};
const resetSettings = async () => {
    try {
        await fetch(window.location.href + "settings/reset");
        await restart();
    } catch (err) {
        await displayNotification(err, "error");
    }
}
const backupSettings = async () => {
    let backup = { gpios, automations, slaves, settings};
    var dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(backup));
    var dlAnchorElem = document.getElementById("downloadAnchorElem");
    dlAnchorElem.setAttribute("href",dataStr);
    dlAnchorElem.setAttribute("download", "backup.json");
    dlAnchorElem.click();
}
const importBackup = async () => {
    document.getElementById("blocker-title").innerText = "Importing backup, please wait...";
    const backupFile = document.getElementById("backup-file");
    var reader = new FileReader();
    reader.onload = async (event) => {
        try {
            await request("import/backup",JSON.parse(event.target.result),true);
            document.getElementById("blocker-title").innerText = "Restarting device, please wait...";
            await restart();
        } catch (err) {
            blocker.classList.add("hidden");
            await displayNotification(err, "error");
        }
    };
    reader.readAsText(backupFile.files[0]);
}
const fillBackupInput = (element) => {
    const fileName = element.value.split("\\");
    document.getElementById("backup-file-label").innerHTML = fileName[fileName.length - 1];
    document.getElementById("import-backup").classList.remove("disable");
};