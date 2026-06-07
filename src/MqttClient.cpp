#include "MqttClient.h"
#include "Libraries/pubsubclient-2.8/src/PubSubClient.h"

#ifdef ESP32
static void __mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                 int32_t event_id, void *event_data);
#else
#define CLIENT() ((PubSubClient *)client)
#define MQTT_SOCKET_TIMEOUT_SECONDS 5
#endif

MqttClient::MqttClient(Stream &debugLog) : PrintLog(debugLog, "MqttClient") {
#ifdef ESP32
#else
  client = NULL;
#endif
}

MqttClient::~MqttClient() {}

bool MqttClient::begin(String uri) {
  if (isBegin) {
    logInfo("Already begin, calll 'end' and try again");
    return true;
  }
  if (uri.isEmpty()) {
    Serial.println("Mqtt uri is empty");
    return false;
  }

  this->uri = uri;
  logInfo("Init uri: " + uri);

#ifdef ESP32
  /** config esp_mqtt client */
  esp_mqtt_client_config_t config = {
      .uri = this->uri.c_str(),
  };

  /** init client */
  client = esp_mqtt_client_init(&config);
  if (client == NULL) {
    logError("Init client failed");
    return false;
  }

  /** Register event */
  if (esp_mqtt_client_register_event(client, MQTT_EVENT_ANY,
                                     __mqtt_event_handler, this) != ESP_OK) {
    logError("Register event failed");
    return false;
  }

  if (esp_mqtt_client_start(client) != ESP_OK) {
    logError("Client start failed");
    return false;
  }
#else
  user = "";
  password = "";
  server = "";
  port = 0;

  String uriBody;
  if (this->uri.startsWith("mqtt://")) {
    uriBody = this->uri.substring(7);
  } else if (this->uri.startsWith("mqtts://")) {
    uriBody = this->uri.substring(8);
  } else {
    logError("Server invalid");
    return false;
  }

  int atIndex = uriBody.indexOf('@');
  String serverPort = uriBody;
  if (atIndex >= 0) {
    String userPass = uriBody.substring(0, atIndex);
    serverPort = uriBody.substring(atIndex + 1);

    int separator = userPass.indexOf(':');
    if (separator <= 0 || separator == (int)userPass.length() - 1) {
      logError("User and Password invalid");
      return false;
    }

    user = userPass.substring(0, separator);
    password = userPass.substring(separator + 1);

    logInfo("Username: " + user);
    logInfo("Password: " + password);
  }

  int portSeparator = serverPort.lastIndexOf(':');
  if (portSeparator <= 0 || portSeparator == (int)serverPort.length() - 1) {
    logError("Server and port invalid");
    return false;
  }

  server = serverPort.substring(0, portSeparator);
  logInfo("Server: " + server);

  port = (uint16_t)serverPort.substring(portSeparator + 1).toInt();
  if (port == 0) {
    logError("Port invalid");
    return false;
  }
  logInfo("Port: " + String(port));

  if (client == NULL) {
    client = new PubSubClient(__wifiClient);
    if (client == NULL) {
      return false;
    }
  }

  CLIENT()->setServer(server.c_str(), port);
  CLIENT()->setBufferSize(1024);
  CLIENT()->setSocketTimeout(MQTT_SOCKET_TIMEOUT_SECONDS);
  connected = false;
#endif

  isBegin = true;
  connectionFailedCount = 0;
  return true;
}

void MqttClient::end(void) {
  if (!isBegin) {
    logWarning("Already end, call 'begin' and try again");
    return;
  }
#ifdef ESP32
  esp_mqtt_client_disconnect(client);
  esp_mqtt_client_stop(client);
  esp_mqtt_client_destroy(client);
  client = NULL;
#else
  CLIENT()->disconnect();
#endif
  isBegin = false;
  this->uri = "";

  logInfo("end");
}

void MqttClient::_updateConnected(bool connected) {
  this->connected = connected;
  if (connected) {
    connectionFailedCount = 0;
  } else {
    connectionFailedCount++;
    logWarning("Connection failed count " + String(connectionFailedCount));
  }
}

bool MqttClient::publish(const char *topic, const char *payload, int len) {
  if (!isBegin) {
    logError("No-initialized");
    return false;
  }
  if (!connected) {
    logError("Client disconnected");
    return false;
  }

#ifdef ESP32
  if (esp_mqtt_client_publish(client, topic, payload, len, 0, 0) == ESP_OK) {
    logInfo("Publish success");
    return true;
  }
#else
  if (CLIENT()->publish(topic, payload)) {
    logInfo("Publish success");
    return true;
  }
#endif
  logError("Publish failed");
  return false;
}

/**
 * @brief Check that URI is same as current initialized  URI
 *
 * @param uri Target URI
 * @return true Same
 * @return false Difference
 */
bool MqttClient::isCurrentUri(String &uri) {
  if (this->uri == uri) {
    return true;
  }
  return false;
}

/**
 * @brief Get MQTT client connected status
 *
 * @return true Connected
 * @return false Disconnected
 */
bool MqttClient::isConnected(void) {
  return connected;
}

/**
 * @brief Get number of connection failed
 *
 * @return int
 */
int MqttClient::getConnectionFailedCount(void) { return connectionFailedCount; }

#ifdef ESP8266
bool MqttClient::connect(String id) {
  if (isBegin == false) {
    return false;
  }

  if (this->uri.isEmpty()) {
    return false;
  }

  connected = false;
  if (user.isEmpty()) {
    logInfo("Connect without auth");
    connected = CLIENT()->connect(id.c_str());
  } else {
    logInfo("Connect with auth");
    connected = CLIENT()->connect(id.c_str(), user.c_str(), password.c_str());
  }
  return connected;
}

void MqttClient::handle(void) {
  if (isBegin == false) {
    return;
  }
  CLIENT()->loop();
}
#endif

#ifdef ESP32
static void __mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                 int32_t event_id, void *event_data) {
  MqttClient *mqtt = (MqttClient *)handler_args;

  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t client = event->client;

  int msg_id;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    mqtt->logInfo("MQTT_EVENT_CONNECTED");
    mqtt->_updateConnected(true);
    break;
  case MQTT_EVENT_DISCONNECTED:
    mqtt->logInfo("MQTT_EVENT_DISCONNECTED");
    mqtt->_updateConnected(false);
    break;
  case MQTT_EVENT_SUBSCRIBED:
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    break;
  case MQTT_EVENT_PUBLISHED:
    break;
  case MQTT_EVENT_DATA:
    break;
  case MQTT_EVENT_ERROR:
    Serial.println("MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      mqtt->logError("Reported from esp-tls: " +
                     String(event->error_handle->esp_tls_last_esp_err));
      mqtt->logError("Reported from tls stack: " +
                     String(event->error_handle->esp_tls_stack_err));
      mqtt->logError("Captured as transport's socket errno: " +
                     String(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    Serial.printf("Other event id:%d\r\n", event->event_id);
    break;
  }
}
#endif
