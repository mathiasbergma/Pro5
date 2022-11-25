/**
 * @brief Library for uBlox SARA R410M LTE Cat M1/NB1 module
 * 
 * @file NB_R410M.h
 * @author Bergma
 * @date 2022-11
 * @details Use this library to initialize the module, setup the APN, connect to the network, and communicate via MQTT.
 * @details Steps to use this library:
 * @details 1. Call initModule() to initialize the module and enable AT interface and Timezone update
 * @details 2. Call setAPN() to set the operator APN
 * @details 3. Call getNetwork() to get status of network aquisition
 * @details 4. Call printInfo() to print connection information (TODO)
 * @details 5. Call setCertMQTT() to import certificates (If using SSL/TLS). This function is called 3 times, once for each certificate (CA, CERT, KEY)
 * @details 6. Call setSSL() to enable SSL/TLS
 * @details 7. Call setMQTT() to set broker hostname and port
 * @details 8. Call willconfigMQTT() to set Last Will topic
 * @details 9. Call willmsgMQTT() to set Last Will message
 * @details 10. Call setMQTTtimeout() to set MQTT timeout
 * @details 11. Call setMQTTping() to set MQTT keepalive interval
 * @details 12. Call enableMQTTkeepalive() to enable MQTT keepalive
 * @details 13. Call loginMQTT() to login to MQTT broker
 * @details 14. Call publishMessage() to publish message to MQTT broker
 */


#include <Arduino.h>
#include "AT_commands.h"
#include <HardwareSerial.h>

HardwareSerial lteSerial(2);

#define SerialMonitor Serial
#define LTEShieldSerial lteSerial
/*
int setMQTT(const char *host, int port);
int publishMessage(const char *topic, const char *message, int QoS, int retain);
int loginMQTT();
int willconfigMQTT(const char * topic);
int willmsgMQTT(const char * message);
int getResponse(const char *response, int timeout);
int enableMQTTkeepalive();
int setMQTTping(int timeout);
int setCertMQTT(const char *cert, int type, const char *name);
void transmitCommand(const char *command);
void initModule();
void getNetwork();
int setAPN(const char *apn);
*/

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
  LTEShieldSerial.begin(9600);

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
 * @brief Enables SSL/TLS for mqtt connection
 * @param profile SSL profile number
 * @return 1 if successful, 0 if not
 */
int enableSSL(int profile)
{
  char *command = NULL;
  command = (char *)malloc(strlen(AT) + strlen(SARA_MQTT_SECURE) + 5);
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }

  sprintf(command, "%s%s%d", AT, SARA_MQTT_SECURE, profile);
  transmitCommand(command);
  if (getResponse(SARA_MQTT_SECURE_SET_RESPONSE, 5000))
  {
    SerialMonitor.println("SSL enabled");
    return 1;
  }
  else
  {
    SerialMonitor.println("SSL not enabled");
    return 0;
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
 * @return 1 if successful, 0 if not
 */
int setAPN(const char *apn)
{
  int result = 0;
  char *command = NULL;
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_SET_APN) + strlen(apn) + 5);
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }

  sprintf(command, "%s%s\"%s\"", AT, LTE_SHIELD_SET_APN, apn);
  transmitCommand(command);
  if (getResponse("OK", 1000))
  {
    Serial.println("APN set");
    result = 1;
  }
  else
  {
    Serial.println("APN not set");
  }
  free(command);
  return result;
}

/**
 * @brief Transmit certificate data to the LTE shield
 * @param cert Certificate data. Can be CA, client certificate or client key
 * @param type Certificate type. 0 = CA, 1 = client certificate, 2 = client key
 * @param name Certificate name. Can be any name, but must be unique
 * @return 1 if successful, 0 if not
 */
int setCertMQTT(const char *cert, int type, const char *name)
{
  int result = 0;
  char *command = NULL;
  char *response = NULL;
  response = (char *)malloc(strlen(LTE_SHIELD_IMPORT_CERT_OK) + 5);
  if (response == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }
  command = (char *)malloc((strlen(AT) + strlen(LTE_SHIELD_IMPORT_CERT) + strlen(cert) + strlen(name) + 15));
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }
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
      result = 1;
    }
    else
    {
      SerialMonitor.println("Certificate import failed");
    }
  }
  else
  {
    SerialMonitor.println("Failed to import certificate - not ready");
  }
  free(command);
  free(response);
  return result;
}

/**
 * @brief Sets the MQTT ping interval
 * @param timeout Timeout in seconds
 * @return 1 if successful, 0 if not
 */
int setMQTTping(int timeout)
{
  int result = 0;
  char *command = NULL;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_TIMEOUT) + 5);
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }
  sprintf(command, "%s%s%d", AT, LTE_SHIELD_TIMEOUT, timeout);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(LTE_SHIELD_TIMEOUT_SET_RESPONSE, 10000))
  {
    SerialMonitor.println("MQTT ping timeout set to " + String(timeout));
    result = 1;
  }
  else
  {
    SerialMonitor.println("Error setting MQTT ping timeout");
  }
  free(command);
  return result;
}

/**
 * @brief Enables MQTT keepalive
 * @return 1 if successful, 0 if not
 */
int enableMQTTkeepalive()
{
  int result = 0;
  char *command = NULL;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_PING) + 5);
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }
  sprintf(command, "%s%s", AT, LTE_SHIELD_PING);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(LTE_SHIELD_PING_OK, 10000))
  {
    SerialMonitor.println("MQTT keepalive enabled");
    result = 1;
  }
  else
  {
    SerialMonitor.println("Error enabling MQTT keepalive");
  }
  free(command);
  return result;
}

/**
 * @brief Publishes a message to the MQTT broker
 * @param topic The topic to publish to
 * @param message The message to publish
 * @param qos The QoS level to publish at
 * @param retain Whether to retain the message
 * @return 1 if successful, 0 if not, -1 if memory allocation failed
 */
int publishMessage(const char *topic, const char *message, int QoS, int retain)
{
  int result = 0;
  char *command = NULL;

  unsigned long LTE_SHIELD_SEND_TIMEOUT = 60000;

  // SerialMonitor.printf("Allocating memory for command\n");
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_SEND_MQTT) + strlen(message) + strlen(topic) + 8);
  if (command == NULL)
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
    result = 1;
  }
  else
  {
    SerialMonitor.println("Error sending message");
  }

  free(command);
  return result;
}

/**
 * @brief Login to the MQTT broker
 * @return 1 if successful, 0 if not
 */
int loginMQTT()
{
  int result = 0;
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 60000;

  SerialMonitor.printf("Sending login command: %s\n", LTE_SHIELD_LOGIN);
  // Empties buffer and transmits the command
  transmitCommand(LTE_SHIELD_LOGIN);

  if (getResponse(LTE_SHIELD_LOGIN_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    SerialMonitor.println("MQTT login successful");
    result = 1;
  }
  else
  {
    SerialMonitor.println("Error logging in to MQTT");
  }
  return result;
}

/**
 * @brief Configures MQTT Will topic
 * @param topic The topic to publish to
 * @return 1 if successful, 0 if not
 */
int willconfigMQTT(const char * topic)
{
  int result = 0;
  char *command = NULL;
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_TOPIC_MQTT) + strlen(topic) + 10);
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }
  sprintf(command, "%s%s%d,%d,\"%s\"", AT, LTE_SHIELD_WILL_TOPIC_MQTT, 0, 0, topic);

  SerialMonitor.printf("Sending will topic command: %s\n", command);
  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(LTE_SHIELD_TOPIC_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    Serial.println("Will topic set");
    result = 1;
  }
  else
  {
    Serial.println("Will topic not set");
  }

  free(command);

  return result;
}

/**
 * @brief Configures MQTT Will message
 * @param message The message to publish
 * @return 1 if successful, 0 if not
 */
int willmsgMQTT(const char *message)
{
  int result = 0;
  char *command = NULL;

  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_MESSAGE_MQTT) + strlen(message) + 10);
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }
  sprintf(command, "%s%s\"%s\"", AT, LTE_SHIELD_WILL_MESSAGE_MQTT, message);

  SerialMonitor.printf("Sending will message command: %s\n", command);
  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(LTE_SHIELD_MESSAGE_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    Serial.println("Will message set");
    result = 1;
  }
  else
  {
    Serial.println("Will message not set");
  }
  free(command);
  return result;
}

/**
 * @brief Configures MQTT broker hostname and port
 * @param hostname The hostname of the MQTT broker
 * @param port The port of the MQTT broker
 * @return 1 if successful, 0 if not
 */
int setMQTT(const char *host, int port)
{
  int result = 0;
  char *command = NULL;

  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(sizeof(char) * (strlen(LTE_SHIELD_CONNECT_MQTT) + strlen(host) + 15));
  if (command == NULL)
  {
    SerialMonitor.println("Malloc failed");
    return 0;
  }

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
    result = 1;
  }
  else
  {
    Serial.println("MQTT hostname & port not set");
  }

  free(command);
  return result;
}