// set global querie variables
const mode = document.getElementsByName('mode');
const disabledBox = document.querySelector("#disabled-box")
const roadTrafficLightInputs = document.getElementsByName('road-lights');
const roadTrafficLightsError = document.querySelector("#road-lights-error-text");
const waterTrafficLightInputs = document.getElementsByName('waterway-lights');
const waterTrafficLightsError = document.querySelector("#water-lights-error-text");
const trafficLightsError = document.querySelector("#traf-lights-error-text")
const northUS = document.querySelector("#north-us");
const underUS = document.querySelector("#under-us");
const southUS = document.querySelector("#south-us");
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

    // disable load cell sensor inputs
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

    // enable load cell sensor inputs
    loadCell.disabled = false;

    // enable Limit Switches inputs
    bTopLimit.disabled = false;
    bBottomLimit.disabled = false;
    gTopLimit.disabled = false;
    gBottomLimit.disabled = false;

    // enable audio inputs
    audio.forEach(input => {
        input.disabled = false;
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
