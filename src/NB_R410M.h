#include <Arduino.h>
#include "AT_commands.h"
#include <HardwareSerial.h>

extern HardwareSerial lteSerial;

#define SerialMonitor Serial
#define LTEShieldSerial lteSerial

void setMQTT(const char *host, int port);
int publishMessage(const char *topic, const char *message, int QoS, int retain);
void loginMQTT();
void willconfigMQTT();
void willmsgMQTT();
int getResponse(const char *response, int timeout);
void enableMQTTkeepalive();
void setMQTTping(int timeout);
void setCertMQTT(const char *cert, int type, const char *name);
void transmitCommand(const char *command);

/**
 * @brief Empties the serial buffer and transmits the command
 * @param command The string command to be transmitted
 */
void transmitCommand(const char *command)
{
  //SerialMonitor.printf("Command: %s\n", command);
  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  }
  // Transmit the command
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");
}

/**
 * @brief gets response from the LTE shield and compares it to the expected response
 * @param response Expected response
 * @param timeout Timeout in milliseconds
 */
int getResponse(const char *response, int timeout)
{
  unsigned long timeIn = millis();
  char ret[128];
  int destIndex = 0;
  int index = 0;
  char c;
  int size = strlen(response);

  while (millis() - timeIn < timeout)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      //Serial.print(c);
      ret[destIndex++] = c;

      if (c == response[index])
      {
        index++;
        if (index == size)
        {
          return 1;
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
  return 0;
}

void initModule()
{
  SerialMonitor.printf("Transmitting AT\n");
  while (!getResponse("OK", 500))
  {
    SerialMonitor.printf(".");
    transmitCommand("AT"); // Send AT command to enable the interface
  }
  SerialMonitor.printf("Got OK\n");

  SerialMonitor.printf("Disabling echo\n");
  transmitCommand("ATE0"); // Disable echo
  if (getResponse("OK", 200))
  {
    SerialMonitor.println("Echo disabled");
  }
  else
  {
    SerialMonitor.println("Echo not disabled");
  }

  transmitCommand("AT+CTZU=1"); // Enable automatic time zone update
  if (getResponse("OK", 5000))
  {
    SerialMonitor.println("Automatic time zone update enabled");
  }
  else
  {
    SerialMonitor.println("Automatic time zone update not enabled");
  }
}

/**
 * @brief Displays the network aquisition status
*/
void getNetwork()
{
  int retval;
  int last = 99;
  while (retval != 1)
  {

    // Empty the serial buffer
    while (LTEShieldSerial.available())
    {
      LTEShieldSerial.read();
    }
    transmitCommand("AT+CEREG?");
    unsigned long timeIn = millis();
    char ret[128];
    int destIndex = 0;
    int index = 0;
    char c;
    int size = 12;
    retval = 4;

    while (millis() - timeIn < 1000)
    {
      if (LTEShieldSerial.available())
      {
        c = (char)LTEShieldSerial.read();
        //Serial.print(c);
        ret[index++] = c;
        //SerialMonitor.printf("index: %d\n", index);
        if (index > size)
        {
          break;
        }
      }
    }
    ret[++index] = 0;
    //SerialMonitor.printf("ret: %s\n", ret);
    sscanf(ret, " +CEREG: 0,%d", &retval);
    switch (retval)
    {
    case 0:
      if (last != retval)
      {
        SerialMonitor.printf("Not registered, MT is not currently searching a new operator to register to\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    case 1:
      if (last != retval)
      {
        SerialMonitor.printf("Registered, home network\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    case 2:
      if (last != retval)
      {
        SerialMonitor.printf("Scanning for network\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    case 3:
      if (last != retval)
      {
        SerialMonitor.printf("Registration denied\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    case 4:
      if (last != retval)
      {
        SerialMonitor.printf("Unknown\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    case 5:
      if (last != retval)
      {
        SerialMonitor.printf("Registered, roaming\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    default:
      if (last != retval)
      {
        SerialMonitor.printf("Unknown\n");
        last = retval;
      }
      else
      {
        SerialMonitor.printf(".");
      }
      break;
    }
  }
}

/**
 * @brief Sets APN on LTE module
 * @param apn The APN to be set
 */
void setAPN(const char *apn)
{
  char *command;
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_SET_APN) + strlen(apn) + 5);
  sprintf(command, "%s%s\"%s\"", AT, LTE_SHIELD_SET_APN, apn);
  transmitCommand(command);
  if (getResponse("OK", 1000))
  {
    Serial.println("APN set");
  }
  else
  {
    Serial.println("APN not set");
  }
  free(command);
}

/**
 * @brief Transmit certificate data to the LTE shield
 * @param cert Certificate data. Can be CA, client certificate or client key
 * @param type Certificate type. 0 = CA, 1 = client certificate, 2 = client key
 * @param name Certificate name. Can be any name, but must be unique
 */
void setCertMQTT(const char *cert, int type, const char *name)
{
  char *command;
  char *response;
  response = (char *)malloc(strlen(LTE_SHIELD_IMPORT_CERT_OK) + 5);
  command = (char *)malloc((strlen(AT) + strlen(LTE_SHIELD_IMPORT_CERT) + strlen(cert) + strlen(name) + 15));
  sprintf(command, "%s%s%d,\"%s\",%d", AT, LTE_SHIELD_IMPORT_CERT, type, name, strlen(cert));
  SerialMonitor.printf("Importing certificate: %s\n", name);
  //SerialMonitor.printf("Command: %s\n", command);
  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(LTE_SHIELD_IMPORT_CERT_READY, 10000))
  {
    SerialMonitor.println("Ready to receive certificate:");
    // SerialMonitor.printf("%s\n",cert);

    sprintf(response, "%s%d", LTE_SHIELD_IMPORT_CERT_OK, type);

    // Empties buffer and transmits the command
    transmitCommand(cert);

    if (getResponse(response, 10000))
    {
      SerialMonitor.println("Certificate imported");
    }
    else
    {
      SerialMonitor.println("Certificate import failed");
    }

    free(response);
  }
  else
  {
    SerialMonitor.println("Failed to import certificate");
  }
  free(command);
}

/**
 * @brief Sets the MQTT ping interval
 * @param timeout Timeout in seconds
 */
void setMQTTping(int timeout)
{
  char *command;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_TIMEOUT) + 5);
  sprintf(command, "%s%s%d", AT, LTE_SHIELD_TIMEOUT, timeout);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(LTE_SHIELD_TIMEOUT_SET_RESPONSE, 10000))
  {
    SerialMonitor.println("MQTT ping timeout set to " + String(timeout));
  }
  else
  {
    SerialMonitor.println("Error setting MQTT ping timeout");
  }
  free(command);
}

/**
 * @brief Enables MQTT keepalive
 */
void enableMQTTkeepalive()
{
  char *command;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_PING) + 5);
  sprintf(command, "%s%s", AT, LTE_SHIELD_PING);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(LTE_SHIELD_PING_OK, 10000))
  {
    SerialMonitor.println("MQTT keepalive enabled");
  }
  else
  {
    SerialMonitor.println("Error enabling MQTT keepalive");
  }
}

/**
 * @brief Publishes a message to the MQTT broker
 * @param topic The topic to publish to
 * @param message The message to publish
 * @param qos The QoS level to publish at
 * @param retain Whether to retain the message
 */
int publishMessage(const char *topic, const char *message, int QoS, int retain)
{
  char *command = NULL;

  unsigned long LTE_SHIELD_SEND_TIMEOUT = 60000;

  // SerialMonitor.printf("Allocating memory for command\n");
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_SEND_MQTT) + strlen(message) + strlen(topic) + 8);
  if (command != NULL)
  {
    // SerialMonitor.printf("Memory allocated\n");
  }
  else
  {
    SerialMonitor.printf("Memory allocation failed\n");
    return -1;
  }
  // SerialMonitor.printf("Building command\n");
  sprintf(command, "%s%s,%d,%d,%s,%s", AT, LTE_SHIELD_SEND_MQTT, QoS, retain, topic, message);

  // SerialMonitor.printf("Command built....Sending\n");

  // Empties buffer and transmits the command
  transmitCommand(command);

  // SerialMonitor.printf("Command sent....Waiting for response\n");
  if (getResponse(LTE_SHIELD_SEND_OK, LTE_SHIELD_SEND_TIMEOUT))
  {
    SerialMonitor.println("Message sent");
  }
  else
  {
    SerialMonitor.println("Error sending message");
  }

  free(command);
  return 0;
}

/**
 * @brief Login to the MQTT broker
 */
void loginMQTT()
{
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 60000;

  SerialMonitor.printf("Sending login command: %s\n", LTE_SHIELD_LOGIN);
  // Empties buffer and transmits the command
  transmitCommand(LTE_SHIELD_LOGIN);

  if (getResponse(LTE_SHIELD_LOGIN_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    SerialMonitor.println("MQTT login successful");
  }
  else
  {
    SerialMonitor.println("Error logging in to MQTT");
  }
}

/**
 * @brief Configures MQTT Will topic
 */
void willconfigMQTT()
{
  char *command;
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;
  char willtopic[] = "test/will";

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_TOPIC_MQTT) + strlen(willtopic) + 10);
  sprintf(command, "%s%s%d,%d,\"%s\"", AT, LTE_SHIELD_WILL_TOPIC_MQTT, 0, 0, willtopic);

  SerialMonitor.printf("Sending will topic command: %s\n", command);
  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(LTE_SHIELD_TOPIC_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    Serial.println("Will topic set");
  }
  else
  {
    Serial.println("Will topic not set");
  }

  delay(1000);
  free(command);

  return;
}

/**
 * @brief Configures MQTT Will message
 */
void willmsgMQTT()
{
  char *command;

  char willmessage[] = "NB_disconnected";
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_MESSAGE_MQTT) + strlen(willmessage) + 10);
  sprintf(command, "%s%s\"%s\"", AT, LTE_SHIELD_WILL_MESSAGE_MQTT, willmessage);

  SerialMonitor.printf("Sending will message command: %s\n", command);
  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(LTE_SHIELD_MESSAGE_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    Serial.println("Will message set");
  }
  else
  {
    Serial.println("Will message not set");
  }
  free(command);
}

/**
 * @brief Configures MQTT broker hostname and port
 * @param hostname The hostname of the MQTT broker
 * @param port The port of the MQTT broker
 */
void setMQTT(const char *host, int port)
{
  char *command;

  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(sizeof(char) * (strlen(LTE_SHIELD_CONNECT_MQTT) + strlen(host) + 15));
  size_t err;

  // Create the command
  sprintf(command, "AT%s=%d,\"%s\",%d", LTE_SHIELD_CONNECT_MQTT, 2, host, port);
  // Send the command
  SerialMonitor.println("Setting MQTT hostname & port");

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(LTE_SHIELD_RESPONSE_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    Serial.println("MQTT hostname & port set");
  }
  else
  {
    Serial.println("MQTT hostname & port not set");
  }

  free(command);
}