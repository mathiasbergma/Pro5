#ifndef AT_COMMANDS_H
#define AT_COMMANDS_H

const char AT[] = "AT";
const char LTE_SHIELD_TIMEOUT[] = "+UMQTT=10,"; // 10 is the timeout command
const char LTE_SHIELD_TIMEOUT_SET_RESPONSE[] = "+UMQTT: 10,1";

const char LTE_SHIELD_PING[] = "+UMQTTC=8,1";
const char LTE_SHIELD_PING_OK[] = "+UMQTTC: 8,1";

const char LTE_SHIELD_SEND_MQTT[] = "+UMQTTC=2";
const char LTE_SHIELD_SEND_OK[] = "+UMQTTC: 2,1";

const char LTE_SHIELD_LOGIN[] = "AT+UMQTTC=1";
const char LTE_SHIELD_LOGIN_OK[] = "+UUMQTTC: 1,0";

const char *LTE_SHIELD_WILL_TOPIC_MQTT = "+UMQTTWTOPIC=";
const char * LTE_SHIELD_TOPIC_OK = "+UMQTTWTOPIC: 1";

const char *LTE_SHIELD_WILL_MESSAGE_MQTT = "AT+UMQTTWMSG=";
const char * LTE_SHIELD_MESSAGE_OK = "+UMQTTWMSG: 1";

const char *LTE_SHIELD_CONNECT_MQTT = "+UMQTT";
const char LTE_SHIELD_RESPONSE_OK[] = "+UMQTT: 2,1";



#endif //AT_COMMANDS_H