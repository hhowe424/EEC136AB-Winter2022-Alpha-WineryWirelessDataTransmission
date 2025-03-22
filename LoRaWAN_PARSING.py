# Team Alpha - Winery Senior Design Project 2025
# Heather Howe, 02.25.25
# 
# This program parses the gateway router data on packets received
# from the LoRa RAK3172 and outputs the organized data into a txt file.
# It also averages the RSSI and SNR (signal strength variables) over all
# the packets received

import json

# Load the JSON data
with open('End_device_packets (1).json', 'r') as file:
    data = json.load(file)

# Open the file to write the parsed data
with open('Update.txt', 'w') as file:
    avg_rssi = 0
    avg_snr = 0
    packet_count = 0

    # Iterate over each entry in the JSON
    for entry in data:
        # Extract data from the entry
        fPort = entry.get('fPort', 'N/A')

        log_object = entry.get('logObject', {})
        log_payload = log_object.get('payload', {})
        devEUI = log_payload.get('devEUI', 'N/A')
        device_name = log_payload.get('deviceName', 'N/A')
        payload_rxInfo = log_payload.get('rxInfo', [])
        for info in payload_rxInfo:
            rxInfo_loRaSNR = info["loRaSNR"]
            rxInfo_rssi = info["rssi"]

        payload = entry.get('payload', 'N/A')
        ascii_string = ''.join(chr(int(payload[i:i+2], 16)) for i in range(0, len(payload), 2))

        timestamp = entry.get('timestamp', 'N/A')

        entry_type = entry.get('type', 'N/A')
        
        # Write the organized data to the file
        file.write(f"fPort: {fPort}\n")

        file.write(f"Device EUI: {devEUI}\n")
        file.write(f"Device Name: {device_name}\n")
        file.write(f"LoRa SNR: {rxInfo_loRaSNR}\n")
        file.write(f"RSSI: {rxInfo_rssi}\n")

        file.write(f"Payload: {payload}\n")
        file.write(f"Decrypted Payload: {ascii_string}\n")

        file.write(f"Timestamp: {timestamp}\n")
        file.write(f"Type: {entry_type}\n")
        file.write("-" * 40 + "\n")

        avg_rssi += rxInfo_rssi
        avg_snr += rxInfo_loRaSNR
        packet_count += 1

    # print out average RSSI and SNR
    file.write(f"Average LoRa SNR over {packet_count} packets = {round(avg_snr / packet_count, 2)}\n")
    file.write(f"Average RSSI over {packet_count} packets = {round(avg_rssi / packet_count, 2)}\n\n")

    # notify that the parsed data has been stored in a txt
    file.write("Parsed data written to 'Update.txt'\n")
