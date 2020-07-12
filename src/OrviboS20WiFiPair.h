#pragma once

#include <Arduino.h>
#include <WiFiUDP.h>
#include <functional>

enum OrviboStopReason
{
    REASON_TIMEOUT = -2,
    REASON_COMMAND_FAILED = -1,
    REASON_STOPPED_BY_USER = 0, /* User called stop() */
    REASON_PAIRING_SUCCESSFUL
};

class OrviboS20WiFiPairClass
{
public:
    typedef std::function<void(const uint8_t *bssid)> event_callback_t;
    typedef std::function<void(const uint8_t *bssid, const char cmd[])> command_callback_t;
    typedef std::function<void(OrviboStopReason reason)> stopped_callback_t;

    /* This callback is called when a device with SSID "WiWo-S20" is found */
    void onFoundDevice(event_callback_t cb)
    {
        m_found_device_cb = cb;
    }
    /* This callback is called for each command sent to the device */
    void onSendingCommand(command_callback_t cb)
    {
        m_sending_cmd_cb = cb;
    }
    /*
     * This callback is called when the pairing process is stopped.
     * This means that you need to call begin() again if you want to do another pairing.
     * See OrviboStopReason for stop reason
     */
    void onStopped(stopped_callback_t cb)
    {
        m_stopped_cb = cb;
    }
    void onSuccess(event_callback_t cb)
    {
        m_success_cb = cb;
    }

    /*
     * Call begin() to start the WiFi pairing process for a WiWo S20
     * The ESP must be in STA or AP+STA mode for it to work (use WiFi.mode())
     * Set ssid and passphrase to the AP you want the S20 to connect to.
     * The pairing process will timeout after 60 seconds if it is not able to find
     * and connect a device.
     * Note: When passphrase is set this class will configure auth/enc to WPA2/AES,
     *       which is the default for ESP. Hence, if you try to pair it with another
     *       AP it may not work depending on AP config.
     */
    bool begin(const char *ssid, const char *passphrase = nullptr);
    bool begin(const String &ssid, const String &passphrase = emptyString)
    {
        return begin(ssid.c_str(), passphrase.c_str());
    }

    /* Stop the pairing process */
    void stop();

    /* Returns true if the pairing process is still active */
    bool isActive()
    {
        return (m_state != S_STOPPED);
    }

    /* Call this from loop() */
    void handle();

protected:
    enum State
    {
        S_STOPPED = 0,
        S_IDLE,
        S_SCAN,
        S_CONNECT,
        S_SEND_COMMANDS,
        S_PAIRING_COMPLETE,
        S_COMMAND_FAILED = -1,
        S_TIMEOUT = -2
    };

    enum CommandId
    {
        CMD_FIRST = 0,

        CMD_ASSISTTHREAD = CMD_FIRST,
        CMD_SSID,
        CMD_KEY,
        CMD_MODE,
        CMD_Z,

        CMD_LAST
    };

    enum PacketType
    {
        PKT_NONE,
        PKT_OK,
        PKT_ERROR,
        PKT_UNKNOWN
    };

    String m_ssid;
    String m_passphrase;
    State m_state = S_STOPPED;
    WiFiUDP m_udp;
    CommandId m_current_cmd;
    int m_cmd_retransmit_counter;
    int m_state_timer;
    int m_tmo_timer;
    unsigned long m_last_tick_time = 0;

    event_callback_t m_found_device_cb = nullptr;
    command_callback_t m_sending_cmd_cb = nullptr;
    stopped_callback_t m_stopped_cb = nullptr;
    event_callback_t m_success_cb = nullptr;

    void sendString(const char *str);
    void sendCommand(CommandId cmdId);
    PacketType checkRxPacket();
    State enterState(State state);
    State executeState(State state);
};

extern OrviboS20WiFiPairClass OrviboS20WiFiPair;
