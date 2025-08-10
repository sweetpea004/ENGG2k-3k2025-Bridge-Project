// set global querie variables
const mode = document.getElementsByName('mode');
const trafficLightInputs = document.getElementsByName('traffic-lights');
const trafficLightsError = document.querySelector("#traf-lights-error-text");
const speaker = document.getElementsByName('speaker');
const speakerError = document.querySelector("#speaker-error-text");
const northUS = document.querySelector("#north-us");
const underUS = document.querySelector("#under-us");
const southUS = document.querySelector("#south-us");
const loadCell = document.querySelector("#loadcell");
const gates = document.getElementsByName('gates');
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
trafficLightInputs.forEach(input => {
    input.addEventListener("click", validateTrafficLights)
});

// checks if speaker has been inputted
speaker.forEach(input => {
    input.addEventListener("click", validateSpeaker)
});

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

function validateTrafficLights() {
    let selected = document.querySelector('input[name="traffic-lights"]:checked');
    if (selected || isAuto) {
        trafficLightsError.classList.add("hidden");
        return true;
    } else {
        trafficLightsError.classList.remove("hidden");
        return false;
    }
}

function validateSpeaker() {
    let selected = document.querySelector('input[name="speaker"]:checked');
    if (selected || isAuto) {
        speakerError.classList.add("hidden");
        return true;
    } else {
        speakerError.classList.remove("hidden");
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
    let validTrafficLights = validateTrafficLights();
    let validSpeaker = validateSpeaker();
    let validGates = validateGates();
    let validBridge = validateBridge();

    allValid = validTrafficLights && validSpeaker && validGates && validBridge;

    if (!allValid) {
        // stops the form from submitting
        event.preventDefault();
    }
}

function disableInputs() {

    // hide errors
    trafficLightsError.classList.add("hidden");
    gatesError.classList.add("hidden");
    bridgeError.classList.add("hidden");

    // disable Traffic light inputs
    trafficLightInputs.forEach(input => {
        input.disabled = true;
        input.checked = false;
    });

    // disable Speaker inputs
    speaker.forEach(input => {
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

    // enable Traffic light inputs
    trafficLightInputs.forEach(input => {
        input.disabled = false;
        console.log(input.disabled);
    });

    // enable Speaker inputs
    speaker.forEach(input => {
        input.disabled = false;
    });

    // enable Ultrasonic sensor inputs
    northUS.disabled = false;
    underUS.disabled = false;
    southUS.disabled = false;

    // enable load cell sensor inputs
    loadCell.disabled = false;

    // enable gate inputs
    gates.forEach(input => {
        input.disabled = false;
    });

    // enable bridge inputs
    bridge.forEach(input => {
        input.disabled = false;
    });
}
