// set global elements for status panel
const textBridgeStat = document.getElementById("text-bridge-status");
const textGateStat = document.getElementById("text-gate-status");
const textConnectionStat = document.getElementById("text-conn-status");
const textErrorCode = document.getElementById("text-error-code");
const textRoadTrafficLights = document.getElementById("text-road-traf");
const textWaterwayTrafficLights = document.getElementById("text-waterway-traf");
const textNorthUS = document.getElementById("text-north-us");
const textUnderUS = document.getElementById("text-under-us");
const textSouthUS = document.getElementById("text-south-us");
const textRoadUS = document.getElementById("text-road-us");
const textLoadCell = document.getElementById("text-load-cell");
const textBridgeTopLimitSwitch = document.getElementById("text-bridge-top-limit");
const textBridgeBottomLimitSwitch = document.getElementById("text-bridge-bottom-limit");
const textGateTopLimitSwitch = document.getElementById("text-gate-top-limit");
const textGateBottomLimitSwitch = document.getElementById("text-gate-bottom-limit");
const textAudio = document.getElementById("text-audio");

// set global querie variables for form
const mode = document.getElementsByName('mode');
const disabledBox = document.querySelector("#disabled-box");
const roadTrafficLightInputs = document.getElementsByName('road-lights');
const roadTrafficLightsError = document.querySelector("#road-lights-error-text");
const waterTrafficLightInputs = document.getElementsByName('waterway-lights');
const waterTrafficLightsError = document.querySelector("#water-lights-error-text");
const trafficLightsError = document.querySelector("#traf-lights-error-text");
const northUS = document.querySelector("#north-us");
const underUS = document.querySelector("#under-us");
const southUS = document.querySelector("#south-us");
const roadUS = document.querySelector("#road-us");
const loadCell = document.querySelector("#loadcell");
const bTopLimit = document.querySelector("#bridge-top");
const bBottomLimit = document.querySelector("#bridge-bottom");
const gTopLimit = document.querySelector("#gate-top");
const gBottomLimit = document.querySelector("#gate-bottom");
const audioError = document.querySelector("#audio-error-text");
const audio = document.getElementsByName('audio');
const gates = document.getElementsByName('gates');
const errorCode = document.querySelector("#error-code");
const gatesError = document.querySelector("#gate-error-text");
const bridge = document.getElementsByName('bridge');
const bridgeError = document.querySelector("#bridge-error-text");
const push = document.querySelector("#push");
const emergencyButton = document.querySelector("#emergency");

// AUTOMATIC VS MANUAL MODE
// When Automatic, disable form input
// When Manual, enable form input

// when startup, disable inputs (automatic mode)
disableInputs();
let isAuto = true;

// auto update status and connection data every 1.0 second
var socket = io();

socket.on('connect', function() {
    console.log('Connected to SocketIO server');
});

socket.on('update_stat_data', function(status, conn) {
    // bridge status text
    switch (status.bridge_status) {
        case "OPEN":
            textBridgeStat.innerText = "OPEN";
            textBridgeStat.classList = "green-text";
            break;
        case "MOVN":
            textBridgeStat.innerText = "MOVING";
            textBridgeStat.classList = "orange-text";
            break;
        case "CLOS":
            textBridgeStat.innerText = "CLOSED";
            textBridgeStat.classList = "green-text";
            break;
        case "EMER":
            textBridgeStat.innerText = "EMERGENCY";
            textBridgeStat.classList = "yellow-text";
            break;
    }

    // gate Status text
    switch (status.gate_status) {
        case "OPEN":
            textGateStat.innerText = "OPEN";
            textGateStat.classList = "green-text";
            break;
        case "MOVN":
            textGateStat.innerText = "MOVING";
            textGateStat.classList = "orange-text";
            break;
        case "CLOS":
            textGateStat.innerText = "CLOSED";
            textGateStat.classList = "green-text";
            break;
        case "EMER":
            textGateStat.innerText = "EMERGENCY";
            textGateStat.classList = "yellow-text";
            break;
    }

    // connection status text
    switch (conn) {
        case true:
            textConnectionStat.innerText = "STABLE";
            textConnectionStat.classList = "green-text";
            break;
        case false:
            textConnectionStat.innerText = "DISCONNECTED";
            textConnectionStat.classList = "red-text";
            break;
    }

    // error code text
    switch (status.error_code) {
        case "0":
            textErrorCode.innerText = "0 - NO ERROR";
            textErrorCode.classList = "green-text";
            break;
        case "1":
            textErrorCode.innerText = "1 - Bridge Servo position misaligned with limit switch";
            textErrorCode.classList = "red-text";
            break;
        case "2":
            textErrorCode.innerText = "2 - Gate Servo position misaligned with limit switch";
            textErrorCode.classList = "red-text";
            break;
        case "3":
            textErrorCode.innerText = "3 - Bridge and Gate Servo position misaligned with limit switch";
            textErrorCode.classList = "red-text";
            break;
        case "4":
            textErrorCode.innerText = "4 - Road traffic detection error";
            textErrorCode.classList = "red-text";
            break;
        case "5":
            textErrorCode.innerText = "5 - Road traffic detection error and Bridge Servo position misaligned with limit switch";
            textErrorCode.classList = "red-text";
            break;
        case "6":
            textErrorCode.innerText = "6 - Road traffic detection error and Gate Servo position misaligned with limit switch";
            textErrorCode.classList = "red-text";
            break;
        case "7":
            textErrorCode.innerText = "7 - All errors";
            textErrorCode.classList = "red-text";
            break;
    }

    // road traffic light text
    switch (status.road_lights) {
        case "GOGO":
            textRoadTrafficLights.innerText = "GO";
            textRoadTrafficLights.classList = "green-text";
            break;
        case "SLOW":
            textRoadTrafficLights.innerText = "SLOW";
            textRoadTrafficLights.classList = "oramge-text";
            break;
        case "STOP":
            textRoadTrafficLights.innerText = "STOP";
            textRoadTrafficLights.classList = "red-text";
            break;
        case "EMER":
            textRoadTrafficLights.innerText = "EMERGENCY";
            textRoadTrafficLights.classList = "yellow-text";
            break;
    }

    // waterway traffic light text
    switch (status.waterway_lights) {
        case "GOGO":
            textWaterwayTrafficLights.innerText = "GO";
            textWaterwayTrafficLights.classList = "green-text";
            break;
        case "SLOW":
            textWaterwayTrafficLights.innerText = "SLOW";
            textWaterwayTrafficLights.classList = "oramge-text";
            break;
        case "STOP":
            textWaterwayTrafficLights.innerText = "STOP";
            textWaterwayTrafficLights.classList = "red-text";
            break;
        case "EMER":
            textWaterwayTrafficLights.innerText = "EMERGENCY";
            textWaterwayTrafficLights.classList = "yellow-text";
            break;
    }

    // ultrasonics text
    switch (status.north_us) {
        case "SHIP":
            textNorthUS.innerText = "SHIP DETECTED";
            textNorthUS.classList = "orange-text";
            break;
        case "NONE":
            textNorthUS.innerText = "NOTHING DETECTED";
            textNorthUS.classList = "green-text";
            break;
    }

    switch (status.under_us) {
        case "SHIP":
            textUnderUS.innerText = "SHIP DETECTED";
            textUnderUS.classList = "orange-text";
            break;
        case "NONE":
            textUnderUS.innerText = "NOTHING DETECTED";
            textUnderUS.classList = "green-text";
            break;
    }

    switch (status.south_us) {
        case "SHIP":
            textSouthUS.innerText = "SHIP DETECTED";
            textSouthUS.classList = "orange-text";
            break;
        case "NONE":
            textSouthUS.innerText = "NOTHING DETECTED";
            textSouthUS.classList = "green-text";
            break;
    }

    // road detection text
    switch(status.road_us) {
        case "TRAF":
            textRoadUS.innerText = "TRAFFIC DETECTED";
            textRoadUS.classList = "orange-text";
            break;
        case "NONE":
            textRoadUS.innerText = "NOTHING DETECTED";
            textRoadUS.classList = "green-text";
            break;
    }

    switch (status.road_load) {
        case "TRAF":
            textLoadCell.innerText = "TRAFFIC DETECTED";
            textLoadCell.classList = "orange-text";
            break;
        case "NONE":
            textLoadCell.innerText = "NOTHING DETECTED";
            textLoadCell.classList = "green-text";
            break;
    }

    // limit switches
    switch (status.bridge_top_limit) {
        case "TRIG":
            textBridgeTopLimitSwitch.innerText = "TRIGGERED";
            textBridgeTopLimitSwitch.classList = "orange-text";
            break;
        case "NONE":
            textBridgeTopLimitSwitch.innerText = "NOTHING DETECTED";
            textBridgeTopLimitSwitch.classList = "green-text";
            break;
    }

    switch (status.bridge_bottom_limit) {
        case "TRIG":
            textBridgeBottomLimitSwitch.innerText = "TRIGGERED";
            textBridgeBottomLimitSwitch.classList = "orange-text";
            break;
        case "NONE":
            textBridgeBottomLimitSwitch.innerText = "NOTHING DETECTED";
            textBridgeBottomLimitSwitch.classList = "green-text";
            break;
    }

    switch (status.gate_top_limit) {
        case "TRIG":
            textGateTopLimitSwitch.innerText = "TRIGGERED";
            textGateTopLimitSwitch.classList = "orange-text";
            break;
        case "NONE":
            textGateTopLimitSwitch.innerText = "NOTHING DETECTED";
            textGateTopLimitSwitch.classList = "green-text";
            break;
    }

    switch (status.gate_bottom_limit) {
        case "TRIG":
            textGateBottomLimitSwitch.innerText = "TRIGGERED";
            textGateBottomLimitSwitch.classList = "orange-text";
            break;
        case "NONE":
            textGateBottomLimitSwitch.innerText = "NOTHING DETECTED";
            textGateBottomLimitSwitch.classList = "green-text";
            break;
    }

    // audio text
    switch (status.audio) {
        case "DONE":
            textAudio.innerText = "BRIDGE STARTING OR COMPLETING MOVEMENT";
            textAudio.classList = "orange-text";
            break;
        case "MOVN":
            textAudio.innerText = "BRIDGE MOVING";
            textAudio.classList = "orange-text";
            break;
        case "EMER":
            textAudio.innerText = "EMERGENCY WITH BRIDGE";
            textAudio.classList = "yellow-text";
            break;
        case "NONE":
            textAudio.innerText = "NO AUDIO";
            textAudio.classList = "green-text";
            break;
    }
});

function update() {
    socket.emit('retrieve_stat_data');
}

const intervalId = setInterval(update, 1000);

// add event listening for changing modes
mode.forEach(input => {
    input.addEventListener("click", changeMode)
});

// checks if traffic lights have been inputted
roadTrafficLightInputs.forEach(input => {
    input.addEventListener("click", validateRoadTrafficLights)
});
waterTrafficLightInputs.forEach(input => {
    input.addEventListener("click", validateWaterTrafficLights)
});

audio.forEach(input => {
    input.addEventListener("click", validateAudio)
})

// checks if gates have been inputted
gates.forEach(input => {
    input.addEventListener("click", validateGates)
});

// checks if bridge has been inputted
bridge.forEach(input => {
    input.addEventListener("click", validateBridge)
});

// submit button
push.addEventListener("click", validateAll);

function changeMode() {
    let selected = document.querySelector('input[name="mode"]:checked');
    if (selected.value == "auto") {
        isAuto = true;
        disableInputs();
    } else if (selected.value == "manual") {
        isAuto = false;
        enableInputs();
    }
}

function validateRoadTrafficLights() {
    let roadSelected = document.querySelector('input[name="road-lights"]:checked');
    if (roadSelected || isAuto) {
        trafficLightsError.classList.add("hidden");
        roadTrafficLightsError.classList.add("hidden");
        return true;
    } else {
        trafficLightsError.classList.remove("hidden");
        roadTrafficLightsError.classList.remove("hidden");
        return false;
    }
}

function validateWaterTrafficLights() {
    let waterSelected = document.querySelector('input[name="waterway-lights"]:checked');
    if (waterSelected || isAuto) {
        trafficLightsError.classList.add("hidden");
        waterTrafficLightsError.classList.add("hidden");
        return true;
    } else {
        trafficLightsError.classList.remove("hidden");
        waterTrafficLightsError.classList.remove("hidden");
        return false;
    }
}

function validateAudio() {
    let selected = document.querySelector('input[name="audio"]:checked');
    if (selected || isAuto) {
        audioError.classList.add("hidden");
        return true;
    } else {
        audioError.classList.remove("hidden");
        return false;
    }
}

function validateGates() {
    let selected = document.querySelector('input[name="gates"]:checked');
    if (selected || isAuto) {
        gatesError.classList.add("hidden");
        return true;
    } else {
        gatesError.classList.remove("hidden");
        return false;
    }
}

function validateBridge() {
    let selected = document.querySelector('input[name="bridge"]:checked');
    if (selected || isAuto) {
        bridgeError.classList.add("hidden");
        return true;
    } else {
        bridgeError.classList.remove("hidden");
        return false;
    }
}

function validateAll(event) {
    // calls each validation functions
    let validRoadTrafficLights = validateRoadTrafficLights();
    let validWaterTrafficLights = validateWaterTrafficLights();
    let validAudio = validateAudio();
    let validGates = validateGates();
    let validBridge = validateBridge();

    allValid = validRoadTrafficLights && validWaterTrafficLights && validAudio && validGates && validBridge;

    if (!allValid) {
        // stops the form from submitting
        event.preventDefault();
    }
}

function disableInputs() {

    // remove hidden from disabled box
    disabledBox.classList.remove("content-box");
    disabledBox.classList.add("disabled-box");

    // hide errors
    trafficLightsError.classList.add("hidden");
    roadTrafficLightsError.classList.add("hidden");
    waterTrafficLightsError.classList.add("hidden");
    audioError.classList.add("hidden");
    gatesError.classList.add("hidden");
    bridgeError.classList.add("hidden");

    // disable Traffic light inputs
    roadTrafficLightInputs.forEach(input => {
        input.disabled = true;
        input.checked = false;
    });
    waterTrafficLightInputs.forEach(input => {
        input.disabled = true;
        input.checked = false;
    });

    // disable Ultrasonic sensor inputs
    northUS.disabled = true;
    northUS.checked = false;

    underUS.disabled = true;
    underUS.checked = false;

    southUS.disabled = true;
    southUS.checked = false;

    // disable road detection sensor inputs
    roadUS.disabled = true;
    roadUS.checked = false;

    loadCell.disabled = true;
    loadCell.checked = false;

    // disable Limit Switches inputs
    bTopLimit.disabled = true;
    bTopLimit.checked = false;

    bBottomLimit.disabled = true;
    bBottomLimit.checked = false;

    gTopLimit.disabled = true;
    gTopLimit.checked = false;
    
    gBottomLimit.disabled = true;
    gBottomLimit.checked = false;

    // disable audio inputs
    audio.forEach(input => {
        input.disabled = true;
        input.checked = false;
    });

    // disable error code inputs
    errorCode.disabled = true;
    

    // disable gate inputs
    gates.forEach(input => {
        input.disabled = true;
        input.checked = false;
    });

    // disable bridge inputs
    bridge.forEach(input => {
        input.disabled = true;
        input.checked = false;
    });
}

function enableInputs() {

    // remove hidden from disabled box
    disabledBox.classList.remove("disabled-box");
    disabledBox.classList.add("content-box");

    // enable Traffic light inputs
    roadTrafficLightInputs.forEach(input => {
        input.disabled = false;
    });
    waterTrafficLightInputs.forEach(input => {
        input.disabled = false;
    });

    // enable Ultrasonic sensor inputs
    northUS.disabled = false;
    underUS.disabled = false;
    southUS.disabled = false;

    // enable road detection sensor inputs
    roadUS.disabled = false;
    loadCell.disabled = false;

    // enable Limit Switches inputs
    bTopLimit.disabled = false;
    bBottomLimit.disabled = false;
    gTopLimit.disabled = false;
    gBottomLimit.disabled = false;

    // enable audio inputs
    audio.forEach(input => {
        input.disabled = false;
        // automatically select None option
        if (input.value == "NONE") {
            input.checked = true;
        }
    });

    // enable error code inputs
    errorCode.disabled = false;

    // enable gate inputs
    gates.forEach(input => {
        input.disabled = false;
    });

    // enable bridge inputs
    bridge.forEach(input => {
        input.disabled = false;
    });
}
