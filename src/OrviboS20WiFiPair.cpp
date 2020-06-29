#include <ESP8266WiFi.h>
#include "OrviboS20WiFiPair.h"

/***********************************************************************************
 * Consts
 ***********************************************************************************/

#define GLOBAL_TIMEOUT_S 60
#define CONNECT_TIMEOUT_S 10
#define COMMAND_TIMEOUT_S 3

const String WIWO_S20_SSID = "WiWo-S20";
const String ASSIST_THREAD[] = "HF-A11ASSISTHREAD";
const uint16_t UDP_PORT = 48899;

/***********************************************************************************
 * Variables
 ***********************************************************************************/

OrviboS20WiFiPairClass OrviboS20WiFiPair;

/***********************************************************************************
 * Class definition
 ***********************************************************************************/

void OrviboS20WiFiPairClass::sendString(const char *str)
{
    uint32_t subnet = WiFi.subnetMask();
    uint32_t mcast = (uint32_t)WiFi.localIP() | (~subnet);

    m_udp.beginPacket(mcast, UDP_PORT);
    m_udp.write(str, strlen(str));
    m_udp.endPacket();
}

void OrviboS20WiFiPairClass::sendCommand(CommandId cmdId)
{
    String cmd;
    switch (cmdId)
    {
    case CMD_ASSISTTHREAD:
        cmd = "HF-A11ASSISTHREAD";
        break;
    case CMD_SSID:
        cmd = "AT+WSSSID=" + m_ssid + "\r";
        break;
    case CMD_KEY:
        cmd = "AT+WSKEY=";
        if (m_passphrase == "")
            cmd += "OPEN,NONE,\r";
        else
            cmd += "WPA2PSK,AES," + m_passphrase + "\r";
        break;
    case CMD_MODE:
        cmd = "AT+WMODE=STA\r";
        break;
    case CMD_Z:
        cmd = "AT+Z\r";
        break;
    }
    if (m_sending_cmd_cb)
    {
        m_sending_cmd_cb(WiFi.BSSID(), cmd.c_str());
    }
    sendString(cmd.c_str());
}

OrviboS20WiFiPairClass::PacketType OrviboS20WiFiPairClass::checkRxPacket()
{
    char rx_buffer[64];
    if (m_udp.parsePacket() <= 0)
    {
        return PKT_NONE;
    }
    int len = m_udp.read(rx_buffer, sizeof(rx_buffer) - 1);
    if (len < 0)
    {
        return PKT_NONE;
    }
    // Null-terminate the string
    rx_buffer[len] = 0;
    String rsp = rx_buffer;
    rsp.toUpperCase();
    if (rsp.startsWith("+OK"))
    {
        return PKT_OK;
    }
    else if (rsp.startsWith("+ERR"))
    {
        return PKT_ERROR;
    }
    else if (rsp.endsWith("HF-LPB100") && (m_current_cmd == CMD_ASSISTTHREAD))
    {
        sendString("+ok");
        return PKT_OK;
    }
    return PKT_UNKNOWN;
}

OrviboS20WiFiPairClass::State OrviboS20WiFiPairClass::enterState(State state)
{
    switch (state)
    {
    case S_IDLE:
        m_tmo_timer = GLOBAL_TIMEOUT_S;
        WiFi.disconnect();
        break;
    case S_SCAN:
        WiFi.scanNetworks(true, false);
        break;
    case S_CONNECT:
        m_state_timer = CONNECT_TIMEOUT_S;
        WiFi.begin(WIWO_S20_SSID);
        break;
    case S_SEND_COMMANDS:
        m_state_timer = COMMAND_TIMEOUT_S;
        m_current_cmd = CMD_FIRST;
        m_cmd_retransmit_counter = 0;
        sendCommand(m_current_cmd);
        break;
    case S_STOPPED:
        WiFi.disconnect();
        if (m_state != S_STOPPED)
        {
            m_udp.stop();
            if (m_stopped_cb)
            {
                OrviboStopReason reason;
                switch (m_state)
                {
                case S_PAIRING_COMPLETE:
                    reason = REASON_PAIRING_SUCCESSFUL;
                    break;
                case S_COMMAND_FAILED:
                    reason = REASON_COMMAND_FAILED;
                    break;
                case S_TIMEOUT:
                    reason = REASON_TIMEOUT;
                    break;
                default:
                    reason = REASON_STOPPED_BY_USER;
                }
                m_stopped_cb(reason);
            }
        }
        break;
    case S_PAIRING_COMPLETE:
        if (m_success_cb)
        {
            m_success_cb(WiFi.BSSID());
        }
        return enterState(S_STOPPED);
    case S_COMMAND_FAILED:
        return enterState(S_STOPPED);
    case S_TIMEOUT:
        return enterState(S_STOPPED);

    default:
        break;
    }
    return state;
}

OrviboS20WiFiPairClass::State OrviboS20WiFiPairClass::executeState(State state)
{
    bool state_timeout, global_timeout;
    int networksFound;
    PacketType pkt = checkRxPacket();

    if (millis() - m_last_tick_time > 1000)
    {
        m_last_tick_time = millis();
        if (m_state_timer >= 0)
            m_state_timer--;
        if (m_tmo_timer >= 0)
            m_tmo_timer--;
    }
    state_timeout = m_state_timer <= 0;
    global_timeout = m_tmo_timer <= 0;

    if (state != S_STOPPED && global_timeout)
    {
        return enterState(S_TIMEOUT);
    }

    switch (state)
    {
    case S_IDLE:
        return enterState(S_SCAN);

    case S_SCAN:
        networksFound = WiFi.scanComplete();
        if (networksFound >= 0)
        {
            for (int i = 0; i < networksFound; i++)
            {
                if (WiFi.SSID(i) == WIWO_S20_SSID)
                {
                    if (m_found_device_cb)
                    {
                        m_found_device_cb(WiFi.BSSID(i));
                    }
                    return enterState(S_CONNECT);
                }
            }
            return enterState(S_SCAN);
        }
        break;

    case S_CONNECT:
        if (WiFi.isConnected())
        {
            return enterState(S_SEND_COMMANDS);
        }
        if (state_timeout)
        {
            return enterState(S_SCAN);
        }
        break;

    case S_SEND_COMMANDS:
        if (pkt == PKT_OK)
        {
            state_timeout = COMMAND_TIMEOUT_S;
            m_cmd_retransmit_counter = 0;
            m_current_cmd = static_cast<CommandId>(static_cast<int>(m_current_cmd) + 1);

            if (m_current_cmd == CMD_LAST)
            {
                // We should actually never get here since the last AT+Z does not send any response
                return enterState(S_PAIRING_COMPLETE);
            }
            // Send next command
            sendCommand(m_current_cmd);
        }
        else if (pkt == PKT_ERROR)
        {
            // Something went wrong..
            return enterState(S_COMMAND_FAILED);
        }
        else if (state_timeout)
        {
            if (m_cmd_retransmit_counter++ < 2)
            {
                state_timeout = COMMAND_TIMEOUT_S;
                sendCommand(m_current_cmd);
                if (m_current_cmd == CMD_Z)
                {
                    // ATZ is last command and since S20 will not send any response for this cmd we're done..
                    return enterState(S_PAIRING_COMPLETE);
                }
            }
            else
            {
                return enterState(S_SCAN);
            }
        }
        break;

    default:
        break;
    }
    return state;
}

bool OrviboS20WiFiPairClass::begin(const char *ssid, const char *passphrase)
{
    m_ssid = ssid;
    if (passphrase == nullptr)
        m_passphrase = "";
    else
        m_passphrase = passphrase;

    m_state = enterState(S_IDLE);
    return m_udp.begin(UDP_PORT);
}

void OrviboS20WiFiPairClass::stop()
{
    m_state = enterState(S_STOPPED);
}

void OrviboS20WiFiPairClass::handle()
{
    m_state = executeState(m_state);
}
