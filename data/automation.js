/*global settings, health, gpios, slaves, availableGpios, automations, versionsList, isSettingsMenuActivated, delay, request, displayNotification, closeAnySettings, createSpinner, createGpioControlRow, createI2cSlaveControlRow, createAutomationRow, fetchAutomations*/
const deleteRowEditor = (element) => {
    const isCondition = element.id.split("-")[0] === "deleteCondition";
    const rowNumber = +element.id.split("-")[1] || 0;
    document.getElementById(`${isCondition ? "condition" : "action"}-${rowNumber}`).remove();
};
const addConditionEditor = (condition) => {
    let selectedGpio = 0;
    let selectedSign = 0;
    let selectedValue = 0;
    let selectedNextSign = 0;
    if (condition) {
        selectedGpio = +condition[0];
        selectedSign = +condition[1];
        selectedValue = condition[2];
        selectedNextSign = +condition[3];
    }
    // If time type is selected, put the value in HH:MM format
    if (selectedGpio === -1) {
        let hours = Math.floor(selectedValue / 100);
        if (hours<10) {
            hours = "0"+hours;
        }
        let minutes = selectedValue % 100;
        if (minutes<10) {
            minutes = "0"+minutes;
        }
        selectedValue = hours + ":" + minutes;
    }

    let gpioConditionOptions = `<option value=-2 ${selectedGpio === -2 ? "selected" : ""}>Weekday</option><option value=-1 ${selectedGpio === -1 ? "selected" : ""}>Time</option>`;
    gpioConditionOptions += gpios.reduce((acc, gpio) => {
        if (gpio.mode !== -100) {
            return acc + `<option value=${gpio.pin} ${selectedGpio === +gpio.pin ? "selected" : ""}>${gpio.label}</option>`;
        }
        return acc;
    }, "");

    const conditionEditorElement = document.getElementById("condition-editor-result");
    const conditionNumber = "-" + conditionEditorElement.childElementCount;
    const rowElement = document.createElement("div");
    rowElement.id = `condition${conditionNumber}`;
    rowElement.classList.add("row");
    rowElement.innerHTML = `<select onchange="updateConditionValueType(this)" id="addGpioCondition${conditionNumber}" name="gpioCondition">${gpioConditionOptions}</select>
                    <select id="addSignCondition${conditionNumber}" name="signCondition">
                        <option value=1 ${selectedSign === 1 ? "selected" : ""}>=</option>
                        <option value=2 ${selectedSign === 2 ? "selected" : ""}>!=</option>
                        <option value=3 ${selectedSign === 3 ? "selected" : ""}>></option>
                        <option value=4 ${selectedSign === 4 ? "selected" : ""}><</option>
                    </select>
                    <input type="${selectedGpio === -1 ? "time" : "number"}" id="addValueCondition${conditionNumber}" name="valueCondition" value="${selectedValue}" placeholder="value">
                    <select id="addNextSignCondition${conditionNumber}" name="nextSignCondition">
                        <option value=0 ${selectedNextSign === 0 ? "selected" : ""}>none</option>
                        <option value=1 ${selectedNextSign === 1 ? "selected" : ""}>AND</option>
                        <option value=2 ${selectedNextSign === 2 ? "selected" : ""}>OR</option>
                        <option value=3 ${selectedNextSign === 3 ? "selected" : ""}>XOR</option>
                    </select>
                    <a onclick="deleteRowEditor(this)" id="deleteCondition${conditionNumber}" class="btn delete">x</a>`;
    conditionEditorElement.appendChild(rowElement);
    // Update conditions left number
    document.getElementById("condition-editor-title").innerText = `Condition editor (${settings.general.maxConditions - conditionEditorElement.childElementCount})`;
};
const addActionEditor = (action) => {
    let selectedType = 1;
    let selectedValue;
    let selectedPin;
    let selectedSign = 1;
    let selectedHttpMethod = 1;
    let selectedHttpAddress;
    let selectedHttpContent;
    let sendTelegramMessageWithPicture = 0;
    if (action) {
        selectedType = +action[0];
        selectedValue = action[1];
        selectedPin = +action[2];
        selectedSign = action[3];
        selectedHttpMethod = action[1];
        selectedHttpAddress = action[2];
        selectedHttpContent = action[3];
        sendTelegramMessageWithPicture = +action[2];
    }
    let gpioActionOptions = gpios.reduce((acc, gpio) => {
        if (gpio.mode !== -100) {
            return acc + `<option value=${gpio.pin} ${selectedPin === +gpio.pin ? "selected" : ""}>${gpio.label}</option>`
        }
        return acc;
    }, "");
    let automationOptions = automations.reduce((acc, automation) => acc + `<option value=${automation.id} ${+selectedValue === +automation.id ? "selected" : ""}>${automation.label}</option>`, "");
    const actionEditorElement = document.getElementById("action-editor-result");
    const actionNumber = "-" + actionEditorElement.childElementCount;
    const rowElement = document.createElement("div");
    rowElement.id = `action${actionNumber}`;
    rowElement.classList.add("row");
    rowElement.innerHTML = `<select onchange="updateActionType(this)" id="addTypeAction${actionNumber}" name="signAction">
                        <option value=1 ${selectedType === 1 ? "selected" : ""}>Set Gpio pin</option>
                        <option value=2 ${selectedType === 2 ? "selected" : ""}>Send telegram message</option>
                        <option value=3 ${selectedType === 3 ? "selected" : ""}>Serial print</option>
                        <option value=4 ${selectedType === 4 ? "selected" : ""}>Delay</option>
                        <option value=5 ${selectedType === 5 ? "selected" : ""}>DelayMicro</option>
                        <option value=6 ${selectedType === 6 ? "selected" : ""}>HTTP</option>
                        <option value=7 ${selectedType === 7 ? "selected" : ""}>Automation</option>
                    </select>
                    <select id="addHTTPMethod${actionNumber}" name="httpType" class="${selectedType === 6 ? "" : "hidden"}">
                        <option value=1 ${selectedHttpMethod === 1 ? "selected" : ""}>GET</option>
                        <option value=2 ${selectedHttpMethod === 2 ? "selected" : ""}>POST</option>
                    </select>
                    <input id="addHTTPAddress${actionNumber}" type="text" name="httpAddress" class="${selectedType === 6 ? "" : "hidden"}" placeholder="http://www.placeholder.com/" value="${selectedHttpAddress}">
                    <input id="addHTTPBody${actionNumber}" type="text" name="httpBody" class="${selectedType === 6 ? "" : "hidden"}" placeholder="Body in json format" value="${selectedHttpContent}">
                    <select id="addGpioAction${actionNumber}" name="gpioAction" class="${selectedType === 1 ? "" : "hidden"}">${gpioActionOptions}</select>
                    <select id="addSignAction${actionNumber}" name="signAction" class="${selectedType === 1 ? "" : "hidden"}">
                        <option value=1 ${selectedSign === 1 ? "selected" : ""}>=</option>
                        <option value=2 ${selectedSign === 2 ? "selected" : ""}>+=</option>
                        <option value=3 ${selectedSign === 3 ? "selected" : ""}>-=</option>
                        <option value=4 ${selectedSign === 4 ? "selected" : ""}>*=</option>
                    </select>
                    <select id="addAutomation${actionNumber}" name="automation" class="${selectedType === 7 ? "" : "hidden"}">${automationOptions}</select>
                    <input id="addValueAction${actionNumber}" type="text" name="valueAction" value="${selectedValue}" class="${selectedType === 6 || selectedType === 7 ? "hidden" : ""}" placeholder="${selectedType === 1 ? "value" : "text"}">
                    <div id="addPictureToTelegramAction${actionNumber}-container" class="switch ${selectedType !== 2 || !camera.model ? "hidden" : ""}">
                        <label for="addPictureToTelegramAction${actionNumber}">Send picture:</label>
                        <input id="addPictureToTelegramAction${actionNumber}" type="checkbox" class="switch-input" value="${sendTelegramMessageWithPicture}">
                        <label class="slider" for="addPictureToTelegramAction${actionNumber}"></label>
                    </div>
                    <a onclick="deleteRowEditor(this)" id="deleteAction${actionNumber}" class="btn delete">x</a>`;
    actionEditorElement.appendChild(rowElement);
    // Update actions left number
    document.getElementById("action-editor-title").innerText = `Action editor (${settings.general.maxActions - actionEditorElement.childElementCount})`;
};
// The edit panel for setting gpios
const createEditAutomationPanel = (automation) => {
    if (!automation) {
        automation = {
            id: "new",
            conditions: [],
            type: 1,
            loopCount: 0
        };
    }
    let child = document.createElement("div");
    child.classList.add("set");
    child.innerHTML = `<div class="set-inputs">
            <div class="row ${automation.id === "new" ? "hidden" : ""}">
                <label for="setAutomationLabel-${automation.id}">Id: ${automation.id}</label>
            </div>
            <div class="row">
                <label for="setAutomationLabel-${automation.id}">Label:</label>
                <input id="setAutomationLabel-${automation.id}" type="text" name="label" value="${automation.label || ""}" placeholder="Describe your automation">
            </div>
            <div class="row">
                <div class="switch">
                    <label for="setAutomationAutoRun-${automation.id}">Event triggered:</label>
                    <input id="setAutomationAutoRun-${automation.id}" type="checkbox" class="switch-input" value="${automation.autoRun}">
                    <label class="slider" for="setAutomationAutoRun-${automation.id}"></label>
                </div>
            </div>
            <div class="row">
                <label for="setAutomationDebounceDelay-${automation.id}">Debounce delay (ms):</label>
                <input id="setAutomationDebounceDelay-${automation.id}" type="number" name="debounceDelay" value="${automation.debounceDelay || ""}" placeholder="Minimum time between each run">
            </div>
            <div class="row">
                <label for="setAutomationLoopCount-${automation.id}">Repeat automation:</label>
                <input id="setAutomationLoopCount-${automation.id}" type="number" name="loopCount" value="${automation.loopCount || 1}" placeholder="How many times the automation must be repeat">
            </div>
            <div class="column">
                <div id="condition-editor" class="row">
                    <div id="condition-editor-title">Condition editor (${settings.general.maxConditions})</div>
                    <a onclick="addConditionEditor()" id="addCondition-${automation.id}" class="btn save">+</a>
                </div>
                <div id="condition-editor-result" class="column"></div>
            </div>
            <div class="column">
                <div id="action-editor" class="row">
                    <div id="action-editor-title">Action editor (${settings.general.maxActions})</div>
                    <a onclick="addActionEditor()" id="addAction-${automation.id}" class="btn save">+</a>
                </div>
                <div id="action-editor-result" class="column"></div>
            </div>
            </div>
        <div class="set-buttons">
            <a onclick="closeAnySettings()" id="cancelAutomation-${automation.id}" class="btn cancel">cancel</a>
            ${automation.id === "new" ? "" : `<a onclick="deleteAutomation(this)" id="deleteAutomation-${automation.id}" class="btn delete">delete</a>`}
            <a onclick="saveAutomationSetting(this)" id="saveAutomation-${automation.id}" class="btn save">save</a>
        </div>`;
    return child;
};
const updateAutomationTypes = (id) => {
    const selectType = document.getElementById(`setAutomationType-${id || "new"}`);
    if (+selectType.value !== 4 && +selectType.value !== 5) {
        document.getElementById(`setAutomationPinC-${id || "new"}`).parentElement.classList.add("hidden");
        document.getElementById(`setAutomationPinValueC-${id || "new"}`).parentElement.classList.add("hidden");
        document.getElementById(`setAutomationMessage-${id || "new"}`).parentElement.classList.remove("hidden");
    } else {
        document.getElementById(`setAutomationPinC-${id || "new"}`).parentElement.classList.remove("hidden");
        document.getElementById(`setAutomationPinValueC-${id || "new"}`).parentElement.classList.remove("hidden");
        document.getElementById(`setAutomationMessage-${id || "new"}`).parentElement.classList.add("hidden");
    }
};
const updateActionType = (element) => {
    const rowNumber = +element.id.split("-")[1];
    const value = +element.value;
    if (value === 1) {
        document.getElementById(`addGpioAction-${rowNumber}`).classList.remove("hidden");
        document.getElementById(`addSignAction-${rowNumber}`).classList.remove("hidden");
    } else {
        document.getElementById(`addGpioAction-${rowNumber}`).classList.add("hidden");
        document.getElementById(`addSignAction-${rowNumber}`).classList.add("hidden");
    }
    if (value === 2 && camera.model) {
        document.getElementById(`addPictureToTelegramAction-${rowNumber}-container`).classList.remove("hidden");
    } else {
        document.getElementById(`addPictureToTelegramAction-${rowNumber}-container`).classList.add("hidden");
    }
    if (value === 6 || value === 7) {
        document.getElementById(`addValueAction-${rowNumber}`).classList.add("hidden");
    } else {
        document.getElementById(`addValueAction-${rowNumber}`).classList.remove("hidden");
    }
    if (value === 6) {
        document.getElementById(`addHTTPMethod-${rowNumber}`).classList.remove("hidden");
        document.getElementById(`addHTTPAddress-${rowNumber}`).classList.remove("hidden");
        document.getElementById(`addHTTPBody-${rowNumber}`).classList.remove("hidden");
    } else {
        document.getElementById(`addHTTPMethod-${rowNumber}`).classList.add("hidden");
        document.getElementById(`addHTTPAddress-${rowNumber}`).classList.add("hidden");
        document.getElementById(`addHTTPBody-${rowNumber}`).classList.add("hidden");
    }
    if ( value === 7) {
        document.getElementById(`addAutomation-${rowNumber}`).classList.remove("hidden");
    } else {
        document.getElementById(`addAutomation-${rowNumber}`).classList.add("hidden");
    }
};
const updateConditionValueType = (element) => {
    const rowNumber = +element.id.split("-")[1];
    const isHourCondition = (+element.value === -1);
    if (isHourCondition) {
        document.getElementById(`addValueCondition-${rowNumber}`).type = "time";
    } else {
        document.getElementById(`addValueCondition-${rowNumber}`).type = "number";
    }
};
const openAutomationSetting = (element) => {
    closeAnySettings();
    const automation = automations.find((automation) => automation.id === +element.id.split("-")[1]);
    const row = document.getElementById("rowAutomation-" + automation.id);
    if (!row.classList.value.includes("open")) {
        row.appendChild(createEditAutomationPanel(automation));
        // Fill conditions
        automation.conditions.forEach((condition) => {
            // Check if the condition contains a valid math operator sign
            if (condition[1]) {
                addConditionEditor(condition);
            }
        })
        // Fill actions
        automation.actions.forEach((action,index) => {
            // Check if the action contains a valid type
            if (action[0]) {
                addActionEditor(action);
                document.getElementById(`addPictureToTelegramAction-${index}`).checked = action[2];
            }
        })
        row.classList.add("open");
        document.getElementById(`setAutomationAutoRun-${automation.id}`).checked = automation.autoRun;
    }
};
const saveAutomationSetting = async (element) => {
    const automationId = element.id.split("-")[1];
    const isNew = (automationId === "new");
    let req = { settings: {} };
    if (!isNew) {
        req.id = automationId;
    }
    req.settings.label = document.getElementById(`setAutomationLabel-${automationId}`).value;
    req.settings.conditions = [...document.getElementById("condition-editor-result").childNodes].map((rowElement) => {
        const id = +rowElement.id.split("-")[1];
        return [
            +document.getElementById(`addGpioCondition-${id}`).value,
            +document.getElementById(`addSignCondition-${id}`).value,
            +document.getElementById(`addValueCondition-${id}`).value.split(":").join(""),
            +document.getElementById(`addNextSignCondition-${id}`).value,
        ];
    });
    req.settings.actions = [...document.getElementById("action-editor-result").childNodes].map((rowElement) => {
        const id = +rowElement.id.split("-")[1];
        const type = document.getElementById(`addTypeAction-${id}`).value;
        if (+type === 6) {
            return [type, document.getElementById(`addHTTPMethod-${id}`).value,
                document.getElementById(`addHTTPAddress-${id}`).value,
                document.getElementById(`addHTTPBody-${id}`).value
            ];
        } else if (+type === 7) {
            return [type, document.getElementById(`addAutomation-${id}`).value, "", ""];
        } else if (+type === 2 && camera.model) {
            return [type, document.getElementById(`addValueAction-${id}`).value, `${+document.getElementById(`addPictureToTelegramAction-${id}`).checked}`, ""];
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
            throw new Error("Parameters missing, please fill at least label and type inputs.");
        }
        const newSetting = await request("automation",req,isNew);
        let column = document.getElementById("automations");
        if (isNew) {
            automations.push(newSetting);
            column.insertBefore(createAutomationRow(newSetting), column.firstChild);
            closeAnySettings();
        } else {
            automations = automations.map((oldAutomation) => (+oldAutomation.id === +automationId) ? { ...newSetting } : oldAutomation);
            let oldRow = document.getElementById("rowAutomation-" + automationId);
            column.replaceChild(createAutomationRow(newSetting), oldRow);
        }
        await displayNotification("Automation saved", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const deleteAutomation = async (element) => {
    const automationId = element.id.split("-")[1];
    try {
        await fetch(`${window.location.href}automation?id=${automationId}`, { method: "DELETE" });
        await fetchAutomations();
        let row = document.getElementById("rowAutomation-" + automationId);
        closeAnySettings();
        row.remove();
        await displayNotification("Automation removed", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
};
const runAutomation = async (element) => {
    const id = element.id.split("-")[1];
    element.classList.add("loading");
    element.classList.add("disable");
    try {
        await fetch(window.location.href + "automation/run?id=" + id);
        await displayNotification("Automation run", "success");
    } catch (err) {
        await displayNotification(err, "error");
    }
    element.classList.remove("loading");
};
const addAutomation = () => {
    closeAnySettings();
    const topBar = document.getElementById("automation-header-bar");
    if (!topBar.classList.value.includes("open")) {
        topBar.appendChild(createEditAutomationPanel());
        topBar.classList.add("open");
    }
};