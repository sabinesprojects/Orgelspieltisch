// Pedal-Sensoren
boolean keyIsDown[30];
int sensorInput;
int pinMapping[30] = { 53, 51, 49, 47, 45, 22, 24, 30, 28, 26,
                       32, 34, 43, 41, 39, 37, 35, 48, 46, 44,
                       42, 40, 38, 36, 23, 25, 27, 29, 31, 33
                     };
unsigned long delayAfterOnTime[30], delayAfterOffTime[30];
unsigned long delayAfterOn = 50, delayAfterOff = 50;


// MIDI
const int C = 36;
const int pedal_channel = 1;
const int noteON = 145;
const int noteOFF = 129;


void setup() {
    // Init Pedal
    for (int i = 0; i < 30; i++) {
        pinMode(pinMapping[i], INPUT);
        keyIsDown[i] = false;
        delayAfterOnTime[i] = 0; delayAfterOffTime[i] = 0;
    }
    
    // Init Midi
    Serial1.begin(31250);
    MIDImessage(176, 123, 0);
}


void loop() {
    // Prüfen, ob ein Pedal gedrückt oder losgelassen wurde
    for (int i = 0; i < 30; i++) {
        sensorInput = digitalRead(pinMapping[i]);
        if ((sensorInput == LOW) && !keyIsDown[i] && millis() > delayAfterOffTime[i]) {
            MIDImessage(noteON + pedal_channel, C + i, 100);
            keyIsDown[i] = true;
            delayAfterOnTime[i] = millis() + delayAfterOn;
        }else if
            ((sensorInput == HIGH) && keyIsDown[i] && millis() > delayAfterOnTime[i]) {
            MIDImessage(noteOFF + pedal_channel, C + i, 0);
            keyIsDown[i] = false;
            delayAfterOffTime[i] = millis() + delayAfterOff;
        }
    }
}


void MIDImessage(int command, int note, int velocity) {
    Serial1.write(command);
    Serial1.write(note);
    Serial1.write(velocity);
}
