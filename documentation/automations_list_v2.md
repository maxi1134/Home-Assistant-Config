# Automations List
### Here you will find an almost complete and almost kept-to-date list of all my automations!

<p align="center"> <a href="/node-red"><img src="https://img.shields.io/badge/Nodered%20FLows-purple" alt="Red leader standing by, or something"></p>




Automations:

    - Home-Wide:

        Set ACs to Away when all registered users are gone

        Change lights when movies are played

        Mute room speakers when movies are played

        Close room blinds when movies are played

        Set ACs to Home when users get nearby the apartment

        Notify me when a package is "Out for delivery" on 17track

        Notify me when a package is "Delivered" on 17track

        Turn lights on and off per room on occupancy detection and actual time

        Stop speakers after 10 minutes of pause

        Set the Speakers volume depending on the time

        Set the Speakers volume to 20% when they are turned off ( to limit the volume of the broadcasts )

        Set the Chromecasts volume depending on the time upon initial start

        Turn off all the lights when all the users are gone

        Cycle lights on random colors

        Arm Alarm system when all registered users are away

        Tablets:

           Turn On/OFF the tablets chargers when reaching 20%/80% of battery charge

        Armed Away Mode:

           Any motion will send a notification to all users

            Motion willturn off all the lights

            Motion will close all the blinds

            Motion will repeat broadcast threats on all the speakers

        All Asleep Mode: #Triggered by Nate and Maxi sleep being enabled

            Dim all interior lights to 1% when triggered if someone walks nearby

    - Front door:

        Turn the light on and off depending on the sun position

        Turn on the light when there is someone if it is not already on

        Broadcast the name of the person unlocking

        Turn on chosen devices by to the person entering

        Party Mode:

           Let known guests in and an announce them if the front of the apartment is occupied 

        Maxi Sleep Mode:

            Turn off the front door light

            Restrict the light brightness to 25%

    - Bedroom Maxi:

        Playing music on any of the speakers mutes the PC

        Playing media on the PC mutes the speakers

        Mute the Maxi speakers if the TV is playing and unmuted

        Switch the TV input to the Chromecast upon playback detection

        Switch the TV input back to PC upon end of playback on the chromecast

        Switch the TV input to Chromecast when the PC screens are turned off

        Switch the TV input to PC when the PC screens are turned on

        Turn on the Screens when motion is detected in the bedroom and that the lights are not off.

        Adjust the blinds height in relation to the AC state so the cold air can pass.

        Close the blinds if I am out of the room for more than 15 minutes

        When cololight turns on, chose a random effect

        When "Relaxing sounds" is detected on one of the speakers, move the playback to "Maxi speakers"

        Mute the Maxi speakers if discords detects a game activity and the PC input is set to Gaming Headset 

        Mute the Maxi speakers if an audio session is detected on the PC while unmuted

        Unmute the Maxi speakers when the audio session on the PC is stopped

        Turn up the volume of the speakers when the AC is on then down when it stops

        3-taps on the Hue Tap lower button trigger Sleep Mode

        Sleep Mode: #Sleep mode is turned off by bayesian sensor when it detects I am up for more than 20 minutes

          Turn off all lights

          Turn off all displays

          Start rain sound on Maxi speakers

          Set Maxi speakers volume to 40%

          Turn night lights on upon motion while this mode is active

    - Entrance:

        Auto-Lock the door after 3 minutes when the Front door is closed

        Repeating broadcast when the door stays open for no reason 

          Reasons: "Aerating home" condition turned on

        Repeating broadcast when the door stays unlocked for 3+ minutes

        Auto Unlock with Frigate/Double-Take (Conditional on Party-mode/Waiting-Someone On and Occupancy in the front)

        Return the PTZ camera to the "Doorway" position 10 minutes after it looks elsewhere

    - Trip Closet:

        Entering the closet should turn on the light unless "Sleep mode" is on

        Return the brightness to it's initial percentage upon color change ( bug mitigation)

        Sleep Mode: #Sleep mode is turned on and off by the dial remote click

          Mutes the entrance speaker

          Turn off the lights and restrict the motion sensor

          Restrics the entrance inner light brightness to 40%

        Party Mode: #Party mode is detected through wifi count

          Start a random effect on the addressable leds upon each entering

    - Living Room:

        Chromecast active and unmuted playback mutes the speakers in the room

        Ajust blinds height if closed when the AC Starts/Stops

        Turn up the volume of the speakers and TV when the AC is on then down when it stops

        Sleep Mode: #Sleep mode is a toggle virtual switch

          Turn off the lights and restrict their turning on

          Mute the speakers

          Close the blinds

        Movie mode: #Movie mode is triggered by a bayesian sensor that looks for the lenght of the media playing and it's categorization 

          Turn the lights to purple at 30% brightness during media playback

          Turn the lights brightness to 80% upon pausing the playback

          Close the blinds if the sun is up

          Lowers the entrance tablet's brightness to 1%

        Trip Mode: #trip mode is a toggle virtual switch

          Close the blinds

          Start the plasmaball

    - Bathroom:

        Shuffle muzak on occupancy

        Set variable volume on door closing

        Turn the lights on for 8 seconds when someone walks by the door

        Turn lights on until the room is emptied when occupancy is detected

        Starting Shower Speaker mutes Main Speaker (This allow for the non-interuption of a group when the bathroom user desire different music than elsewhere)

        Stopping Shower speaker unmutes the Main Speaker

        End Shower Speaker playback when the door opens

        Mute Main speaker when the door opens

        Shower Mode: #Shower mode is triggered on when the door is closed and the humidity is over 65%

          Start the extractor until humidity goes down to less than 60

          Raise the speakers volume by 10% (Lower by 10% when shower ends)

    - Kitchen:

        Broadcast repeat alerts when the microwave is done

        Broadcast repeat alerts when the water is boiling

        Broadcast repeat alerts when there is a water leakage

        Broadcast repeat alerts when smoke is detected

        Broadcast repeat alerts when the dryer is completed

        Broadcast repeat alerts when the fridge door is left open

        Broadcast repeat alerts when the kitchen timer is finished

        Set timer.kitchen_timer with the value of the alarm/timer detected on the Kitchen google mini

        Turn off the speaker if empty for 20 minutes unless a group using it is playing.

        Turn on Back Door light if Double-take detects someone

        Broadcast the name of the person unlocking the back door

    - Hotbox Down:

        Set AC to 26 when the occupancy turns off

      Set AC to 24 when the occupancy turns on

        Movie mode: #Movie mode is toggled by the detection of a movie on the Hotbox Top Chromecast

          Turn the lights to purple at 50% brightness upon playing

          Turn the lights brightness to 80% upon pausing

          Mute the Hotbox down speakers

          Dim or mute the Hotbox top speakers depending on occupancy (Mute if empty, and lower to 20% if occupied)

        Sleep mode: #Sleep mode is toggled by a physical button and toggled off 12 hours later if not toggled off before

          Turn the lights off

          Limits the hotbox top lights to the table lamp

          Mute and stop the Hotbox down media players

          Dim or mute the Hotbox top media players depending on occupancy (Mute if empty, and lower to 20% if occupied)

    - Hotbox top:

        Turn the TV on on occupancy detected

        Pause playback on chromecast  for 20 minutes if the occupancy is off [Unless party/kink/Trip mode is on]

        Stop playback on chromecast completely 30 minutes after occupancy off [Unless party/kink/Trip mode is on]

        When Roku changes to Xbox turn the lights to green and set the brightness to 35%

        When Roku changes to Netflix menu turn the lights red and set them to 35%

      When Roku changes to Plex (while not playing) turn the lights to yellow and set them to 35%

        Movie mode:

          Turn the lamp to purple at 60% brightness

          Turn off the ceiling light upon playback

          Turn the lights brightness to 80% upon pausing

          Mute the Hotbox Top Speakers

          Dim or mute the Hotbox down speakers depending on occupancy (Mute if empty, and lower to 20% if occupied)

    - Server:

        Notification on servers CPUs running over 65C for more than 5 minutes

        Mute speakers when the Patio chromecast is playing

        Home-wide alert when the door stays open for no reason while below 20C outside

          - Reasons: Server running too hot, Patio occupied, "Aerating home" condition turned on, Vacuum running

    - Patio:

        Home-wide alert when the door stays open for no reason while below 20C outside

          - Reasons: Server running too hot, Patio occupied, "Aerating home" condition turned on, Vacuum running

        On excessive noise alert occupants to lower the volume on the TV speakers/Display

        On subsequent excessive noise alert occupants to lower volume sternly and shut down lights for 30 seconds

        On last warning for excessive noise, order occupants to go inside and turn off lights and TV for 30 minutes

        Quiet time mode: #Triggered by a physical button and is disabled when the patio is unoccupied for 15 minutes

            Turn off and restrics the lights

            Turn off and restricts the server lights

            Turn off the TV

    - Maxi's Phone:

        Upon ringing, announce a call is incoming on the closest speaker in relation Maxi

      Trigger speakers nearby Maxi when the alarm goes off on the phone

      Double press Power button to trigger lights in the current room

    - Maxi's Watch:

        Toggle light in the occupied room when doubletapping the watch button

    - Carla:

        Alert carla to move her car at 9AM when she is sleeping over

    - Fay:

        Upon receiving text from fay starting with "Pending task:" Add task to next event with her

    - Vacuum:

        Repeating broadcast on all speakers when the vacuum is stuck

        Vacuum the entrance every 3 people entering

Event automations:

    - Party Mode: #Auto triggers on 7+ guests

      Turn the lights of each room to a color per room. (Fully saturated colors + 30% white channel )

      Start a different style of music per zone of the apartment ( Volume related to the current time )

      Shuffle Soft visuals across the TVs

      Close the bedroom blinds

    - Trip Mode: #Triggers on saying "Ok google, it's morbing time"

          Close all blinds

          Turn all 40 lights to random colors (Fully saturated colors only, no white channel )

          Shuffle Trippy visuals across the TVs

          Start Psychedelic Music

    - Kink Mode: #Kink mode is a toggle virtual switch

          Turn the lights of each Zone to a color (Fully saturated colors between Purple and Red)

          Shuffle the Kinky playlist simultaneously across the front speakers (Volume depending on time)

          Shuffle Soft Aftercare music in the Hotbox ( 5 speakers 10% volume )

          Shuffle Soft visuals across the TVs

          Close all the blinds

          Change doorbell alert from "Someone is at the door" to "A perv is at the door"

Announcements:

  Trash time on trash day at 10:00pm/10:30pm

  Recycling time on recycling day at 10:00pm/10:30pm

Actionnable Notifications: 

    When someone texts me "on my way" or equivalent, gives me an actionnable notifications that can enable the "Waiting for someone" mode; 

        When this mode is triggered, the door will auto-unlock if the person ringing is recognised by Double-take

    When someone rings, gives me an actionnable notifications with picture of the ringer that can unlock the door