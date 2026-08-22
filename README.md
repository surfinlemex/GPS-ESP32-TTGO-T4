# GPS for ESP32 TTGO T4 v1.3 from LillyGo 

![image](https://user-images.githubusercontent.com/31878095/111103954-c35b2700-8579-11eb-8a67-e59c4c10139d.png)
![image](https://user-images.githubusercontent.com/31878095/116801395-6b797f00-ab2b-11eb-824c-a04a0c49f790.png)
![image](https://user-images.githubusercontent.com/31878095/117013195-755cd700-ad11-11eb-8207-c819f995bc89.png)

Specifications:

    ESP32 chip (240Mhz dual core processor)
    Flash memory: 4MB
    PSRAM: 8MB
    Built-in Wi-Fi
    Built-in Bluetooth
    Built-in 2.2 inch TFT display (ILI9341 driver)
    Built-in power management chip (IP5306) with Li-ion/Li-Po battery charging circuit (charge up to ~2A)
    Built-in micro SD card connection
    I2C connector: 5p JST-PH
    I2C cable included
    Battery connector: 2p Molex Picoblade
    Battery cable included
    Dimensions PCB: 65.9x40.8mm

## MGMT-UDP-Server connection

The firmware sends a `STATUS` UDP request every 10 seconds after Wi-Fi connects
and logs the server response. Button presses are sent as `BUTTON1_PRESSED`,
`BUTTON2_PRESSED`, or `BUTTON3_PRESSED`, and the server acknowledges them as
`ACK:<event>`. It resolves `hq-mmd-3.xelox.org` through DNS and uses UDP port
`5000`.

Start the server on the computer's LAN interface:

```bash
./build/MGMT-UDP-Server 5000
```

The expected response in the ESP32 log is `STATUS:UP`.

ASCII for Fonts:

![image](https://user-images.githubusercontent.com/31878095/113403478-8b047700-93c8-11eb-945d-d2453c8c6a79.png)


