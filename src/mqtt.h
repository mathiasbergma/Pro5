#include <Arduino.h>
#include "AT_commands.h"
#include <HardwareSerial.h>

extern HardwareSerial lteSerial;

#define SerialMonitor Serial
#define LTEShieldSerial lteSerial
void setMQTT(const char * host, int port);
int publishMessage(const char * topic, const char * message, int QoS, int retain);
void loginMQTT();
void willconfigMQTT();
void willmsgMQTT();
int getResponse(const char * response, int timeout);
void enableMQTTkeepalive();
void setMQTTping(int timeout);


void setMQTTping(int timeout)
{
  char * command;
  
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_TIMEOUT) + 5);
  sprintf(command, "%s%s%d", AT, LTE_SHIELD_TIMEOUT, timeout);

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  }
  // Transmit the command
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");

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
void enableMQTTkeepalive()
{
  char * command;
  
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_PING) + 5);
  sprintf(command, "%s%s", AT, LTE_SHIELD_PING);

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  }
  // Transmit the command
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");
  
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
int getResponse(const char * response, int timeout)
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
int publishMessage(const char * topic, const char * message, int QoS, int retain)
{
  char * command = NULL;
  
  unsigned long LTE_SHIELD_SEND_TIMEOUT = 60000;

  //SerialMonitor.printf("Allocating memory for command\n");
  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_SEND_MQTT) + strlen(message) + strlen(topic) + 8);
  if (command != NULL)
  {
    //SerialMonitor.printf("Memory allocated\n");
  }
  else
  {
    SerialMonitor.printf("Memory allocation failed\n");
    return -1;
  }
  //SerialMonitor.printf("Building command\n");
  sprintf(command, "%s%s,%d,%d,%s,%s", AT, LTE_SHIELD_SEND_MQTT, QoS, retain, topic, message);
  
  //SerialMonitor.printf("Command built....Sending\n");

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  } 
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");

  //SerialMonitor.printf("Command sent....Waiting for response\n");
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
void loginMQTT()
{
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 60000;
  
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

  if (getResponse(LTE_SHIELD_LOGIN_OK, LTE_SHIELD_IP_CONNECT_TIMEOUT))
  {
    SerialMonitor.println("MQTT login successful");
  }
  else
  {
    SerialMonitor.println("Error logging in to MQTT");
  }
  
}
void willconfigMQTT()
{
  char *command;
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;
  char willtopic[] = "test/will";

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_TOPIC_MQTT) + strlen(willtopic) +10);
  sprintf(command, "%s%s%d,%d,\"%s\"", AT, LTE_SHIELD_WILL_TOPIC_MQTT, 0, 0, willtopic);

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    char c = LTEShieldSerial.read();
    //SerialMonitor.print(c);
  }
  SerialMonitor.printf("Sending will topic command: %s\n", command);
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");

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
void willmsgMQTT()
{
  char* command;
  
  char willmessage[] = "NB_disconnected";
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(strlen(AT) + strlen(LTE_SHIELD_WILL_MESSAGE_MQTT) + strlen(willmessage) +10);
  sprintf(command, "%s%s\"%s\"", AT, LTE_SHIELD_WILL_MESSAGE_MQTT, willmessage);

  // Empty the serial buffer
  while (LTEShieldSerial.available())
  {
    char c = LTEShieldSerial.read();
    //SerialMonitor.print(c);
  }
  SerialMonitor.printf("Sending will message command: %s\n", LTE_SHIELD_WILL_MESSAGE_MQTT);
  LTEShieldSerial.print(LTE_SHIELD_WILL_MESSAGE_MQTT);
  LTEShieldSerial.print("\r");

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
void setMQTT(const char * host, int port)
{
  char *command;
  
  unsigned long LTE_SHIELD_IP_CONNECT_TIMEOUT = 10000;

  command = (char *)malloc(sizeof(char) * (strlen(LTE_SHIELD_CONNECT_MQTT) + strlen(host) + 15));
  size_t err;

  // Emtpy the buffer before sending the command
  while (LTEShieldSerial.available())
  {
    LTEShieldSerial.read();
  }
  // Create the command
  sprintf(command, "AT%s=%d,\"%s\",%d", LTE_SHIELD_CONNECT_MQTT, 2, host, port);
  // Send the command
  SerialMonitor.println("Setting MQTT hostname & port");
  LTEShieldSerial.print(command);
  LTEShieldSerial.print("\r");
  
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