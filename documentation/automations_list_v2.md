


# Automations List

### Here you will find an almost complete and almost kept-to-date list of all my automations!

  

<p  align="center">  <a  href="/node-red"><img  src="https://img.shields.io/badge/Nodered%20FLows-purple"  alt="Red leader standing by, or something"></p> 

- [House-Wide](/node-red/flows/House-Wide.json):
  - Set ACs to away or present depending on users presence
  - Turn all lights off when all users are away
  - Set alarm to away when all users are away
  - Set alarm to home when at least one user is present
  - Calculate and set the appropriate volume of speakers when they start playback
  - Notify my phone and broadcast an alert when a package is delivered
  - Completely stop speakers after 10 minutes of paused state  - 
  - Calculate and set the appropriate volume of TVs when they start playback
  - Set speakers volume to 20% after they are stopped
  - Calculate and set the appropriate brightness of lights when motion is detected
  - Turn lights off in empty rooms
  - Turn all lights off when the house is empty
  - Mute the speakers in a room if the TV is started
  - Unmute the speakers in a room when the TV is stopped
  - Close the blinds in a room when the TV is playing a movie
  - Turn the lights to purple in a room when the TV is playing a movie
  - Turn the lights to brighter purple in a room when the TV has a paused movie
  - Return the lights in a room to white when a movie is stopped on the TV
  - Close the blinds when all are asleep
  - Add artists playing on TVs to Lidarr for automatic acquisition
  - Turn on "party mode" when 5 friends are over
     -  Play soft visuals on all screens when "party mode" is turned on
     - Change light groups to semi-saturated random colors when "party mode" is turned on
     - Start music on each speaker group when "party mode" is turned on
   - Reload tablets when hass is rebooted
   - Turn on "Waiting Uber" when an uber email is detected
- [Trigger-Based](/node-red/flows/Trigger-Based.json):
  - Start visuals on all inactive TVs
  - Generate and set "Fake white" on each active light group
  - Generate and set warm saturated colors on each active light
  - Generate and set warm pale colors on each active light
  - Generate and set a random saturated color on each active light
  - Generate and set a random pastel color on each active light
  - Generate and set random saturated colors when "acid time" is turned on
  - Play trippy visuals on all inactive TVs  when "acid time" is turned on
  - Start music across the house-group when "acid time" is turned on
  - Close all blinds when either "acid time" or "kink party" is turned on
  - Generate and set warm colors on each light group when "kink party" is turned on
  - Generate an improved color loop when "trippy lights effect" is turned on
  - Return lights to white when "trippy lights effect" is turned off- 
- [Security/Conditions](/node-red/flows/Security%2Fconditions.json):
   - Notifify my phone upon breach in my bedroom
   - Automatically set the following boolean inputs to off after a set amount of time
     - Waiting Uber
     - Party mode
     - Acid time
     - Aerating Appartment
     - Sleeper in Hotbox
     - Sleeper in Livingroom
     - Sleeper in Patio
     - Waiting someone
   - Automatically set the the following boolean inputs to off after a condition is met
      - Acid time
- [Alexe](/node-red/flows/Alexe.json):
   - Tells Alexe take out the dog from 9 am until the dog is seen by Frigate on one of the cameras
- [Back Door](/node-red/flows/Back%20door.json):
   - Automatically lock the door when it is closed
   - Announce if someone is using the door to enter or exit
- [Front Door](/node-red/flows/Front%20Door.json):
  - Automatically turn the porch light on and off depending on the sun position
  - Brighten the porch light when someone is passing in front of the front door camera
  - Lock the door when the front door is closed
  - Lock the door after 10 minutes of being unlocked
  - Detect when the front door is left open and broadcast and alert every 5 minutes until it is closed
  - Let people with "conditional" pins in if I am present and announce them
  - Detect who is entering and announce them while disabling the alarm system
  - Turn all lights to green when someone exits
- [Bathroom](/node-red/flows/Bathroom.json):
  - Start muzak in the bathroom when someone enters
  - Stop the bathroom speaker when appropriate
  - Automatically turns the light off when appropriate
  - Automatically adjust the bathroom speaker volume
  - Mute the main speaker when the shower speaker is started and stops it when not needed anymore
  - Automatically start the bathroom fan when humidity goes over 65%
  - Raise the speaker volume when the bathroom fan goes on 
  - 
  