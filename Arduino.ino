
/*
   MQTT 32 output pins for driving 2 16-channel relay boards on an arduino mega with ethernet shield.

   NOTE: the topic is hardcoded: domogik/in/relay/r + relay number : e.g

   mosquitto_pub -t cmd/megarelay/POWER/r1 -m 1  -> turn on
   mosquitto_pub -t cmd/megarelay/POWER/r1 -m 0  -> turn off
   mosquitto_pub -t cmd/megarelay/POWER/r1 -m 2  -> toggle; so turn on again

  By Jeroen Schaeken
  Derived from http://www.esp8266.com/viewtopic.php?f=29&t=8746

  It connects to an MQTT server then:
  - on 0 switches off relay
  - on 1 switches on relay
  - on 2 switches the state of the relay

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.

  There is a 400ms between 2 mutuallyExcluded relays


*/
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <AsyncDelay.h>

// Update these with values suitable for your network.
byte mac[]    = {  0xDE, 0xAB, 0xBE, 0xEF, 0xFE, 0xEF };
byte server[] = { 192, 168, 8, 30 };
byte ip[]     = { 192, 168, 8, 40 };

EthernetClient ethernetClient;
PubSubClient client(ethernetClient);
long lastMsg = 0;
char msg[50];
int value = 0;
AsyncDelay delay_400ms;
AsyncDelay delay_30s;

//MutuallyExclude the 'on' state between pairs of relays (for shutter to prevent up and down at the same time)
//ask R1 to turn on while R2 = on -> turns off R2 first (and broadcasts 'domogik/in/relay/r2 0')
//ask R2 to turn on while R1 = on -> turns off R1 first (and broadcasts 'domogik/in/relay/r1 0')
bool isMutuallyExclude = false;

const char* outTopic = "stat/megarelay/state";
const char* inTopic = "cmd/megarelay/POWER/#";
const char* outRelayTopic = "stat/megarelay/POWER";
const char* outPinTopic = "stat/megarelay/switch/";

bool relayStates[] = {
  HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH
};

int relayPins[] = {
21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44
};       // an array of pin numbers to which LEDs are attached
int pinCount = 24;

#define PIN_DETECT_1 2
#define PIN_DETECT_2 3
#define PIN_DETECT_3 4
#define PIN_DETECT_4 5
#define PIN_DETECT_5 6
#define PIN_DETECT_6 7
#define PIN_DETECT_7 7
//-------- NEW INPUT PINS ----------------
#define PIN_DETECT_15 15  // Input 8
#define PIN_DETECT_16 16  // Input 9 
#define PIN_DETECT_17 17  // Input 10
#define PIN_DETECT_18 18  // Input 11
#define PIN_DETECT_19 19  // Input 12
#define PIN_DETECT_20 20  // Input 13
#define PIN_DETECT_45 45  // Input 14
#define PIN_DETECT_46 46  // Input 15
#define PIN_DETECT_47 47  // Input 16
#define PIN_DETECT_48 48  // Input 17
#define PIN_DETECT_49 49  // Input 18
#define PIN_DETECT_54 54  // Input 19
#define PIN_DETECT_55 55  // Input 20
#define PIN_DETECT_56 56  // Input 21
#define PIN_DETECT_57 57  // Input 22
#define PIN_DETECT_58 58  // Input 23
#define PIN_DETECT_59 59  // Input 24
#define PIN_DETECT_60 60  // Input 25
#define PIN_DETECT_61 61  // Input 26
#define PIN_DETECT_62 62  // Input 27
#define PIN_DETECT_63 63  // Input 28
#define PIN_DETECT_64 64  // Input 29
#define PIN_DETECT_65 65  // Input 30
#define PIN_DETECT_66 66  // Input 31
#define PIN_DETECT_67 67  // Input 32
//---------END NEW PINS ------------------
Bounce debounce1 = Bounce();
Bounce debounce2 = Bounce();
Bounce debounce3 = Bounce();
Bounce debounce4 = Bounce();
Bounce debounce5 = Bounce();
Bounce debounce6 = Bounce();
Bounce debounce7 = Bounce();
Bounce debounce15 = Bounce();
Bounce debounce16 = Bounce();
Bounce debounce17 = Bounce();
Bounce debounce18 = Bounce();
Bounce debounce19 = Bounce();
Bounce debounce20 = Bounce();
Bounce debounce45 = Bounce();
Bounce debounce46 = Bounce();
Bounce debounce47 = Bounce();
Bounce debounce48 = Bounce();
Bounce debounce49 = Bounce();
Bounce debounce54 = Bounce(); //A0
Bounce debounce55 = Bounce(); //A1
Bounce debounce56 = Bounce(); //A2
Bounce debounce57 = Bounce(); //A3
Bounce debounce58 = Bounce(); //A4
Bounce debounce59 = Bounce(); //A5
Bounce debounce60 = Bounce(); //A6
Bounce debounce61 = Bounce(); //A7
Bounce debounce62 = Bounce(); //A8
Bounce debounce63 = Bounce(); //A9
Bounce debounce64 = Bounce(); //A10
Bounce debounce65 = Bounce(); //A11
Bounce debounce66 = Bounce(); //A12
Bounce debounce67 = Bounce(); //A13
// -------------------------------

boolean doSwitch = false;
boolean doAllOff = false;

void setup_ethernet() {

  delay(10);
  // We start by connecting to a WiFi network
  Ethernet.begin(mac, ip);

  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);

}

void delayTimers() {
  delay_400ms.start(50, AsyncDelay::MILLIS);
  delay_30s.start(1, AsyncDelay::MILLIS);
}
void  publishStates() {
  for (int thisPin = 0; thisPin < pinCount; thisPin++) {
    char* state = relayStates[thisPin] == HIGH ? "OFF" : "ON";
    publishRelayState(thisPin + 1, state);
  }
}

void switchRelay(char* switchState, int pos) {
  if (switchState == '0') {
    //Turning off is safe to be done immediately
    digitalWrite(relayPins[pos], HIGH);
    relayStates[pos] = HIGH;
    publishRelayState(pos +1,"OFF") ;
  } else if (switchState == '1') {
    //turning on is done in setStates
    digitalWrite(relayPins[pos], LOW);
    publishRelayState(pos +1,"ON") ;
    publishRelayState ;
    //
    relayStates[pos] = LOW;
  }
}

void setStates() {
  if (doSwitch) {
    for (int thisPin = 0; thisPin < pinCount; thisPin++) {
      digitalWrite(relayPins[thisPin], relayStates[thisPin]);
    }
    doSwitch = true;
    doAllOff = false;
    delay_30s.start(30000, AsyncDelay::MILLIS);
  }
}

void turnAllOff() {
  if (doAllOff) {
    doAllOff = false;
    for (int thisPin = 0; thisPin < pinCount; thisPin++) {
      relayStates[thisPin] = HIGH;
      publishRelayState(thisPin + 1, "OFF");
      switchRelay('0', thisPin);
    }
  }
}
void publishRelayState(int relayNbr, char* state) {
  char outputTopicBuff[100];
  strcpy(outputTopicBuff, outRelayTopic);
  char relaybuffer[5];
  sprintf(relaybuffer, "%d", relayNbr);
  strcat(outputTopicBuff, relaybuffer);
  client.publish(outputTopicBuff, state);
}

void publishPinState(int pinNbr, char* state) {
  char outputTopicBuff[100];
  strcpy(outputTopicBuff, outPinTopic);
  char relaybuffer[5];
  sprintf(relaybuffer, "%d", pinNbr);
  strcat(outputTopicBuff, relaybuffer);
  client.publish(outputTopicBuff, state);
}

void mutuallyExcludePair(char* switchState, int pos) {
  if (pos % 2 == 0 && switchState == '1') //even
  {
    if (pinCount > (pos + 1) && relayStates[pos + 1] == LOW) {
      switchRelay('0', pos + 1);
      publishRelayState(pos + 2, "OFF");
    }
  }
  else if (pos % 2 == 1 && switchState == '1') //odd
  {
    if ((pos - 1) >= 0 && relayStates[pos - 1] == LOW) {
      switchRelay('0', pos - 1);
      publishRelayState(pos, "OFF");
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //Comes in as domogik/in/relay/r1 ... domogik/in/relay/r16
  String topicString = String(topic);
  int relayNumber = topicString.substring(21, 25).toInt();
  int posInArray = relayNumber - 1;
  char* switchState = (char)payload[0];

  if (switchState == '2') {
    if (relayStates[posInArray] == HIGH) {
      switchState = '1';
    }
    else {
      switchState = '0';
    }
  }

  if (isMutuallyExclude) {
    mutuallyExcludePair(switchState, posInArray);
  }

  switchRelay(switchState, posInArray);
  if (switchState == '1') {
    doSwitch = true;
  }

  delayTimers();
}


void reconnect() {
  while (!client.connected()) {
    if (client.connect("Megarelay")) {
      Serial.println("connected");
      client.publish(outTopic, "Mega Relay Connected");
      digitalWrite(13, HIGH);
      publishStates();
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
       digitalWrite(13, LOW);
       delay(500);
        digitalWrite(13, HIGH);
        delay(500);
        digitalWrite(13, LOW);
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for (int i = 0; i < 5000; i++) {
        delay(1);
      }
    }
  }
}


void setup() {

  //Relayboard
  for (int thisPin = 0; thisPin < pinCount; thisPin++) {
    digitalWrite(relayPins[thisPin], HIGH);
    pinMode(relayPins[thisPin], OUTPUT);
  }
  pinMode(13, OUTPUT);

  //init state relayboard
  //turnAllOff();

  delayTimers();

  //input
  pinMode(PIN_DETECT_1, INPUT_PULLUP);
  pinMode(PIN_DETECT_2, INPUT_PULLUP);
  pinMode(PIN_DETECT_3, INPUT_PULLUP);
  pinMode(PIN_DETECT_4, INPUT_PULLUP);
  pinMode(PIN_DETECT_5, INPUT_PULLUP);
  pinMode(PIN_DETECT_6, INPUT_PULLUP);
  pinMode(PIN_DETECT_7, INPUT_PULLUP);
  pinMode(PIN_DETECT_15, INPUT_PULLUP);
   pinMode(PIN_DETECT_16, INPUT_PULLUP);
   pinMode(PIN_DETECT_17, INPUT_PULLUP);
   pinMode(PIN_DETECT_18, INPUT_PULLUP);
   pinMode(PIN_DETECT_19, INPUT_PULLUP);
   pinMode(PIN_DETECT_20, INPUT_PULLUP);
   pinMode(PIN_DETECT_45, INPUT_PULLUP);
   pinMode(PIN_DETECT_46, INPUT_PULLUP);
   pinMode(PIN_DETECT_47, INPUT_PULLUP);
   pinMode(PIN_DETECT_48, INPUT_PULLUP);
   pinMode(PIN_DETECT_49, INPUT_PULLUP);
   pinMode(PIN_DETECT_54, INPUT_PULLUP);
   pinMode(PIN_DETECT_55, INPUT_PULLUP);
   pinMode(PIN_DETECT_56, INPUT_PULLUP);
   pinMode(PIN_DETECT_57, INPUT_PULLUP);
   pinMode(PIN_DETECT_58, INPUT_PULLUP);
   pinMode(PIN_DETECT_59, INPUT_PULLUP);
   pinMode(PIN_DETECT_60, INPUT_PULLUP);
   pinMode(PIN_DETECT_61, INPUT_PULLUP);  
   pinMode(PIN_DETECT_62, INPUT_PULLUP);  
   pinMode(PIN_DETECT_63, INPUT_PULLUP);  
   pinMode(PIN_DETECT_64, INPUT_PULLUP);  
   pinMode(PIN_DETECT_65, INPUT_PULLUP);  
   pinMode(PIN_DETECT_66, INPUT_PULLUP);
   pinMode(PIN_DETECT_67, INPUT_PULLUP);       
  // -------------------------------------
  debounce1.attach(PIN_DETECT_1);
  debounce2.attach(PIN_DETECT_2);
  debounce3.attach(PIN_DETECT_3);
  debounce4.attach(PIN_DETECT_4);
  debounce5.attach(PIN_DETECT_5);
  debounce6.attach(PIN_DETECT_6);
  debounce6.attach(PIN_DETECT_7);
  debounce15.attach(PIN_DETECT_15);
  debounce16.attach(PIN_DETECT_16);
  debounce17.attach(PIN_DETECT_17);
  debounce18.attach(PIN_DETECT_18);
  debounce19.attach(PIN_DETECT_19);
  debounce20.attach(PIN_DETECT_20);
  debounce45.attach(PIN_DETECT_45);
  debounce46.attach(PIN_DETECT_46);
  debounce47.attach(PIN_DETECT_47);
  debounce48.attach(PIN_DETECT_48);
  debounce49.attach(PIN_DETECT_49);
  debounce54.attach(PIN_DETECT_54);
  debounce55.attach(PIN_DETECT_55);
  debounce56.attach(PIN_DETECT_56);
  debounce57.attach(PIN_DETECT_57);
  debounce58.attach(PIN_DETECT_58);
  debounce59.attach(PIN_DETECT_59);
  debounce60.attach(PIN_DETECT_60);
  debounce61.attach(PIN_DETECT_61);
  debounce62.attach(PIN_DETECT_62);
  debounce63.attach(PIN_DETECT_63);
  debounce64.attach(PIN_DETECT_64);
  debounce65.attach(PIN_DETECT_65);
  debounce66.attach(PIN_DETECT_66);
  debounce67.attach(PIN_DETECT_67);
  // -----------------------------------
  debounce1.interval(50);
  debounce2.interval(50);
  debounce3.interval(50);
  debounce4.interval(50);
  debounce5.interval(50);
  debounce6.interval(50);
  debounce7.interval(50);
  debounce15.interval(50);
   debounce16.interval(50);
   debounce17.interval(50);
   debounce18.interval(50);
   debounce19.interval(50);
   debounce20.interval(50);
  debounce45.interval(50); 
  debounce46.interval(50); 
  debounce47.interval(50); 
  debounce48.interval(50); 
  debounce49.interval(50); 
  debounce54.interval(50); 
  debounce55.interval(50); 
  debounce56.interval(50); 
  debounce57.interval(50); 
  debounce58.interval(50);
  debounce59.interval(50);
  debounce60.interval(50);
  debounce61.interval(50);
  debounce62.interval(50);
  debounce63.interval(50);
  debounce64.interval(50);
  debounce65.interval(50);
  debounce66.interval(50);
  debounce67.interval(50);
  // ------------------------------------ 


  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);

  Serial.begin(115200);
  setup_ethernet();
  client.setServer(server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  if (delay_400ms.isExpired()) {
    setStates();
    delay_400ms.repeat();
  }
  if (delay_30s.isExpired()) {
    turnAllOff();
    delay_30s.repeat();
  }
  debounce1.update();
  if ( debounce1.fell() ) {
    publishPinState(1, "ON");
  }
  if ( debounce1.rose() ) {
    publishPinState(1, "OFF");
  }
  debounce2.update();
  if ( debounce2.fell() ) {
    publishPinState(2, "ON");
  }
  if ( debounce2.rose() ) {
    publishPinState(2, "OFF");
  }
  debounce3.update();
  if ( debounce3.fell() ) {
    publishPinState(3, "ON");
  }
  if ( debounce3.rose() ) {
    publishPinState(3, "OFF");
  }
  debounce4.update();
  if ( debounce4.fell() ) {
    publishPinState(4, "ON");
  }
  if ( debounce4.rose() ) {
    publishPinState(4, "OFF");
  }
  debounce5.update();
  if ( debounce5.fell() ) {
    publishPinState(5, "ON");
  }
  if ( debounce5.rose() ) {
    publishPinState(5, "OFF");
  }
  debounce6.update();
  if ( debounce6.fell() ) {
    publishPinState(6, "ON");
  }
  if ( debounce6.rose() ) {
    publishPinState(6, "OFF");
  }
    debounce7.update();
  if ( debounce7.fell() ) {
    publishPinState(7, "ON");
  }
  if ( debounce7.rose() ) {
    publishPinState(7, "OFF");
  }
      debounce15.update();
  if ( debounce15.fell() ) {
    publishPinState(15, "ON");
  }
  if ( debounce15.rose() ) {
    publishPinState(15, "OFF");
  }
// ---------------------------------
      debounce16.update();
  if ( debounce16.fell() ) {
    publishPinState(16, "ON");
  }
  if ( debounce16.rose() ) {
    publishPinState(16, "OFF");
  }

        debounce17.update();
  if ( debounce17.fell() ) {
    publishPinState(17, "ON");
  }
  if ( debounce17.rose() ) {
    publishPinState(17, "OFF");
  }

         debounce18.update();
    if ( debounce18.fell() ) {
 publishPinState(18, "ON");
  }
    if ( debounce18.rose() ) {
 publishPinState(18, "OFF");
  }

         debounce19.update();
    if ( debounce19.fell() ) {
 publishPinState(19, "ON");
  }
    if ( debounce19.rose() ) {
 publishPinState(19, "OFF");
  }

         debounce20.update();
    if ( debounce20.fell() ) {
 publishPinState(20, "ON");
  }
    if ( debounce20.rose() ) {
 publishPinState(20, "OFF");
  }
         debounce45.update();
    if ( debounce45.fell() ) {
 publishPinState(45, "ON");
  }
    if ( debounce45.rose() ) {
 publishPinState(45, "OFF");
  }

         debounce46.update();
    if ( debounce46.fell() ) {
 publishPinState(46, "ON");
  }
    if ( debounce46.rose() ) {
 publishPinState(46, "OFF");
  }
         debounce47.update();
    if ( debounce47.fell() ) {
 publishPinState(47, "ON");
  }
    if ( debounce47.rose() ) {
 publishPinState(47, "OFF");
  }
          debounce48.update();
    if ( debounce48.fell() ) {
 publishPinState(48, "ON");
  }
    if ( debounce48.rose() ) {
 publishPinState(48, "OFF");
  }
          debounce49.update();
    if ( debounce49.fell() ) {
 publishPinState(49, "ON");
  }
    if ( debounce49.rose() ) {
 publishPinState(49, "OFF");
  }
         debounce54.update();
    if ( debounce54.fell() ) {
 publishPinState(54, "ON");
  }
    if ( debounce54.rose() ) {
 publishPinState(54, "OFF");
  }
         debounce55.update();  //---------
    if ( debounce55.fell() ) {
 publishPinState(55, "ON");
  }
    if ( debounce55.rose() ) {
 publishPinState(55, "OFF");
  }
         debounce56.update(); //--------
    if ( debounce56.fell() ) {
 publishPinState(56, "ON");
  }
    if ( debounce56.rose() ) {
 publishPinState(56, "OFF");
  }
         debounce57.update(); // -------
    if ( debounce57.fell() ) {
 publishPinState(57, "ON");
  }
    if ( debounce57.rose() ) {
 publishPinState(57, "OFF");
  }
         debounce58.update(); // -------
    if ( debounce58.fell() ) {
 publishPinState(58, "ON");
  }
    if ( debounce58.rose() ) {
 publishPinState(58, "OFF");
  }
         debounce59.update(); // -------
    if ( debounce59.fell() ) {
 publishPinState(59, "ON");
  }
    if ( debounce59.rose() ) {
 publishPinState(59, "OFF");
 }
         debounce60.update(); // -------
    if ( debounce60.fell() ) {
 publishPinState(60, "ON");
  }
    if ( debounce60.rose() ) {
 publishPinState(60, "OFF");
  }
         debounce61.update(); // -------
    if ( debounce61.fell() ) {
 publishPinState(61, "ON");
  }
    if ( debounce61.rose() ) {
 publishPinState(61, "OFF");
  }
         debounce62.update(); // -------
    if ( debounce62.fell() ) {
 publishPinState(62, "ON");
  }
    if ( debounce62.rose() ) {
 publishPinState(62, "OFF");
  }
         debounce63.update(); // -------
    if ( debounce63.fell() ) {
 publishPinState(63, "ON");
  }
    if ( debounce63.rose() ) {
 publishPinState(63, "OFF");
  }
         debounce64.update(); // -------
    if ( debounce64.fell() ) {
 publishPinState(64, "ON");
  }
    if ( debounce64.rose() ) {
 publishPinState(64, "OFF");
  }
         debounce65.update(); // -------
    if ( debounce65.fell() ) {
 publishPinState(65, "ON");
  }
    if ( debounce65.rose() ) {
 publishPinState(65, "OFF");
  }
         debounce66.update(); // -------
    if ( debounce66.fell() ) {
 publishPinState(66, "ON");
  }
    if ( debounce66.rose() ) {
 publishPinState(66, "OFF");
  }
         debounce67.update(); // -------
    if ( debounce67.fell() ) {
 publishPinState(67, "ON");
  }
    if ( debounce67.rose() ) {
 publishPinState(67, "OFF");
  }

  client.loop();
}
