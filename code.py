# -*- coding: UTF-8 -*-
from aliyunIoT import Device  # The IoT component is used to connect to the Alibaba Cloud IoT platform
import network  # The library where Wi-Fi functionality resides
import ujson  # JSON string parsing library
import utime  # The component where delay API resides
import ssd1306  # Import OLED display driver module
import bme280  # Import BME280 pressure sensor driver module
from machine import Pin, I2C, ADC  # Import ADC for MQ2 sensor
import paho.mqtt.client as mqtt  # Import MQTT client

global_var = 0
button = Pin(0, Pin.IN, Pin.PULL_UP)
led_value = 0
led = Pin(2, Pin.OUT)
led.value(led_value)
fan = Pin(2, Pin.OUT)  # The fan uses pin 2, opposite control of the LED; 0 to turn the fan on. Red (RD) and Black (BK) wires
fan.value(1)

# Add MQ2 smoke sensor and buzzer related configuration
mq2 = ADC(Pin(34))  # MQ2 smoke sensor using analog pin (adjust pin number as needed)
buzzer = Pin(15, Pin.OUT)  # Buzzer using digital pin
# Smoke concentration threshold, trigger buzzer alarm when exceeded
smoke_threshold = 300  # Adjust this value based on actual conditions

# MQTT configuration
MQTT_SERVER = "broker-cn.emqx.io"
MQTT_PORT = 1883
CLIENT_ID = "50d5130975b4c93a4e8dddde521abcd"
PUB_TOPIC = "zxl/home/room/temp"
SUB_TOPIC = "zxl/home/room/switch"

# MQTT client setup
mqtt_client = mqtt.Client(CLIENT_ID)

# Flag for connection to the IoT platform
iot_connected = False
wlan = None

# Triplet information
productKey = "k1b21bN12Fq6q"
deviceName = "IOT_LED1"
deviceSecret = "8e42143c9670299f292613ec2b4269bf042"

# IoT device instance
device = None

# Wi-Fi SSID and Password settings
wifiSsid = "zxl"
wifiPassword = "987654321"

# Define the external interrupt function for the button
def button_irq(button):
    time.sleep_ms(80)
    if button.value() == 1:
        data = {'params': ujson.dumps({"temprature": "26:36"})}
        device.postProps(data)
        led.value(not led.value())
        post_led_value()

# Wait for Wi-Fi to successfully connect to the router
def get_wifi_status():
    global wlan
    wifi_connected = False
    wlan.active(True)  # Activate the interface
    wlan.scan()  # Scan access points
    wlan.connect(wifiSsid, wifiPassword)  # Connect to the specified router

    while True:
        wifi_connected = wlan.isconnected()  # Get Wi-Fi connection status
        if wifi_connected:
            print("Wi-Fi connection status:", wifi_connected)
            oled.fill(0)
            oled.text('WiFi Connected', 0, 0)
            oled.show()
            utime.sleep(2)
            break
        else:
            utime.sleep(0.5)

    ifconfig = wlan.ifconfig()  # Get the IP/netmask/gw/DNS address of the interface
    print(ifconfig)
    utime.sleep(0.5)

# Callback function for successful connection to IoT platform
def on_connect(data):
    global iot_connected
    iot_connected = True

# Set the props event reception function (when the cloud platform sends properties to the device)
def on_props(request):
    global led_value
    print(request)
    try:
        props = eval(request['params'])
        if "LEDSwitch" in props.keys():
            print(props)
            led_value = props["LEDSwitch"]
            if led_value == 1:
                led.value(1)
            else:
                led.value(0)
            post_led_value()
    except Exception as e:
        print("Error:", e)

def post_props(data):
    global device
    if isinstance(data, dict):
        data = {'params': ujson.dumps(data)}
    ret = device.postProps(data)
    return ret

def post_led_value():
    global led_value
    string = {"LEDSwitch": led_value}
    post_props(string)

def connect_lk(productKey, deviceName, deviceSecret):
    global device, iot_connected
    key_info = {
        'region': 'cn-shanghai',
        'productKey': productKey,
        'deviceName': deviceName,
        'deviceSecret': deviceSecret,
        'keepaliveSec': 60
    }
    # Set the triplet information to the IoT component
    device = Device()
    device.on(Device.ON_CONNECT, on_connect)
    device.on(Device.ON_PROPS, on_props)
    device.connect(key_info)

    while not iot_connected:
        utime.sleep(1)
    utime.sleep(2)

# MQTT callbacks
def on_connect_mqtt(client, userdata, flags, rc):
    print("Connected to MQTT broker with result code " + str(rc))
    client.subscribe(SUB_TOPIC)

def on_message_mqtt(client, userdata, msg):
    print(f"Message received on topic {msg.topic}: {msg.payload.decode()}")
    if msg.topic == SUB_TOPIC:
        if msg.payload.decode() == "ON":
            led.value(1)
        elif msg.payload.decode() == "OFF":
            led.value(0)

if __name__ == '__main__':
    # global global_var
    # Construct a software I2C object
    i2c1 = I2C(scl=Pin(22), sda=Pin(21), freq=400000)
    # Construct an OLED display object
    oled = ssd1306.SSD1306_I2C(128, 64, i2c1)
    # Clear the screen
    oled.fill(0)
    # Display text
    oled.text('Hello', 0, 0)
    oled.text('MicroPython', 0, 18)
    oled.text('ESP32', 0, 33)
    oled.text('ESP8266', 0, 48)
    # Refresh the display
    oled.show()

    wlan = network.WLAN(network.STA_IF)
    get_wifi_status()
    connect_lk(productKey, deviceName, deviceSecret)

    # Set MQTT callbacks and connect to broker
    mqtt_client.on_connect = on_connect_mqtt
    mqtt_client.on_message = on_message_mqtt
    mqtt_client.connect(MQTT_SERVER, MQTT_PORT, 60)
    mqtt_client.loop_start()

    i2c = I2C(scl=Pin(16), sda=Pin(17))
    bme = bme280.BME280(i2c=i2c)

    # Read temperature and humidity data
    temperature = bme.values
    # Print temperature and humidity data
    print("BME280 temperature:", temperature[0])
    print("BME280 pressure:", temperature[1])
    print("BME280 humidity:", temperature[2])
    while True:
        # post_led_value()
        temperature = bme.values
        string1 = {"temperature": temperature[0]}
        post_props(string1)
        string2 = {"pressure": temperature[1]}
        post_props(string2)
        string3 = {"humidity": temperature[2]}
        post_props(string3)
        # Read smoke concentration from MQ2 smoke sensor
        smoke_level = mq2.read()
        print("Smoke level:", smoke_level)
        if smoke_level > smoke_threshold:
            print("Smoke detected! Activating buzzer!")
            buzzer.value(1)  # Turn on the buzzer
        else:
            buzzer.value(0)  # Turn off the buzzer
        # Clear screen
        oled.fill(0)
        # Display text
        oled.text('Environment', 0, 0)
        oled.text('Temp: '+temperature[0], 0, 18)
        oled.text('Humi: '+temperature[2], 0, 33)
        oled.text('Pres: '+temperature[1], 0, 48)
        oled.text('Smoke: '+str(smoke_level), 0, 58)

        if float(temperature[0]) >= 28:
            fan.value(0)
        else:
            fan.value(1)

        # Refresh Display
        oled.show()
        # global_var += 1
        # print(global_var)
        # if global_var >= 10:
        # global_var = 0
        # with open('Environment.txt', 'a') as f:
        # f.write(temperature[0]+'  '+temperature[1]+'  '+temperature[2]+'\n')
        # f.close()
        utime.sleep(10)
