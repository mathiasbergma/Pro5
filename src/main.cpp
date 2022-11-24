#include <Arduino.h>
#include <HardwareSerial.h>
#include <SparkFun_LTE_Shield_Arduino_Library.h>
#include "certs.h"

HardwareSerial lteSerial(2);
LTE_Shield lte;

#define SerialMonitor Serial
#define LTEShieldSerial lteSerial

#include "AT_commands.h"
#include "NB_R410M.h"



const struct connection_info_t
{
  const char *HostName = "bergma.westeurope.cloudapp.azure.com";
  const char *identity = "subscriber";
  const char *username = "NBIoTLS.azure-devices.net/subscriber/?api-version=2021-04-12";
  const char *topic = "devices/subscriber/messages/events/";
  const char *SharedAccessKeyName = "iothubowner";
  const char *SharedAccessKey = "iQJQVxmSHgOYmmq1lSHP9Uwpj0Hh+JHJcl0IpllDQLw=";
  const int Port = 1883;
} connection_info;



// We need to pass a Serial or SoftwareSerial object to the LTE Shield
// library. Below creates a SoftwareSerial object on the standard LTE
// Shield RX/TX pins:
// Note: if you're using an Arduino board with a dedicated hardware
// serial port, comment out the line below. (Also see note in setup)


// Create a LTE_Shield object to be used throughout the sketch:


// To support multiple architectures, serial ports are abstracted here.
// By default, they'll support AVR's like the Arduino Uno and Redboard
// For example, on a SAMD21 board SerialMonitor can be changed to SerialUSB
// and LTEShieldSerial can be set to Serial1 (hardware serial port on 0/1)

#define POWERPIN 33

// Network operator can be set to either:
// MNO_SW_DEFAULT -- DEFAULT
// MNO_ATT -- AT&T
// MNO_VERIZON -- Verizon
// MNO_TELSTRA -- Telstra
// MNO_TMO -- T-Mobile
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_SW_DEFAULT;
const String MOBILE_NETWORK_STRINGS[] = {"Default", "SIM_ICCD", "AT&T", "VERIZON",
                                         "TELSTRA", "T-Mobile", "CT", "TeliaDK Telia"};

// APN -- Access Point Name. Gateway between GPRS MNO
// and another computer network. E.g. "hologram
const char APN[] = "lpwa.telia.iot";

// This defines the size of the ops struct array. Be careful making
// this much bigger than ~5 on an Arduino Uno. To narrow the operator
// list, set MOBILE_NETWORK_OPERATOR to AT&T, Verizeon etc. instead
// of MNO_SW_DEFAULT.
#define MAX_OPERATORS 5

#define DEBUG_PASSTHROUGH_ENABLED

void printInfo(void);
void printOperators(struct operator_stats *ops, int operatorsAvailable);
void serialWait();
void sendHologramMessage(String message);
/*

*/

void setup()
{
  int opsAvailable;
  struct operator_stats ops[MAX_OPERATORS];
  String currentOperator = "";
  bool newConnection = true;

  SerialMonitor.begin(9600);
  while (!SerialMonitor)
    ; // For boards with built-in USB

  SerialMonitor.println(F("Initializing the LTE Shield..."));
  SerialMonitor.println(F("...this may take ~25 seconds if the shield is off."));
  SerialMonitor.println(F("...it may take ~5 seconds if it just turned on."));

  SerialMonitor.printf("Cycling powerpin\n");
  
  pinMode(POWERPIN, OUTPUT);
  digitalWrite(POWERPIN, LOW);
  delay(100);
  pinMode(POWERPIN, INPUT); // Return to high-impedance, rely on SARA module internal pull-up

  LTEShieldSerial.begin(9600);

  // Initialize the LTE Shield and enable AT interface and Timezone update
  initModule();

  // Set the operator APN
  setAPN(APN);

  // Get status of network aquisition
  getNetwork();

  // At the very end print connection information
  char topic[] = "test/";
  char msg[] = "NB-IoT_checkin";
  //printInfo();

  // Import CA certificate
  setCertMQTT(CA, 0, "CA");

  // Import client certificate
  setCertMQTT(CERT, 1, "CERT");

  // import client private key
  setCertMQTT(KEY, 2, "KEY");

  // Set broker hostname and port
  setMQTT(connection_info.HostName, connection_info.Port);

  // Set Last Will topic
  willconfigMQTT(connection_info.topic);

  // Set Last Will message
  willmsgMQTT("Unintented disconnect");

  // Enable MQTT keepalive
  setMQTTping(60);
  enableMQTTkeepalive();

  // Login to MQTT broker
  loginMQTT();

  // Publish message to MQTT broker
  publishMessage(topic, msg, 0, 0);
  
}

void loop()
{
  // Send a message to Hologram every 10 seconds
  // sendHologramMessage("Hello from the LTE Shield!");

  // delay(10000);
  //  Loop won't do much besides provide a debugging interface.
  //  Pass serial data from Arduino to shield and vice-versa
#ifdef DEBUG_PASSTHROUGH_ENABLED
  if (LTEShieldSerial.available())
  {
    SerialMonitor.write((char)LTEShieldSerial.read());
  }
  if (SerialMonitor.available())
  {
    LTEShieldSerial.write((char)SerialMonitor.read());
  }
#endif
}
