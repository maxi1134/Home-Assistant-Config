
<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>


# Home Assistant Automations List

### Here you will find an almost complete and almost kept-to-date list of all my automations created within the Home-Assistant automation system!

  

<p  align="center">  <a  href="/automations.yaml><img  src="https://img.shields.io/badge/Automations-purple"  alt="The automations!"></p> 

- [Maxi](/automations.yaml):
   - Start rain sounds where maxi is sleeping
   - Play Random Music when maxi awakes at home
   - Alert maxi in the room where he is when his phone rings
   - Sleep actions when "maxi is sleeping" is turned on anywhere
   - Undo sleeping actions for Maxi when he awakes
   - Turn off Phone Bluetooth emitter when maxi leaves home
   - Turn on Phone Bluetooth emitter when maxi arrives home
  [Maxi's laptop](/automations.yaml)
   - Alert maxi that his laptop battery is low
- [House-Wide](/automations.yaml):
   - Close blinds when the house is empty
   - Turn off lights when the house is empty
- [Lights](/automations.yaml):
   - Turn on the front door light in relation to the sun position
   - Turn on lights automatically when motion is detected in a room ( And leave rooms with a sleeper in them off)
   - Turn off lights when a room is empty
   - Automatically turn off bathroom light
- [Trigger-Based](/automations.yaml):
   - Start visuals on all inactive TVs
   - Change light to random colors on press
   - Change light to random pastel colors on press
- [Security](/automations.yaml):
   - Arm Alarm-Away when "User home" is off for 10 minutes
   - Lock back door 5 minutes after unlocking
   - Lock back door when it is closed for 5 seconds
   - Lock front door 5 minutes after unlocking
   - Lock front door 5 minutes after unlocking
   - Disarm alarm-away when "User Home" turns on
   - Turn back door lights on when someone is detected
   - Alert that the front door remained unlocked
   - Alert maxi if someone goes into the hotbox during a party
- [Bathroom](/automations.yaml):
   - Play Muzak in the bathroom when someone enters
   - Raise music on the playing speaker when the Bathroom fans goes on
   - Start/stop bathroom fan when Bathroom humidity changes
- [Bedroom](/automations.yaml):
   - Stop bedroom speaker when bedroom sleeper awake
- [Media](/automations.yaml):
   - Close covers in the room when chromecasts start movie
   - Mute chromecasts when they start visuals
   - Mute speakers in the room if maxi uses a microphone
   - Mute speaers when chromecast start playing in the same room
   - Mute speakers when the room is empty
   - Raise lights to 60% brightness when a movie is paused and the brightness is below 30%
   - Set speakers volume on initial playback
   - Turn lights to purple when a movie is played
   - Turn lights to white when we stop a movie
   - Turn off speakers after 15 minutes of pause
   - Conditionally Unmute Speakers when a room is occupied
     - Conditions: No Sleeper, No unmuted TV and no microphone in use
   - Unmute speakers when TV stops
   - Dim Wall lights when livingroom TV is playing and Beige couch or matress is occupied (To avoid reflections in the TV)
- [Tablets](/automations.yaml):
   - Control brightness of Closet tablet to avoid discharge
   - Adjust closet tablet screen brigthess to match the lights
   - Auto close closet tablet screen when someone is sleeping
   - Control Hallway Tablet charging
   - Lock tablets on triggered alarm
   - Unlock tablets on disarmed alarm
   - Reload tablets when frigate changes state
   - Reload tablets 4 minutes after HA restarts
   - Show main screen when music stops on Hallway tablet
   - Reload tablet page when motion is detected in the kitchen
   - Control brightness of Front door Phone to avoid discharge
- [Climate](/automations.yaml):
   - Turn ACs on and Off depending on house occupancy
   - Turn off Closet fan after 5 minutes of non occupancy
   - Alert that the front door remained open during cold time
- [Kitchen](/automations.yaml):
  - Alert when dryer finishes 
  - Alert when microwave finishes 
  - Alert when washer finishes 
- [Misc](/automations.yaml):
  - Alert that the vacuum is stuck
- [Maxi's Computer in the hotbox](/automations.yaml):
  - Mute Speakers in the hotbox when computer mic is in use
  - Unmute Speakers in the hotbox when computer mic goes off
  - Lower hotbox down light when maxi open his webcam
  - Set Wallpaper engine colors to match the lights color
- [Party](/automations.yaml):
  - Alert maxi when people are too noisy on the patio during a party
  - Lower volume if police is detected in the front during a party
  - Change all single lights to random saturated colors when acid mode turns on
  - Change room lights to semi-saturated colors when party turns on
  - Start 4K visuals when acid time turns on
  - Start visuals when Party mode is turned on
  - Turn on "party mode" when over 5 friends are over
  - Turn off acid time when everyone goes to bed
  - Automatically turn off party mode
- [Voice assistant endpoints](/automations.yaml):
  - Assist turn on ESP32 endpoint screens when room is occupied
  - Assist turn off ESP32 endpoint screens when room is unoccupied
  - Assist mute ESP32 endpoints when room is unoccupied
  - Assist Unmute ESP32 endpoints when room is occupied
- [LLM Accessible Scripts](/scripts.yaml):
  - LLM Script to start plex media playback on TVs
  - Set whole house to cozy ambiance
  - Set whole house to a normal ambiance
  - Set whole house to kinky ambiance
  - Set whole house to party ambiance
  - Set whole house to psychedelic ambiance
  - Set whole house to sexy ambiance
  - Generate patio camera description
  - Generate front door camera description
  - Generate back door camera description
  - Find phone (Makes it ring)
  - Changes all on lights to warm pale colors
  - Changes all on lights to warm colors
  - Changes all on lights to random colors
  - Reset tablet endpoint interface





