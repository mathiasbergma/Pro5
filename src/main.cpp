#include <Arduino.h>
//#include "certs.h"
#include "AT_commands.h"
#include "NB_R410M.h"

// LTE_Shield lte;

#define SerialMonitor Serial
#define CA_FILE "/ca.pem"
#define CERT_FILE "/cert.der"
#define KEY_FILE "/key.der"

#define CA_NAME "ca"
#define CERT_NAME "cert"
#define KEY_NAME "key"
#define SEC_PROFILE 0

const struct connection_info_t
{
  const char *HostName = "NBIoTLS.azure-devices.net ";
  const char *identity = "subscriber";
  const char *username = "NBIoTLS.azure-devices.net/subscriber/?api-version=2021-04-12";
  const char *topic = "devices/subscriber/messages/events/";
  const char *subTopic = "devices/subscriber/inboundmessages/events/";
  const char *SharedAccessKeyName = "iothubowner";
  const char *SharedAccessKey = "iQJQVxmSHgOYmmq1lSHP9Uwpj0Hh+JHJcl0IpllDQLw=";
  const int Port = 8883;
} connection_info;

#define POWERPIN 33

// APN -- Access Point Name. Gateway between GPRS MNO
// and another computer network. E.g. "hologram
const char APN[] = "lpwa.telia.iot";

#define DEBUG_PASSTHROUGH_ENABLED

/*


*/
char *IP = NULL;

void setup()
{
  SerialMonitor.begin(9600);
  while (!SerialMonitor)
    ; // For boards with built-in USB

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  SerialMonitor.println(F("Initializing the LTE Shield..."));
  SerialMonitor.println(F("...this may take ~25 seconds if the shield is off."));
  SerialMonitor.println(F("...it may take ~5 seconds if it just turned on."));

  SerialMonitor.printf("Cycling powerpin\n");

  pinMode(POWERPIN, OUTPUT);
  digitalWrite(POWERPIN, LOW);
  delay(100);
  pinMode(POWERPIN, INPUT); // Return to high-impedance, rely on SARA module internal pull-up

  // Initialize the LTE Shield and enable AT interface and Timezone update
  if (!initModule(120000))
  {
    SerialMonitor.println(F("Failed to initialize the LTE Shield!"));
    while (1)
    {
      if (LTEShieldSerial.available())
      {
        SerialMonitor.write((char)LTEShieldSerial.read());
      }
      if (SerialMonitor.available())
      {
        LTEShieldSerial.write((char)SerialMonitor.read());
      }
    }
  }

  // Set the operator APN
  setAPN(APN);

  // Get status of network aquisition
  getNetwork();

  // Get IP address
  IP = printInfo();
  if (IP != NULL)
  {
    SerialMonitor.printf("IP: %s\n", IP);
  }
  else
  {
    SerialMonitor.printf("IP: NULL\n");
  }

  // Import CA certificate
  loadCertMQTT(CA_FILE, 0, CA_NAME);

  // Import client certificate
  loadCertMQTT(CERT_FILE, 1, CERT_NAME);

  // import client private key
  loadCertMQTT(KEY_FILE, 2, KEY_NAME);


  // Assign the certificates to a security profile
  assignCert(SEC_PROFILE, CA_NAME, 3);
  assignCert(SEC_PROFILE, CERT_NAME, 5);
  assignCert(SEC_PROFILE, KEY_NAME, 6);

  // Set the security profile to be used by MQTT
  enableSSL(SEC_PROFILE);

  // Set MQTT ID
  setMQTTid(connection_info.identity);

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

  char msg[] = "Hello World from NB_IoT module!";
  // Publish message to MQTT broker
  publishMessage(connection_info.topic, msg, 0, 0);
}

void loop()
{
  
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