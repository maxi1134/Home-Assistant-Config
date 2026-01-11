
## 🚀 Quick Links & Navigation

<p align="center">
  <a href="documentation/automations_list_v2.md"><img src="https://img.shields.io/badge/Automations%20List-purple" alt="Automations List"></a>
  <a href="documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="Hardware Specs"></a> 
  <a href="documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="Zigbee Devices"></a>
  <a href=".storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Interfaces"></a>
  <a href="documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="Localization"></a>
  <a href="documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Software Usage"></a>
  <a href="documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Networking"></a>
  <a href="documentation/general_thoughts.md"><img src="https://img.shields.io/badge/My%20Thoughts-red" alt="Thoughts"></a>
</p>

---
## Project Overview

Welcome to my smart home configuration repository!  
My journey into home automation began in 2018 after a couple of… unfortunate incidents with lost keys.

This, forcing my former roommate to come to my rescue, twice , led him to buy a [Schlage Zwave Lock](https://www.schlage.com/en/home/smart-locks/connect-zwave.html) coupled with a [Samsung SmartThings hub (2018 version)](https://www.amazon.ca/Samsung-SmartThings-Smart-Home-Hub/dp/B010NZV0GE).

 That sparked an obsession, leading to a full transformation: multiple hubs, tons of devices, and a constant hunt for optimal automation.

Once that was acquired, there was no going back: quickly connected to Smart-things hub, added some Philips hues with their hub, sprinkled in some Xiaomi sensors along yet another hub, tinkered with custom Zigbee integrations… and hit the ceiling of hub chaos. 3 hubs (Samsung ,Xiaomi and Philips), overlapping frequencies, and rising frustration.

Eventually, I consolidated everything to a [HUSBZB-1 stick](https://www.amazon.ca/-/fr/QuickStick-Combo-HUSBZB-1-Nortek-Cert/dp/B0157GOEA8/ref=sr_1_5?__mk_fr_CA=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=2JP8XY8SQ0LKQ&keywords=nortek&qid=1676432194&s=hi&sprefix=nortek%2Ctools%2C79&sr=1-5) running [ZHA](https://www.home-assistant.io/integrations/zha/) and then moved to [Z2M](https://github.com/Koenkk/zigbee2mqtt) and a [ZBT-2](https://www.home-assistant.io/connect/zbt-2/).
Along the timeline, a [ZWA-2](https://www.home-assistant.io/connect/zwa-2/) was also added for some locks and heavy duty smart locks.

No more Zigbee drops, no more Z-Wave issues — just smooth, unified device management.


*This repo includes everything from core configs to documentation, hardware/software details, and personal lessons learned. Whether you want to replicate my setup, troubleshoot your own, or just browse home automation stories, you’ll find something useful here!*

---


## 🔎 Documentation Highlights

| Section | Description |
| ------- | ----------- |
| [Automations List v2](documentation/automations_list_v2.md) / [v3](documentation/automations_list_v3_sorted.md) | Detailed descriptions of all automations — triggers, routines, special modes, security, etc. |
| [Hardware](documentation/hardware.md) | Host server specs, expansion devices, Z-Wave/Zigbee sticks, network gear… |
| [Software](documentation/software.md) | OSes, Home Assistant config, tooling, add-ons, VM specs |
| [WiFi & Networking](documentation/wifi.md) | VLANs, network architecture, buy-again notes, access strategies |
| [Zigbee Devices](documentation/zigbee.md) | All my Zigbee devices, types, and placement |
| [Indoor Localization](documentation/indoor_localization.md) | Presence via ESPresense, floorplans, and proximity tracking |
| [Voice Assistant/LLM (Guide)](documentation/guides/voice_assistance_guide.md) | How to install and use voice assistant stack (Ollama, Open-WebUI, Home-LLM, ESPHOME) |
| [General Thoughts](documentation/general_thoughts.md) | Lessons learned, network/POS planning, VLANs, and rants |
| [Floorplan Creator for ESPresense](espresense/floorplan_creator/Readme.md) | A GUI tool and workflow for producing floorplans for ESP-based indoor localization |
| [Work-in-Progress List](wip%20list.md) | Experiments, project backlog, and automation ideas |

---
### A Few Screenshots;

<h5 align="center">Wall Displays</h5>
<p align="center">
  <img src="/assets/Tablet_A8/musicdemo.gif" width="43%" />
  <img src="/assets/Tablet_S7FE/framed_light_panel_lowres.gif" width="38%" />
</p>




<h5 align="center">An interactive plan with projected shadows and lights</h5>
<p align="center">
  <img src="/assets/interactive_plan/color_doors_demo.gif" width="90%" />
</p>

---

## 📝 Contributing & Feedback

I love to connect with others working on home automation. Feel free to fork, open issues, or reach out with suggestions or questions!

---

*For the most current state of the repo and all detailed configurations, always browse the [full repository on GitHub](https://github.com/maxi1134/Home-Assistant-Config).*



<p align="center">
  <strong>Last Updated:</strong> January 2026<br>
  <strong>Status:</strong> Active & Evolving<br>
  <a href="https://github.com/maxi1134/Home-Assistant-Config">View on GitHub</a> • 
  <a href="https://www.buymeacoffee.com/maxi1134">Buy Me A Coffee</a>
</p>