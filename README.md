# EEC136AB-Winter2022-Alpha-WineryWirelessDataTransmission
A custom PCB board with an RAK3172 microcontroller to be integrated into the Integrated Fermentation Control System (IFCS) at the UC Davis Winery.

# Team Alpha
Members: Marcos Alvear, Jes Vincent Ballesteros, Angelita Daza-Mendez, Heather Howe, Clarissa Lei, and Michelle Lu

# Contents:
1) LoRaWAN_OTAA_CONNECTION
- Arduino code for LoRaWAN connection between the RAK3172 and RAK gateway with Over-The-Air Activation (OTAA). After establishing a connection user inputted data (fake data) is packaged into the payload and sent in an uplink message to the gateway every 10 seconds. In this code, the data includes temperature, density, and pressure values.
2) LORAWAN_PARSING
- A Python script that parses the packets downloaded from the Gateway that were received from the RAK3172. It takes a .json file that contains a large pythons dictionary of values and extracts the average Received Signal Strength Indicator (RSSI) and Signal Noise Ratio (SNR) values over all the packets received as well as the payload in hex, the payload decoded into ASCII, and others. This code was used to parse data received when testing the signal strength of the RAK3172 in different locations in the Winery
3) ESP32
- Arduino file that contains the Team Pinots (previous winery senior design project) code for extracting sensor data from the sensor box using I2C communication which then is sent to a cloud server using the MQTT protocol. The part of this code that parsed the bits of the read data over the I2C connection and decoded them into char arrays of the Temperature and other sensor data was used and merged with our new Arduino code. However, we were unable to test the integration because of the hardware error in our I2C pins but once working the char arrays of data would be packaged into uplink messages and easily sent over the LoRaWAN OTAA connection rather than with the MQTT protocol. 
