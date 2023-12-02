<p align="center">
<a href="/homeassistant/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/homeassistant/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/homeassistant/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/homeassistant/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/homeassistant/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/homeassistant/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/homeassistant/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

# Software informations.

Here you will find different informations regarding the various systems that I run in order to maintain a smart-home.

## Home Assistant

My Home Assistant installation went through multiple iterations;
From Generic X86 custom install, to a Home Assistant Container and, finally to a VM running the [Home Assistant Operating system](https://www.home-assistant.io/installation/generic-x86-64#install-home-assistant-operating-system) on top of a [Proxmox](https://www.proxmox.com/en/) host.

#### Some notes;

##### On users
I try to create one "local access only" user per dashboard device as this allows me to know which tablet made what action and what moment. Thus offering more granularity in the logbook than a simple generic account for all dashboards.

<table>
<tr><th> VM Specs </th><th> Installed Software </th></tr>
<tr><td>

| Setting | Value |
| :----- | -----------: |
|Cores| 8 | 
|Ram |  16Gb |
|Boot Disk| 210Gb | 
|Swap |  Disabled |
| USB passthrough | [HUSBZB-1 stick](https://www.amazon.ca/-/fr/QuickStick-Combo-HUSBZB-1-Nortek-Cert/dp/B0157GOEA8/ref=sr_1_5?__mk_fr_CA=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=2JP8XY8SQ0LKQ&keywords=nortek&qid=1676432194&s=hi&sprefix=nortek%2Ctools%2C79&sr=1-5)|
|USB Passthrough |[Sonoff Zigbee 3.0 Dongle](https://sonoff.tech/product/gateway-and-sensors/sonoff-zigbee-3-0-usb-dongle-plus-p/) |

</td><td>

| Software | Usage |
| :----- | -----------: |
|[Home-Assistant OS](https://www.home-assistant.io/installation/generic-x86-64#install-home-assistant-operating-system)| Operating System/Home Management |
|[Node-RED](https://nodered.org/)| Automation Creation  | 
|[HydroQC](https://hydroqc.ca/)| Electricity Mettering  | 
|[HACS](https://hacs.xyz/)| Home-Assistant Community Store  | 
|[Zigbee2Mqtt](https://www.zigbee2mqtt.io/)| Zigbee Controller | 
|[Appdaemon](https://appdaemon.readthedocs.io/en/latest/)| Python apps for Home-Assistant | 
|[MariaDB](https://mariadb.org/)| Database for Home-Assistant | 

</td></tr> </table>


## Plex
As you can imagine, the Plex integration didn't escape the multiple iterations path neither.
What started as a "Dell Powerdge 2900" with 8*1Tb drives, quickly became the monster that it is today. Going from CPU transcoding to a dedicated GPU for the task, this allows me to store most movies and shows in HEVC. This in returns saves space on the drives thanks to the better compression.


#### Some notes;

##### On encoding
Not all systems can transcode multiple streams at once. Storing everything in HEVC/H265 comes with space saving, but at the cost of higher power and ressource usages during transcoding.

*But maxi; what is transcoding?* you may ask. Transcoding is the process by which your Media server will convert the format/encoding of a file before sending it to the client. This usually happens when the client does not support every specification of the original format/encoding.


##### On subtitles
Not all subtitles are created equal, I *HIGHLY* recommend streamlining your subtitles witht he SRT format, which does not requires to be "burned-in" on the image like PSG subtitles. As "burning-in" subtitles requires to decode and re-encode the video file before sending it to the client.

##### On downloads caching
You may wonder why I have a 500SDD sitting empty 99% of the time. Well, there is an actual reason behind this! 
The reason being that continious low-speed writes to a drive will keep the read/write heads always busy during the downloads. This in return will make read access slower, which would increase buffering for the plex Users. 
To mitigate this, all downloads are initially saved to the SSD cache drive, before being copied over the mechanical drives at full speed.
Since adding the SSD cache drive, the IoWait issues on my plex VMs have greatly dimisnished.



<table>
<tr><th> VM Specs </th><th> Installed Software </th></tr>
<tr><td>

| Proxmox VM Specs |  |
| :----- | -----------: |
|Cores| 12 | 
|Ram |  20Gb |
|Boot Disk| 150Gb | 
|Storage Disks| 8*8Tb |
|NAS Disks|2*16Tb|
|Swap |  Disabled |
|PCI-E Passthrough|GeForce GTX 1660 Ti| 
</td><td>

| Software | Usage |
| :----- | -----------: |
|[Plex](https://plex.tv/)| Media Server  | 
|[Jellyfin](https://jellyfin.org/)| Media Server  | 
|[Sonarr](https://sonarr.tv/)| TV Show Automatic Acquisition  | 
|[Radarr](https://radarr.video/)| Movies Automatic Acquisition  | 
|[Lidarr](https://github.com/Lidarr/Lidarr)| Music Automatic Acquisition  | 
|[Bazarr](https://www.bazarr.media/)| Subtitles Automatic Acquisition  | 
|[Prowlarr](https://github.com/Prowlarr/Prowlarr) |  Torrents indexer |
|[Unpackerr](https://github.com/Unpackerr/unpackerr)| Automatic Unzip | 
|[MergerFS](https://github.com/trapexit/mergerfs) |  Fake Raid over JBOD |
|[Tautulli](https://tautulli.com/) | Media Usage Management |
| [Qbittorrent w/ Wireguard](https://hotio.dev/containers/qbittorrent/) |VPNed Torrent Acquisition |
| [NVIDIA Driver Patch](https://github.com/keylase/nvidia-patch) | Allows for >2 transcodes on GPU|
|[Glances](https://nicolargo.github.io/glances/)| Remote Monitoring | 

</td></tr> </table>

## Frigate/Machine Learning

Have you ever wondered what it would be to let your guests in during a party without actually having to go to the door, or click a button? *Well I did!*

This lead me to many trial and errors with "Facebox", "Double-Take" and the likes.
Which can be found in the "Face-reco" flows under /node-red/flows/.

During a whole summer, the door would let people in. But always with hiccups, such as the photo not being taken in time, either too late or too soon. Or the pictures being simply too blurry.

This project is mainly in standby for now, but I should return to it sooner or later. 



<table>
<tr><th> VM Specs </th><th> Installed Software </th></tr>
<tr><td>


| Proxmox VM Specs |  |
| :----- | -----------: |
|Cores| 12 | 
|Ram |  16Gb |
|NAS Disks| 6tb |
|Swap |  Disabled |
|PCI-E Passthrough| GeForce GTX 1070 Ti | 
|PCI-E Passthrough |[Coral TPU](https://coral.ai/products/accelerator/) |
</td><td>

| Software | Usage |
| :----- | -----------: |
|[Frigate](https://frigate.video/)| AI NVR  | 
|[Double-Take](https://github.com/jakowenko/double-take)| Facial Recognition Unified API | 
|[MachineBox](https://machinebox.io/)| Facial Recognition | 
|[Compreface](https://github.com/exadel-inc/CompreFace)| Facial Recognition | 
|[Deepstack](https://github.com/johnolafenwa/DeepStack)| Facial Recognition | 

</td></tr> </table>




## Asustor-6404T NAS

With great power, comes great back-up needs.

A few years ago, I had a dream of losing data from my main server.This lead me to purchase a NAS the day after, with enough storage to backup all my config files.

I've opted to go with two 6Tb drives configured in Raid1 as a main volume. With two drive bays still available for further expansion.

The NAS also runs a few softwares that I consider secondary and unworthy of running on the main rig.



<table>
<tr><th> HW Specs </th><th> Installed Software </th></tr>
<tr><td>



| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | 	Intel Celeron J3455 |   
| RAM      | 8Gb   |   
| HDDs   | 2*6TB (raid 1)  | Backups |
| UPS|      750VA |

</td><td>

| Software | Usage |
| :----- | -----------: |
|[Adguard Home](https://adguard.com/en/adguard-home/overview.html)| Local DNS with Adblock | 
|[PhotoPrism](https://www.photoprism.app/)| Photo Manager | 
|[Nextcloud](https://machinebox.io/)| Private Cloud | 

</td></tr> </table>


