# Node-red Flows

## Node-red entity nomenclature to use with REGEX flows such as;
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
