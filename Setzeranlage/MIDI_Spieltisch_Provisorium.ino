/*
 Name:		MIDI_Spieltisch_Provisorium.ino
 Created:	12/18/2021 8:23:05 PM
 Author:	Sabine
*/

#include <SoftWire.h>
#include <SPI.h>
#include <SD.h>
#include <TM1637.h>           // Grove - 4-Digit Display
#include <Wire.h>             // I2C library
#include <SparkFunSX1509.h>   // SX1509 library

// Pins **********************
const byte Ab_PIN = 2;
const byte Ebene_Plus_PIN = 3;
const byte Ebene_Minus_PIN = 4;
const byte Set_PIN = 5;

const byte Kombination_Minus_PIN = 6;
const byte Kombination_Plus_PIN = 7;

const byte SHCP_PIN = 9;
const byte STCP_PIN = 10;
const byte DS_PIN = 11;
const byte KEYPAD_PIN = 12;
const byte DIO_PIN = 16;
const byte CLK_PIN = 17;

// 74HC595 Shiftregister für die linken LEDs
byte bits[3] = { 0x00, 0x00, 0x00 };

// LEDs **********************
byte ledMap[25] = { 35,33,31,29,27,25,23,22,24,26,28,30,32,37,39,41,43,45,47,34,36,38,40,42,44 };

// SX1509 Expander zum Scannen der Registertaster
byte SX1509_ADDRESS = 0x3E;
SX1509 sx1509;
byte scanTime = 16;
byte debounceTime = 8;
unsigned int sleepTime = 0;
byte buttonMap[56] = { 4, 3, 2, 1, 0, 0, 0,10, 9, 8, 7, 5, 6, 0,
                        16,15,14,13,11,12, 0,22,21,20,19,17,18, 0,
                        42,43,44,45,46,47, 0,36,37,38,39,40,41, 0,
                        30,31,32,33,34,35, 0,23,24,25,26,27,28,29 };

// Setzer **********************
boolean setButtonPressed;
boolean setzerButton[6];
unsigned long setzerButtonTimeout[6];
byte kombination[594];
String ebeneName[99];
int kombinationNummer = 0;
int ebeneNummer = 0;
bool registerStatus[47];

// Display **********************
TM1637 tm1637(CLK_PIN, DIO_PIN);

// ***********************************************************************************************************************
// ***********************************************************************************************************************

void setup()
{
    // Serial ports

    Serial.begin(19200);
    Serial1.begin(19200);

    Serial.println("Register Setzer GrandOrgue");

    // Display *******************

    tm1637.init();
    tm1637.set(4);
    tm1637.display(0, 0);
    tm1637.display(1, 0);
    tm1637.display(2, 0);
    tm1637.display(3, 0);

    // 74HC595 *******************

    Serial.println("74HC595");
    message(1);

    pinMode(SHCP_PIN, OUTPUT);
    pinMode(STCP_PIN, OUTPUT);
    pinMode(DS_PIN, OUTPUT);
    push();

    // SX1509 ********************

    Serial.println("SX1509");
    message(2);

    if (!sx1509.begin(SX1509_ADDRESS))
    {
        error(1);
    }

    pinMode(KEYPAD_PIN, INPUT_PULLUP);
    sx1509.keypad(7, 8, sleepTime, scanTime, debounceTime);

    // LEDs **********************

    Serial.println("LEDs");
    message(3);

    for (int i = 22; i < 48; i++) {
        pinMode(i, OUTPUT);
    }

    // SD Card *******************

    Serial.println("SD Card");
    message(4);

    if (!SD.begin()) {
        error(2);
    }

    ReadDirectory();

    // Setzer ********************

    Serial.println("Setzer");
    message(5);
    tm1637.set(0);

    for (int i = 2; i <= 7; i++) {
        pinMode(i, INPUT_PULLUP);
    }

    ladeSpeicherebene(1);
    aktiviereKombination(1);
}

// ***********************************************************************************************************************
// ***********************************************************************************************************************

void loop()
{
    // Registertaste gedrückt?
    byte button = checkRegisterMatrix();

    if (button >= 0 && button < 47) {
        if (registerStatus[button]) { // Falls Register schon gezogen...
            ab(button); // ... dann ab
        }else {
            an(button); // ... sonst an
        }
    }

    // Setzertaste gedrückt?
    checkSetzerButtons();
}

// ***********************************************************************************************************************
// ***********************************************************************************************************************

/// <summary>
/// Set-Knopf gerade niedergedrückt
/// </summary>
void Set_Pressed() {
    Serial.println("S E T");
    tm1637.set(7);
    displayKombinationNr(kombinationNummer);
    setButtonPressed = true;
}

/// <summary>
/// Set-Knopf gerade losgelassen
/// </summary>
void Set_Released() {
    byte offset = 6 * (kombinationNummer - 1);
    for (int r = 0; r < 47; r++) {
        if (registerStatus[r]) {
            bitSet(kombination[offset + r / 8], r % 8);
        }else {
            bitClear(kombination[offset + r / 8], r % 8);
        }
    }

    writeKombinationToSD();

    tm1637.set(0);
    displayKombinationNr(kombinationNummer);
    setButtonPressed = false;
}

/// <summary>
/// Kombination eins runter
/// </summary>
void Kombination_Minus() {
    if (kombinationNummer > 1) {
        kombinationNummer--;
        if (setButtonPressed) {
            displayKombinationNr(kombinationNummer);
        }else {
            aktiviereKombination(kombinationNummer);
        }
    }
}

/// <summary>
/// Kombination eins rauf
/// </summary>
void Kombination_Plus() {
    if (kombinationNummer < 99) {
        kombinationNummer++;
        if (setButtonPressed) {
            displayKombinationNr(kombinationNummer);
        }else {
            aktiviereKombination(kombinationNummer);
        }
    }
}

/// <summary>
/// Ebene eins runter
/// </summary>
void Ebene_Minus() {
    if (ebeneNummer > 1) {
        ebeneNummer--;
        ladeSpeicherebene(ebeneNummer);
        if (setButtonPressed) {
            displayKombinationNr(1);
        }else {
            aktiviereKombination(1);
        }
    }
}

/// <summary>
/// Ebene eins rauf
/// </summary>
void Ebene_Plus() {
    if (ebeneNummer < 99) {
        ebeneNummer++;
        ladeSpeicherebene(ebeneNummer);
        if (setButtonPressed) {
            displayKombinationNr(1);
        }else {
            aktiviereKombination(1);
        }
    }
}

/// <summary>
/// Ab-Knopf gedrückt
/// </summary>

void Ab_Pressed() {
    for (int i = 0; i < 47; i++) {
        ab(i);
    }
}


/// <summary>
/// Aktiviert Kombination
/// </summary>
/// <param name="kombinationNummer">Zu aktivierende Kombination</param>
void aktiviereKombination(byte nummer) {
    byte offset = 6 * (nummer - 1);

    for (int r = 0; r < 47; r++) {
        if (bitRead(kombination[offset + r / 8], r % 8)) {
            an(r);
        }else {
            ab(r);
        }
    }

    displayKombinationNr(nummer);
    kombinationNummer = nummer;
    Serial.print("Kombination ");
    Serial.println(kombinationNummer);
}

/// <summary>
/// Lädt Speicherebene von der SD-Karte
/// </summary>
/// <param name="speicherebene">Speicherebene</param>
void ladeSpeicherebene(byte speicherebene) {
    String fileName = String(speicherebene, DEC) + ".SEQ";
    File myFile = SD.open(fileName);
    int fileSize = myFile.size();
    myFile.read(kombination, 594);
    myFile.close();

    displayEbeneNr(speicherebene);
    ebeneNummer = speicherebene;
    Serial.println(ebeneName[ebeneNummer]);
}

/// <summary>
/// Register wird gezogen
/// </summary>
/// <param name="registerNummer"> Registernummer</param>
void an(byte registerNummer) {
    // Linke Seite
    if (registerNummer >= 0 && registerNummer < 22) {
        linkesRegisterSetzen(registerNummer);
        MidiSendRegisterStatus(registerNummer, true);
    }

    // Rechte Seite
    if (registerNummer >= 22) {
        digitalWrite(ledMap[registerNummer - 22], HIGH);
        MidiSendRegisterStatus(registerNummer, true);
    }

    registerStatus[registerNummer] = true;
}

/// <summary>
/// Register wird abgestoßen
/// </summary>
/// <param name="registerNummer"> Registernummer</param>
void ab(byte registerNummer) {
    //Serial.print(registerNummer, DEC);
    //Serial.println(" ab");

    // Linke Seite
    if (registerNummer >= 0 && registerNummer < 22) {
        linkesRegisterAb(registerNummer);
        MidiSendRegisterStatus(registerNummer, false);
    }

    // Rechte Seite
    if (registerNummer >= 22) {
        digitalWrite(ledMap[registerNummer - 22], LOW);
        MidiSendRegisterStatus(registerNummer, false);
    }

    registerStatus[registerNummer] = false;
}

void message(int m) {
    tm1637.display(0, 10);
    tm1637.display(1, 10);
    tm1637.display(2, m / 10 % 10);
    tm1637.display(3, m % 10);
}

void error(int fehler) {
    tm1637.set(7);
    tm1637.display(0, 14);
    tm1637.display(1, 14);
    tm1637.display(2, fehler / 10 % 10);
    tm1637.display(3, fehler % 10);
    while (true);
}

void displayKombinationNr(int nummer) {
    tm1637.display(2, nummer / 10 % 10);
    tm1637.display(3, nummer % 10);
}

void displayEbeneNr(int nummer) {
    tm1637.display(0, nummer / 10 % 10);
    tm1637.display(1, nummer % 10);
}

/// <summary>
/// Prüft, welche Setzer-Taster gedrückt sind.
/// </summary>
void checkSetzerButtons() {
    for (int i = 2; i <= 7; i++) {
        if (digitalRead(i) == LOW) {
            if (!setzerButton[i]) {
                setzerButton[i] = true;
                setzerButtonTimeout[i] = millis()+100;
                buttonPressed(i);
            }
        }
        else if (setzerButton[i] && (millis() > setzerButtonTimeout[i])) {
                setzerButton[i] = false;
                buttonReleased(i);
        }
    }
}

void buttonPressed(byte button) {

    switch (button)
    {
    case Ebene_Minus_PIN:
        Ebene_Minus();
        break;
    case Ebene_Plus_PIN:
        Ebene_Plus();
        break;
    case Kombination_Minus_PIN:
        Kombination_Minus();
        break;
    case Kombination_Plus_PIN:
        Kombination_Plus();
        break;
    case Set_PIN:
        Set_Pressed();
        break;
    case Ab_PIN:
        Ab_Pressed();
        break;
    default:
        break;
    }
}

void buttonReleased(byte button) {
    switch (button)
    {
    case Set_PIN:
        Set_Released();
        break;
    default:
        break;
    }
}

/// <summary>
/// Prüft, ob ein Registertaster gedrückt ist.
/// Liest dazu die Werte aus dem SX1509 Expander.
/// </summary>
/// <returns>Die Nummer des gedrückten Registertasters - sonst 255.</returns>
byte checkRegisterMatrix() {
    byte mappedValue = 255;

    if (!digitalRead(KEYPAD_PIN))
    {
        unsigned int keyData = sx1509.readKeypad();

        byte row = sx1509.getRow(keyData);
        byte col = sx1509.getCol(keyData);

        mappedValue = buttonMap[7 * col + row] - 1;
    }

    return mappedValue;
}

/// <summary>
/// Sendet dem Midi-Arduino über die Serial1-Schnittstelle den Befehl, dass das Register "nummer" AN oder AB gesetzt werden soll.
/// Der MIDI-Arduino sendet das dann als MIDI-Befehl an GrandOrgue.
/// </summary>
/// <param name="nummer">Registernummer</param>
/// <param name="status">AN oder AB</param>
void MidiSendRegisterStatus(byte nummer, bool status) {

    Serial1.write(status ? bitSet(nummer, 7) : nummer);
}

/// <summary>
/// Setzt Register nummer "registerNummer".
/// </summary>
/// <param name="registerNummer">Registernummer</param>
void linkesRegisterSetzen(int registerNummer) {
    bitSet(bits[registerNummer / 8], registerNummer % 8);
    push();
}

/// <summary>
/// Setzt Register nummer "registerNummer" zurück.
/// </summary>
/// <param name="registerNummer">Registernummer</param>
void linkesRegisterAb(int registerNummer) {
    bitClear(bits[registerNummer / 8], registerNummer % 8);
    push();
}

/// <summary>
/// Prüft, ob Register "registerNummer" gesetzt ist.
/// </summary>
/// <param name="registerNummer">Registernummer</param>
/// <returns>Ist das Register gesetzt?</returns>
bool isLinkesRegisterGesetzt(int registerNummer) {
    //return (bitRead(bits[byteNr[registerNummer]], bitNr[registerNummer]) == 1);
    return (bitRead(bits[registerNummer / 8], registerNummer % 8) == 1);
}

/// <summary>
/// Schiebt die drei Bytes bits[0...2] in das Schieberegister.
/// </summary>
void push() {
    digitalWrite(STCP_PIN, LOW);
    shiftOut(DS_PIN, SHCP_PIN, MSBFIRST, bits[2]);
    shiftOut(DS_PIN, SHCP_PIN, MSBFIRST, bits[1]);
    shiftOut(DS_PIN, SHCP_PIN, MSBFIRST, bits[0]);
    digitalWrite(STCP_PIN, HIGH);
}

/// <summary>
/// Erzeugt ein leeres Setzer-File auf der SD-Karte. 
/// </summary>
/// <param name="name">Der Name des Files.</param>
void writeNewFile(String name) {
    const byte emptyByte = 0;

    File myFile = SD.open(name, FILE_WRITE);
    if (myFile) {
        for (int b = 0; b < 594; b++) {
            myFile.write(emptyByte);
        }
        myFile.close();
    }
}

void writeKombinationToSD() {
    String fileName = String(ebeneNummer, DEC) + ".SEQ";
    Serial.print("Write File to SD: " + fileName + " ... ");
    File myFile = SD.open(fileName, 0X02);
    myFile.write(kombination, 594);
    myFile.close();
    Serial.println(" done");
}

void ReadDirectory() {
    File myFile = SD.open("dir.txt");
    String fileContent = "";
    char input;

    if (myFile) {
        while (myFile.available()) {
            input = myFile.read();
            fileContent += input;
        }
        myFile.close();

        extractStrings(fileContent);
    }
    else {
        error(3);
    }
}

/// <summary>
/// Weist jede Zeile aus dem String "buffer" einer Ebene als Name zu.
/// </summary>
/// <param name="buffer">Der Inhalt des Files "dir.txt".</param>
void extractStrings(String stringbuffer) {
    char newline = 10;
    int pointer = 0;
    int counter = 1;
    int index = stringbuffer.indexOf(newline);

    while (index != -1) {
        ebeneName[counter] = stringbuffer.substring(pointer, index - 1);
        Serial.print(counter);
        Serial.print(" : ");
        Serial.println(ebeneName[counter++]);

        pointer = index + 1;
        index = stringbuffer.indexOf(newline, pointer);
    }
}