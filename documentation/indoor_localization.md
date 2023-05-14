<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

# Indoor Localisation Detection

Since I am always trying new things, I've explored indoor-localisation using 7 ESP32s and a [Minew Bracelet](https://www.minew.com/products/b7-wristband-beacon.html) attached to my ankle.  Alternatively, if the ankle monitor is not detected while I am still considered at home, my phone and watch BLE signals are checked to determine my current location.  
[Here you can find the code to that sensor](https://github.com/maxi1134/Home-Assistant-Config/blob/ff6df00148dd92bebe839b90e1f71abd99405a4d/includes/template.yaml#L75)  
  
This allows for a high level of precisions when it comes to knowing in which room I am located.  
  
  
As for the the software used for this, it is [free and open source](https://espresense.com/).

### The ESP32s are located as follow in the appartment, offering good detection in "main" rooms, and being less precise with others. <br>
<p align="middle">
  <img src="/assets/misc/esp32map.png" width="90%" />
<p>