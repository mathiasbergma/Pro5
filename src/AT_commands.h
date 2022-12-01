/**
 * @file AT_commands.h
 * @author Mathias
 * @brief .h file containing constants with AT commands for U-Blox R410M-02B
 * @version 0.1
 * @date 2022-11-28
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef AT_COMMANDS_H
#define AT_COMMANDS_H

const char AT[] = "AT";

const char SARA_TIMEOUT[] = "+UMQTT=10,"; // 10 is the timeout command
const char SARA_TIMEOUT_SET_RESPONSE[] = "+UMQTT: 10,1";

const char SARA_PING[] = "+UMQTTC=8,1";
const char SARA_PING_OK[] = "+UMQTTC: 8,1";

const char SARA_SEND_MQTT[] = "+UMQTTC=2";
const char SARA_SEND_OK[] = "+UMQTTC: 2,1";

const char SARA_LOGIN[] = "AT+UMQTTC=1";
const char SARA_LOGIN_OK[] = "+UUMQTTC: 1,0";

const char SARA_WILL_TOPIC_MQTT[] = "+UMQTTWTOPIC=";
const char SARA_TOPIC_OK[] = "+UMQTTWTOPIC: 1";

const char SARA_WILL_MESSAGE_MQTT[] = "+UMQTTWMSG=";
const char SARA_MESSAGE_OK[] = "+UMQTTWMSG: 1";

const char SARA_CONNECT_MQTT[] = "+UMQTT";
const char SARA_RESPONSE_OK[] = "+UMQTT: 2,1";

const char SARA_IMPORT_CERT[] = "+USECMNG=0,"; // 0 is the import command
const char SARA_IMPORT_CERT_READY[] = ">";
const char SARA_IMPORT_CERT_OK[] = "+USECMNG: 0,";

const char SARA_SET_APN[] = "+CGDCONT=1,\"IP\",";

const char SARA_NETWORK_INFO_GET[] = "+CGDCONT?";

const char SARA_MQTT_SECURE[] = "+UMQTT=11,1,"; // 11 is the secure command
const char SARA_MQTT_SECURE_SET_RESPONSE[] = "+UMQTT: 11,1";

const char SARA_SECURITY_PROFILE[] = "+USECPRF=";



#endif //AT_COMMANDS_H