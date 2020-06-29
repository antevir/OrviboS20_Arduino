#pragma once

#include <Arduino.h>
#include <functional>

class OrviboS20Class
{
public:
    typedef std::function<void(uint8_t *)> found_device_callback_t;

    /*
     * This callback will be called when a packet is received and successfully parsed
     * with Orvibo protocol for a previously unknown device
     */
    void onFoundDevice(found_device_callback_t cb)
    {
        m_found_device_callback = cb;
    }

    /* Start UDP communication */
    bool begin();
    /* Stop UDP communication */
    void stop();
    /* Call this from loop() */
    void handle();

protected:
    bool m_started = false;
    found_device_callback_t m_found_device_callback = nullptr;

    void checkIfNewDevice(uint8_t *mac);
    void checkRxPacket();
};

class OrviboS20Device
{
public:
    typedef std::function<void(OrviboS20Device &device)> connect_callback_t;
    typedef std::function<void(OrviboS20Device &device, bool)> state_change_callback_t;

    OrviboS20Device(const char name[] = "");
    OrviboS20Device(const uint8_t mac[], const char name[] = "");
    ~OrviboS20Device();

    /* Sets the relay state (true = on) */
    bool setState(bool state);

    /* Returns last known relay state */
    bool getState();

    /*
     * Returns MAC address of the device
     * If no MAC was specified in the constructor it will return 00:00:00:00:00:00
     * until a new Orvibo device connects
     */
    const uint8_t *getMac()
    {
        return m_mac;
    }

    /* Returns name specified in constructor */
    const char *getName()
    {
        return m_name;
    }

    bool isConnected()
    {
        return m_connected;
    }

    /*
     * This callback is called when an UDP packet is received from the device
     */
    void onConnect(connect_callback_t cb)
    {
        m_connect_callback = cb;
    }

    /*
     * This callback is called when the device is disconnected
     * Note: It will take up to ~3 min before this is called after the device is unplugged
     */
    void onDisconnect(connect_callback_t cb)
    {
        m_disconnect_callback = cb;
    }

    /* This callback is called when the relay changes state */
    void onStateChange(state_change_callback_t cb)
    {
        m_state_change_callback = cb;
    }

protected:
    IPAddress m_ip = {};
    uint8_t m_mac[6] = {};
    char m_name[32];
    OrviboS20Device *m_next = {};
    bool m_any_mac;
    int m_last_state = -1;
    bool m_connected = false;
    unsigned int m_last_rx_time = 0;
    connect_callback_t m_connect_callback = nullptr;
    connect_callback_t m_disconnect_callback = nullptr;
    state_change_callback_t m_state_change_callback = nullptr;

    void sendCommand(uint16_t command, uint8_t *payload, size_t length);
    void subscribe();
    void checkConnectTimeout();
    void updateConnectState(bool connected);
    void handlePacket(uint16_t command, uint8_t *payload, size_t length);

    friend class OrviboS20Class;
    friend class SharedData;
};

extern OrviboS20Class OrviboS20;
