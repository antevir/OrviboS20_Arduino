# OrviboS20_Arduino
This is an unofficial Arduino library for Orvibo WiWo S20 smart plugs using a ESP8266. It supports:
* Subscription (keeps track of the current relay state)
* Setting relay state
* WiFi "pairing" (i.e. configure a S20 plug with a new WiFi SSID and passkey)
* Multiple devices

Credits to the protocol information available here: https://github.com/Grayda/node-orvibo

## Rationale
The S20 devices are not sold anymore so why bother making a library? I happen to have some WiWo S20 lying around and from time to time I have projects that requires controlling 230V devices. Instead of using DIY relay boards that is sometimes not that safe it seems like a better idea to control the 230V device with a WiWo S20 connected to an ESP8266.

