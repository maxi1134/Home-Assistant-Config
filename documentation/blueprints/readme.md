<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

# Blueprints
##### In this folder, you will find documentation regarding my blueprints!
- ##### Light effects
    -   [Random Colors Generation](../blueprints/script/maxi1134/light_effects/llm_random_color_generation.yaml)
        This blueprint allows the creation of a script with a selector for an `Area` or an `Entity` that should be switched to a random color. It also offers a setable `Saturation` and `Transition speed`  A `Color buffer` value can selected upon importing the blueprint, to avoid generating a color close to the actual one.

    -  [Ranged Colors Generation](../../blueprints/script/maxi1134/light_effects/llm_ranged_color_generation.yaml)
        This blueprint allows the creation of a script with a selector for an `Area` or an `Entity` that should be switched to a 'ranged' random color. It also offers a setable `Saturation` and `Transition speed`.
         A `Color buffer` value can also selected upon importing the blueprint, as well two `Ranges` of color. Helping avoid a specific color ranges. Settings this to `1->20` and `300->360` would mean that no blue is ever selected per instance.