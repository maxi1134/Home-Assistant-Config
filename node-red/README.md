<p align="center">
<a href="documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a> <a href=".storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>
# Node-red Flows



<a href="/documentation/automations_list.md"><img src="https://img.shields.io/badge/Automations%20List-purple" alt="A part of me lives here">

# Node-red entity nomenclature to use with REGEX flows such as;
<p align="middle">
  <img src="/assets/node-red/example_flow.png" width="95%" />
</p>

## You will need to follow the following Naming Convention for Media Players and Speaker Groups in Home-Assistant

The naming convention for media players and speaker groups in home-assistant follows a specific structure to provide a clear and concise way to identify the type, location, and specific location within a room of these entities in the home.

### Individual Media Players

For individual media players, the naming convention consists of three parts, separated by dots:

1. `media_player.`: This part of the name refers to the type of entity, in this case, a media player. This is a device that can play audio and/or video content.

2. Room or location identifier: The second part of the name is a room or location identifier that specifies where the media player is located. For example, `media_player.bedroom_speaker` is a media player located in the bedroom.

3. Modifier for the specific location within the room: The third part of the name is a modifier for the specific location within the room, such as "top", "center", or "down". For example, `media_player.livingroom_center_speaker` is a speaker located in the center of the living room.

### Speaker Groups

For speaker groups, the naming convention consists of four parts, separated by dots:

1. `media_player.`: This part of the name refers to the type of entity, in this case, a media player group.

2. `group`: This part of the name indicates that the entity is a group of media players.

3. Room or location identifier: The third part of the name is a room or location identifier that specifies where the media players in the group are located. For example, `media_player.group_hotbox_speakers` refers to a group of media players located in a "hotbox".

4. Modifier for the specific location within the room: The fourth part of the name is a modifier for the specific location within the room, such as "top", "center", or "down". For example, `media_player.group_hotbox_upper_speakers` refers to a group of media players located in the upper part of a "hotbox".

5. "speakers": This part of the name indicates that the entity represents a group of speakers.


##  General nomenclature rules to follow


The nomenclature of the entity names in my Home Assistant follows a consistent pattern that provides important information about the type of device, its location, and any sublocation or grouping information.

For example, in the entity name `media_player.bedroom_speaker`, the `"media_player"` prefix indicates that the entity represents a media player device. The `"bedroom"` part of the name specifies the location of the device within the house, in this case the bedroom. The `"speaker"` suffix indicates the type of device, in this case, a speaker.

Similarly, in the entity name `light.bedroom_chandelier_light`, the `"light"` prefix indicates that the entity represents a light, the `"bedroom"` part of the name specifies the location as the bedroom, and the `"chandelier"` part of the name provides a sublocation within the room, in this case a chandelier. The `"light"` suffix indicates the type of device, in this case a light.

In some cases, the entity name may also include information about groups. For example, the entity name `light.group_bedroom_lights` indicates that the entity represents a group of lights, in this case, lights located in the bedroom. The `"group_"` prefix and the `"lights"` suffix indicate that the entity is a group of lights, while the `"bedroom"` part of the name specifies the location of the lights within the house.

Similarly, the entity name `media_player.group_hotbox_upper_speakers` represents a group of media player speakers, located in the hotbox room and specifically in the upper part of the room. The `"group_"` prefix, `"hotbox"` part of the name indicating the room location, and `"upper"` part of the name specifying the sublocation, combined with the `"speakers"` suffix indicate that the entity is a group of speakers in the upper part of the hotbox room.

To summarize, the nomenclature of the entity names in Home Assistant provides a clear and concise way of identifying the type of device, its location, sublocation, and any grouping information.


##  In regard to media_player.plex_* entities
These entities are the players created by the plex integration. I recommend following usual nomenclature, but inserting `plex_` inbetween `media_player.` and the device regular name, such as `group_hotbox_speakers`.  
`media_player.group_hotbox_speakers` would become `media_player.plex_group_hotbox_speakers`

## Naming Convention for Home Assistant Motion Sensor Entities

When you add a motion sensor to Home Assistant while using my automatic regex flows. The entity name should follow a specific format, which includes:

- `binary_sensor`: This is the entity domain, which indicates that the entity represents a binary sensor. Binary sensors are sensors that can detect the presence or absence of something, such as motion or a door opening/closing.
- `{location}_{sublocation}_motion_occupancy`: This is the entity name, which includes the location and sublocation (if applicable) of the motion sensor, followed by the words "motion" and "occupancy" to indicate that the sensor is detecting motion and occupancy status.

The location and sublocation are typically descriptive names that indicate where the sensor is located in your home. Here are some examples of motion sensor entity names that follow this format:

- `binary_sensor.living_room_motion_occupancy`: This entity represents a motion sensor located in the living room.
- `binary_sensor.kitchen_ceiling_motion_occupancy`: This entity represents a motion sensor mounted on the ceiling in the kitchen.
- `binary_sensor.basement_stairs_motion_occupancy`: This entity represents a motion sensor located on the stairs leading to the basement.

By following this naming convention, it becomes easy to identify and organize the motion sensors in your home. You can also use these entities in Home Assistant automations and scripts to create custom actions based on motion detection.


## Naming Convention for Home Assistant Light Entities

My Home Assistant lights also use a specific naming convention for their entities to make it easy for all to identify and control them. The naming convention includes four parts: Type, Location, Sub Location, and Suffix. 

#### Type and Location
The type is always "light." followed by the location, which identifies the room or area where the light is located. For example, `light.livingroom_center_light` identifies a light that is located in the living room, in the center of the room. Similarly, `light.kitchen_1_light` identifies a light that is located in the kitchen, and `light.office_lamp1_light` identifies a lamp in the office.

#### Sub Location

The sub-location is an optional field that provides more specific information about the location of the light. For example, `light.hotbox_down_behind_tv_light` identifies a light that is located in the hotbox (a specific area of the home), below the TV, and behind it. Similarly, `light.bathroom_door_light` identifies a light that is located in the bathroom, and specifically by the door.

#### Suffix

The suffix is a descriptive word or phrase that indicates the purpose or type of light. For example, `light.bedroom_chandelier_light` identifies a light in the bedroom that is a chandelier. Similarly, `light.office_quotra_light` identifies a light that is connected to the Quotra brand. Other examples of this naming convention include `light.hotbox_top_quotra_light` and `light.chillingroom_colo_light`.

## Groups

In addition to the individual lights identified by their Location, Sub Location, and Suffix, there are also groups of lights that are identified by the Location and always end with the word "lights." These groups allow users to control multiple lights in a specific location with a single command. For example, `light.group_livingroom_lights` refers to a group of lights in the living room that can be turned on or off together. Similarly, `light.group_bedroom_lights` refers to a group of lights in the bedroom, and `light.group_kitchen_lights` refers to a group of lights in the kitchen. Other examples of this naming convention include `light.group_all_lights` for all the lights in the smart home and `light.group_all_outside_lights` for all the outdoor lights.

In addition to these general groups, there are also more specific groups that include the type of light in their names. For example, `light.group_all_inside_lights` refers to all the inside lights in the smart home, while `light.group_all_zigbee_lights` refers to all the lights that are connected via Zigbee. These specific groups allow for even more granular control and customization of the lighting in the smart home.

It is worth noting that some lights are identified as "main" or "decorative." This distinction allows users to easily differentiate between lights that provide ambient lighting for a room and those that serve a more decorative purpose. For example, `light.group_chillingroom_main_lights` refers to the main lights in the chilling room, while `light.group_chillingroom_decorative_lights` refers to the decorative lights in the same room.

Overall, the naming convention used by Home Assistant entities for lights is designed to make it easy for users to identify and control their lighting in a convenient and efficient manner.


