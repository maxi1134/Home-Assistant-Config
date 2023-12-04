<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>





# Some things I've learned the hard way

### Here you will find things that I wish I had known sooner!
##### (These are all opinions, and none of this is officially recommended nor disrecommended by HA)
#### On user accounts
Ideally, you should strive to create one account per device, endpoint, and user.
This will allow you for better debugging in the logboog, where each user's actions are logged.

Using one account for everything becomes problematic once your installation gets large enough.

#### On publicly accessible endpoints
Although everything should be automated as much as possible, there are still unknowns that need to be dealt with.
Who knows what color of lights your friend who just smoked a bunch will want. This is when publicly accessible endpoints become useful.
Be it a tablet, along a very simple interface to allow your intoxicated friends to understand it, or a smart speaker.

Having many dispered endpoint also enables you to quickly override automations

#### On VLANing
Having a large network can soon become overwhelming, this is why I recommend making it even more complex by creating VLANs to segregate your network. 
I personally recommend having one network for IoT devices, one for media players, one for cameras , one for your Personal Usage ( Your LAN ), and one for Guests (That can ideally access the media players).

This will allow for more granular control of what can see what. Allowing more control over your network.

#### On standardization
No one likes to do work twice, this is where standardization comes in handy.
By always following the same naming convention you will be able to reuse and recycle your automations (Node-red or HA) by simply changing parts of the called entity_id. 
This will make for an easier refactoring and allow for better usage of subflows in node-red.

[More in-depth info on how I use this in Node-red here](/node-red)

#### On updating
Although some people chose to do periodic updates, I chose to update as soon as an update is released.
This allows for the obtention of the latest bugfixes, you can even go a step further and use beta-releases.

He who lives dangerously, can sometimes be rewarded. ᵃⁿᵈ ʷᵉ ⁿᵉᵉᵈ ᵇᵉᵗᵃ ᵗᵉˢᵗᵉʳˢ


#### On the cloud
Avoid that thing like the pest, it has latency, it will break if your internet is down, and it will one day close. 

You should strive to keep everything local, this will improve your life.
 
