<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

# Network Infrastructure




## Network Segregation:

Running a homelab is not always easy, especially when comes time to route, segregate and firewall multiple networks, each with a very specific use. I ended up with with 6 different networks:
1) Private LAN
   - This network is meant for my usage only. This is where all the main machines are connected, Servers and Main PC. No guests are allowed on this network.
2) Friends LAN
   - This is the "Hotspot" network that I offer to my guests during events. They cannot interact with other networks except for the "Google Cast" network, to allow them to cast to the speakers and chromecasts.
3) IoT LAN
   - This network is reserved for the IoT devices, a few ports are allowed in and out to the "Home-Assistant" Virtual Machine, such as LIFX ports and MQTT ports.
4) Neighbors Wifi
   - This network is reserved for the use of the upstairs neighbors.
5) Game Server LAN
   - This network is higly restricted as the machine living on it is directly exposed to the internet on many ports to allow for game servers such as; Minecraft, Valheim, Conan, ARK, 7 days to die, Project Zomboid. Only a few ports are allowed within the lan. I.e: Inbound port 22 from my PC.
6) Cameras LAN
   - This network is only accessible from the "Frigate" machine. Cameras can only see that machine and cannot ping home.

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

# WiFi Infrastructure
|  Model      | Device Type |Buy Again? |Some Notes |  Quantity In Use     |
| ----------- | ----------- | :----------: |  ----------- | ----------- |
| 	[Cloud Gateway Ultra](https://ca.store.ui.com/ca/en/collections/cloud-gateway-ultra/products/ucg-ultra) | Router  |:heavy_check_mark:  | Decent router, good network options. | 1 |
| [U6-Lite](https://store.ui.com/collections/unifi-network-wireless/products/u6-lite-us) | Access Points | :grey_question:  | I can only reach 200Mbps with these Antennas in Wifi6 |   3 |
| [U6-PRO](https://store.ui.com/collections/unifi-network-wireless/products/unifi-ap6-professional) | Access Points | :heavy_check_mark: |More decent speeds at 300Mbps|   1 |
| [USW-Flex-Mini](https://store.ui.com/collections/unifi-network-switching/products/usw-flex-mini) | Switches |:heavy_check_mark:|Good little switch, not too pricey| 3 |
|   [USW-Lite-8-PoE](https://store.ui.com/collections/unifi-network-switching/products/unifi-switch-lite-8-poe)| Switches|:heavy_check_mark: |Also decent POE switch for edge of the network.| 2 |
| Ethernet Cat6 | Cable    |:heavy_check_mark:| |   Lots |


# WiFi devices

| Model | Device Type | Buy Again? | Some Notes |  Quantity In Use     |
| ----- | ----------- | :--------: | ---------- | :---: |
|[Wiim Pro](https://wiimhome.com/WiiMPro/Overview) | Smart Speaker Receiver | :heavy_check_mark: | Very decent integration into Home-Assistant through the cast and Airplay/SlimProto integrations| 7 | 
| Chromecast | Smart TV Dongle | :small_red_triangle: | Very decent integration into Home-Assistant| 2 | 
|[Chromecast Ultra](https://www.bestbuy.com/site/google-chromecast-ultra-4k-streaming-media-player-black/5578628.p?skuId=5578628) | Smart TV Dongle | :small_red_triangle: | Very decent integration into Home-Assistant| 3 | 
|[Google Nest Hub](https://store.google.com/us/product/nest_hub_2nd_gen?hl=en-US) | Smart Hub | :warning: | You "can" cast your Home-Assistant interface on them which is what I do for both Hotbox dashboards| 2 | 
|[LIFX Tiles](https://support.lifx.com/lifx-tile-H1b_fuiLu)| Smart Lights | :no_entry: | Very pretty, but two sets out of four broke (PSU issues) | 2 |
|[LIFX Nightvision BR30](https://www.lifx.com/products/lifx-color-br30-nightvision-edition-1pk)| Smart Bulb | :thumbsdown: | Very bright, but has wifi connectivity issues | 1 |
|[LIFX Color E26 ](https://www.lifx.com/products/lifx-color-1pk)| Smart Bulb | :thumbsdown: | Very bright, but has wifi connectivity issues | 1 |
|[Twinkly Strings ](https://twinkly.com/en-ca/products/strings-multicolor?variant=42970659061982)| Smart Lights | :heavy_check_mark: | Very bright and adressable | 3 |
|[Roborock Qrevo Curv S5X ](https://ca.roborock.com/products/roborock-qrevo-curv-s5x)| RobotVac | :heavy_check_mark: | Changed my life | 1 |
|[ESP32](https://www.espressif.com/en/products/modules/esp32)| Micro-Controller | :heavy_check_mark:  | Used as BLE trackers with the 'Bermuda' integration | 17 |
|[Samsung Galaxy Tab S7 FE ](https://www.samsung.com/ca/support/model/SM-T733NZKAXAC/)| Tablet| :heavy_check_mark:  | Used as Hallway dashboard | 1 |
|[Samsung Galaxy Tab A7 Lite ](https://www.samsung.com/us/business/support/owners/product/galaxy-tab-a-7-lite-wi-fi/)| Tablet | :heavy_check_mark:  | Used as lounge desk dashboard | 1 
|[Samsung Galaxy Tab E](https://www.samsung.com/ca/support/model/SM-T377WZKAXAC/)| Tablet | :heavy_check_mark:  | Used as Closet and Kitchen  dashboards | 2
|[Samsung Galaxy Tab A8](https://www.samsung.com/ca/tablets/galaxy-tab-a/galaxy-tab-a8-wifi-dark-gray-64gb-sm-x200nzaexac/)| Tablet | :heavy_check_mark:  | Used as workshop dashboard | 1 
|[Samsung Galaxy S6 Lite ](https://www.samsung.com/ca/tablets/galaxy-tab-s/galaxy-tab-s6-lite-gray-64gb-sm-p620nzaaxac/)| Tablet | :heavy_check_mark:  | Used as kitchen dashboard | 1 
|[Aqara FP2 MMWave sensor](https://www.amazon.ca/Aqara-Positioning-Multi-Person-Detection-Assistant/dp/B0BXWZMQJ3)| Presence Sensor | :heavy_check_mark:  | Used for their zoning capacities in a room | 2
|[Everything Presence Lite ](https://shop.everythingsmart.io/products/everything-presence-lite)| Presence Sensor | :heavy_check_mark:  | Used for their zoning capacities in a room | 6
|[ESP32-S3-BOX-3 ](https://www.espressif.com/en/dev-board/esp32-s3-box-3-en)| Voice Assistant| :heavy_check_mark:  | Custom Voice Assistant EndPoints | 6
|[ Home Assistant Voice Preview Edition ](https://www.home-assistant.io/voice-pe/)| Voice Assistant | :heavy_check_mark:  | Home Assistant Voice Assistant EndPoints | 2