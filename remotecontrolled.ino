#include <ESP8266WebServer.h>

#include <Servo.h>



// --- 1. PINOUT CONFIGURATION ---

#define ENA D1

#define ENB D2

#define left1 D5

#define left2 D6

#define right1 D8

#define right2 D7

#define LED_PIN LED_BUILTIN

#define SERVO_PIN_LEFT D3

#define SERVO_PIN_RIGHT D4



// --- 2. GLOBAL VARIABLES & OBJECTS ---

const char* ssid = "SumoBot-Control_1";

const char* password = "roboclub";



int motorSpeed = 200;



// Servo position limits

#define RAMP_DOWN_POS 90 // The lowest angle the ramp can go

#define RAMP_UP_POS   10 // The highest angle the ramp can go



// State machine for ramp control

enum RampState { RAMP_STOPPED, RAMP_MOVING_UP, RAMP_MOVING_DOWN };

RampState currentRampState = RAMP_STOPPED;

int currentRampAngle = RAMP_DOWN_POS; // Stores the current position of the ramp



// Timer for smooth servo movement

unsigned long lastRampUpdateTime = 0;

const int rampUpdateInterval = 15; // ms between each servo step (lower is faster)



ESP8266WebServer server(80);

Servo rampServoLeft;

Servo rampServoRight;



// --- 3. HTML WEB PAGE ---

const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE HTML><html><head>

<title>ESP8266 SumoBot</title>

<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">

<style>

  body { font-family: Arial, sans-serif; text-align: center; background: #222; color: white; -webkit-user-select: none; user-select: none; }

  .btn { background: #3498db; border: none; color: white; padding: 20px;

         font-size: 24px; border-radius: 10px; width: 100px; margin: 10px; cursor: pointer; }

  .btn-ramp { width: 130px; background: #27ae60; }

  .btn:active, .btn-ramp:active { transform: scale(0.95); } /* Visual feedback on press */

  #stop-btn { background: #e74c3c; }

  .grid-container { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 5px; max-width: 360px; margin: auto; padding-top: 20px;}

  .grid-item { display: flex; justify-content: center; align-items: center; }

  #fwd { grid-column: 2; grid-row: 1; }

  #left { grid-column: 1; grid-row: 2; }

  #stop { grid-column: 2; grid-row: 2; }

  #right { grid-column: 3; grid-row: 2; }

  #back { grid-column: 2; grid-row: 3; }

  .controls-container { max-width: 360px; margin: 20px auto; }

  input[type=range] { width: 80%; }

</style>

</head><body>

<h1>SumoBot Controller</h1>

<div class="grid-container">

  <div id="fwd" class="grid-item"><button class="btn" onmousedown="sendCommand('F')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('F')" ontouchend="sendCommand('S')">w</button></div>

  <div id="left" class="grid-item"><button class="btn" onmousedown="sendCommand('L')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('L')" ontouchend="sendCommand('S')">a</button></div>

  <div id="stop" class="grid-item"><button class="btn" id="stop-btn" onclick="sendCommand('S')">STOP</button></div>

  <div id="right" class="grid-item"><button class="btn" onmousedown="sendCommand('R')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('R')" ontouchend="sendCommand('S')">d</button></div>

  <div id="back" class="grid-item"><button class="btn" onmousedown="sendCommand('B')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('B')" ontouchend="sendCommand('S')">s</button></div>

</div>



<div class="controls-container">

  <button class="btn btn-ramp" onmousedown="sendCommand('U')" onmouseup="sendCommand('H')" ontouchstart="sendCommand('U')" ontouchend="sendCommand('H')">Ramp Up</button>

  <button class="btn btn-ramp" onmousedown="sendCommand('D')" onmouseup="sendCommand('H')" ontouchstart="sendCommand('D')" ontouchend="sendCommand('H')">Ramp Down</button>

</div>



<div class="controls-container">

  <p>Speed: <span id="speedVal">200</span></p>

  <input type="range" min="100" max="255" value="200" id="speedSlider" onchange="setSpeed(this.value)">

</div>

<script>

function sendCommand(cmd) { fetch('/action?cmd=' + cmd); }

function setSpeed(val) {

  document.getElementById('speedVal').innerText = val;

  fetch('/speed?val=' + val);

}

</script>

</body></html>

)rawliteral";





// --- 4. MOTOR AND RAMP CONTROL FUNCTIONS ---

void Fwd() {

  analogWrite(ENA, motorSpeed);

  analogWrite(ENB, motorSpeed);

  digitalWrite(left1, HIGH);

  digitalWrite(left2, LOW);

  digitalWrite(right1, HIGH);

  digitalWrite(right2, LOW);

}



void Backwd() {

  analogWrite(ENA, motorSpeed);

  analogWrite(ENB, motorSpeed);

  digitalWrite(left1, LOW);

  digitalWrite(left2, HIGH);

  digitalWrite(right1, LOW);

  digitalWrite(right2, HIGH);

}



void Left() {

  analogWrite(ENA, motorSpeed);

  analogWrite(ENB, motorSpeed);

  digitalWrite(left1, LOW);

  digitalWrite(left2, HIGH);

  digitalWrite(right1, HIGH);

  digitalWrite(right2, LOW);

}



void Right() {

  analogWrite(ENA, motorSpeed);

  analogWrite(ENB, motorSpeed);

  digitalWrite(left1, HIGH);

  digitalWrite(left2, LOW);

  digitalWrite(right1, LOW);

  digitalWrite(right2, HIGH);

}



void Stop() {

  digitalWrite(left1, LOW);

  digitalWrite(left2, LOW);

  digitalWrite(right1, LOW);

  digitalWrite(right2, LOW);

  analogWrite(ENA, 0);

  analogWrite(ENB, 0);

}



void updateRamp() {

  // Only check for updates every rampUpdateInterval milliseconds

  if (millis() - lastRampUpdateTime > rampUpdateInterval) {

    lastRampUpdateTime = millis();

    bool angleChanged = false;



    if (currentRampState == RAMP_MOVING_UP && currentRampAngle > RAMP_UP_POS) {

      currentRampAngle--; // Move one step up

      angleChanged = true;

    } else if (currentRampState == RAMP_MOVING_DOWN && currentRampAngle < RAMP_DOWN_POS) {

      currentRampAngle++; // Move one step down

      angleChanged = true;

    }



    // Only write to the servos if the angle has actually changed

    if (angleChanged) {

      rampServoLeft.write(currentRampAngle);

      rampServoRight.write(180 - currentRampAngle);

    }

  }

}



// --- 5. WEB SERVER HANDLERS ---

void handleRoot() {

  server.send(200, "text/html", index_html);

}



void handleAction() {

  String cmd = server.arg("cmd");

  Serial.println("Command received: " + cmd);



  if (cmd == "F") Fwd();

  else if (cmd == "B") Backwd();

  else if (cmd == "L") Left();

  else if (cmd == "R") Right();

  else if (cmd == "S") Stop();

  else if (cmd == "U") currentRampState = RAMP_MOVING_UP;

  else if (cmd == "D") currentRampState = RAMP_MOVING_DOWN;

  else if (cmd == "H") currentRampState = RAMP_STOPPED;

  

  server.send(200, "text/plain", "OK");

}



void handleSpeed() {

  if (server.hasArg("val")) {

    motorSpeed = server.arg("val").toInt();

    motorSpeed = constrain(motorSpeed, 0, 255);

    Serial.println("Speed set to: " + String(motorSpeed));

  }

  server.send(200, "text/plain", "OK");

}



void handleNotFound() {

  server.send(404, "text/plain", "Not found");

}



// --- 6. SETUP FUNCTION ---

void setup() {

  Serial.begin(115200);



  pinMode(ENA, OUTPUT);

  pinMode(ENB, OUTPUT);

  pinMode(left1, OUTPUT);

  pinMode(left2, OUTPUT);

  pinMode(right1, OUTPUT);

  pinMode(right2, OUTPUT);

  pinMode(LED_PIN, OUTPUT);



  // Attach servos and set initial position

  rampServoLeft.attach(SERVO_PIN_LEFT);

  rampServoRight.attach(SERVO_PIN_RIGHT);

  rampServoLeft.write(currentRampAngle);

  rampServoRight.write(180 - currentRampAngle);



  analogWriteRange(255);

  Stop();



  Serial.println("\nStarting Wi-Fi AP...");

  WiFi.softAP(ssid, password);



  IPAddress myIP = WiFi.softAPIP();

  Serial.print("AP IP address: ");

  Serial.println(myIP);

  digitalWrite(LED_PIN, LOW);



  server.on("/", HTTP_GET, handleRoot);

  server.on("/action", HTTP_GET, handleAction);

  server.on("/speed", HTTP_GET, handleSpeed);

  server.onNotFound(handleNotFound);



  server.begin();

  Serial.println("Web server started.");

}



// --- 7. MAIN LOOP ---

void loop() {

  server.handleClient();

  updateRamp();

}