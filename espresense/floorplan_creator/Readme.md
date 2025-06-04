# FloorPlan creator for ESPresense-companion

This project has been created to make it easier to create floorplan for ESPresenseIPS (https://github.com/ESPresense/ad-espresense-ips) or ESPresense-companion (https://github.com/ESPresense/ESPresense-companion) by having a gui.
It offers snap to rooms for easy alignement, gives you mesures on each side of your cursor when on a wall.
This application is developed in HTML/JS/CSS, no libraries (but fontawesome on a cdn), Canvas power !

## Demo & online floorplan creator
You can now try the app on : https://espresense.com/Floorplan-Creator/   
You will be able to draw you plans, test the app and generate exports.
Mqtt integration should work but with a public instance of mqtt in https. (the ssl mqtt is now implemented)

## How to use
- Download the project
- open "index.html" with a browser
- start drawing each rooms
- add esp32 devices inside each room
- click the "export to yaml" button
- copy generated code over to ESPresenseIPS app.js file
- Optional :
    - after config is copied and AppDaemon restarted
    - open MQTT configuration panel (⚙) and enter your config
    - devices will show up on the floorplan in real time
    - devices will also appear in the list in mqtt config panel
    - enjoy tweaking everything to make it perfect

## Changes
12-03-2023
- Modified yaml output to be compatible with https://github.com/ESPresense/ESPresense-companion

05-04-2022
- Fixed calculation of most top and left rooms (in some cases where most top and left rooms are not the same room) - Thanks @Shuttleu

21-03-2022
- Added unit selector, when selecting feet, values will be converted to meters before Yaml export.
- You can toggle unit on an already drawn plan
- App used to save unit in the localstorage, it is now a variable.
- If you update you will see unit on plan and right elements as mm and mft (because old "m" unit is in the label and I append the new variable unit)
- Just a UI bug, will work as expected
- You can manually remove the "m" from room labels in the localstorage if it bothers you.

05-03-2022
- Make MQTT allow anonymous connections - Thanks @DTTerastar
- Catch MQTT disconnection exeptions to prevent javascript execution from breaking - Thanks @DTTerastar
- SSL implemented in the MQTT client - Thanks @DTTerastar
- SSL configurable from settings panel
- Hide/Show devices on the floorplan via MQTT Settings panel
- Change devices color on the floorplan via MQTT Settings panel

28-02-2022
- New tools menu on the left   
- New label toggle to hide/show plan labels (room title and meusres)   
- New file structure (the project was not supposed to grow this much)   
- Fix indentation on yaml export      
- Add name to esp32 devices. The name is exported in the yaml     

27-02-2022
- Show arrow indicators on top, left, bottom and right of canvas if floorplan is out of screen to indicate on which side/s it is

26-02-2022
- MQTT controle panel now displays connected devices (and known devices even if not connected anymore)
- MQTT Client to track devices
- MQTT settings page to input config
- show devices on the floorplan realtime

26-02-2022
- Show ESP32 device coverage button
- ESP32 coverage shown as a circle around the device
- Coverage distance is ajustable
- Circle color is adjustable
- Coverage circle can be visible when placing device to help positioning it right
- Now you can even make art with the app.

25-02-2022
- Dark Theme
- Fix yaml floorplan coordinates (was in cm needs m)
- Icons and small UI tweaks
- Add ESP32 to Rooms
- Edit ESP32 Z value and see its yaml code
- yaml export now exports esp32 position as well

24-02-2022
- Scroll/Mouse Wheel to move floorplan in canvas

## What is planned.  
[  ] Better UI/UX.  
[✓] Scroll/Wheel on the canvas to move floorplan.   
[  ] Zoom on the canvas.    
[  ] Pan on the canvas.      
[✓] Ability to add esp32 in rooms and position them with precision.  
[✓] Export esp32 position from rooms to ESPresenseIPS yaml format.  
[✓] Have each esp32 bluetooth signal radius visible on plan to make sure you have at least 3 signals in each rooms. (will help determine where is the best place to put them ;)).  
[✓] MQTT client to show devices on the floorplan directly.   
[  ] Allow changing the precision while dragging/creating a room.   
[✓] SSL for MQTT (to be able to host the app on home assistant www folder on https instances)    
[✓] Left toolbar  
[  ] Left toolbar with values modifiers to allow a user to customize snap thresholds and precision and more  
[✓] Fix indentation in Yaml       
[  ] Code refactoring, add comments.

## Technical

U / L Shaped rooms :
- Create multiple rooms to acheve the desired shape
- Name them all with the same name
- ESPresenseIPS should send the same name to home assistant if the user is in any of those rooms.

Positions are calculated as followed : 
- find the room with the smallest x value (left side of screen)
- find the room with the smallest y value (top of screen)
- generate an offset with those 2 values (which means "smallestX, smallestY" is now the "0,0" position
- every position saved from drawn rooms are then recalculated with this offset

Scroll :
- The scroll moves the floorplan on the canvas
- the scroll position are updated directly in the storage (rather than offsetting everywhere in the code)
- the initial coordinates of each room WILL change if you scroll
- This is of no effect on the final exported values thanks to the way they are calculated before exporting.

ESP32 Devices :
- Devices export only x, y, z values
- Device has its room's name when exported to yaml
- z value is set to 0 by default and can be changed when opening the device modal (green square under room name in right menu)
- coverage and color value are only for display purposes (they are not exported)

Recovery / YAML to JSON / Deleted Local storage.... Oups ! (if you have exported the yaml code before) :
- in the app, click the recovery button.
- read the Alert, click ok
- read the text on top of the modal
- paste your yaml code into the yaml (left) area.
- on the right you should see the corresponding json object.
- click "save" and reload, you should see your floorplan again.
- This feature is experimental, the conversion process is poorly coded and might be working only for a specific syntax (see floorplan code below for example of what the conversion process has been coded for)
- The conversion does NOT recalculate label position for rooms so you will have labels top left and some undefined in the right menu. but it can save you some time and you can still work on it and export updated yaml without issues
- This is a bonus safety net, if it works, be happy, if it dosent, then start drawing. I probably won't updating this "feature"


Try to have rooms: ... then roomplans: ... , for roomplans, have y1, x1, y2, x2 in that order. rooms: names should match the roomplans: names (because the app need to put a device in a room and the mathcing is done with the name)
```
rooms:
    kitchen: [0.035, 2.285, 0]
    bedroom: [3.68, 11.045, 1.2]
    livingroom: [3.59, 5.805, 1.2]
    second_bedroom: [7.275, 5.559928991794586, 0]
    office: [10.48, 2.715, 0]
roomplans:
    - name: kitchen
        y1: 0
        x1: 0
        y2: 4.29
        x2: 3.59
    - name: bathroom
        y1: 4.29
        x1: 0
        y2: 6.72
        x2: 2.36
    - name: toilet
        y1: 6.72
        x1: 0
        y2: 7.98
        x2: 2.36
    - name: second_bedroom
        y1: 7.98
        x1: 0
        y2: 12.06
        x2: 3.68
    - name: bedroom
        y1: 7.98
        x1: 3.68
        y2: 12.06
        x2: 7.6
    - name: entrance
        y1: 4.29
        x1: 2.36
        y2: 7.98
        x2: 3.59
    - name: entrance
        y1: 6.2
        x1: 3.59
        y2: 7.98
        x2: 7.6
    - name: livingroom
        y1: 1.37
        x1: 3.59
        y2: 6.2
        x2: 7.6
    - name: office
        y1: 0
        x1: 7.6
        y2: 4.92
        x2: 10.53
```

## A little glimps (Outdated but I can't take new ones currently - Check demo to see new desing and features)

<img width="1918" alt="Screenshot 2022-02-26 at 00 01 42" src="https://user-images.githubusercontent.com/3304418/155815186-3fe68408-f55f-4bed-b310-8d8b059f4660.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 02 04" src="https://user-images.githubusercontent.com/3304418/155815196-1a08bef6-4824-4a6b-a175-0e3ad8fa14c4.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 02 17" src="https://user-images.githubusercontent.com/3304418/155815203-79cc8971-2a16-441a-8a07-0785d31a6b72.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 02 40" src="https://user-images.githubusercontent.com/3304418/155815206-9bebe308-3fd3-4868-9984-bd1ce0aabe06.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 02 57" src="https://user-images.githubusercontent.com/3304418/155815210-9b43120a-ac5a-45a5-9bdc-06835045ba35.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 58 53" src="https://user-images.githubusercontent.com/3304418/155819156-1cbcd5cd-0398-4d0b-9bf2-86c5503221d3.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 59 07" src="https://user-images.githubusercontent.com/3304418/155819162-809ad234-b525-481e-b9cf-479d37a42360.png">

<img width="1920" alt="Screenshot 2022-02-26 at 00 59 29" src="https://user-images.githubusercontent.com/3304418/155819171-a63a55f4-196e-4850-898f-15ab5a6188b5.png">


