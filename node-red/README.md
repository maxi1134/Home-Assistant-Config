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

In summary, the naming convention for media players and speaker groups in home-assistant provides a clear and concise way to identify and control these entities in the home.
