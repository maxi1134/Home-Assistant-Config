<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

In this guide I will detail the steps I've taken to get [Home-LLM](https://github.com/acon96/home-llm) and [Local-AI](https://github.com/mudler/LocalAI/) working together in conjunction with [Home-Assistant](https://www.home-assistant.io/)!

- 1: You will first need to [follow this guide to install Home-LLM](https://github.com/acon96/home-llm/blob/develop/docs/Setup.md)into your [Home-Assistant](https://www.home-assistant.io/) installation.


     If you simply want to install the [Home-LLM](https://github.com/acon96/home-llm) component through HACS,  you  can press on this button:




     [![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?category=Integration&repository=home-llm&owner=acon96) 

- 2: Add `Home LLM Conversation` integration to HA. 




    - 1: Access the `Settings` page.
    - 2: Click on `Devices & services`.
    - 3: Click on `+ ADD INTEGRATION` on the lower-right part of the screen.
    - 4: Type and then select `Local LLM Conversation`.
    - 5: Select the `Generic OpenAI Compatible API`.
    - 6: Enter the hostname or IP Address of your LocalAI host.
    - 7: Enter the used port (Default is `8080`).
    - 8: Enter `mistral-7b-instruct-v0.3` as the `Model Name*`
      - Leave `API Key` empty
      - Do not check `Use HTTPS`
      - leave `API Path*` as `/v1` 
    - 9: Press `Next`
    - 10: Select `Assist` under `Selected LLM API`
    - 11: Make sure the `Prompt Format*` is set to `Mistral`
    - 12: Make sure `Enable in context learning (ICL) examples` is checked.
    - 13: Press `Sumbit`
    - 14: Press `Finish`

 <center> A video of the process! </center>
<p align="middle">
  <img src="/config/assets/home_llm_guide/home_llm_installation_video.gif" width="50%" />
<p>


- 3:  Configure the Voice assistant.
  - 1: Access the `Settings` page.
  - 2: Click on `Voice assistants`.
  - 3: Click on `+ ADD ASSISTANT`.
  - 4: Name the Assistant `HomeLLM`.
  - 5: Select `English` as the Language.
  - 6: Set the `Conversation agent` to the newly created `LLM Model 'mistral-7b-instruct-v0.3' (remote)`.
  - 7: Set your `Speech-to-text` `Wake word`, and `Text-to-speech` to the ones you use. Leave to `None` if you don't have any. 
  - 8: Click `Create`
  
- 4: Select the newly created voice assistant as the default one.
    - While remaining on the `Voice assistants` page click on the newly create assistant, and press the start at the top-right corner.


There you go! Your Assistant should now be working with Local-AI through Home-LLM!
 - Make sure that the entities you want to control are exposted to Assist within Home-Assistant!