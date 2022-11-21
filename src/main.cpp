#include <Arduino.h>
#include <HardwareSerial.h>
#include <SparkFun_LTE_Shield_Arduino_Library.h>

HardwareSerial lteSerial(2);
LTE_Shield lte;

#define SerialMonitor Serial
#define LTEShieldSerial lteSerial

#include "AT_commands.h"
#include "mqtt.h"



struct connection_info_t
{
  const char *HostName = "bergma.westeurope.cloudapp.azure.com";
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
const String APN = "lpwa.telia.iot";

const char HOLOGRAM_URL[] = "cloudsocket.hologram.io";
const unsigned int HOLOGRAM_PORT = 9999;

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
  
  // Call lte.begin and pass it your Serial/SoftwareSerial object to
  // communicate with the LTE Shield.
  // Note: If you're using an Arduino with a dedicated hardware serial
  // port, you may instead slide "Serial" into this begin call.
  if (lte.begin(LTEShieldSerial, 9600))
  {
    SerialMonitor.println(F("LTE Shield connected!\r\n"));
  }
  else
  {
    SerialMonitor.println("Unable to initialize the shield.");
    while (1)
      ;
  }
  //LTEShieldSerial.print("AT+COPS=0,0");
  //LTEShieldSerial.print("\r");
  if (lte.getOperator(&currentOperator) != LTE_SHIELD_SUCCESS)
  {
    SerialMonitor.println("Waiting 30 seconds for module to register with network...");
    delay(30000);
  }
  
  // First check to see if we're already connected to an operator:
  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS)
  {
    SerialMonitor.print(F("Already connected to: "));
    SerialMonitor.println(currentOperator);
    // If already connected provide the option to type y to connect to new operator
    SerialMonitor.println(F("Press y to connect to a new operator, or any other key to continue.\r\n"));
    while (!SerialMonitor.available())
      ;
    if (SerialMonitor.read() != 'y')
    {
      newConnection = false;
    }
    while (SerialMonitor.available())
      SerialMonitor.read();
  }

  if (newConnection)
  {
    // Set MNO to either Verizon, T-Mobile, AT&T, Telstra, etc.
    // This will narrow the operator options during our scan later
    SerialMonitor.println(F("Setting mobile-network operator"));
    if (lte.setNetwork(MOBILE_NETWORK_OPERATOR))
    {
      SerialMonitor.print(F("Set mobile network operator to "));
      SerialMonitor.println(MOBILE_NETWORK_STRINGS[MOBILE_NETWORK_OPERATOR] + "\r\n");
    }
    else
    {
      SerialMonitor.println(F("Error setting MNO. Try cycling power to the shield/Arduino."));
      while (1)
        ;
    }

    // Set the APN -- Access Point Name -- e.g. "hologram"
    SerialMonitor.println(F("Setting APN..."));
    if (lte.setAPN(APN) == LTE_SHIELD_SUCCESS)
    {
      SerialMonitor.println(F("APN successfully set.\r\n"));
    }
    else
    {
      SerialMonitor.println(F("Error setting APN. Try cycling power to the shield/Arduino."));
      while (1)
        ;
    }

    // Wait for user to press button before initiating network scan.
    SerialMonitor.println(F("Press any key scan for networks.."));
    serialWait();

    SerialMonitor.println(F("Scanning for operators...this may take up to 3 minutes\r\n"));
    // lte.getOperators takes in a operator_stats struct pointer and max number of
    // structs to scan for, then fills up those objects with operator names and numbers
    opsAvailable = lte.getOperators(ops, MAX_OPERATORS); // This will block for up to 3 minutes

    if (opsAvailable > 0)
    {
      // Pretty-print operators we found:
      SerialMonitor.println("Found " + String(opsAvailable) + " operators:");
      printOperators(ops, opsAvailable);

      // Wait until the user presses a key to initiate an operator connection
      SerialMonitor.println("Press 1-" + String(opsAvailable) + " to select an operator.");
      char c = 0;
      bool selected = false;
      while (!selected)
      {
        while (!SerialMonitor.available())
          ;
        c = SerialMonitor.read();
        int selection = c - '0';
        if ((selection >= 1) && (selection <= opsAvailable))
        {
          selected = true;
          SerialMonitor.println("Connecting to option " + String(selection));
          if (lte.registerOperator(ops[selection - 1]) == LTE_SHIELD_SUCCESS)
          {
            SerialMonitor.println("Network " + ops[selection - 1].longOp + " registered\r\n");
          }
          else
          {
            SerialMonitor.println(F("Error connecting to operator. Reset and try again, or try another network."));
          }
        }
      }
    }
    else
    {
      SerialMonitor.println(F("Did not find an operator. Double-check SIM and antenna, reset and try again, or try another network."));
      while (1)
        ;
    }
  }

  // At the very end print connection information
  char topic[] = "test/";
  char msg[] = "NB-IoT_checkin";
  printInfo();

  // Set broker hostname and port
  setMQTT(connection_info.HostName, connection_info.Port);
  // Set Last Will topic
  willconfigMQTT();
  // Set Last Will message
  willmsgMQTT();
  // Enable MQTT keepalive
  setMQTTping(600);
  enableMQTTkeepalive();
  // Login to MQTT broker
  loginMQTT();
  // Publish message to MQTT broker
  publishMessage(topic, msg, (int)0, (int)0);
  
  
  //sendHologramMessage("Hello World");
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

void sendHologramMessage(String message)
{
  int socket = -1;
  String hologramMessage;

  // New lines are not handled well
  message.replace('\r', ' ');
  message.replace('\n', ' ');

  // Construct a JSON-encoded Hologram message string:
  // hologramMessage = "{\"k\":\"" + HOLOGRAM_DEVICE_KEY + "\",\"d\":\"" +
  //  message + "\"}";

  // Open a socket
  socket = lte.socketOpen(LTE_SHIELD_TCP);
  // On success, socketOpen will return a value between 0-5. On fail -1.
  if (socket >= 0)
  {
    // Use the socket to connec to the Hologram server
    Serial.println("Connecting to socket: " + String(socket));
    if (lte.socketConnect(socket, connection_info.HostName, connection_info.Port) == LTE_SHIELD_SUCCESS)
    {
      Serial.println("Connected to socket: " + String(socket));
     //setMQTT();
      // Send our message to the server:
      // Serial.println("Sending: " + String(hologramMessage));
      /*
      if (lte.socketWrite(socket, hologramMessage) == LTE_SHIELD_SUCCESS)
      {
        // On succesful write, close the socket.
        if (lte.socketClose(socket) == LTE_SHIELD_SUCCESS) {
          Serial.println("Socket " + String(socket) + " closed");
        }
      } else {
        Serial.println(F("Failed to write"));
      }
      
      if (lte.socketClose(socket) == LTE_SHIELD_SUCCESS)
      {
        Serial.println("Socket " + String(socket) + " closed");
      }
      */
    }
  }
}

void printInfo(void)
{
  String currentApn = "";
  IPAddress ip(0, 0, 0, 0);
  String currentOperator = "";

  SerialMonitor.println(F("Connection info:"));
  // APN Connection info: APN name and IP
  if (lte.getAPN(&currentApn, &ip) == LTE_SHIELD_SUCCESS)
  {
    SerialMonitor.println("APN: " + String(currentApn));
    SerialMonitor.print("IP: ");
    SerialMonitor.println(ip);
  }

  // Operator name or number
  if (lte.getOperator(&currentOperator) == LTE_SHIELD_SUCCESS)
  {
    SerialMonitor.print(F("Operator: "));
    SerialMonitor.println(currentOperator);
  }

  // Received signal strength
  SerialMonitor.println("RSSI: " + String(lte.rssi()));
  SerialMonitor.println();
}

void printOperators(struct operator_stats *ops, int operatorsAvailable)
{
  for (int i = 0; i < operatorsAvailable; i++)
  {
    SerialMonitor.print(String(i + 1) + ": ");
    SerialMonitor.print(ops[i].longOp + " (" + String(ops[i].numOp) + ") - ");
    switch (ops[i].stat)
    {
    case 0:
      SerialMonitor.println(F("UNKNOWN"));
      break;
    case 1:
      SerialMonitor.println(F("AVAILABLE"));
      break;
    case 2:
      SerialMonitor.println(F("CURRENT"));
      break;
    case 3:
      SerialMonitor.println(F("FORBIDDEN"));
      break;
    }
  }
  SerialMonitor.println();
}

void serialWait()
{
  while (!SerialMonitor.available())
    ;
  while (SerialMonitor.available())
    SerialMonitor.read();
}