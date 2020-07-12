/*
 * This example illustrates how to toggle the relay of multiple Orvibo WiWo S20 devices
 *
 * The example will setup a soft AP with SSID "ORVIBO" and wait for two S20 device to connect.
 * When connected it will toggle the relay each 10 sec.
 *
 * Note: This example expects the S20 devices to be "paired" with the SSID "ORVIBO" using
 *       WPA2/AES with passkey "WIWO_S20". Simplest way to do this is to use the included
 *       "PairAndTogglePlug" example.
 */
#include <ESP8266WiFi.h>

#include "OrviboS20.h"

const char *ssid = "ORVIBO";
const char *password = "WIWO_S20";

OrviboS20Device s20_1("Plug1");
OrviboS20Device s20_2("Plug2");
// The above will use any two S20 devices found.
// If you want to use specific devices you can specify the MAC like:
//   uint8_t mac1[] = { 0xac,0xcf,0x23,0x35,0x55,0xb6 };
//   uint8_t mac2[] = { 0xac,0xcf,0x23,0x35,0xf3,0xe2 };
//   OrviboS20Device s20_1(mac1, "Plug1");
//   OrviboS20Device s20_2(mac2, "Plug2");

void onS20Connect(OrviboS20Device &device)
{
  Serial.printf("S20 device \"%s\" connected\n", device.getName());
}

void onS20Disconnect(OrviboS20Device &device)
{
  Serial.printf("S20 device \"%s\" disconnected\n", device.getName());
}

void onS20StateChange(OrviboS20Device &device, bool newState)
{
  Serial.printf("S20 device \"%s\" changed state to: %d\n", device.getName(), newState);
}

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

  // Set callbacks for S20 devices
  s20_1.onConnect(onS20Connect);
  s20_1.onDisconnect(onS20Disconnect);
  s20_1.onStateChange(onS20StateChange);
  s20_2.onConnect(onS20Connect);
  s20_2.onDisconnect(onS20Disconnect);
  s20_2.onStateChange(onS20StateChange);

  // Set OrviboS20 communication callbacks
  OrviboS20.onFoundDevice([](uint8_t *mac) {
    Serial.printf("New Orvibo device detected, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

    // Get last known relay state for first device
    bool state = s20_1.getState();
    // Set relays
    s20_1.setState(!state);
    s20_2.setState(state);
  }
}
