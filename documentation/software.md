# Software informations.

Here you will find different informations regarding the various systems that I run in order to maintain a smart-home.

## Home Assistant

My Home Assistant installation went through multiple iterations;
From Generic X86 custom install, to a Home Assistant Container and, finally to a VM running the [Home Assistant Operating system](https://www.home-assistant.io/installation/generic-x86-64#install-home-assistant-operating-system) on top of a [Proxmox](https://www.proxmox.com/en/) host.


| Proxmox VM Specs |  |
| :----- | -----------: |
|Cores| 8 | 
|Ram |  16Gb |
|Boot Disk| 210Gb | 
|Swap |  Disabled |
| USB passthrough | [HUSBZB-1 stick](https://www.amazon.ca/-/fr/QuickStick-Combo-HUSBZB-1-Nortek-Cert/dp/B0157GOEA8/ref=sr_1_5?__mk_fr_CA=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=2JP8XY8SQ0LKQ&keywords=nortek&qid=1676432194&s=hi&sprefix=nortek%2Ctools%2C79&sr=1-5)|
|USB Passthrough |[Sonoff Zigbee 3.0 Dongle](https://sonoff.tech/product/gateway-and-sensors/sonoff-zigbee-3-0-usb-dongle-plus-p/) |


## Plex



| Proxmox VM Specs |  |
| :----- | -----------: |
|Cores| 12 | 
|Ram |  20Gb |
|Boot Disk| 150Gb | 
|Storage Disks| 8*8Tb |
|NAS Disks|2*16Tb|
|Swap |  Disabled |
|PCI-E Passthrough|GeForce GTX 1660 Ti| 


## Frigate/ML


| Proxmox VM Specs |  |
| :----- | -----------: |
|Cores| 12 | 
|Ram |  16Gb |
|NAS Disks| 6tb |
|Swap |  Disabled |
|PCI-E Passthrough| GeForce GTX 1070 Ti | 
|PCI-E Passthrough |[Coral TPU](https://coral.ai/products/accelerator/) (PCI-E Passthrough works better than a simple USB Passthrough)|