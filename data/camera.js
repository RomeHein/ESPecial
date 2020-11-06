/*global settings, camera, health, gpios, slaves, availableGpios, automations, versionsList, isSettingsMenuActivated, delay, request, displayNotification, closeAnySettings, createSpinner*/
const createEditCameraPanel = () => {
    let child = document.createElement("div");
    child.classList.add("set");
    child.innerHTML = `<div class="set-camera">
            <div class="row">
                <label for="setCameraModel">Camera model:</label>
                <select id="setCameraModel" name="model">
                    <option ${+camera.model === 1 ? "selected" : ""} value=1>WROVER KIT</option>
                    <option ${+camera.model === 2 ? "selected" : ""} value=2>ESP EYE</option>
                    <option ${+camera.model === 3 ? "selected" : ""} value=3>M5STACK PSRAM</option>
                    <option ${+camera.model === 4 ? "selected" : ""} value=4>M5STACK WIDE</option>
                    <option ${+camera.model === 5 ? "selected" : ""} value=5>AI THINKER</option>
                </select>
            </div>
        </div>
        <div class="set-buttons">
            <a onclick="closeAnySettings()" id="cancel-camera" class="btn cancel">cancel</a>
            <a onclick="initCamera(this)" id="initCamera" class="btn save">Init camera</a>
        </div>`;
    return child;
};

const initCamera = async () => {
    const model = document.getElementById("setCameraModel").value;
    try {
        if (!model) {
            throw new Error("Parameters missing, please fill all the inputs");
        }
        const res = await fetch(window.location.href+`camera/init?model=${model}`);
        const newGpios = await res.json();
        if (newGpios.constructor === Array) {
            gpios = newGpios;
            document.getElementById("camera").classList.remove("hidden");
            closeAnySettings();
            document.getElementById("add-camera").classList.add("hidden");
            await displayNotification("Camera initialised", "success");
        }
    } catch (err) {
        await displayNotification(err, "error");
    }
};

const switchStream = (input) => {
    const streamView = document.getElementById("stream");
    streamView.src = (input.checked ? "/camera/stream" : "");
    if (input.checked) {
        streamView.classList.remove("hidden");
    } else {
        // window.stop();
        streamView.classList.add("hidden");
    }
};

const switchFr = (input) => {
    const facerecognitionView = document.getElementById("face-recognition");
    if (input.checked) {
        facerecognitionView.classList.remove("hidden");
    } else {
        // window.stop();
        facerecognitionView.classList.add("hidden");
    }
};

const addCamera = () => {
    closeAnySettings();
    const topBar = document.getElementById("camera-header-bar");
    if (!topBar.classList.value.includes("open")) {
        topBar.appendChild(createEditCameraPanel());
        topBar.classList.add("open");
    }
};