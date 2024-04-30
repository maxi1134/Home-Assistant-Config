
<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>


# Home Assistant Automations List

### Here you will find an almost complete and almost kept-to-date list of all my automations created within the Home-Assistant automation system!

  

<p  align="center">  <a  href="/automations.yaml><img  src="https://img.shields.io/badge/Automations-purple"  alt="The automations!"></p> 

- [House-Wide]():
  - Play music when Maxi awakes at home
  - Close blinds when the house is empty
  - Turn off lights when the house is empty
  - Turn on lights automatically when motion is detected in a room ( And leave rooms with a sleeper in them off)
  - Reload tablets when frigate changes state
  - Reload tablets 4 minutes after HA restarts
  - Turn on "party mode" when over 5 friends are over
     - Play soft visuals on all screens when "party mode" is turned on
     - Change light groups to semi-saturated random colors when "party mode" is turned on
- [Trigger-Based]():
  - Start visuals on all inactive TVs
  - Change light to random colors on press
  - Change light to random pastel colors on press
- [Security/Conditions]():
   - Arm Alarm-Away when "User home" is off for 10 minutes
   - Lock back door 5 minutes after unlocking
   - Lock back door when it is closed for 5 seconds
   - Lock front door 5 minutes after unlocking
   - Lock front door 5 minutes after unlocking
   - Disarm alarm-away when "User Home" turns on
   - Turn back door lights on when someone is detected
- [Bathroom]():
   - Play Muzak in the bathroom when someone enters
   - Raise music on the playing speaker when the Bathroom fans goes on
   - Start/stop bathroom fan when Bathroom humidity changes
- [Bedroom]():
   - Stop bedroom speaker when bedroom sleeper awake
- [Media]():
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
- [Climate]():
   - Turn ACs on and Off depending on house occupancy
   - Turn off Closet fan after 5 minutes of non occupancy
   - Alert that the front door remained open
- [Kitchen]():
  - Alert when dryer finishes (WIP)
  - Alert when microwave finishes (WIP)
  - Alert when washer finishes (WIP)
- [Misc]():
  - Alert that the vacuum is stuck
- [Office]():
  - Mute Speakers in the office when computer mic is in use
  - Unmute Speakers in the office when computer mic goes off
- [Party]():
  - Change all single lights to random saturated colors when acid mode turns on
  - Change room lights to semi-saturated colors when party turns on
  - Start 4K visuals when acid time turns on
  - Start visuals when Party mode is turned on
- [Voice Assist]():
  - Assist play music (default) 
  - Assist play specific artist
  - Assist play playlist
  - Assist play song by artist now
  - Assist play song by artist next
  - Assist play specific song
  - Assist restart Music Assistant
  - Assist Set Thermostats