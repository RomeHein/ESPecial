const switchPage = () => {
    isSettingsMenuActivated = !isSettingsMenuActivated;
    if (isSettingsMenuActivated) {
        document.getElementById("go-to-settings-button").classList.add("hidden");
        document.getElementById("gpio-container").classList.add("hidden");
        document.getElementById("automation-container").classList.add("hidden");
        document.getElementById("home-button").classList.remove("hidden");
        document.getElementById("setting-container").classList.remove("hidden");
    } else {
        document.getElementById("go-to-settings-button").classList.remove("hidden");
        document.getElementById("gpio-container").classList.remove("hidden");
        document.getElementById("automation-container").classList.remove("hidden");
        document.getElementById("home-button").classList.add("hidden");
        document.getElementById("setting-container").classList.add("hidden");
    }
};
// Update software
const fillUpdateInput = (element) => {
    const fileName = element.value.split("\\");
    document.getElementById("file-update-label").innerHTML = fileName[fileName.length - 1];
    document.getElementById("submit-update-file").classList.remove("disable");
};
const selectFirmwareVersion = (element) => {
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

const submitMqtt = async (e) => {
    e.preventDefault();
    const active = document.getElementById("mqtt-active").checked;
    const fn = document.getElementById("mqtt-fn").value;
    const host = document.getElementById("mqtt-host").value;
    const port = document.getElementById("mqtt-port").value;
    const user = document.getElementById("mqtt-user").value;
    const password = document.getElementById("mqtt-password").value;
    const topic = document.getElementById("mqtt-topic").value;
    try {
        await fetch(window.location.href + "mqtt", {
            method: "POST",
            headers: { contentType: false, processData: false },
            body: JSON.stringify({ active, fn, host, port, user, password, topic })
        });
        settings.mqtt = { active, fn, host, port, user, password, topic };
        await displayNotification("Mqtt parameters saved", "success");
        await mqttConnect();
    } catch (err) {
        await displayNotification(err, "error");
    }
};
// Telegram
const submitTelegram = async (e) => {
    e.preventDefault();
    const active = +document.getElementById("telegram-active").checked;
    const token = document.getElementById("telegram-token").value;
    const users = document.getElementById("telegram-users").value.split(",").map((id) => +id);
    if (token !== settings.telegram.token || active !== +settings.telegram.active || (JSON.stringify(users.sort()) !== JSON.stringify(settings.telegram.users.sort()))) {
        try {
            const res = await fetch(window.location.href + "telegram", {
                method: "POST",
                headers: { contentType: false, processData: false },
                body: JSON.stringify({ token, active, users })
            });
            settings.telegram = { active, token };
            await displayNotification("Telegram parameters saved", "success");
        } catch (err) {
            await displayNotification(err, "error");
        }
    }
};