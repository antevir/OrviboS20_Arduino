/*
 * This example illustrates how to pair an Orvibo WiWo S20 device to a soft AP
 *
 * The example will setup a soft AP with SSID "ORVIBO". It will then search for a S20
 * device in pairing mode. When found it will connect to this device as a WiFi station
 * and send AT commands to the S20 device to set it up for our soft AP.
 * When the setup is done, the S20 device should connect to the AP and the relay should
 * toggle each 10 sec.
 *
 * To set a WiWo S20 device in pairing mode you need to hold its button for 5 sec.
 * The device will then enter "reset" mode indicated by rapidly blinking red. Hold
 * the button again to enter "pairing" mode indicated by rapidly blinking blue.
 *
 * Note: The pairing process will timeout 60 seconds after OrviboS20WiFiPair.begin()
 */
#include <ESP8266WiFi.h>

#include "OrviboS20.h"
#include "OrviboS20WiFiPair.h"

const char *ssid = "ORVIBO";
const char *password = "WIWO_S20";

// We use this instance to control the S20 after the pairing is done
OrviboS20Device s20;

void setup()
{
  Serial.begin(115200);

  WiFi.softAPdisconnect(true);

  // There seems to be an issue with the ESP DHCP server and a re-connecting S20
  // If you reboot the ESP with a S20 device connected without this delay the IP assignment
  // will not work and also the wifi_softap_get_station_info() will get stuck.
  Serial.println("Long delay...");
  delay(30000);

  Serial.println("Configuring Access Point ...");
  WiFi.mode(WIFI_AP_STA); // OrviboS20WiFiPair MUST have STA enabled, OrviboS20 requires AP
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Set callbacks for S20 device
  s20.onConnect([](OrviboS20Device &device) {
    Serial.println("S20 connected!");
  });
  s20.onDisconnect([](OrviboS20Device &device) {
    Serial.println("S20 disconnected!");
  });
  s20.onStateChange([](OrviboS20Device &device, bool new_state) {
    Serial.printf("S20 state changed to: %d\n", new_state);
  });

  // Set OrviboS20 communication callbacks
  OrviboS20.onFoundDevice([](uint8_t mac[]) {
    Serial.printf("<OrviboS20> Detected new Orvibo device: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  });

  // Set callbacks for OrviboS20WiFiPair
  OrviboS20WiFiPair.onFoundDevice([](const uint8_t mac[]) {
    // This is called when OrviboS20WiFiPair finds a device with SSID "WiWo-S20"
    // OrviboS20WiFiPair will then try to connect to the device
    Serial.println("<OrviboS20WiFiPair> Found S20 device in pairing mode");
  });
  OrviboS20WiFiPair.onSendingCommand([](const uint8_t mac[], const char cmd[]) {
    // This is called for each command sent to the S20 device to be paired
    Serial.print("<OrviboS20WiFiPair> Sending command: ");
    Serial.println(cmd);
  });
  OrviboS20WiFiPair.onStopped([](OrviboStopReason reason) {
    // This is called when the pairing process is stopped
    switch (reason)
    {
    case REASON_TIMEOUT:
      Serial.println("<OrviboS20WiFiPair> Pairing timeout");
      break;
    case REASON_COMMAND_FAILED:
      Serial.println("<OrviboS20WiFiPair> Command failed");
      break;
    case REASON_STOPPED_BY_USER:
    case REASON_PAIRING_SUCCESSFUL:
      // When reason == REASON_PAIRING_SUCCESSFUL onSuccess() will also be called
      break;
    }
  });
  OrviboS20WiFiPair.onSuccess([](const uint8_t mac[]) {
    // The pairing is now complete and the S20 device should now connect to our "WIWO" SSID
    Serial.println("<OrviboS20WiFiPair> Pairing successful!");
  });

  // Start S20 communication (this class handles the communication for all OrviboS20Device instances)
  OrviboS20.begin();

  // Start S20 pairing process and make it connect to our AP
  OrviboS20WiFiPair.begin(ssid, password);
}

void loop()
{
  // Handle S20 communication
  OrviboS20.handle();
  // Handle WiFi pairing communication
  OrviboS20WiFiPair.handle();

  // Toggle relay of S20 each 10 sec
  static unsigned int lastTime = 0;
  if (millis() - lastTime > 10000)
  {
    lastTime = millis();

    // Get last known relay state
    bool state = s20.getState();
    // Set relay to inverted state
    s20.setState(!state);
  }
}
