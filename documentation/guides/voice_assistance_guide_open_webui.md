<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

_____

# In this guide I will explain how I've setup my Local voice assistant and satellites!  
A few softwares will be used in this guide.

[HACS](https://hacs.xyz/) for easy installation of the other tools on Home Assistant.  
[Ollama](https://hacs.xyz/) for the backend of the LLM.  
[Open-Webui](https://github.com/open-webui/open-webui) midleware to add functions to our Ollama llm.  
[Home-LLM](https://github.com/acon96/home-llm) to connect our Open-Webui instance to Home-assistant.  
[HA-Fallback-Conversation](https://github.com/m50/ha-fallback-conversation) to allow HA to use both the baked-in intent as well as the LLM as a fallback if no intent is found.  
[ESPHOME](https://esphome.io/components/voice_assistant.html) for the ESP32 sattelites.  


# Step 1) Installing Ollama

Ollama can be installed using a one-line: `curl -fsSL https://ollama.com/install.sh | sh`
You can also follow the manual installation steps on [Github](https://github.com/ollama/ollama/blob/main/docs/linux.md)

_____

# Step 1.a) Downloading the LLM model

Once Ollama if installed, you should be able to run this command to pull our Llama3.1 trained model: `ollama pull finalend/llama-3.1-storm:8b-q8_0`

Ollama should now be running on your machine learning host.

_____

# Step 2) Installing Open WebUI

We will follow the `Ollama` installation by installing `Open WebUI` on our machine learning host.   
I recommend using a good machine with access to a GPU with at least 12 GB of Vram. As The models can take a lot of ram depending on the Quantization and parameter amount.

**Here an example of the VRAM usage for  `Open-Webui` and `Ollama` with the `finalend/llama-3.1-storm:8b-q8_0` model loaded :**
```
+-----------------------------------------------------------------------------------------+
| NVIDIA-SMI 555.42.02              Driver Version: 555.42.02      CUDA Version: 12.5     |
|-----------------------------------------+------------------------+----------------------+
| GPU  Name                 Persistence-M | Bus-Id          Disp.A | Volatile Uncorr. ECC |
| Fan  Temp   Perf          Pwr:Usage/Cap |           Memory-Usage | GPU-Util  Compute M. |
|                                         |                        |               MIG M. |
|=========================================+========================+======================|
|   0  NVIDIA GeForce RTX 3090        Off |   00000000:01:00.0 Off |                  N/A |
| 63%   45C    P8             18W /  370W |   12191MiB /  24576MiB |      0%      Default |
|                                         |                        |                  N/A |
+-----------------------------------------+------------------------+----------------------+

+-----------------------------------------------------------------------------------------+
| Processes:                                                                              |
|  GPU   GI   CI        PID   Type   Process name                              GPU Memory |
|        ID   ID                                                               Usage      |
|=========================================================================================|
|    0   N/A  N/A      1722      C   python3                                      1850MiB |
|    0   N/A  N/A    606593      C   ...unners/cuda_v11/ollama_llama_server       9964MiB |
|    0   N/A  N/A    607966      C   /usr/local/bin/python                         358MiB |
+-----------------------------------------------------------------------------------------+


```

I've chosen the Docker-Compose method for my Open-Webui installation, this allows for easy management and easier upgrades when new releases are available.  
This method allows us to quickly create a container running Open-Webui on our machine.  

In order to do so, create a file called `docker-compose.yaml` with the following content:

**Be aware that this Docker configuration requires a GPU**
```
name: open-webui
services:
    open-webui:
        ports:
            - 3000:8080
        deploy:
            resources:
                reservations:
                    devices:
                        - driver: nvidia
                          count: all
                          capabilities:
                              - gpu
        extra_hosts:
            - host.docker.internal:host-gateway
        volumes:
            - "HOST PATH TO THIS FOLDER":/app/backend/data 
#    example: /home/maxi1134/docker/open-webui:/home/maxi1134/docker/open-webui
        environment:
            - GLOBAL_LOG_LEVEL=0
            - WEBUI_AUTH=false
        container_name: open-webui
        restart: always
        image: ghcr.io/open-webui/open-webui:cuda

```


Once that is done simply use `sudo docker compose up -d` and your Open Web-UI instance should now be available at: 
`http://{{host}}:3000/`
_____

# Step 2.a) Testing the LLM

We will now need to test the LLM itself.
To do so; Access `http://{{host}}:3000/` through a browser, and select `finalend/llama-3.1-storm:8b-q8_0` at the top iof it is not done. You should then be able to ask the LLM a question to test it.

<p align="center">A picture of the answer! </p>
<p align="middle">
<img src="/assets/open-webui_guide/test_llm.png" width="50%" />
</p>

___

# Step 3) Installing Home-LLM




- 1: You will first need to install the Home-LLM integration to Home-Assistant   
    Thankfuly, there is a neat button to do that easely on [their repo](https://github.com/acon96/home-llm)!



     [![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?category=Integration&repository=home-llm&owner=acon96) 

- 2: Restart `Home Assistant`

- 3: You will then need to add the  `Home LLM Conversation` integration to Home-Assistant in order to connect Open WebUI to it.
    - 1: Access the `Settings` page.
    - 2: Click on `Devices & services`.
    - 3: Click on `+ ADD INTEGRATION` on the lower-right part of the screen.
    - 4: Type and then select `Local LLM Conversation`.
    - 5: Select the `Ollama API`.
    - 6: Enter the hostname or IP Address of your Open WebUI host.
    - 7: Enter the used port (You must append it with `/ollama/` such as `3000/ollama`).
    - 8: Enter `finalend/llama-3.1-storm:8b-q8_0` as the `Model Name*`
      - Enter your [Open Webui API Key](https://docs.openwebui.com/api/) under `API key`
      - Do not check `Use HTTPS`
      - leave `API Path*` as `/v1` 
    - 9: Press `Next`
    - 10: Select `Assist` under `Selected LLM API`
    - 11: Make sure the `Prompt Format*` is set to `Llama`
    - 12: Make sure `Enable in context learning (ICL) examples` is checked.
    - 13: Press `Sumbit`
    - 14: Press `Finish`

<p align="center">A gif of the process! </p>
<p align="middle">
  <img src="/assets/open-webui_guide/demo_process.gif" width="75%" />
<p>

_____

# Step 4) Installing [HA-Fallback-Conversation](https://github.com/m50/ha-fallback-conversation)


- 1:  Integrate Fallback Conversation to Home-Assistant
  - 1: Access the `HACS` page.
  - 2: Search for `Fallback`
  - 3: Click on `fallback_conversation`.
  - 4: Click on `Download` and install the integration
  - 5: Restart `Home Assistant` for the integration to be detected.
  - 6: Access the `Settings` page.
  - 7: Click on `Devices & services`.
  - 8: Click on `+ ADD INTEGRATION` on the lower-right part of the screen.
  - 8: Search for `Fallback`
  - 9: Click on `Fallback Conversation Agent`.
  - 10 Set the debug level at `Some Debug` for now. (Change it to `No debug` once everything is working.)
  - 11: Click `Sumbit`
  
- 2: Configure the Voice assistant within Home-assistant to use the newly added model through the `Fallback Conversation Agent`.
  - 1: Access the `Settings` page.
  - 2: Click on `Devices & services`.
  - 3: Click on `Fallback Conversation Agent`.
  - 4: Click on `CONFIGURE`.
  - 5: Select `Home assistant` as the `Primary Conversation Agent`.
  - 6: Select `LLM Model 'finalend/llama-3.1-storm:8b-q8_0' (remote)` as the `Falback conversation Agent`.


_____

# Step 5) Selecting the right agent in the Voice assistant settings.


 - 1: Access the `Settings` page.
 - 2: Click on `Voice assistants` page.
 - 3: Click on `Add Assistant`.
 - 4: Set the fields as wanted except for `Conversation Agent`.
 - 5: Select `Fallback Conversation Agent` as the `Conversation agent`.

_____

# Step 6) Setting up ESPHOME Voice assistant satellites.

The voice assistant are set on ESPHOME using ESP32-S3-Boxes.

You should be able to upload the following YAML to them through ESPhome and be able to add them to Home-assistant after that through the "Devices menu".

 - 1: Access the `ESPHOME` page.
 - 2: Click on `New Device`.
 - 3: Chose a name for this device.
 - 3: Click on `Skip this step`.
 - 4: Select `ESP32-S3`
 - 5: Click `Skip` On the `configuration created!` page.
 - 6: Click `Edit` on the newly created esphome entry.
 - 7: Copy and paste the following code to replace any present.
 - 8: Make sure that your `secrets` are properly set for wifi and the `API` key.


```
substitutions:
  name: esp32-s3-box-office-assistant
  friendly_name: ESP32 S3 Box 3 Office Assistant
  micro_wake_word_model: alexa
packages:
  esphome.voice-assistant: github://esphome/wake-word-voice-assistants/esp32-s3-box-3/esp32-s3-box-3.yaml@main
esphome:
  name: ${name}
  name_add_mac_suffix: false
  friendly_name: ${friendly_name}
api:
  encryption:
    key: !secret api_key_voice_assistant


wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

  - 9: Click on `Install`
  - 10: Select `Plug into this computer` (Be aware that using a browser with HTTPS is required for this step)
  - 11: Select the right COM port and click on `connect`.
  - 12: Add the newly detected ESPHome device to HA.
  - 13: Access the device in the device menu and press `configure` beside it.
  - 14: Check `Allow the device to perform Home Assistant actions.`

