#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <EEPROM.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(5, 4); //SIM800L Tx & Rx is connected to Arduino #3 & #2

const int zuheizer_out = 6;
const int standheizung_out = 7;

const int zuheizer_in = A2;
const int standheizung_in = A3;


const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data

char lastNumber[20];

boolean newData = false;
boolean showNext = false;

boolean debug = false;
boolean zuheizer_ctrl = false;
boolean zuheizer_ext = false;
boolean zuheizer_impuls = false;
boolean standheizung_ctrl = false;
boolean standheizung_ext = false;
boolean standheizung_impuls = false;
int Signal = 0;


void setup()
{

  pinMode(zuheizer_out, OUTPUT);
  pinMode(standheizung_out, OUTPUT);

  pinMode(zuheizer_in, INPUT);
  pinMode(standheizung_in, INPUT);

  EEPROM.get(0, debug);
  EEPROM.get(1, zuheizer_ctrl);
  EEPROM.get(2, zuheizer_impuls);
  EEPROM.get(3, standheizung_ctrl);
  EEPROM.get(4, standheizung_impuls);
  
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);

  //Begin serial communication with Arduino and SIM800L
  mySerial.begin(9600);

  Serial.println("Initializing...");
  delay(1000);
  mySerial.println("AT"); //Once the handshake test is successful, it will back to OK
  delay(50);
  mySerial.println("AT+CSQ");
  delay(50);
  mySerial.println("AT+CMGF=1"); // Configuring TEXT mode
  delay(50);
  mySerial.println("AT+CNMI=1,2,0,0,0"); // Decides how newly arrived SMS messages should be handled
  delay(50);

  mySerial.println("AT+CSCLK=2"); // Configuring Sleep
  delay(50);

  //sendMessage("Hello");
}

unsigned long idleTime;

void ring() {
  detachInterrupt(digitalPinToInterrupt(2));
}

void loop()
{
  readResponse();


  /* if (millis() - idleTime >= 5000) {
     Serial.println("Sleep");
     attachInterrupt(digitalPinToInterrupt(2), ring, LOW);
     set_sleep_mode(SLEEP_MODE_PWR_DOWN);
     sleep_enable();
     sleep_mode();
     sleep_disable();
     delay(100);
    }
  */
}

void readResponse() {
  recvWithEndMarker();
  showNewData();
}

void halteAktiv() {
  idleTime = millis();
}

void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (mySerial.available() > 0 && newData == false) {
    halteAktiv();
    rc = mySerial.read();

    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
}

void showNewData() {
  if (newData == true) {
    newData = false;
    if (showNext) {
      // Prüfe auf SMS erhalten Flag
      // Falls true, parse Nachricht
      parseMessage();

      // Setze Flag zurück
      showNext = false;
    }
    if (strstr(receivedChars, "+CMT")) {      
      char _lastNumber[20];
      // SMS erhalten, setze Flag
      showNext = true;

      // Ermittle Rufnummer des Senders
      // SMS Header sieht so aus: +CMT: "+49xxxxxxxxxxxxx","","21/02
      
      // Suche erstes Vorkommen von " 
      char* pos = strstr(receivedChars, "\"");
      // Erzeuge Position aus Pointer
      int index = pos - receivedChars;
      // Erzeuge Substring ab erstem "
      strcpy(_lastNumber, &receivedChars[index+1]);
      // Suche zweites " in Substring
      pos = strstr(_lastNumber, "\"");
      // Erzeuge Position aus Pointer
      index = pos - _lastNumber;
      // Erzeuge Substring, der nur noch Rufnummer enthält
      strncpy(lastNumber, _lastNumber, index);
      // Füge Null-Char am Ende ein
      lastNumber[index] = '\0'; 
    }
    
    else if (strstr(receivedChars, "+CSQ:")) {
      // Hole Signalstärke aus String
      char signalChars[2];
      signalChars[0] = receivedChars[6];
      signalChars[1] = receivedChars[7];
      Signal = atoi(signalChars);
    }
    Serial.print("raw: ");
    Serial.println(receivedChars);

  }
}

void parseMessage() {
  if (strstr(receivedChars, "Zuheizer") || strstr(receivedChars, "zuheizer") ) {
    if (strstr(receivedChars, "1") || strstr(receivedChars, "an") || strstr(receivedChars, "ein") || strstr(receivedChars, "on") || strstr(receivedChars, "An") || strstr(receivedChars, "Ein") || strstr(receivedChars, "On")) {
      if (strstr(receivedChars, "Impuls") || strstr(receivedChars, "impuls")) {
        Serial.println("Zuheizer Impulsmodus aktiviert");
        if (debug) sendMessage("Zuheizer_Impuls ein");
        EEPROM.put(2, zuheizer_impuls);
      }
      else {
        Serial.println("Zuheizer einschalten");
        if (debug) sendMessage("Zuheizer ein");
        zuheizer_ctrl = true;
        EEPROM.put(1, zuheizer_ctrl);
      }
    }

    else if (strstr(receivedChars, "0") || strstr(receivedChars, "aus") || strstr(receivedChars, "off") || strstr(receivedChars, "Aus") || strstr(receivedChars, "Off")) {
      if (strstr(receivedChars, "Impuls") || strstr(receivedChars, "impuls")) {
        Serial.println("Zuheizer Impulsmodus deaktiviert");
        if (debug) sendMessage("Zuheizer_Impuls aus");
        zuheizer_impuls = false;
        EEPROM.put(2, zuheizer_impuls);
      }
      else {
        Serial.println("Zuheizer ausschalten");
        if (debug) sendMessage("Zuheizer aus");
        zuheizer_ctrl = false;
        EEPROM.put(1, zuheizer_ctrl);
      }
    }
  }

  else if (strstr(receivedChars, "Standheizung") || strstr(receivedChars, "standheizung") ) {
    if (strstr(receivedChars, "1") || strstr(receivedChars, " an") || strstr(receivedChars, "ein") || strstr(receivedChars, "on") || strstr(receivedChars, "An") || strstr(receivedChars, "Ein") || strstr(receivedChars, "On")) {
      if (strstr(receivedChars, "Impuls") || strstr(receivedChars, "impuls")) {
        Serial.println("Standheizung Impulsmodus aktiviert");
        if (debug) sendMessage("Standheizung_Impuls ein");
        standheizung_impuls = true;
        EEPROM.put(4, standheizung_impuls);
      }
      else {
        Serial.println("Standheizung einschalten");
        if (debug) sendMessage("Standheizung ein");
        standheizung_ctrl = true;
        EEPROM.put(3, standheizung_ctrl);
      }
    }

    else if (strstr(receivedChars, "0") || strstr(receivedChars, "aus") || strstr(receivedChars, "off") || strstr(receivedChars, "Aus") || strstr(receivedChars, "Off")) {
      if (strstr(receivedChars, "Impuls") || strstr(receivedChars, "impuls")) {
        Serial.println("Standheizung Impulsmodus deaktiviert");
        if (debug) sendMessage("Standheizung_Impuls aus");
        standheizung_impuls = false;
        EEPROM.put(4, standheizung_impuls);        
      }
      else {
        Serial.println("Standheizung ausschalten");
        if (debug) sendMessage("Standheizung aus");
        standheizung_ctrl = false;
        EEPROM.put(3, standheizung_ctrl);
      }
    }
  }

  else if (strstr(receivedChars, "Debug") || strstr(receivedChars, "debug") ) {
    if (strstr(receivedChars, "1") || strstr(receivedChars, "an") || strstr(receivedChars, "ein") || strstr(receivedChars, "on") || strstr(receivedChars, "An") || strstr(receivedChars, "Ein") || strstr(receivedChars, "On")) {
      sendMessage("Debugmodus aktiviert");
      debug = true;
      EEPROM.put(0, debug);
    }

    if (strstr(receivedChars, "0") || strstr(receivedChars, "aus") || strstr(receivedChars, "off") || strstr(receivedChars, "Aus") || strstr(receivedChars, "Off")) {
      sendMessage("Debugmodus deaktiviert");
      debug = false;
      EEPROM.put(0, debug);
    }
  }

  else if (strstr(receivedChars, "Status") || strstr(receivedChars, "status") ) {
    
    String message = "Zuheizer: ";
    if (zuheizer_ext) message += "ein\r\n";
    else message += "aus\r\n";
    message += "Standheizung: ";
    if (standheizung_ext) message += "ein\r\n";
    else message += "aus\r\n";
    message += "\r\nDebug: ";
    if (debug) message += "ein\r\n";
    else message += "aus\r\n";
    message += "Signalstaerke: ";
    message += Signal;
    message += "/31\r\n";
    message += "Impulsmodus Zuheizer: ";
    if (zuheizer_impuls) message += "ein\r\n";
    else message += "aus\r\n";
    message += "Impulsmodus Standheizung: ";
    if (standheizung_impuls) message += "ein\r\n";
    else message += "aus\r\n";

    sendMessage(message);
  }

}

void sendMessage(String message) {
  // Wecke Sim800L Modul wieder auf
  mySerial.println("AT"); 
  // Warte
  delay(2000);
  
  // Configuring TEXT mode
  mySerial.println("AT+CMGF=1"); 
  delay(50);

  // Erzeuge Sende SMS Befehl
  char SendNumber[30];
  strcpy(SendNumber, "AT+CMGS=\"");
  strcat(SendNumber, lastNumber);
  strcat(SendNumber, "\"");

  //Sende Befehl
  mySerial.println(SendNumber);
  delay(50);
  // Sende Nachricht
  mySerial.print(message); 
  delay(50);
  // Sende End-Char
  mySerial.write(26);
  delay(50);
}
