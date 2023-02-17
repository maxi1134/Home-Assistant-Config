# Software informations.

Here you will find different informations regarding the various systems that I run in order to maintain a smart-home.

## Home Assistant

My Home Assistant installation went through multiple iterations;
From Generic X86 custom install, to a Home Assistant Container and, finally to a VM running the [Home Assistant Operating system](https://www.home-assistant.io/installation/generic-x86-64#install-home-assistant-operating-system) on top of a [Proxmox](https://www.proxmox.com/en/) host.

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

## Frigate/ML

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


