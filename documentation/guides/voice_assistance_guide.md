<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

_____

# In this guide I will explain how I've setup my Local voice assistant and satellites!  
A few softwares will be used in this guide.

[Ollama](https://ollama.com/) for the backend of the LLM.   
[ESPHOME](https://esphome.io/) for the ESP32-s3 sattelites.  
[Piper](https://www.home-assistant.io/integrations/piper/) For the text to speech.  
[Whisper](https://www.home-assistant.io/integrations/Whisper) For the speech to text


_____

# Step 1) Installing Ollama

We will start by installing `Ollama` on our machine learning host.   
I recommend using a good machine with access to a GPU with at least 12 GB of Vram. (24 if you are to use the same model as me, at it takes 19GB on its own). 
(This can run also with as low as 3-4gb! Using Llama 3.2 )
I also thinkg it's better to keep the model loaded in the machine at all time for speedy reaction times on our satellites.

**Here an example of the IDLE VRAM usage for  `Ollama` with the `qwen2.5:14b-instruct-q8_0` model:**
```

+-----------------------------------------------------------------------------------------+
| NVIDIA-SMI 565.57.01              Driver Version: 565.57.01      CUDA Version: 12.7     |
|-----------------------------------------+------------------------+----------------------+
| GPU  Name                 Persistence-M | Bus-Id          Disp.A | Volatile Uncorr. ECC |
| Fan  Temp   Perf          Pwr:Usage/Cap |           Memory-Usage | GPU-Util  Compute M. |
|                                         |                        |               MIG M. |
|=========================================+========================+======================|
|   0  NVIDIA GeForce RTX 3090        Off |   00000000:01:00.0 Off |                  N/A |
|  0%   35C    P8             17W /  370W |   18025MiB /  24576MiB |      0%      Default |
|                                         |                        |                  N/A |
+-----------------------------------------+------------------------+----------------------+

+-----------------------------------------------------------------------------------------+
| Processes:                                                                              |
|  GPU   GI   CI        PID   Type   Process name                              GPU Memory |
|        ID   ID                                                               Usage      |
|=========================================================================================|
|    0   N/A  N/A   4171841      C   ...unners/cuda_v12/ollama_llama_server      18016MiB |
+-----------------------------------------------------------------------------------------+

```

I've chosen the default install method  method for my Ollama installation.

In order to do so, simply run this command:

`curl -fsSL https://ollama.com/install.sh | sh`


# Step 1.a) Pulling the LLM model

Once Ollama if installed, we will need to pull the model we want to use.

I recommend using `qwen2.5:14b-instruct-q8_0` if you can spare the VRAM. 
( ~21GB of it).

If you happen to be more VRAM-limited, you can also try using a Llama3.2 model, which run on 3B parameters. (should run on a 6GB GPU, maybe even 4GB with a lower Quantization) 

You can do so with:
`ollama pull qwen2.5:14b-instruct-q8_0`
or
`ollama pull llama3.2:3b-instruct-q8_0`

_____

# Step 2) Integrating Ollama into Home-Assistant



- : You will need to add the  `Home LLM Conversation` integration to Home-Assistant in order to connect LocalAI to it.
    - 1: Access the `Settings` page.
    - 2: Click on `Devices & services`.
    - 3: Click on `+ ADD INTEGRATION` on the lower-right part of the screen.
    - 4: Type and then select `Ollama`.
    - 5: Enter the hostname or IP Address of your Ollama host.
    - 6: Select the model you pulled
    - 7: Click "Sumbit"
    - 8: click "Finish"

<p align="center">A video of the process! </p>
<p align="middle">
  <img src="/assets/voice_assistance_guide/installation.gif" width="100%" />
<p>

_____

# Step 3) Integrating Ollama into our Voice assistant


- 1:  Integrate Fallback Conversation to Home-Assistant
  - 1: Access the `Settings` page.
  - 2: Click on `Voice Assistants`.
  - 3: Click on `Add Assistant`
  - 4: Enter the name `LLM Voice Assistant`
  - 5: Select `llama3.2` or `qwen2.5:14b-instruct-q8_0`
  - 6: Check `Prefer handling commands locally` so it is on.
  - 7: Select your `Speech-to-text` [service](https://www.home-assistant.io/integrations/Whisper).
  - 8: Select your `Text-to-speech` [service](https://www.home-assistant.io/integrations/piper/).
_____
_____

# Step 4) Setting up ESPHOME Voice assistant satellites.


- 1: Will now need to install ESPHome on our ESP32-S3-Boxes (This assumes that your ESPHome addon is already set).
    - 1: Access the `ESPHome Compiler` page.
    - 2: Click on `+ NEW DEVICE`.
    - 3: Enter a name, such as `ESPHome Assistant`
    - 4: Connect your ESP32-S3-Box to the computer. (Be sure to be running your Home-assistant interface in a chromium browser with HTTPS ENABLED!)
    - 5: Click `Connect`
    - 6: Select the `JTAG/Serial debug unit` Com port corresponding to your ESP32-S3-Box 
      - (This can be guessed by disconnecting the ESP32 and see which ones don't disapear from the list)
    - 7: Click `Connect`
    - 8: Wait until the installation is cxompleted.
- 2: Now, we will set the correct firmware below on them:
    - 1: Click `Edit` beside our new `ESPHome Assistant`device.
    - 2: Paste the code under and make sure to create the corresponding secrets.
  ```
  substitutions:
    name: esphome-assistant
    friendly_name: ESPHome Assistant
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
    - 3: Click on `Install`
    - 4: Select `Wirelessly` or `Plug into this computer`.
    - 5: Wait for the install to finish and then click `Close`.


# Step 5) Integrating The Assistant sattelite into Home-Assistant

    - 1: Access the `Settings` page.
    - 2: Click on `Devices & services`.
    - 3: Click on `+ ADD INTEGRATION` on the lower-right part of the screen.
    - 4: Type and then select `ESPHome`.
    - 5: Select the newly discovered Assistant
    - 6: Click "Sumbit"
    - 7: click "Finish"
    - 8: Select the corresponding Area
    - 9: click "Finish"