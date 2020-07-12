/*
 * This example illustrates how to toggle the relay of a single Orvibo WiWo S20 device
 *
 * The example will setup a soft AP with SSID "ORVIBO" and wait for a S20 device to connect.
 * When connected it will toggle the relay each 10 sec.
 *
 * Note: This example expects the S20 device to be "paired" with the SSID "ORVIBO" using
 *       WPA2/AES with passkey "WIWO_S20". Simplest way to do this is to use the included
 *       "PairAndTogglePlug" example.
 */
#include <ESP8266WiFi.h>

#include "OrviboS20.h"

const char *ssid = "ORVIBO";
const char *password = "WIWO_S20";

OrviboS20Device s20;
// The above will use any two S20 devices found.
// If you want to use specific devices you can specify the MAC like:
//   uint8_t mac[] = { 0xac,0xcf,0x23,0x35,0x55,0xb6 };
//   OrviboS20Device s20(mac);

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
  WiFi.mode(WIFI_AP);
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
    Serial.printf("New Orvibo device detected: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  });

  // Start S20 communication (this class handles the communication for all OrviboS20Device instances)
  OrviboS20.begin();
}

void loop()
{
  // Handle S20 communication
  OrviboS20.handle();

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
