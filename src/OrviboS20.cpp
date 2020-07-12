#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "OrviboS20.h"

/***********************************************************************************
 * Types
 ***********************************************************************************/

struct
{
    uint8_t mac[6];
    bool used;
} mac_entry_t;

/***********************************************************************************
 * Consts
 ***********************************************************************************/

#define MAX_ORVIBO_DEVICES 10

static const unsigned int ORVIBO_UDP_PORT = 10000;
static const uint16_t ORVIBO_HEADER_LEN = 2 /*magic*/ + 2 /*len*/ + 2 /*cmd*/ + 6 /*mac*/ + 6 /*pad*/;
static const uint8_t ORVIBO_MAGIC[] = {0x68, 0x64};
static const uint8_t ORVIBO_MAC[] = {0xAC, 0xCF, 0x23};

static const uint16_t CMD_SUBSCRIBE = 0x636C;
static const uint16_t CMD_SET_STATE = 0x6463;
static const uint16_t CMD_DISCOVER = 0x7161;
static const uint16_t CMD_STATE_CHANGE = 0x7366;

static const uint8_t MAC_PADDING[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
static const uint8_t ZERO_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned int SUBSCRIBE_INTERVAL_MS = 1000 * 60;
static const unsigned int CONNECTION_TMO_MS = 1000 * 150;
static const unsigned int CHECK_TMO_INTERVAL_MS = 1000 * 10;

/***********************************************************************************
 * Variables
 ***********************************************************************************/

OrviboS20Class OrviboS20;

/***********************************************************************************
 * Static functions
 ***********************************************************************************/

static void reverse(uint8_t *dst, uint8_t *src, size_t len)
{
    for (int i = 0; i < len; i++)
    {
        dst[len - i - 1] = src[i];
    }
}

/***********************************************************************************
 * Singleton class for shared data between OrviboS20Device and OrviboS20Class
 ***********************************************************************************/

class SharedData
{
private:
    OrviboS20Device *m_device_list = nullptr;

public:
    WiFiUDP udp;

    static SharedData &getInstance()
    {
        static SharedData instance;
        return instance;
    }

    void addDeviceToList(OrviboS20Device *dev)
    {
        if (m_device_list == nullptr)
        {
            m_device_list = dev;
        }
        else
        {
            OrviboS20Device *iter = m_device_list;
            while (iter->m_next)
            {
                iter = iter->m_next;
            }
            iter->m_next = dev;
        }
    }

    void removeDeviceFromList(OrviboS20Device *dev)
    {
        if (m_device_list == dev)
        {
            m_device_list = dev->m_next;
        }
        else
        {
            OrviboS20Device *iter = m_device_list;
            while (iter->m_next)
            {
                if (iter->m_next == dev)
                {
                    iter->m_next = dev->m_next;
                    break;
                }
                iter = iter->m_next;
            }
        }
    }

    OrviboS20Device *getFirstDevice()
    {
        return m_device_list;
    }
};

/***********************************************************************************
 * OrviboS20Device class definition
 ***********************************************************************************/

OrviboS20Device::OrviboS20Device(const char name[])
{
    strncpy(m_name, name, sizeof(m_name));
    m_any_mac = true;
    SharedData::getInstance().addDeviceToList(this);
}

OrviboS20Device::OrviboS20Device(const uint8_t mac[], const char name[])
{
    strncpy(m_name, name, sizeof(m_name));
    m_any_mac = false;
    memcpy(m_mac, mac, 6);
    SharedData::getInstance().addDeviceToList(this);
}

OrviboS20Device::~OrviboS20Device()
{
    SharedData::getInstance().removeDeviceFromList(this);
}

void OrviboS20Device::updateConnectState(bool connected)
{
    if (connected == m_connected)
        return;

    m_connected = connected;
    if (connected)
    {
        if (m_connect_callback)
        {
            m_connect_callback(*this);
        }
    }
    else
    {
        if (m_disconnect_callback)
        {
            m_disconnect_callback(*this);
        }
    }
}

void OrviboS20Device::sendCommand(uint16_t command, uint8_t *payload, size_t length)
{
    uint16_t tot_len = ORVIBO_HEADER_LEN + length;
    auto &udp = SharedData::getInstance().udp;

    // Send UDP packet
    udp.beginPacket(m_ip, ORVIBO_UDP_PORT);
    udp.write(ORVIBO_MAGIC, sizeof(ORVIBO_MAGIC));
    udp.write((uint8_t)(tot_len >> 8));
    udp.write((uint8_t)tot_len);
    udp.write((uint8_t)(command >> 8));
    udp.write((uint8_t)command);
    udp.write(m_mac, 6);
    udp.write(MAC_PADDING, sizeof(MAC_PADDING));
    udp.write(payload, length);
    udp.endPacket();
}

void OrviboS20Device::subscribe()
{
    uint8_t payload[12];
    reverse(payload, m_mac, 6);
    memcpy(&payload[6], MAC_PADDING, 6);
    sendCommand(CMD_SUBSCRIBE, payload, sizeof(payload));
}

bool OrviboS20Device::setState(bool state)
{
    uint8_t payload[5];
    memset(payload, 0, 4);
    payload[4] = state;
    sendCommand(CMD_SET_STATE, payload, sizeof(payload));
}

bool OrviboS20Device::getState()
{
    return m_last_state == 1;
}

void OrviboS20Device::checkConnectTimeout()
{
    // The only reason this is needed is because if you unplug a S20 while it is
    // connected the ESP8266WiFi will still keep the device in wifi_softap_get_station_info()
    // list. It seems that a device is only removed from this list if a device sends a graceful
    // WiFi disassociation request
    if (m_connected)
    {
        if (millis() - m_last_rx_time > CONNECTION_TMO_MS)
        {
            updateConnectState(false);
        }
    }
}

void OrviboS20Device::handlePacket(uint16_t command, uint8_t *payload, size_t length)
{
    m_last_rx_time = millis();
    updateConnectState(true);

    switch (command)
    {
    case CMD_STATE_CHANGE:
        if (length == 5)
        {
            int new_state = payload[4];
            if (new_state != m_last_state)
            {
                m_last_state = new_state;
                if (m_state_change_callback)
                {
                    m_state_change_callback(*this, new_state);
                }
            }
        }
        break;
    default:
        break;
    }
}

/***********************************************************************************
 * OrviboS20Class class definition
 ***********************************************************************************/

void OrviboS20Class::checkIfNewDevice(uint8_t *mac)
{
    static int known_orvibo_mac_count = 0;
    static uint8_t known_orvibo_mac_list[MAX_ORVIBO_DEVICES][6];

    if (known_orvibo_mac_count >= MAX_ORVIBO_DEVICES)
    {
        // List is full
        return;
    }

    for (int i = 0; i < known_orvibo_mac_count; i++)
    {
        if (memcmp(known_orvibo_mac_list[i], mac, 6) == 0)
        {
            return;
        }
    }

    memcpy(known_orvibo_mac_list[known_orvibo_mac_count], mac, 6);
    known_orvibo_mac_count++;

    m_found_device_callback(mac);
}

void OrviboS20Class::checkRxPacket()
{
    uint8_t rx_buffer[64];
    auto &udp = SharedData::getInstance().udp;

    if (udp.parsePacket() <= 0)
    {
        return;
    }
    int len = udp.read(rx_buffer, sizeof(rx_buffer));
    if ((len < ORVIBO_HEADER_LEN) || (memcmp(rx_buffer, ORVIBO_MAGIC, sizeof(ORVIBO_MAGIC)) != 0))
    {
        // Invalid packet
        return;
    }
    uint16_t totlen = (rx_buffer[2] << 8) | rx_buffer[3];
    if (totlen != len)
    {
        // Invalid length
        return;
    }

    uint16_t cmd = (rx_buffer[4] << 8) | rx_buffer[5];
    uint8_t *src_mac = &rx_buffer[6];
    uint8_t *payload = &rx_buffer[ORVIBO_HEADER_LEN];
    size_t payload_length = len - ORVIBO_HEADER_LEN;

    if (cmd == CMD_DISCOVER)
    {
        // Probably a bug in Orvibo S20 firmware
        src_mac++;
        payload++;
        payload_length--;
    }

    checkIfNewDevice(src_mac);

    OrviboS20Device *any_mac_dev = nullptr;
    OrviboS20Device *iter = SharedData::getInstance().getFirstDevice();
    while (iter)
    {
        if (memcmp(iter->m_mac, src_mac, 6) == 0)
        {
            iter->m_ip = udp.remoteIP();
            iter->handlePacket(cmd, payload, payload_length);
            break;
        }
        if (iter->m_any_mac && !any_mac_dev)
        {
            any_mac_dev = iter;
        }
        iter = iter->m_next;
    }

    // If the MAC didn't match we check if there are any "any MAC" devices
    if (!iter && any_mac_dev)
    {
        any_mac_dev->m_any_mac = false;
        memcpy(any_mac_dev->m_mac, src_mac, 6);
        any_mac_dev->m_ip = udp.remoteIP();
        any_mac_dev->handlePacket(cmd, payload, payload_length);
    }
}

bool OrviboS20Class::begin()
{
    if (SharedData::getInstance().udp.begin(ORVIBO_UDP_PORT))
    {
        m_started = true;
        return true;
    }
    return false;
}

void OrviboS20Class::stop()
{
    if (m_started)
    {
        m_started = false;
        SharedData::getInstance().udp.stop();
        OrviboS20Device *iter = SharedData::getInstance().getFirstDevice();
    }
}

void OrviboS20Class::handle()
{
    if (m_started)
    {
        static unsigned long s_last_subscribe_time = 0;
        if (millis() - s_last_subscribe_time >= SUBSCRIBE_INTERVAL_MS)
        {
            // Time for subscription
            OrviboS20Device *iter = SharedData::getInstance().getFirstDevice();
            while (iter)
            {
                iter->subscribe();
                iter = iter->m_next;
            }
            s_last_subscribe_time = millis();
        }

        static unsigned long s_last_tmo_check_time = 0;
        if (millis() - s_last_tmo_check_time >= CHECK_TMO_INTERVAL_MS)
        {
            // Check connection timeout
            OrviboS20Device *iter = SharedData::getInstance().getFirstDevice();
            while (iter)
            {
                iter->checkConnectTimeout();
                iter = iter->m_next;
            }
            s_last_tmo_check_time = millis();
        }

        // Check incomming packets
        checkRxPacket();
    }
}
