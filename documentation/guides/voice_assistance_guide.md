<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

_____

# In this guide I will explain how I've setup my Local voice assistant and satellites!  
A few softwares will be used in this guide, so please make sure they are all installed and up to date before continuing!

[HACS](https://hacs.xyz/) for easy installation of the other tools on Home Assistant.  
[LocalAI](https://localai.io/) for the backend of the LLM.  
[Home-LLM](https://github.com/acon96/home-llm) to connect our LocalAI instance to Home-assistant.  
[HA-Fallback-Conversation](https://github.com/m50/ha-fallback-conversation) to allow HA to use both the baked-in intent as well as the LLM as a fallback if no intent is found.  
[Willow](https://heywillow.io/) for the ESP32 sattelites.  


_____

# Step 1) Installing LocalAI

We will start by installing `LocalAI` on our machine learning host.   
I recommend using a good machine with access to a GPU with at least 12 GB of Vram. As `Willow` itself can takes up to 6gb of Vram with another 4-5GB for our LLM model.   I recommend keeping those loaded in the machine at all time for speedy reaction times on our satellites.

**Here an example of the VRAM usage for  `Willow` and `LocalAI` with the `Llampa 8B` model:**
```

+-----------------------------------------------------------------------------------------+
| NVIDIA-SMI 555.42.02              Driver Version: 555.42.02      CUDA Version: 12.5     |
|-----------------------------------------+------------------------+----------------------+
| GPU  Name                 Persistence-M | Bus-Id          Disp.A | Volatile Uncorr. ECC |
| Fan  Temp   Perf          Pwr:Usage/Cap |           Memory-Usage | GPU-Util  Compute M. |
|                                         |                        |               MIG M. |
|=========================================+========================+======================|
|   0  NVIDIA GeForce RTX 3090        Off |   00000000:01:00.0 Off |                  N/A |
|  0%   39C    P8             16W /  370W |   10341MiB /  24576MiB |      0%      Default |
|                                         |                        |                  N/A |
+-----------------------------------------+------------------------+----------------------+

+-----------------------------------------------------------------------------------------+
| Processes:                                                                              |
|  GPU   GI   CI        PID   Type   Process name                              GPU Memory |
|        ID   ID                                                               Usage      |
|=========================================================================================|
|    0   N/A  N/A      2862      C   /opt/conda/bin/python                        3646MiB |
|    0   N/A  N/A      2922      C   /usr/bin/python                              2108MiB |
|    0   N/A  N/A   2724851      C   .../backend-assets/grpc/llama-cpp-avx2       4568MiB |
+-----------------------------------------------------------------------------------------+
```

I've chosen the Docker-Compose method for my LocalAI installation, this allows for easy management and easier upgrades when new relases are available.  
This allows us to quickly create a container running LocalAI on our machine.  

In order to do so, create a file called `docker-compose.yaml` with the following content:

**Be aware that this Docker configuration requires a GPU**
```
version: "3.9"
services:
  api:
    image: localai/localai:latest-aio-gpu-nvidia-cuda-12
    restart: always
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/readyz"]
      interval: 1m
      timeout: 20m
      retries: 5
    ports:
      - 8080:8080
    env_file:
      - .env
    environment:
      - DEBUG=true
    volumes:
      - /home/maxi1134/docker/local-ai/models:/build/models:cached 
        #Be sure to replace the username in the path
    runtime: nvidia
    deploy:
      resources:
        reservations:
          devices:
            - capabilities: [gpu]
```


Once that is done simply use `sudo docker compose up -d` and your LocalAI instance should now be available at: 
`http://{{host}}:8080/`
_____

# Step 1.a) Downloading the LLM model

Once LocalAI if installed, you should be able to browse to the "Models" tab, that redirects to ``http://{{host}}:8080/browse``. There we will search for the `mistral-7b-instruct-v0.3` model and install it.

Once that is done, make sure that the model is working by heading to the `Chat` tab and selecting the model `mistral-7b-instruct-v0.3` and initiating a chat. 

![alt text](/assets/ai_guide/chat_example.png)

_____

# Step 2) Installing Home-LLM




- 1: You will first need to install the Home-LLM integration to Home-Assistant   
    Thankfuly, there is a neat button to do that easely on [their repo](https://github.com/acon96/home-llm)!



     [![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?category=Integration&repository=home-llm&owner=acon96) 

- 2: Restart `Home Assistant`

- 3: You will then need to add the  `Home LLM Conversation` integration to Home-Assistant in order to connect LocalAI to it.
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

<p align="center">A video of the process! </p>
<p align="middle">
  <img src="/assets/home_llm_guide/home_llm_installation_video.gif" width="50%" />
<p>

_____

# Step 3) Installing [HA-Fallback-Conversation](https://github.com/m50/ha-fallback-conversation)


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
  - 10 Set the debug level at `Some Debug` for now.
  - 11: Click `Sumbit`
  - 
  
- 2: Configure the Voice assistant within Home-assistant to use the newly added model through the `Fallback Conversation Agent`.
  - 1: Access the `Settings` page.
  - 2: Click on `Devices & services`.
  - 3: Click on `Fallback Conversation Agent`.
  - 4: Click on `CONFIGURE`.
  - 5: Select `Home assistnat` as the `Primary Conversation Agent`.
  - 6: Select `LLM MODEL 'mistral-7b-instruct-v0.3'(remote)` as the `Falback conversation Agent`.


_____

# Step 4) Selecting the right agent in the Voice assistant settings.


- 1:  Integrate Fallback Conversation to Home-Assistant
 - 1: Access the `Settings` page.
 - 2: Click on `Voice assistants` page.
 - 3: Click on `Add Assistant`.
 - 4: Set the fields as wanted except for `Conversation Agent`.
 - 5: Select `Fallback Conversation Agent` as the `Conversation agent`.

_____

# Step 5) Setting up Willow Voice assistant sattelites.
