<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

## Home Assistant Host Specifications

| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | Ryzen 5560U    |   
| RAM      | 16GB   |   
| USB1   | Nabu Casa ZBT-2   |  Zigbee devices |
| USB2   | Nabu Casa ZWA-2    |  Zwave devices |
| SSD1   | 500GB NVMe    |  Operating Systems |
| UPS|      950VA |




## Frigate Host Specifications

| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | Intel N100   |   
| RAM      | 16GB   |   
| USB1   | Coral TPU   |  Frigate NVR |
| SSD1   | 1000GB NVMe    |  Operating Systems / Storage|
| SSD2   | 2TB Sata SSD |  Storage|
| UPS|      950VA |

## Proxmox Main Host Specifications

| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | Ryzen 3900X    |   |
| RAM      | 128Gb   |   |
| GPU1   | Nvidia P4000     | Jellyfin Transcoding |
| GPU2   | Nvidia RTX 5050     | Nextcloud Transcoding |
| GPU2   | Nvidia RTX 3090        |  Machine Learning |
| HDDs   | 7*8TB       |  Plex Storage|
| SSD1   | 2TB PCIe3 M.2      |  Operating Systems |
| SSD2   | 500GB PCIe3 M.2        |  Incoming Download Cache |
| SSD3   | 1*2TB       |  Nextcloud Storage | 
| UPS|      1500VA |

## Proxmox Cluster Hosts Specifications
This is a cluster containing 2 chassis with 4 nodes each
Each node contains 2 CPUS; The shown config is per CHASSIS.

#### Chassis 1
| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | `CPU E5-2680 v2` * 8    | Total of 80 cores and 160 threads |
| RAM      | 512GB   |  128GB per node  |
| HDDs   | 500GB x 4     | Host Storages|
| UPS|      1500VA | |
#### Chassis 2
| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | `CPU E5-2680 v2` * 8    | Total of 80 cores and 160 threads |
| RAM      | 512GB   |  128GB per node  |
| HDDs   | 500GB x 4     | Host Storages|

## TrueNas Host Specifications

| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | AMD FX-6100    |   
| RAM      | 8Gb   |   
| HDDs   | 4*16TB       |  Plex Storage|
| HDDs   | 1*24TB       |  Plex Storage|
| HDDs   | 1*8TB       |  Plex Storage|
| HDD1   | 300gb     |  TrueNAS OS |
| HDD2   | 2000gb     |  TrueNAS Nextcloud transactional drive |


## Asustor AS6404T NAS

| Part      | Description |  Usage |
| ----------- | ----------- | -----------  |
| CPU      | 	Intel Celeron J3455 |   
| RAM      | 8Gb   |   
| HDDs   | `6TB`\*4 (`raid 1`\*2)   | Backups |
| UPS|      750VA |

## Network Hardware

| Part      | Description | 
| ----------- | ----------- |
| Router      | 	UCG-Ultra |
| Access Points (3)      | U6-Lite   |   
| Access Points (1)      | U6-PRO  |   
| Switches (3)   | USW-Flex-Mini | 
| Switches (2)|   USW-Lite-8-PoE |
| Cable      | Ethernet Cat6   |   

## Security Hardware

| Part      | Description |  Usage | 
| ----------- | ----------- | -- |
| Front door camera  | Reolink RLC-423    | With PTZ functions.  |
| Patio Camera      | Brillcam 4k   |  | 
| Backyard Camera   |  RLC-520A     |  |
| Parking Camera  | RLC-520A  |  Sketchy Parking camera |
| PArking Exit Camera   | RLC-1224A   |  Sketchy Parking exit camera |
| Back Door Camera   | RLC-1212A   |   |