<p align="center">
<a href="documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

Here, I will compile a list of my Zigbee devices and my thought on them.

These items are all linked to Z2M using the Z2M EDGE addon on Home-Assistant!

## Buy again notes:

| Symbol | Means |
| :---: | --- |
| :heavy_check_mark: | Yes |
| :pause_button: | Waiting on the next generation of this. |
| :grey_question: | Maybe - it's not bad, but I'm looking for something better |
| :thumbsdown: | Unlikely, it's not terrible, but it has some issues |
| :small_red_triangle: | There's a newer version that's better |
| :no_entry: | Discontinued |
| :warning: | Won't buy again |

# Zigbee devices

| Model | Device Type | Buy Again? | Some Notes |  Quantity In Use     |
| ----- | ----------- | :--------: | ---------- | :---: |
|[Sonoff Zigbee 3.0 Dongle](https://sonoff.tech/product/gateway-and-sensors/sonoff-zigbee-3-0-usb-dongle-plus-p/) | Coordinator | :heavy_check_mark: | Don't forget to update the Firmware!| 1|
|[Ikea Symfonisk Dial](https://www.zigbee2mqtt.io/devices/E1744.html#ikea-e1744)| Remote | :no_entry: | It sometimes spam commands until touched again. | 10 |
|[Philips Hue Bulb](https://www.zigbee2mqtt.io/devices/9290012573A.html#philips-9290012573a)| Bulb | :heavy_check_mark: | Great color render! | 6 |
|[Philips Hue Bulb](https://www.zigbee2mqtt.io/devices/9290022166.html#philips-9290022166)| Bulb | :heavy_check_mark: | Great color render! | 12 |
|[Philips Hue Bulb](https://www.zigbee2mqtt.io/devices/9290023351.html#philips-9290023351)| Bulb | :heavy_check_mark: | Warm White only | 2 |
|[Philips Hue Bulb](https://www.zigbee2mqtt.io/devices/9290024717.html#philips-9290024717)| Bulb | :heavy_check_mark: | Great color render! | 1 |
|[Xiaomi Motion Sensor](https://www.zigbee2mqtt.io/devices/RTCGQ11LM.html#xiaomi-rtcgq11lm)| Motion Sensor | :small_red_triangle: | Overall Solid Motion sensor | 5 |
|[Xiaomi Motion Sensor](https://www.zigbee2mqtt.io/devices/RTCGQ01LM.html#xiaomi-rtcgq01lm)| Motion Sensor | :small_red_triangle: | Overall Solid Motion sensor | 10 |
|[Ikea Motion Sensor](https://www.zigbee2mqtt.io/devices/E1525_E1745.html#ikea-e1525%252Fe1745)| Motion Sensor | :heavy_check_mark: | Cheaper than the competition and reliable | 10 |
|[Xiaomi FP1 MMwave Sensor](https://www.zigbee2mqtt.io/devices/RTCZCGQ11LM.html#xiaomi-rtczcgq11lm)| Microwave Sensor | :heavy_check_mark: | Regions are now supported! | 1 |
|[Xiaomi Thermometer Sensor](https://www.zigbee2mqtt.io/devices/WSDCGQ11LM.html#xiaomi-wsdcgq11lm)| Temperature/Humidity Sensor | :heavy_check_mark: | Work well under their rated temprature ( tested down to -35 ) | 6 |
|[Xiaomi Water Detector](https://www.zigbee2mqtt.io/devices/WSDCGQ11LM.html#xiaomi-wsdcgq11lmhttps://www.zigbee2mqtt.io/devices/SJCGQ11LM.html#xiaomi-sjcgq11lm)| Water Detector | :heavy_check_mark: | Saved me from many water incidents | 1 |
|[Xiaomi Button](https://www.zigbee2mqtt.io/devices/WXKG01LM.html#xiaomi-wxkg01lm)| Button | :heavy_check_mark: | Is used as a doorbell | 1 |
|[Ikea Single Button](https://www.zigbee2mqtt.io/devices/E1812.html#ikea-e1812)| Button | :heavy_check_mark: | This is used to enable "sleep mode" in certain rooms with a double tap | 2 |
|[Ikea Blind Button](https://www.zigbee2mqtt.io/devices/E1766.html#ikea-e1766)| Button | :heavy_check_mark: | Used to control the blinds | 2 |
|[Ikea Blinds](https://www.zigbee2mqtt.io/devices/E1757.html#ikea-e1757)| Blinds | :grey_question: | They are a pain to connect to Z2M but work perfectly once connected | 2 |
|[Xiaomi Door Sensor](https://www.zigbee2mqtt.io/devices/MCCGQ01LM.html#xiaomi-mccgq01lm)| Door Sensor | :heavy_check_mark: | reliable door sensor | 1 |
|[Xiaomi Door Sensor](https://www.zigbee2mqtt.io/devices/MCCGQ11LM.html#xiaomi-mccgq11lm)| Door Sensor | :heavy_check_mark: | reliable door sensor | 7 |
|[Xiaomi Magic Cube](https://www.zigbee2mqtt.io/devices/MFKZQ01LM.html#xiaomi-mfkzq01lm)| Remote | :warning: | Very cool, but very gimmicky | 1 |
|[Sonoff Smart Plug](https://www.zigbee2mqtt.io/devices/S31ZB.html#sonoff-s31zb)| Smart Plug | :heavy_check_mark: | Reliable, but no consumption sensor | 9 |
|[Schlague Century Lock](https://www.zigbee2mqtt.io/devices/BE468.html#schlage-be468)| Lock | :no_entry: | Reliable at -35c since 4 winters | 1 |
|[Quotra Circle Lamp](https://www.zigbee2mqtt.io/devices/QV-RGBCCT.html#quotra-vision-qv-rgbcct)| Smart Lamp | :no_entry: | Very neat, but has a physical button that can over-ride smart controls | 2 |