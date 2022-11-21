#include <Arduino.h>
#include <HardwareSerial.h>

#include <SparkFun_LTE_Shield_Arduino_Library.h>

struct connection_info_t
{
  const char *HostName = "bergma.westeurope.cloudapp.azure.com";
  const char *SharedAccessKeyName = "iothubowner";
  const char *SharedAccessKey = "iQJQVxmSHgOYmmq1lSHP9Uwpj0Hh+JHJcl0IpllDQLw=";
  const int Port = 1883;
} connection_info;

const char AT[] = "AT";

// We need to pass a Serial or SoftwareSerial object to the LTE Shield
// library. Below creates a SoftwareSerial object on the standard LTE
// Shield RX/TX pins:
// Note: if you're using an Arduino board with a dedicated hardware
// serial port, comment out the line below. (Also see note in setup)
HardwareSerial lteSerial(2);

// Create a LTE_Shield object to be used throughout the sketch:
LTE_Shield lte;

// To support multiple architectures, serial ports are abstracted here.
// By default, they'll support AVR's like the Arduino Uno and Redboard
// For example, on a SAMD21 board SerialMonitor can be changed to SerialUSB
// and LTEShieldSerial can be set to Serial1 (hardware serial port on 0/1)
#define SerialMonitor Serial
#define LTEShieldSerial lteSerial
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
void setMQTT();
int publishMessage(const char * topic, const char * message, int QoS, int retain);
void loginMQTT();
void willconfigMQTT();
void willmsgMQTT();

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
  setMQTT();
  willconfigMQTT();
  willmsgMQTT();
  loginMQTT();
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
int publishMessage(const char * topic, const char * message, int QoS, int retain)
{
  char * command = NULL;
  const char AT[] = "AT";
  const char LTE_SHIELD_SEND_MQTT[] = "+UMQTTC=2";
  const char LTE_SHIELD_SEND_OK[] = "+UMQTTC:2,1\nOK";
  unsigned long LTE_SHIELD_SEND_TIMEOUT = 60000;
  char c;
  char ret[128];
  int destIndex = 0;
  int index = 0;


  SerialMonitor.printf("Allocating memory for command\n");
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_SEND_MQTT) + strlen(message) + strlen(topic) + 8);
  if (command != NULL)
  {
    SerialMonitor.printf("Memory allocated\n");
  }
  else
  {
    SerialMonitor.printf("Memory allocation failed\n");
    return -1;
  }
  SerialMonitor.printf("Building command\n");
  sprintf(command, "%s%s,%d,%d,%s,%s", AT, LTE_SHIELD_SEND_MQTT, QoS, retain, topic, message);
  
  SerialMonitor.printf("Command built....Sending\n");
  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  } 
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");

  SerialMonitor.printf("Command sent....Waiting for response\n");
  unsigned long timeIn = millis();
  while (millis() - timeIn < LTE_SHIELD_SEND_TIMEOUT)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      Serial.print(c);
      ret[destIndex++] = c;
      
      if (c == LTE_SHIELD_SEND_OK[index])
      {
        index++;
        if (index == strlen(LTE_SHIELD_SEND_OK))
        {
          SerialMonitor.println("Message sent");
          break;
        }
      }
      else
      {
        index = 0;
      }
      
    }
  }
  ret[destIndex] = '\0';
  Serial.println(ret);
  free(command);
  return 0;
}
void loginMQTT()
{
  const char LTE_SHIELD_LOGIN[] = "AT+UMQTTC=1";
  const char LTE_SHIELD_LOGIN_OK[] = "+UUMQTTC: 1,0";
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 60000;
  char c;
  char ret[128];
  int destIndex = 0;
  int index = 0;

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    char c = LTEShieldSerial.read();
    SerialMonitor.print(c);
  }
  SerialMonitor.printf("Sending login command: %s\n", LTE_SHIELD_LOGIN);
  // Send login command
  LTEShieldSerial.print(LTE_SHIELD_LOGIN);
  LTEShieldSerial.print("\r");

  Serial.println("Reading response");
  unsigned long timeIn = millis();
  while (millis() - timeIn < LTE_SHIELD_IP_CONNECT_TIMEOUT)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      //Serial.print(c);
      ret[destIndex++] = c;
      
      if (c == LTE_SHIELD_LOGIN_OK[index])
      {
        index++;
        if (index == strlen(LTE_SHIELD_LOGIN_OK))
        {
          Serial.println("MQTT login successful");
          break;
        }
      }
      else
      {
        index = 0;
      }
      
    }
  }
  ret[destIndex] = '\0';
  Serial.println(ret);
}
void willconfigMQTT()
{
  char *command;
  const char *LTE_SHIELD_WILL_TOPIC_MQTT = "AT+UMQTTWTOPIC=0,0,\"test/will\"";
  const char * LTE_SHIELD_TOPIC_OK = "+UMQTTWTOPIC: 1";
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;
  char c;
  char ret[128];
  int destIndex = 0;
  int index = 0;
  char willtopic[] = "test/will";

  //command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_TOPIC_MQTT) + strlen(willtopic) +5);
  //sprintf(command, "%s%s%d,%d,\"%s\"", AT, LTE_SHIELD_WILL_TOPIC_MQTT, 0, 0, willtopic);

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    char c = LTEShieldSerial.read();
    //SerialMonitor.print(c);
  }
  SerialMonitor.printf("Sending will topic command: %s\n", LTE_SHIELD_WILL_TOPIC_MQTT);
  LTEShieldSerial.print(LTE_SHIELD_WILL_TOPIC_MQTT);
  LTEShieldSerial.print("\r");

  Serial.println("Reading response");
  unsigned long timeIn = millis();
  while (millis() - timeIn < LTE_SHIELD_IP_CONNECT_TIMEOUT)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      //Serial.print(c);
      ret[destIndex++] = c;
      
      if (c == LTE_SHIELD_TOPIC_OK[index])
      {
        index++;
        if (index == strlen(LTE_SHIELD_TOPIC_OK))
        {
          Serial.println("MQTT will topic successful");
          break;
        }
      }
      else
      {
        index = 0;
      }
      
    }
  }
  ret[destIndex] = '\0';
  Serial.println(ret);
  
  delay(1000);
  //free(command);
  
  return;
}
void willmsgMQTT()
{
  char* command1;
  const char *LTE_SHIELD_WILL_MESSAGE_MQTT = "AT+UMQTTWMSG=\"NB_disconnected\"";
  const char * LTE_SHIELD_MESSAGE_OK = "+UMQTTWMSG: 1";
  char ret[128];
  char c;
  int destIndex = 0;
  int index = 0;
  char willmessage[] = "NB_disconnected";
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  //command1 = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_MESSAGE_MQTT) + strlen(willmessage) +5);
  //sprintf(command1, "%s%s\"%s\"", AT, LTE_SHIELD_WILL_MESSAGE_MQTT, willmessage);

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    char c = LTEShieldSerial.read();
    //SerialMonitor.print(c);
  }
  SerialMonitor.printf("Sending will message command: %s\n", LTE_SHIELD_WILL_MESSAGE_MQTT);
  LTEShieldSerial.print(LTE_SHIELD_WILL_MESSAGE_MQTT);
  LTEShieldSerial.print("\r");

  Serial.println("Reading response");
  unsigned long timeIn = millis();
  while (millis() - timeIn < LTE_SHIELD_IP_CONNECT_TIMEOUT)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      //Serial.print(c);
      ret[destIndex++] = c;
      
      if (c == LTE_SHIELD_MESSAGE_OK[index])
      {
        index++;
        if (index == strlen(LTE_SHIELD_MESSAGE_OK))
        {
          Serial.println("MQTT will message successful");
          break;
        }
      }
      else
      {
        index = 0;
      }
      
    }
  }
  ret[destIndex] = '\0';
  Serial.println(ret);
  delay(1000);
  
  //SerialMonitor.printf("Freeing command\n");
  //free(command1);
  //SerialMonitor.printf("Command freed\n");
}
void setMQTT()
{
  char *command;
  const char *LTE_SHIELD_CONNECT_MQTT = "+UMQTT";
  const char LTE_SHIELD_RESPONSE_OK[] = "+UMQTT: 2,1";
  const char LTE_SHIELD_LOGIN_OK[] = "+UMQTTC: 1,1";
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;
  char c;
  char ret[128];
  int destIndex = 0;
  int index = 0;

  command = (char *)malloc(sizeof(char) * (strlen(LTE_SHIELD_CONNECT_MQTT) + strlen(connection_info.HostName) + 15));
  size_t err;

  // Emtpy the buffer before sending the command
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  }
  // Create the command
  sprintf(command, "AT%s=%d,\"%s\",%d", LTE_SHIELD_CONNECT_MQTT, 2, connection_info.HostName, connection_info.Port);
  // Send the command
  SerialMonitor.println("Setting MQTT hostname & port");
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");
  unsigned long timeIn = millis();
  Serial.println("Reading response");
  while (millis() - timeIn < LTE_SHIELD_IP_CONNECT_TIMEOUT)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      //Serial.print(c);
      ret[destIndex++] = c;
      if (c == LTE_SHIELD_RESPONSE_OK[index])
      {
        index++;
        if (index == strlen(LTE_SHIELD_RESPONSE_OK))
        {
          Serial.println("MQTT hostname set");
          break;
        }
      }
      else
      {
        index = 0;
      }
    }
  }
  ret[destIndex] = '\0';
  Serial.println(ret);
  free(command);
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