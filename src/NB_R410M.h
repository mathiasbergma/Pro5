/**
 * @brief Library for uBlox SARA R410M LTE Cat M1/NB1 module
 *
 * @file NB_R410M.h
 * @author Bergma (Mike & Anders)
 * @date 2022-11
 * @details Use this library to initialize the module, setup the APN, connect to the network, and communicate via MQTT.
 * @details The function printToConsole() must be populated with the device specific implementation of Serial Print.
 * @details This library is based on the u-blox SARA-R4 AT Commands Manual (UBX-17003787 - R09)
 * @details Steps to use this library:
 * @details   1 Call initModule() to initialize the module and enable AT interface and Timezone update
 * @details   2 Call setAPN() to set the operator APN
 * @details   3 Call getNetwork() to get status of network aquisition
 * @details   4 Call printInfo() to print connection information (TODO)
 * @details   5 Call loadCertMQTT() to loads certificates from filesystem and upload to module (If using SSL/TLS). 
 *                    This function is called 3 times, once for each certificate (CA, CERT, KEY)
 * @details   6 Call assignCert() to assign the certificates to a security profile (If using SSL/TLS)
 * @details   7 Call enableSSL() to enable SSL/TLS
 * @details   8 Call setMQTTid() to set the MQTT ID
 * @details   9 Call setMQTT() to set broker hostname and port
 * @details   10 Call willconfigMQTT() to set Last Will topic
 * @details   11 Call willmsgMQTT() to set Last Will message
 * @details   12 Call setMQTTping() to set MQTT keepalive interval
 * @details   13 Call enableMQTTkeepalive() to enable MQTT keepalive
 * @details   14 Call loginMQTT() to login to MQTT broker
 * @details   15 Call publishMessage() to publish message to MQTT broker
 */

#include <Arduino.h>
#include "AT_commands.h"
#include <HardwareSerial.h>
#include "SPIFFS.h"

HardwareSerial lteSerial(2);

#define SerialMonitor Serial
#define LTEShieldSerial lteSerial

/**
 * @brief Use this function for your own port of UART print to console
 * @param text Text to print
 */
void printToConsole(const char *text)
{
  SerialMonitor.print(text);
}

/**
 * @brief Empties the serial buffer and transmits the command
 * @param command The string command to be transmitted
 */
void transmitCommand(const char *command)
{
  // printToConsole("Command: %s\n", command);
  //  Empty the serial buffer
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
 * @return 1 if response is as expected, 0 if not
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
  //Serial.println(ret);
  return 0;
}

/**
 * @brief Initializes the LTE Shield and enables AT interface and Timezone update
 * @param timeout Timeout in milliseconds (30000 thousand is recommended)
 * @return 0 if successful, 1 if not
 */
int initModule(int timeout)
{
  LTEShieldSerial.begin(115200);
  unsigned long startTime = millis();
  printToConsole("Transmitting AT\n"); 
  while (!getResponse("OK", 500))
  {
    printToConsole(".");
    transmitCommand("AT"); // Send AT command to enable the interface
    
    if (millis() - startTime > timeout)
    {
      printToConsole("No response from module\n");
      return 1;
    }
  }
  printToConsole("Got OK\n");

  printToConsole("Disabling echo\n");
  transmitCommand("ATE0"); // Disable echo
  if (getResponse("OK", 200))
  {
    printToConsole("Echo disabled\n");
  }
  else
  {
    printToConsole("Echo not disabled\n");
  }

  transmitCommand("AT+CTZU=1"); // Enable automatic time zone update
  if (getResponse("OK", 5000))
  {
    printToConsole("Automatic time zone update enabled\n");
  }
  else
  {
    printToConsole("Automatic time zone update not enabled\n");
  }

  transmitCommand("AT+UGPIOC=16,2"); // Set GPIO16 to indicate network status
  if (getResponse("OK", 5000))
  {
    printToConsole("GPIO16 set to indicate network status\n");
  }
  else
  {
    printToConsole("GPIO16 not set to indicate network status\n");
  }

  return 0;
}
/**
 * @brief Enables SSL/TLS for mqtt connection
 * @param profile SSL profile number
 * @return 0 if successful, 1 if not, 2 if malloc failed
 */
int enableSSL(int profile)
{
  int returnVal = 1;
  char *command = NULL;
  command = (char *)malloc(strlen(AT) + strlen(SARA_MQTT_SECURE) + 5);
  if (command == NULL)
  {
    printToConsole("Malloc failed");
    return 2;
  }

  sprintf(command, "%s%s%d", AT, SARA_MQTT_SECURE, profile);
  transmitCommand(command);
  if (getResponse(SARA_MQTT_SECURE_SET_RESPONSE, 5000))
  {
    printToConsole("SSL enabled\n");
    returnVal = 0;
  }
  else
  {
    printToConsole("SSL not enabled\n");
    returnVal = 1;
  }
  free(command);
  return returnVal;
}

/**
 * @brief Assigns loaded certificates to a profile
 * @param profile SSL profile number
 * @param certName Certificate name assigned when loading certificate
 * @param certType Certificate type, 3 for CA, 5 for CERT, 6 for KEY
 * @return 0 if successful, 1 if not, 2 if malloc failed
 */
int assignCert(int profile, const char *certName, int certType)
{
  int returnVal = 1;
  char *command = NULL;
  command = (char *)malloc(strlen(AT) + strlen(SARA_SECURITY_PROFILE) + strlen(certName) + 10);
  if (command == NULL)
  {
    printToConsole("Malloc failed");
    return 2;
  }
  sprintf(command, "%s%s%d,%d,\"%s\"", AT, SARA_SECURITY_PROFILE, profile, certType, certName);
  transmitCommand(command);
  if (getResponse("OK",1000))
  {
    printToConsole("Certificate [");
    printToConsole(certName);
    printToConsole("] assigned\n");
    returnVal = 0;
  }
  else
  {
    printToConsole("Certificate [");
    printToConsole(certName);
    printToConsole("] not assigned\n");
    returnVal = 1;
  }
  free(command);
  return returnVal;
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
        // Serial.print(c);
        ret[index++] = c;
        // printToConsole("index: %d\n", index);
        if (index > size)
        {
          break;
        }
      }
    }
    ret[++index] = 0;
    // printToConsole("ret: %s\n", ret);
    sscanf(ret, " +CEREG: 0,%d", &retval);
    switch (retval)
    {
    case 0:
      if (last != retval)
      {
        printToConsole("Not registered, MT is not currently searching a new operator to register to\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    case 1:
      if (last != retval)
      {
        printToConsole("Registered, home network\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    case 2:
      if (last != retval)
      {
        printToConsole("Scanning for network\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    case 3:
      if (last != retval)
      {
        printToConsole("Registration denied\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    case 4:
      if (last != retval)
      {
        printToConsole("Unknown\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    case 5:
      if (last != retval)
      {
        printToConsole("Registered, roaming\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    default:
      if (last != retval)
      {
        printToConsole("Unknown\n");
        last = retval;
      }
      else
      {
        printToConsole(".");
      }
      break;
    }
    delay(500);
  }
}

/**
 * @brief Sets APN on LTE module
 * @param apn The APN to be set
 * @return 0 if successful, 1 if not
 */
int setAPN(const char *apn)
{
  int result = 0;
  char *command = NULL;
  command = (char *)malloc(strlen(AT) + strlen(SARA_SET_APN) + strlen(apn) + 5);
  if (command == NULL)
  {
    printToConsole("Malloc failed");
    return 1;
  }

  sprintf(command, "%s%s\"%s\"", AT, SARA_SET_APN, apn);
  transmitCommand(command);
  if (getResponse("OK", 1000))
  {
    Serial.println("APN set");
    result = 0;
  }
  else
  {
    Serial.println("APN not set");
  }
  free(command);
  return result;
}

/**
 * @brief Prints the IP address of the LTE module. The returned pointer must be freed by the caller.
 * @return char pointer to the IP address, NULL if not found or error
 */
char *printInfo()
{
  char *ip = NULL;
  char *command = NULL;
  command = (char *)malloc(strlen(AT) + strlen(SARA_NETWORK_INFO_GET) + 5);
  ip = (char *)malloc(16);
  if (ip == NULL)
  {
    printToConsole("Malloc failed");
    return NULL;
  }
  if (command == NULL)
  {
    printToConsole("Malloc failed");
    return NULL;
  }
  sprintf(command, "%s%s", AT, SARA_NETWORK_INFO_GET);
  transmitCommand(command);

  char ret[128];
  int destIndex = 0;
  int index = 0;
  char c;
  char OK[] = "OK";
  int size = 12;
  unsigned long timeIn = millis();
  while (millis() - timeIn < 1000)
  {
    if (LTEShieldSerial.available())
    {
      c = (char)LTEShieldSerial.read();
      // Serial.print(c);
      ret[index] = c;
      // printToConsole("index: %d\n", index);
      if (ret[index - 1] == OK[0] && ret[index] == OK[1])
      {
        break;
      }
      index++;
    }
  }
  
  ret[++index] = 0;
  char * token = strtok(ret, ",\"");
  for (int i = 0; i < 3; i++)
  {
    token = strtok(NULL, "\",\"");
  }
  
  strcpy(ip, token);
  free(command);
  return ip;
}

/**
 * @brief Transmit certificate data to the LTE shield
 * @param cert Certificate data. Can be CA, client certificate or client key
 * @param type Certificate type. 0 = CA, 1 = client certificate, 2 = client key
 * @param name Certificate name. Can be any name, but must be unique
 * @return 0 if successful, 1 if not, 2 if malloc failed
 */
int setCertMQTT(const byte *cert, int size, int type, const char *name)
{
  int result = 1;
  char *command = NULL;
  char *response = NULL;
  response = (char *)malloc(strlen(SARA_IMPORT_CERT_OK) + 5);
  if (response == NULL)
  {
    printToConsole("Malloc failed\n");
    return 2;
  }
  command = (char *)malloc((strlen(AT) + strlen(SARA_IMPORT_CERT) + strlen(name) + 15));
  if (command == NULL)
  {
    printToConsole("Malloc failed\n");
    free(response);
    return 2;
  }
  sprintf(command, "%s%s%d,\"%s\",%d", AT, SARA_IMPORT_CERT, type, name, size);
  printToConsole("Importing certificate: ");
  printToConsole(name);
  printToConsole("\n");
  // printToConsole("Command: %s\n", command);
  //  Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(SARA_IMPORT_CERT_READY, 10000))
  {
    printToConsole("Ready to receive certificate:\n");
    // printToConsole("%s\n",cert);

    sprintf(response, "%s%d", SARA_IMPORT_CERT_OK, type);

    // Empties buffer and transmits the command
    //transmitCommand(cert);
    while (LTEShieldSerial.available())
    {
      LTEShieldSerial.read();
    }
    LTEShieldSerial.write(cert, size);

    if (getResponse(response, 10000))
    {
      printToConsole("Certificate imported\n");
      result = 0;
    }
    else
    {
      printToConsole("Certificate import failed\n");
    }
  }
  else
  {
    printToConsole("Failed to import certificate - not ready\n");
  }
  free(command);
  free(response);
  return result;
}

/**
 * @brief Reads certificates from filesystem and loads into module
 * @param filename Name of the file to read
 * @param type Certificate type. 0 = CA, 1 = client certificate, 2 = client key
 * @param name Certificate name. Can be any name, but must be unique
 * @return -1 if load file fails, return value from setCertMQTT() if successful
 */
int loadCertMQTT(const char *filename, int type, const char *name)
{
  int result = 0;
  File certFile = SPIFFS.open(filename, "r");
  if (!certFile)
  {
    printToConsole("Failed to open file for reading\n");
    return -1;
  }
  int size = certFile.size();
  byte *cert = (byte *)malloc(size);
  if (cert == NULL)
  {
    printToConsole("Malloc failed\n");
    return -1;
  }
  certFile.read(cert, size);
  certFile.close();
  result = setCertMQTT(cert, size, type, name);
  free(cert);
  return result;
}

/**
 * @brief Sets the MQTT ping interval
 * @param timeout Timeout in seconds
 * @return 0 if successful, 1 if not, 2 if malloc failed
 */
int setMQTTping(int timeout)
{
  int result = 1;
  char *command = NULL;

  command = (char *)malloc(strlen(AT) + strlen(SARA_TIMEOUT) + 5);
  if (command == NULL)
  {
    printToConsole("Malloc failed\n");
    return 2;
  }
  sprintf(command, "%s%s%d", AT, SARA_TIMEOUT, timeout);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(SARA_TIMEOUT_SET_RESPONSE, 10000))
  {
    char msgToPrint[50];
    sprintf(msgToPrint, "MQTT ping interval set to %d seconds\n", timeout);
    printToConsole(msgToPrint);
    result = 0;
  }
  else
  {
    printToConsole("Error setting MQTT ping timeout\n");
  }
  free(command);
  return result;
}

/**
 * @brief Sets the MQTT Unique Client ID
 * @param id Unique client ID
 * @return 0 if successful, 1 if not, 2 if malloc failed
 */
int setMQTTid(const char *id)
{
  int result = 1;
  char *command = NULL;

  command = (char *)malloc(strlen(AT) + strlen(SARA_MQTT_ID) + strlen(id) + 5);
  if (command == NULL)
  {
    printToConsole("Malloc failed\n");
    return 2;
  }
  sprintf(command, "%s%s\"%s\"", AT, SARA_MQTT_ID, id);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(SARA_MQTT_ID_SET_RESPONSE, 10000))
  {
    char msgToPrint[50];
    sprintf(msgToPrint, "MQTT client ID set to %s\n", id);
    printToConsole(msgToPrint);
    result = 0;
  }
  else
  {
    printToConsole("Error setting MQTT client ID\n");
  }
  free(command);
  return result;
}

/**
 * @brief Enables MQTT keepalive
 * @return 0 if successful, 1 if not, 2 if malloc failed
 */
int enableMQTTkeepalive()
{
  int result = 1;
  char *command = NULL;

  command = (char *)malloc(strlen(AT) + strlen(SARA_PING) + 5);
  if (command == NULL)
  {
    printToConsole("Malloc failed\n");
    return 2;
  }
  sprintf(command, "%s%s", AT, SARA_PING);

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(SARA_PING_OK, 10000))
  {
    printToConsole("MQTT keepalive enabled\n");
    result = 0;
  }
  else
  {
    printToConsole("Error enabling MQTT keepalive\n");
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
 * @return 0 if successful, 1 if not, 2 if memory allocation failed
 */
int publishMessage(const char *topic, const char *message, int QoS, int retain)
{
  int result = 1;
  char *command = NULL;

  unsigned long SARA_SEND_TIMEOUT = 60000;

  // printToConsole("Allocating memory for command\n");
  command = (char *)malloc(strlen(AT) + strlen(SARA_SEND_MQTT) + strlen(message) + strlen(topic) + 8);
  if (command == NULL)
  {
    printToConsole("Memory allocation failed\n");
    return 2;
  }
  // printToConsole("Building command\n");
  sprintf(command, "%s%s,%d,%d,%s,%s", AT, SARA_SEND_MQTT, QoS, retain, topic, message);

  // printToConsole("Command built....Sending\n");

  // Empties buffer and transmits the command
  transmitCommand(command);

  // printToConsole("Command sent....Waiting for response\n");
  if (getResponse(SARA_SEND_OK, SARA_SEND_TIMEOUT))
  {
    printToConsole("Message sent\n");
    result = 0;
  }
  else
  {
    printToConsole("Error sending message\n");
  }

  free(command);
  return result;
}

/**
 * @brief Login to the MQTT broker
 * @return 0 if successful, 1 if not
 */
int loginMQTT()
{
  int result = 1;
  unsigned long SARA_IP_CONNECT_TIMEOUT = 60000;

  // Empties buffer and transmits the command
  transmitCommand(SARA_LOGIN);

  if (getResponse(SARA_LOGIN_OK, SARA_IP_CONNECT_TIMEOUT))
  {
    printToConsole("MQTT login successfull\n");
    result = 0;
  }
  else
  {
    printToConsole("Error logging in to MQTT\n");
  }
  return result;
}

/**
 * @brief Configures MQTT Will topic
 * @param topic The topic to publish to
 * @return 0 if successful, 1 if not, 2 if memory allocation failed
 */
int willconfigMQTT(const char *topic)
{
  int result = 1;
  char *command = NULL;
  unsigned long SARA_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(strlen(AT) + strlen(SARA_WILL_TOPIC_MQTT) + strlen(topic) + 10);
  if (command == NULL)
  {
    printToConsole("Malloc failed");
    return 2;
  }
  sprintf(command, "%s%s%d,%d,\"%s\"", AT, SARA_WILL_TOPIC_MQTT, 0, 0, topic);

  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(SARA_TOPIC_OK, SARA_IP_CONNECT_TIMEOUT))
  {
    Serial.println("Will topic set\n");
    result = 0;
  }
  else
  {
    Serial.println("Will topic not set\n");
  }

  free(command);

  return result;
}

/**
 * @brief Configures MQTT Will message
 * @param message The message to publish
 * @return 0 if successful, 1 if not, 2 if memory allocation failed
 */
int willmsgMQTT(const char *message)
{
  int result = 1;
  char *command = NULL;

  unsigned long SARA_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(strlen(AT) + strlen(SARA_WILL_MESSAGE_MQTT) + strlen(message) + 10);
  if (command == NULL)
  {
    printToConsole("Malloc failed\n");
    return 2;
  }
  sprintf(command, "%s%s\"%s\"", AT, SARA_WILL_MESSAGE_MQTT, message);

  // Empties buffer and transmits the command
  transmitCommand(command);

  if (getResponse(SARA_MESSAGE_OK, SARA_IP_CONNECT_TIMEOUT))
  {
    Serial.println("Will message set\n");
    result = 0;
  }
  else
  {
    Serial.println("Will message not set\n");
  }
  free(command);
  return result;
}

/**
 * @brief Configures MQTT broker hostname and port
 * @param hostname The hostname of the MQTT broker
 * @param port The port of the MQTT broker
 * @return 0 if successful, 1 if not, 2 if memory allocation failed
 */
int setMQTT(const char *host, int port)
{
  int result = 1;
  char *command = NULL;
  unsigned long SARA_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(sizeof(char) * (strlen(SARA_CONNECT_MQTT) + strlen(host) + 15));
  if (command == NULL)
  {
    printToConsole("Malloc failed\n");
    return 2;
  }
  // Create the command
  sprintf(command, "AT%s=%d,\"%s\",%d", SARA_CONNECT_MQTT, 2, host, port);
  // Send the command
  printToConsole("Setting MQTT hostname & port\n");

  // Empties buffer and transmits the command
  transmitCommand(command);

  // Wait for the response
  if (getResponse(SARA_RESPONSE_OK, SARA_IP_CONNECT_TIMEOUT))
  {
    printToConsole("MQTT hostname & port set\n");
    result = 0;
  }
  else
  {
    printToConsole("MQTT hostname & port not set\n");
  }

  free(command);
  return result;
}