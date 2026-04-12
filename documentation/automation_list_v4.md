<p align="center">
<a href="/documentation/hardware.md"><img src="https://img.shields.io/badge/Hardware%20Specifications-purple" alt="This is what runs everything"></a> <a href="/node-red/"><img src="https://img.shields.io/badge/Nodered%20Flows-red" alt="Read the README!"></a> 
<a href="/documentation/zigbee.md"><img src="https://img.shields.io/badge/Zigbee%20Devices-green" alt="This is what runs everything"></a>  <a href="/.storage/"><img src="https://img.shields.io/badge/Lovelace%20Interfaces-orange" alt="Actually my .storage folder, but eh!"></a>
<a href="/documentation/indoor_localization.md"><img src="https://img.shields.io/badge/Indoor%20Localization-blue" alt="They know where you are..."></a> 
<a href="/documentation/software.md"><img src="https://img.shields.io/badge/Software%20Usage-cyan" alt="Some deets on the softs"></a> <a href="/documentation/wifi.md"><img src="https://img.shields.io/badge/Networking-violet" alt="Some deets on the softs"></a> <br></p></p>

<a id="top"></a>
# Grouped automation analysis

Total automations: 313

## Jump menu

- [Security & Access (85)](#security-access-85)
- [Media, Audio & Entertainment (48)](#media-audio-entertainment-48)
- [Lighting & Ambience (60)](#lighting-ambience-60)
- [Climate, Covers & Environment (26)](#climate-covers-environment-26)
- [Sleep, Presence & Daily Routines (14)](#sleep-presence-daily-routines-14)
- [Voice Assistants & Commands (16)](#voice-assistants-commands-16)
- [Notifications & Personal Devices (15)](#notifications-personal-devices-15)
- [Tablets, Screens & Device Power (22)](#tablets-screens-device-power-22)
- [Household Appliances & Utility Alerts (9)](#household-appliances-utility-alerts-9)
- [System, Network & Integrations (8)](#system-network-integrations-8)
- [Other / Mixed (10)](#other-mixed-10)
<a id="security-access-85"></a>
## Security & Access (85)

[Back to top](#top)

- **AI Event Summary (v1.4.3) (Front and Back Door)** — **Automation ID:** [1751133520036](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9750)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Alert if someone is loitering by the car for too long** — **Automation ID:** [1743004781903](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6295)

  Triggered when `binary_sensor.parking_exit_camera_person_occupancy` changes to `on` for 4m 30s. Runs only if `binary_sensor.parking_exit_camera_car_occupancy` is `on`. Actions: send notification via `notify.maxi_notification_group` ('Person loitering beside the car').

- **Alert if the front door remains unlocked for too long** — **Automation ID:** [1731345211107](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4094)

  Triggered when `lock.lock_front_door` changes to `unlocked` for 6m. Actions: repeat run `script.broadcast_alert_in_the_house`; send notification via `notify.maxi_notification_group` ('Front door is currently unlocked') every 3m.

- **Alert maxi if someone comes in while he is sleeping** — **Automation ID:** [1717038269264](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2256)

  Triggered when `binary_sensor.sensor_patio_door_contact` changes to `on`; `binary_sensor.sensor_server_door_contact` changes to `on`; +3 more triggers. Runs only if `alarm_control_panel.home_alarm` is `armed_night`; `person.maximiliano` is `home`. Actions: send notification via `notify.maxi_notification_group` ('Someone is inside!'); turn on `light.group_all_lights` (brightness_pct=100, color_temp_kelvin=4805); +3 more actions.

- **Alert maxi if someone enters his room while alarm is set to armed home** — **Automation ID:** [1729698938654](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3956)

  Triggered when `binary_sensor.sensor_bedroom_door_contact` changes from `off` to `on`; `binary_sensor.group_bedroom_motion_occupancy` changes from `off` to `on`. Runs only if `alarm_control_panel.home_alarm` is `armed_home`. Actions: send notification via `notify.maxi_notification_group` ('Someone has entered the bedroom'); set volume on `media_player.mass_bedroom_wiim_speaker` (volume_level=1); +2 more actions.

- **alert maxi if someone goes into the hotbox during a party** — **Automation ID:** [1737850261149](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5331)

  Triggered when `binary_sensor.sensor_hotbox_door_contact` changes from `off` to `on`; `binary_sensor.group_hotbox_motion_occupancy` changes from `off` to `on`. Runs only if `alarm_control_panel.home_alarm` is `armed_home`. Actions: send notification via `notify.maxi_notification_group` ('Someone has entered the hotbox').

- **Alert Maxi when the automatic back horn is disabled** — **Automation ID:** [1747075531283](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7530)

  Triggered when `input_boolean.maxi_is_scared` changes from `on` to `off`. Actions: send notification via `notify.maxi_notification_group` ('Automatic Back Horn Disabled').

- **Alert people in the back to go away** — **Automation ID:** [1727032753670](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3330)

  Triggered when `binary_sensor.back_door_loitering_person_occupancy` changes from `off` to `on` for 30s. Runs only if not (`binary_sensor.sensor_back_outside_door_contact` is `on`). Actions: send notification via `notify.maxi_notification_group` ('Alerting loiterers to leave the back door'); repeat turn on `light.back_door_light` (brightness=255); wait 0s; turn off `light.back_door_light`; +2 more actions.

- **Alert that the front door remained open** — **Automation ID:** [1713378916074](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L205)

  Triggered when `binary_sensor.sensor_front_door_contact` changes to `on` for 3m. Runs only if `input_boolean.aerating_apartment` is `off`. Actions: repeat run `script.non_important_broadcast_alert_in_the_house` every 5m.

- **Alert when back horns rings and offer to turn it off** — **Automation ID:** [1743281098829](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6503)

  Triggered when `switch.plug_back_door_horn` changes to `on` for 0s. Actions: call `camera.snapshot` on `camera.back_door_camera`; if `person.myriam` is `home`, send notification via `notify.mobile_app_phone_myriam` ('Back horn started'); +5 more actions.

- **Alert when parking siren goes on and offer to turn it off** — **Automation ID:** [1760287140341](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12112)

  Triggered when `siren.parking_exit_camera_siren` changes to `on` for 0s. Actions: call `camera.snapshot` on `camera.parking_exit_camera`; run in parallel: send notification via `notify.mobile_app_phone_maxi` ('Parking horn started'); wait for event `mobile_app_notification_action` with action=`stop_parking_horn` (timeout 3m); turn off `siren.parking_exit_camera_siren`; if `person.myriam` is `home`, send notification via `notify.mobile_app_phone_myriam` ('Parking horn started'); wait for event `mobile_app_notification_action` with action=`stop_parking_horn` (timeout 3m).

- **Alert when someone is in the back (old)** — **Automation ID:** [1717016815299](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2146)

  Triggered when `binary_sensor.back_entrance_presence_person_occupancy`, `binary_sensor.back_door_loitering_all_occupancy` changes to `on`. Runs only if `input_boolean.party_in_the_back_yard` is `off`. Actions: call `camera.snapshot` on `camera.back_door_camera`; send notification via `notify.maxi_notification_group` ('Someone is in the back!'); +2 more actions.

- **Alert when someone is in the back (Reolink)** — **Automation ID:** [1744811754806](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7248)

  Triggered when `binary_sensor.reolink_back_door_camera_person` changes to `on`. Runs only if `input_boolean.party_in_the_back_yard` is `off`. Actions: call `camera.snapshot` on `camera.reolink_back_door_camera_fluent`; send notification via `notify.maxi_notification_group` ('Someone is in the back! (r)'); +2 more actions.

- **Alert when someone is in the parking** — **Automation ID:** [1743179648448](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6323)

  Triggered when `binary_sensor.parking_exit_camera_person_occupancy` changes to `on`; `binary_sensor.parking_camera_person_occupancy` changes to `on`; `binary_sensor.reolink_parking_camera_person` changes to `on`; `binary_sensor.reolink_parking_exit_camera_person` changes to `on`. Runs only if `input_boolean.party_in_the_back_yard` is `off`; `binary_sensor.maxi_sleeping` is `off`; +1 more conditions. Actions: if trigger id `parking_exit` matched, call `camera.snapshot` on `camera.parking_exit_camera`; send notification via `notify.maxi_notification_group` ('Someone is in the parking exit!'); if trigger id `parking` matched, call `camera.snapshot` on `camera.parking_camera`; send notification via `notify.maxi_notification_group` ('Someone is in the parking!'); +2 more branches; wait for event `mobile_app_notification_action` with action=`blow_horn_back` (timeout 5m); +1 more actions.

- **Alert when the horn goes off and offer to turn it off** — **Automation ID:** [1716935540936](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2086)

  Triggered when `switch.plug_front_door_horn` changes to `on` for 0s. Actions: call `camera.snapshot` on `camera.front_door_camera`; send notification via `notify.maxi_notification_group` ('Horn started'); +3 more actions.

- **Alert when there is motion inside during arm away and trigger alarm** — **Automation ID:** [1717354512446](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2635)

  Triggered when `binary_sensor.sensor_back_outside_door_contact`, `binary_sensor.sensor_server_door_contact`, `binary_sensor.sensor_front_door_contact`, `binary_sensor.sensor_patio_door_contact` +1 more changes from `off` to `on` for 15s; `binary_sensor.multi_room_security_motion_occupancy` changes. Runs only if (`alarm_control_panel.home_alarm` is `armed_away` or `alarm_control_panel.home_alarm` is `triggered`). Actions: turn off `input_boolean.following_music`; turn off `light.group_all_lights`, `light.group_all_inside_lights`, `light.group_patio_lights`, `light.front_door_light` +1 more; +3 more actions.

- **Analyze person in the parking when car is present to detect burglary** — **Automation ID:** [1742998769189](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6181)

  Triggered when `binary_sensor.parking_exit_camera_person_occupancy` changes to `on` for 1m; `binary_sensor.parking_exit_camera_person_occupancy` changes to `on` for 15s; `binary_sensor.group_parking_occupancy` changes to `on` for 15s. Runs only if (`person.myriam` is `home` or `binary_sensor.parking_camera_car_occupancy` is `on`). Actions: call `llmvision.stream_analyzer` on (temperature=0.1, message='Answer only with yes or no; Is this person trying to enter the car?'); +7 more actions.

- **Arm Alarm-Away when "User home" is off for 10 minutes** — **Automation ID:** [1713533768680](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L431)

  Triggered when `binary_sensor.user_home` changes from `on` to `off` for 10m; `zone.home` changes to `0` for 10m. Actions: call `alarm_control_panel.alarm_arm_away` on `alarm_control_panel.home_alarm`.

- **Arm night alarm when everyone is sleeping** — **Automation ID:** [1717284487763](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2386)

  Triggered when `binary_sensor.all_residents_sleeping` changes to `on` for 5m. Actions: call `alarm_control_panel.alarm_arm_night` on `alarm_control_panel.home_alarm`.

- **Assist Show cameras on TVs** — **Automation ID:** [1730428380200](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4037)

  Triggered when conversation command `[Show] [me] [the] cameras  [on] [the] [tvs] [please]`, `Security Audit`, `[Put] [the] cameras [on] [the] [tvs]`. Actions: compute variables; repeat call `dash_cast.load_url` on `{{all_chromecasts[repeat.index - 1]}}`.

- **Auto-lock doors when closed for 5 seconds** — **Automation ID:** [1713378517441](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L96)

  Triggered when `binary_sensor.sensor_front_outside_door_contact` changes to `off` for 5s; `binary_sensor.sensor_back_outside_door_contact` changes to `off` for 5s. Actions: if trigger id `front_door_closed` matched and not (`lock.lock_front_door` is `locked`), lock `lock.lock_front_door`; if trigger id `back_door_closed` matched and not (`lock.back_door` is `locked`), lock `lock.back_door`.

- **Automatically turn back door light back off** — **Automation ID:** [1735346521960](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5205)

  Triggered when `light.back_door_light` changes to `on` for 10m. Runs only if `binary_sensor.back_door_camera_person_occupancy` is `off`. Actions: turn off `light.back_door_light` (transition=60).

- **Automatically turn off Back door light when there is no motion** — **Automation ID:** [1744811669242](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7218)

  Triggered when `binary_sensor.back_door_occupancy` changes from `on` to `off` for 2m. Actions: turn off `light.back_door_light` (transition=60); turn off `light.reolink_back_door_camera_floodlight`.

- **Automatically turn off the back security horn after 2 minutes and ask if it should restart** — **Automation ID:** [1717016954049](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2199)

  Triggered when `switch.plug_back_door_horn` changes to `on` for 2m. Actions: turn off `switch.plug_back_door_horn`; call `camera.snapshot` on `camera.back_door_camera`; +3 more actions.

- **Automatically turn off the front security horn after 2 minutes and ask if it should restart** — **Automation ID:** [1716933958613](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2030)

  Triggered when `switch.plug_front_door_horn` changes to `on` for 2m. Actions: turn off `switch.plug_front_door_horn`; call `camera.snapshot` on `camera.front_door_camera`; +3 more actions.

- **Back door floodlight auto-mode on sunset, off on sunrise** — **Automation ID:** [1744811034444](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7180)

  Triggered when sun event `sunset`; sun event `sunrise`. Actions: if trigger id `sunset` matched, set option on `select.reolink_back_door_camera_floodlight_mode` (option='auto'); if trigger id `sunrise` matched, set option on `select.reolink_back_door_camera_floodlight_mode` (option='off').

- **Back lock automation on main codes** — **Automation ID:** [1749940499172](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9428)

  Triggered when device action `event.notification.notification`. Runs only if template `{{ trigger.event.data.parameters.userId and trigger.event.data.event_label == 'Keypad unlock operation' }}` is true. Actions: compute variables; run `script.broadcast_alert_in_the_house`.

- **Control brightness of Front door Phone to avoid discharge** — **Automation ID:** [1740690697244](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5562)

  Triggered when `sensor.front_door_phone_battery` changes; `sensor.front_door_phone_battery` goes below `50`; `sensor.front_door_phone_battery` goes above `90`. Actions: if `sensor.front_door_phone_battery` is below `15` and `switch.front_door_phone_screen` is `on`, call `number.set_value` on `number.front_door_phone_screen_brightness`; turn off `switch.front_door_phone_screen`; if `sensor.front_door_phone_battery` is below `50` and `switch.front_door_phone_screen` is `on`, call `number.set_value` on `number.front_door_phone_screen_brightness`; +1 more branches.

- **Disable automatic back horn when Alarm system is disarmed ** — **Automation ID:** [1748329710904](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8122)

  Triggered when `alarm_control_panel.home_alarm` changes to `disarmed`. Actions: turn off `input_boolean.maxi_is_scared`.

- **Disable automatic back horn when lylou awakes between 7:45 and 8:45** — **Automation ID:** [1758115238753](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11954)

  Triggered when `binary_sensor.lylou_is_asleep_bayesian` changes from `on` to `off` for 3m. Runs only if time is after `07:45:00` and before `08:45:00`. Actions: turn off `input_boolean.maxi_is_scared`.

- **Disable automatic back horn when myriam awakes** — **Automation ID:** [1750625732180](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9724)

  Triggered when `sensor.myriam_is_sleeping_at_home` changes from `asleep` to `awake` for 3m. Runs only if time is after `06:45:00` and before `20:00:00`. Actions: turn off `input_boolean.maxi_is_scared`.

- **Disable Automatic Back Horn when resident arrives** — **Automation ID:** [1747074693573](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7461)

  Triggered when `person.myriam`, `person.maximiliano`, `person.lylou` changes from `not_home` to `home`. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 120 and trigger.from_state.sta` is true; `input_boolean.maxi_is_scared` is `on`. Actions: wait for `binary_sensor.back_door_camera_person_occupancy` changes from `off` to `on` (timeout 10m); turn off `input_boolean.maxi_is_scared`; turn on `light.back_door_light` (brightness_pct=100, rgb_color=[44, 255, 13]).

- **Disarm alarm night when someone awakes** — **Automation ID:** [1717284510591](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2407)

  Triggered when `binary_sensor.all_residents_sleeping` changes from `on` to `off` for 0s. Runs only if `alarm_control_panel.home_alarm` is `armed_night`; condition `sun`. Actions: call `alarm_control_panel.alarm_disarm` on `alarm_control_panel.home_alarm`.

- **Disarm alarm when sun rises if everyone present is awake** — **Automation ID:** [1760963671925](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12202)

  Triggered when `sun.sun` changes from `below_horizon` to `above_horizon` for 0s. Runs only if `alarm_control_panel.home_alarm` is `armed_night`; `binary_sensor.all_residents_sleeping` is `off`. Actions: call `alarm_control_panel.alarm_disarm` on `alarm_control_panel.home_alarm`.

- **Disarm alarm-away when "User Home" turns on** — **Automation ID:** [1713533595890](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L410)

  Triggered when `binary_sensor.user_home` changes from `off` to `on`. Runs only if `alarm_control_panel.home_alarm` is `armed_away`. Actions: call `alarm_control_panel.alarm_disarm` on `alarm_control_panel.home_alarm`.

- **Doorbell Main automation** — **Automation ID:** [1715717946486](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1704)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to turn lights on with presets on `input_boolean.door_rang` plus 1 other targets.

- **Doorbell Show received notification answer on phone** — **Automation ID:** [1715791112355](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1857)

  Triggered when event `mobile_app_notification_action` with action=`REPLY`. Actions: call `input_text.set_value` on `input_text.front_door_phone_text`.

- **Enable automatic Back Door Horn when Lylou is alone** — **Automation ID:** [1748287626823](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8100)

  Triggered when `binary_sensor.lylou_alone` changes from `off` to `on` for 10m. Actions: turn on `input_boolean.maxi_is_scared`.

- **FRigate Arriving only Notification** — **Automation ID:** [1752001220545](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9945)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Frigate back Doors Notifications ** — **Automation ID:** [1744744086412](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7019)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Frigate front Doors Notifications ** — **Automation ID:** [1745468097447](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7388)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Frigate Parking notifications** — **Automation ID:** [1745467773993](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7336)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Front door automation on unlock for conditional codes** — **Automation ID:** [1729519649402](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3880)

  Triggered when attribute `action` on `lock.lock_front_door` changes to `unlock_failure_invalid_schedule` for 1s. Runs only if template `{{ states('sensor.lock_front_door_action_source_name') == 'keypad' }}` is true; `person.maximiliano` is `home`. Actions: compute variables; if template `{{ "Unknown" in user_opening }}` is true, run `script.broadcast_alert_in_the_house`; if template `{{ "Unknown" not in user_opening }}` is true, unlock `lock.lock_front_door`; run `script.broadcast_alert_in_the_house`.

- **Front door automation on unlock for main codes** — **Automation ID:** [1723214966525](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3242)

  Triggered when `sensor.lock_front_door_action_user` changes to `1`; `sensor.lock_front_door_action_user` changes to `2`; +3 more triggers. Actions: call `alarm_control_panel.alarm_disarm` on `alarm_control_panel.home_alarm`; compute variables; +1 more actions.

- **Lock tablets on triggered alarm** — **Automation ID:** [1717360188815](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3010)

  Triggered when `alarm_control_panel.home_alarm` changes to `triggered`. Actions: turn on `switch.hallway_tablet_maintenance_mode`, `switch.workshop_tablet_maintenance_mode`, `switch.tablet_kitchen_s6_lite_maintenance_mode`, `switch.closet_tablet_maintenance_mode`.

- **Lower volume if police is detected in the front during a party** — **Automation ID:** [1732406375683](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4531)

  Triggered when `sensor.front_door_camera_person_count` goes above `0`. Runs only if `input_boolean.party_mode` is `on`; `sensor.template_average_unmuted_speakers_volume` is above `0.4`. Actions: call `llmvision.image_analyzer` on (temperature=0.1, message="Answer only with 'yes' or 'no', is there a police officer or a security guard on this image? Only use one word to answer"); if template `{{ "YES" in answer.response_text | upper }}` is true, send notification via `notify.maxi_notification_group` ('Police detected at the front door'); set volume on `media_player.mass_hallway_wiim_speaker`, `media_player.mass_workshop_wiim_speaker`, `media_player.mass_kitchen_wiim_speaker`, `media_player.mass_salon_wiim_speaker` +1 more (volume_level=0.3).

- **Notify on potential package** — **Automation ID:** [1716329212232](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1901)

  Triggered when `binary_sensor.front_door_camera_person_occupancy` changes to `on` for 2s; `binary_sensor.front_door_camera_package_occupancy` changes to `on` for 1s. Runs only if `input_boolean.waiting_package` is `on`. Actions: set option on `select.front_door_ptz_camera_ptz_preset` (option='Package'); wait 3s; +2 more actions.

- **Notify when someone leaves through front door** — **Automation ID:** [1729698675763](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3940)

  Triggered when `sensor.lock_front_door_action` changes to `manual_unlock`. Actions: run `script.non_important_broadcast_alert_in_the_house`.

- **Post-alarm disarm recovery actions** — **Automation ID:** [1717356233481](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2731)

  Triggered when `alarm_control_panel.home_alarm` changes from `triggered` to `disarmed`. Actions: stop media on `media_player.group_mass_home_group_speakers`, `media_player.mass_bathroom_vpe_speaker`, `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_closet_wiim_speaker` +5 more; clear playlist on `media_player.group_mass_home_group_speakers`, `media_player.mass_bathroom_vpe_speaker`, `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_closet_wiim_speaker` +5 more; +2 more actions.

- **Re-lock doors 5 minutes after unlocking if door is shut** — **Automation ID:** [1713378679048](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L152)

  Triggered when `lock.lock_front_door` changes to `unlocked` for 5m; `lock.back_door` changes to `unlocked` for 5m. Actions: if trigger id `front_door_unlocked` matched and `binary_sensor.sensor_front_outside_door_contact` is `off`, lock `lock.lock_front_door`; if trigger id `back_door_unlocked` matched and `binary_sensor.sensor_back_outside_door_contact` is `off`, lock `lock.back_door`.

- **Reload tablets when frigate changes state** — **Automation ID:** [1714405984049](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1415)

  Triggered when `sensor.frigate_status` changes for 3m. Actions: press `button.hallway_tablet_load_start_url`, `button.salon_desk_tablet_load_start_url`.

- **Reset doorbell stuff on unlock** — **Automation ID:** [1715719948033](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1783)

  Triggered when `lock.lock_front_door` changes to `unlocked`; `lock.lock_front_door` changes to `locked` for 45s; `input_boolean.door_rang` changes to `on` for 4m; `input_boolean.door_double_rang` changes to `on` for 4m. Actions: turn off `input_boolean.door_rang`, `input_boolean.door_double_rang`, `input_boolean.waiting_someone`; call `input_text.set_value` on `input_text.front_door_phone_text`.

- **Return PTZ to doorway after 10 minutes** — **Automation ID:** [1751650700641](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9846)

  Triggered when event `call_service`. Actions: wait 10m; set option on `select.front_door_ptz_camera_ptz_preset` (option='Doorway').

- **Ring horn in the back if someone tries to approach while maxi is scared** — **Automation ID:** [1743747879302](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6808)

  Triggered when `binary_sensor.back_entrance_presence_person_occupancy` changes from `off` to `on` for 2s; `binary_sensor.back_door_presence_person_occupancy` changes from `off` to `on` for 2s. Runs only if `input_boolean.maxi_is_scared` is `on`. Actions: turn on `switch.plug_back_door_horn`; call `dash_cast.load_url` on `media_player.bedroom_chromecast`; +5 more actions.

- **Ring parking horn on actionnable notification from mobile** — **Automation ID:** [1760286041191](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12073)

  Triggered when event `mobile_app_notification_action` with action=`turn_parking_horn_on`. Actions: turn on `siren.parking_exit_camera_siren`; wait for `binary_sensor.group_parking_occupancy` changes from `on` to `off` for 7s (timeout 5m); +1 more actions.

- **Set front door screen text** — **Automation ID:** [1717038729859](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2336)

  Triggered when event `mobile_app_notification_action` with action=`REPLY`. Actions: turn on `switch.front_door_phone_screen`; call `input_text.set_value` on `input_text.front_door_phone_text`; +4 more actions.

- **Start turning lights on before maxi's alarm rings on week days for work** — **Automation ID:** [1766609371758](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14861)

  Triggered when time trigger at `{'entity_id': 'sensor.phone_maxi_next_alarm', 'offset': '-00:02:00'}`. Runs only if time is after `07:30:00` and before `09:30:00` and weekday in fri, thu, wed, tue, mon. Actions: turn on `{{ area_entities(states('sensor.where_is_maxi_sleeping')) | select('search','^light.group')| reject('search', 'uv_lamp') | list  }}
` (brightness_pct=51, color_temp_kelvin=3311, transition=600); wait 45m.

- **Toggle parking audio detection based on patio occupancy** — **Automation ID:** [1744572056958](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6978)

  Triggered when `binary_sensor.group_patio_motion_occupancy` changes to `on`; `binary_sensor.group_patio_motion_occupancy` changes to `off` for 1m. Actions: if trigger id `patio_occupied` matched, turn off `switch.parking_exit_camera_audio_detection`; if trigger id `patio_clear` matched, turn on `switch.parking_exit_camera_audio_detection`.

- **Trigger back horn when someone loiters for too long at the door** — **Automation ID:** [1731941986104](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4372)

  Triggered when `binary_sensor.back_door_loitering_person_occupancy` changes to `on` for 2m. Runs only if not (`alarm_control_panel.home_alarm` is `disarmed`). Actions: turn on `switch.plug_back_door_horn`; run `script.find_phone`; +3 more actions.

- **Trigger front horn through phone notification** — **Automation ID:** [1717355583664](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2713)

  Triggered when event `mobile_app_notification_action` with action=`blow_front_horn`. Actions: turn on `switch.plug_front_door_horn`.

- **Trigger in-home defense script after the alarm is triggered for 3 continuous minutes** — **Automation ID:** [1717358289455](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2788)

  Triggered when `alarm_control_panel.home_alarm` changes to `triggered` for 3m. Runs only if `binary_sensor.group_security_occupancy_sensors` is `on`. Actions: run `script.in_home_defense_script`.

- **Trigger police announcement on triggered alarm** — **Automation ID:** [1717358415139](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2813)

  Triggered when `alarm_control_panel.home_alarm` changes to `triggered` for 0s. Actions: send notification via `notify.users_notification_group` ('The alarm has been triggered, engaging de-escalation'); turn off `input_boolean.following_music`; +7 more actions.

- **Turn back door lights on when someone is detected** — **Automation ID:** [1713408006382](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L275)

  Triggered when `binary_sensor.back_entrance_presence_person_occupancy`, `binary_sensor.reolink_back_door_camera_person`, `binary_sensor.hotbox_window_presence_person_occupancy`, `binary_sensor.back_door_camera_person_occupancy` changes to `on`. Actions: turn on `light.back_door_light` (brightness=255, color_temp_kelvin=254).

- **Turn camera to doorway on doorbell action** — **Automation ID:** [1716912930623](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1986)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to control lights and media on `select.front_door_ptz_camera_ptz_preset`.

- **Turn off Back camera siren when back horn goes off** — **Automation ID:** [1747066080524](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7443)

  Triggered when `switch.plug_back_door_horn` changes from `on` to `off`. Actions: turn off `siren.reolink_back_door_camera_siren`.

- **Turn off back horn when maxi is scared is disabled** — **Automation ID:** [1747367207590](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7573)

  Triggered when `input_boolean.maxi_is_scared` changes from `on` to `off`. Runs only if `switch.plug_back_door_horn` is `on`. Actions: turn off `switch.plug_back_door_horn`.

- **Turn off cameras on screens when door is locked back** — **Automation ID:** [1757350225543](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11729)

  Triggered when `lock.lock_front_door` changes from `unlocked` to `locked`; `input_boolean.door_rang` changes to `on` for 4m; `input_boolean.door_double_rang` changes to `on` for 4m. Actions: compute variables; turn off `{{ integration_entities('cast') |  select('search', 'chromecast')|  select('is_state_attr', 'app_name', 'DashCast') | expand | map(attribute='entity_id')| list }}
`.

- **Turn off front door screen if it remains open 10 minutes** — **Automation ID:** [1740757033954](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5682)

  Triggered when `switch.front_door_phone_screen` changes to `on` for 10m. Runs only if `switch.front_door_phone_screen` is `on`. Actions: turn off `switch.front_door_phone_screen`.

- **Turn off front door screen on clear of occupancy** — **Automation ID:** [1740691064559](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5657)

  Triggered when `binary_sensor.front_door_camera_person_occupancy` changes from `on` to `off` for 1m. Runs only if `switch.front_door_phone_screen` is `on`. Actions: turn off `switch.front_door_phone_screen`.

- **Turn off Maxi is Scared when all residents sleeping turns off during daytime** — **Automation ID:** [1747634049446](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7797)

  Triggered when `binary_sensor.all_residents_sleeping` changes from `on` to `off` for 0s. Runs only if `sun.sun` is `above_horizon`. Actions: turn off `input_boolean.maxi_is_scared`.

- **Turn off parking light 5 minutes after it turns on if there's no person presence** — **Automation ID:** [1744235712389](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6924)

  Triggered when `light.plug_parking_lamp` changes to `on` for 5m. Actions: if `binary_sensor.parking_camera_person_occupancy` is `off` and `binary_sensor.parking_exit_camera_person_occupancy` is `off`, turn off `light.plug_parking_lamp`; if (`binary_sensor.parking_camera_person_occupancy` is `on` or `binary_sensor.parking_exit_camera_person_occupancy` is `on`), wait for `binary_sensor.parking_exit_camera_person_occupancy` changes to `off` for 5m; turn off `light.plug_parking_lamp`.

- **Turn off Parking light on 5 minutes of no occupancy** — **Automation ID:** [1742862955443](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6159)

  Triggered when `binary_sensor.group_parking_occupancy` changes from `on` to `off` for 5m. Actions: turn off `light.plug_parking_lamp`.

- **Turn off the front door light in relation to the sun position** — **Automation ID:** [1729518874101](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3857)

  Triggered when `sun.sun` changes to `above_horizon` for 15m. Actions: turn off `light.front_door_light` (transition=300).

- **Turn on Automatic back Horn upon receiving notification answer** — **Automation ID:** [1747075906852](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7555)

  Triggered when event `mobile_app_notification_action` with action=`turn_automatic_back_horn_on`. Actions: turn on `input_boolean.maxi_is_scared`.

- **Turn on back door light when door opens** — **Automation ID:** [1743954245142](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6900)

  Triggered when `binary_sensor.sensor_back_outside_door_contact` changes from `off` to `on` for 0s. Actions: turn on `light.back_door_light` (brightness_pct=100, color_temp_kelvin=4882).

- **Turn on back door light when resident arrives through parking** — **Automation ID:** [1749955882836](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9486)

  Triggered when `person.myriam`, `person.maximiliano`, `person.lylou` changes from `not_home` to `home`. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 300 and trigger.from_state.sta` is true. Actions: wait for `binary_sensor.parking_camera_person_occupancy`, `binary_sensor.parking_exit_camera_person_occupancy` changes from `off` to `on` (timeout 10m); turn on `light.back_door_light` (brightness_pct=100, color_temp_kelvin=3726).

- **Turn on camera floodlight when someone is detected and maxi is scared is on** — **Automation ID:** [1744810927278](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7155)

  Triggered when `binary_sensor.hotbox_window_presence_person_occupancy`, `binary_sensor.back_entrance_presence_person_occupancy`, `binary_sensor.reolink_back_door_camera_person` changes from `off` to `on`. Runs only if `input_boolean.maxi_is_scared` is `on`. Actions: turn on `light.reolink_back_door_camera_floodlight` (brightness_pct=100).

- **Turn on front door screen on occupancy** — **Automation ID:** [1740691034464](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5636)

  Triggered when `binary_sensor.front_door_camera_person_occupancy` changes to `on`. Runs only if `switch.front_door_phone_screen` is `off`. Actions: turn on `switch.front_door_phone_screen`.

- **Turn on Maxi is scared upon alarm arming away** — **Automation ID:** [1747705240902](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7908)

  Triggered when `alarm_control_panel.home_alarm` changes to `armed_away`. Runs only if `zone.home` is `0`. Actions: turn on `input_boolean.maxi_is_scared`.

- **Turn on Maxi is scared when Alarm is armed night** — **Automation ID:** [1752029259001](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10014)

  Triggered when `alarm_control_panel.home_alarm` changes to `armed_night`. Runs only if time is after `21:00:00` and before `08:00:00`. Actions: turn on `input_boolean.maxi_is_scared`.

- **Turn on Maxi is scared when All Residents are sleeping** — **Automation ID:** [1747633994244](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7772)

  Triggered when `binary_sensor.all_residents_sleeping` changes from `off` to `on` for 5m. Runs only if time is after `21:30:00` and before `08:00:00`. Actions: turn on `input_boolean.maxi_is_scared`.

- **Turn on Parking Infrared when Parking Exit infrared goes off** — **Automation ID:** [1745368052673](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7302)

  Triggered when `sensor.parking_exit_camera_day_night_state` changes to `day`; `sensor.parking_exit_camera_day_night_state` changes to `unavailable`; `sensor.parking_exit_camera_day_night_state` changes to `unknown`; `switch.parking_exit_camera_infrared_lights_in_night_mode` changes from `on` to `off`. Actions: turn on `switch.reolink_parking_camera_infrared_lights_in_night_mode`.

- **Turn on parking light on any parking/balcony activity** — **Automation ID:** [1742862883906](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6122)

  Triggered when `binary_sensor.parking_exit_camera_person_occupancy`, `binary_sensor.parking_camera_person_occupancy`, `binary_sensor.reolink_parking_camera_person`, `binary_sensor.reolink_parking_exit_camera_person` changes to `on`; `binary_sensor.back_entrance_presence_person_occupancy`, `binary_sensor.reolink_back_door_camera_person` changes to `on`; `binary_sensor.parking_camera_car_occupancy`, `binary_sensor.parking_exit_camera_car_occupancy` changes from `off` to `on` for 3s. Actions: turn on `light.plug_parking_lamp`.

- **Turn on the front door light in relation to the sun position** — **Automation ID:** [1733789430500](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4743)

  Triggered when `sun.sun` changes to `below_horizon` for 1m. Actions: repeat turn on `light.front_door_light` (brightness_pct=100, color_temp_kelvin=4628, transition=20) every 30m.

- **Unlock tablets on disarmed alarm** — **Automation ID:** [1717360231281](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3031)

  Triggered when `alarm_control_panel.home_alarm` changes to `disarmed` for 0s. Actions: turn off `switch.hallway_tablet_maintenance_mode`, `switch.workshop_tablet_maintenance_mode`, `switch.tablet_kitchen_s6_lite_maintenance_mode`, `switch.closet_tablet_maintenance_mode`.

<a id="media-audio-entertainment-48"></a>
## Media, Audio & Entertainment (48)

[Back to top](#top)

- **Add playing Spotify or tidal artist to Lidarr** — **Automation ID:** [1757814887686](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11852)

  Triggered when attribute `media_artist` on `media_player.group_mass_home_group_speakers` changes. Runs only if template `{{ 'spotify' in (trigger.to_state.attributes.media_content_id | default('')) or 'tidal' in (trigger.to_state.attributes.` is true. Actions: call `rest_command.music_brainz_artist_query` on; compute variables; +2 more actions.

- **Adjust all player speakers volume when myriam arrives** — **Automation ID:** [1750445618240](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9688)

  Triggered when `person.myriam` changes from `not_home` to `home`. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 600 and trigger.from_state.sta` is true. Actions: compute variables; repeat run `script.speaker_automatic_volume_adjustment_script`.

- **Adjust Chromecast volume when unmuted after a long moment** — **Automation ID:** [1749001900174](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8819)

  Triggered when attribute `is_volume_muted` on `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +1 more changes. Runs only if template `{{ trigger.from_state.state == trigger.to_state.state }}` is true; template `{{ trigger.from_state.attributes.is_volume_muted == true and trigger.to_state.attributes.is_volume_muted == false }}` is true; +1 more conditions. Actions: run `script.speaker_automatic_volume_adjustment_script`.

- **Automatically adjust bathroom volume on unmute** — **Automation ID:** [1767744894962](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15247)

  Triggered when attribute `is_volume_muted` on `media_player.mass_bathroom_vpe_speaker` changes from `true` to `false`. Runs only if template `{{ is_state('script.broadcast_alert_in_the_house', 'off') and is_state('script.non_important_broadcast_alert_in_the_hous` is true; template `{{ (now() - trigger.from_state.last_changed) > timedelta(seconds=30) }}` is true. Actions: compute variables; run `script.speaker_automatic_volume_adjustment_script`.

- **Automatically turn off TVs** — **Automation ID:** [1716328766276](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1876)

  Triggered when `binary_sensor.group_workshop_motion_occupancy`, `binary_sensor.group_salon_motion_occupancy` changes to `off` for 20m. Actions: send Google Assistant command `Power off {{ [area_id(trigger.entity_id)]    | map('area_entities') | sum(start=[])  |    select('search', '^media_player\.(?!.*?universal.*?).*chromecast') |  map('state_attr', 'friendly_name') | unique | list | first }}
`.

- **Conditionally Unmute Speakers when a room is occupied** — **Automation ID:** [1713719722990](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L830)

  Triggered when `binary_sensor.group_closet_motion_occupancy`, `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_bedroom_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy` +5 more changes to `on` for 0s. Runs only if `input_boolean.following_music` is `on`; template `{{ trigger.to_state.state == "on" }}` is true; +3 more conditions. Actions: compute variables; unmute `{{playing_speakers}}`; +1 more actions.

- **Control volume of kitchen to prevent noise in children room while she sleeps** — **Automation ID:** [1748845695784](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8434)

  Triggered when `input_boolean.sleeper_in_hotbox` changes from `off` to `on`; attribute `volume_level` on `media_player.mass_kitchen_wiim_speaker` changes. Runs only if `input_boolean.sleeper_in_hotbox` is `on`; time is after `21:30:00` and before `09:00:00`; +1 more conditions. Actions: set volume on `media_player.mass_kitchen_wiim_speaker` (volume_level=0.29).

- **Enable Following Music When all residents sleeping goes to ON after being OFF for over 1hour** — **Automation ID:** [1748225477514](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8069)

  Triggered when `binary_sensor.all_present_sleeping` changes from `off` to `on` for 0s. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 3600}}` is true; template `{{ (trigger.from_state.state | string | lower == 'off') }}` is true. Actions: turn on `input_boolean.following_music`.

- **Immediately mute bathroom speaker when door open and stuff is playing around** — **Automation ID:** [1744750478317](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7117)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `off` to `on` for 0s. Runs only if (`media_player.mass_kitchen_wiim_speaker` is `playing` or `media_player.mass_workshop_wiim_speaker` is `playing` +2 more). Actions: mute `media_player.mass_bathroom_vpe_speaker`.

- **Immediately mute bathroom speaker when door open if all residents are sleeping** — **Automation ID:** [1748939696079](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8793)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `off` to `on` for 0s. Runs only if `binary_sensor.all_residents_sleeping` is `on`. Actions: mute `media_player.mass_bathroom_vpe_speaker`.

- **Lower loud volume when someone exits the shower** — **Automation ID:** [1757892274893](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11913)

  Triggered when `binary_sensor.bathroom_radarr_occupancy_sensor_zone_2_occupancy` changes from `on` to `off` for 5s. Runs only if `binary_sensor.bathroom_radarr_occupancy_sensor_zone_1_occupancy` is `on`; `binary_sensor.shower_in_usage` is `on`; +2 more conditions. Actions: set volume on `media_player.mass_bathroom_vpe_speaker` (volume_level=0.67).

- **Lower sound when assist in progress** — **Automation ID:** [1727112478403](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3414)

  Triggered when `assist_satellite.esp32_s3_box_kitchen_assistant_assist_satellite`, `assist_satellite.saloon_assistant_assist_satellite`, `assist_satellite.lv_assistant_assist_satellite`, `assist_satellite.bathroom_assistant_assist_satellite` +5 more changes to `listening`. Actions: compute variables; run in parallel: if template `{{ playing_speakers | length > 0 and playing_speakers[0] != ""}}` is true, compute variables; set volume on `{{playing_speakers.0}}` (volume_level='{{ desired_volume }}\n'); if template `{{ playing_chromecasts | length > 0 and playing_chromecasts[0] != ""}}` is true, compute variables; set volume on `{{playing_chromecasts.0}}` (volume_level='{{ desired_volume }}\n').

- **Music Assistant - Local(only) Voice Support Blueprint** — **Automation ID:** [1753133885395](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11162)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Mute Bathroom Speaker Muzak v3** — **Automation ID:** [1767743397238](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15104)

  Triggered when `binary_sensor.group_bathroom_motion_occupancy` changes from `on` to `off` for 5s; `binary_sensor.sensor_bathroom_door_contact` changes from `off`, `on`. Runs only if `binary_sensor.sensor_bathroom_door_contact` is `on`. Actions: if `binary_sensor.group_bathroom_motion_occupancy` is `on`, if not (`sensor.active_chromecast_rooms` is `[]`), mute `media_player.mass_bathroom_vpe_speaker`; if `media_player.mass_kitchen_wiim_speaker` is `playing`, `on`, `buffering` and `media_player.mass_workshop_wiim_speaker` is `playing`, `on`, `buffering`, mute `media_player.mass_bathroom_vpe_speaker`; otherwise wait for `binary_sensor.group_bathroom_motion_occupancy` changes from `on` to `off` for 15s; mute `media_player.mass_bathroom_vpe_speaker`; +1 more branches; if `binary_sensor.group_bathroom_motion_occupancy` is `off`, mute `media_player.mass_bathroom_vpe_speaker`.

- **Mute bathroom speaker when door opens** — **Automation ID:** [1734911416848](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5070)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `off` to `on` for 5s. Runs only if `binary_sensor.all_residents_sleeping` is `off`. Actions: if `binary_sensor.group_bathroom_motion_occupancy` is `off`, mute `media_player.mass_bathroom_vpe_speaker`; if `binary_sensor.group_bathroom_motion_occupancy` is `on`, if `sensor.active_chromecast_rooms` is `[]`, set volume on `media_player.mass_bathroom_vpe_speaker` (volume_level=0.2); if not (`sensor.active_chromecast_rooms` is `[]`; not (`sensor.active_speakers_rooms` is `['bathroom']`)), mute `media_player.mass_bathroom_vpe_speaker`; otherwise wait for `binary_sensor.group_bathroom_motion_occupancy` changes from `on` to `off` for 15s (timeout 1h 10m); mute `media_player.mass_bathroom_vpe_speaker`.

- **Mute bathroom speaker when no motion and door open** — **Automation ID:** [1738094823714](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5360)

  Triggered when `binary_sensor.group_bathroom_motion_occupancy` changes to `off` for 3m. Runs only if `binary_sensor.sensor_bathroom_door_contact` is `on`. Actions: mute `media_player.mass_bathroom_vpe_speaker`.

- **Mute chromecasts when they play PLEX visuals** — **Automation ID:** [1714150045572](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1226)

  Triggered when `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_bedroom_chromecast` +2 more changes to `playing`; attribute `is_volume_muted` on `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_bedroom_chromecast` +2 more changes; attribute `media_title` on `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_bedroom_chromecast` +2 more changes. Runs only if template `{{ state_attr(state_attr(trigger.entity_id,'active_child'),'media_library_title') == 'Visuals'}}` is true; template `{{trigger.to_state.attributes.is_volume_muted == false }}` is true. Actions: mute `{{trigger.entity_id}}`.

- **Mute Chromecasts when unmuted speakers plays in the same room** — **Automation ID:** [1748932635965](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8682)

  Triggered when `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +1 more changes to `playing`; attribute `is_volume_muted` on `media_player.mass_workshop_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_bedroom_wiim_speaker` changes for 1s; `media_player.mass_workshop_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_bedroom_wiim_speaker` changes to `playing` for 2s. Runs only if template `{% if trigger.id == 'speaker_unmuted' %} {{ trigger.to_state.attributes.is_volume_muted == false }} {% endif %}` is true. Actions: compute variables; if template `{{ unmuted_playing_speakers != [] }}` is true, mute `{{player_chromecasts | list}}`.

- **Mute speaker in room if Myriam receives or makes call** — **Automation ID:** [1750025216771](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9530)

  Triggered when `binary_sensor.myriam_is_talking` changes from `off` to `on`. Runs only if `input_boolean.following_music` is `on`; `binary_sensor.myriam_is_talking` is `on`. Actions: compute variables; mute `{{playing_speakers}}
`; +2 more actions.

- **Mute speakers in rooms if myriam enters while talking ** — **Automation ID:** [1757437097586](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11773)

  Triggered when `sensor.bermuda_myriam_phone_area` changes. Runs only if `input_boolean.following_music` is `on`; `binary_sensor.myriam_is_talking` is `on`. Actions: compute variables; mute `{{playing_speakers}}
`; +2 more actions.

- **Mute speakers in the room if maxi uses a microphone** — **Automation ID:** [1714321256821](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1312)

  Triggered when `binary_sensor.maxi_is_talking` changes from `off` to `on`; `sensor.maxi_location_by_petro` changes for 2s. Runs only if `input_boolean.following_music` is `on`; `binary_sensor.maxi_is_talking` is `on`. Actions: compute variables; mute `{{playing_speakers}}
`.

- **Mute speakers when the room is empty** — **Automation ID:** [1713826329670](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1001)

  Triggered when `binary_sensor.group_closet_motion_occupancy`, `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy`, `binary_sensor.group_kitchen_motion_occupancy` +3 more changes to `off` for 5m. Runs only if `input_boolean.following_music` is `on`; template `{{ states('sensor.maxi_location_by_petro') not in [area_id(trigger.entity_id)] | string }}` is true. Actions: compute variables; if template `{{ trigger.to_state.state == "off" }}` is true, mute `{{playing_speakers}}
`.

- **Mute speakers when unmuted chromecast plays in the same room** — **Automation ID:** [1714403972538](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1353)

  Triggered when `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +3 more changes to `playing`; `media_player.mass_workshop_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_kitchen_wiim_speaker` changes to `playing` for 2s; attribute `is_volume_muted` on `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +2 more changes. Runs only if template `{{trigger.to_state.state != 'unavailable'}}` is true. Actions: compute variables; if template `{{ unmuted_player_chromecasts != [] }}` is true, mute `{{playing_speakers | list}}`.

- **Play Muzak in the bathroom when someone enters** — **Automation ID:** [1713535684569](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L500)

  Triggered when `binary_sensor.group_bathroom_motion_occupancy` changes from `off` to `on`. Runs only if not (`media_player.mass_bathroom_vpe_speaker` is `playing`; `media_player.mass_kitchen_wiim_speaker` is `playing`; +2 more); `binary_sensor.sensor_bathroom_door_contact` is `on`. Actions: call `media_player.shuffle_set` on `media_player.mass_bathroom_vpe_speaker` (shuffle=True); play media via `music_assistant.play_media` on `media_player.mass_bathroom_vpe_speaker` (media_id='Bathroom Muzak', media_type='playlist'); +1 more actions.

- **Play Random Music when maxi awakes at home** — **Automation ID:** [1708472009602](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1)

  Triggered when `sensor.is_maxi_asleep` changes from `asleep` to `awake` for 2m. Runs only if `person.maximiliano` is `home`; `binary_sensor.all_residents_sleeping` is `off`; +2 more conditions. Actions: if time is after `03:00:00` and before `11:00:00`, call `media_player.shuffle_set` on `media_player.group_mass_home_group_speakers` (shuffle=True); play media via `music_assistant.play_media` on `media_player.group_mass_home_group_speakers` (media_id='Myriam Morning Music', media_type='playlist'); otherwise run `script.start_a_random_maxi_playlist`.

- **Raise music on the playing speaker when the Bathroom fans goes on** — **Automation ID:** [1713536409397](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L573)

  Triggered when `switch.bathroom_fan` changes from `off` to `on`. Runs only if `binary_sensor.sensor_bathroom_door_contact` is `off`. Actions: repeat raise volume on `media_player.mass_bathroom_vpe_speaker`.

- **Raise volume of speakers in a room when the vacuum is cleaning it** — **Automation ID:** [1753986114116](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11367)

  Triggered when `sensor.roborock_qrevo_curv_series_current_room` changes. Runs only if `binary_sensor.roborock_qrevo_curv_series_cleaning` is `on`. Actions: compute variables; raise volume on `{{ media_players}}`; +2 more actions.

- **Replace Spotify with Muzak playlist after 3.5 minutes** — **Automation ID:** [1747558481492](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7678)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `off` to `on` for 5m. Runs only if `binary_sensor.sensor_bathroom_door_contact` is `on`; attribute `is_volume_muted` on `media_player.mass_bathroom_vpe_speaker` is `True`; +1 more conditions. Actions: wait for `binary_sensor.group_bathroom_motion_occupancy` changes to `off` for 10m; call `media_player.shuffle_set` on `media_player.mass_bathroom_vpe_speaker` (shuffle=True); +2 more actions.

- **Set the kitchen hub volume to 100% on app launch** — **Automation ID:** [1738201829040](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5401)

  Triggered when `media_player.kitchen_google_hub` changes to `playing`. Runs only if attribute `app_name` on `media_player.kitchen_google_hub` is `Plex`; template `{{ (now() - trigger.from_state.last_changed) > timedelta(seconds=120)}}` is true. Actions: set volume on `media_player.kitchen_google_hub` (volume_level=1).

- **Skips song's myriam don't like whe she is home** — **Automation ID:** [1749235496765](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8894)

  Triggered when attribute `media_title` on `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_closet_wiim_speaker`, `media_player.mass_kitchen_wiim_speaker`, `media_player.mass_lounge_wiim_speaker` +4 more changes. Runs only if `person.myriam` is `home`; template `{% set disliked_songs = ['Mr. Blue Sky','Mr Blue Sky'] %} {% if trigger %} {% if trigger.to_state %} {% if trigger.to_st` is true. Actions: skip to next track on `{{trigger.entity_id}}`.

- **Speaker Automatic Volume v4** — **Automation ID:** [1737729890470](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5252)

  Triggered when `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_closet_wiim_speaker`, `media_player.mass_hallway_wiim_speaker` +3 more changes from `idle` to `playing`; `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_closet_wiim_speaker`, `media_player.mass_hallway_wiim_speaker` +3 more changes from `off`; attribute `is_volume_muted` on `media_player.mass_bedroom_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_closet_wiim_speaker`, `media_player.mass_hallway_wiim_speaker` +3 more changes from `True` to `False`. Runs only if template `{{ 'thunderstorm' not in state_attr(trigger.entity_id,'media_title') | string }}` is true; `input_boolean.following_music` is `on`; +2 more conditions. Actions: compute variables; run `script.speaker_automatic_volume_adjustment_script`.

- **Speaker/Chromecast Mutual Mute Management** — **Automation ID:** `merged_speaker_chromecast_mute`
  Triggered when `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +3 more changes to `playing`; `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +1 more changes from `off`; +6 more triggers. Runs only if template `{{ trigger.to_state.state != 'unavailable' }}` is true. Actions: if template `{{ trigger.id in ['cc_to_playing', 'spk_to_playing', 'cc_mute_status'] }}` is true, compute variables; if template `{{ unmuted_player_chromecasts != [] }}` is true, mute `{{ playing_speakers | list }}`; if template `{{ trigger.id in ['cc_to_playing', 'spk_to_playing', 'spk_mute_status'] and (trigger.id != 'spk_mute_status' or trigger.` is true, compute variables; if template `{{ unmuted_playing_speakers != [] }}` is true, mute `{{ player_chromecasts | list }}`; +2 more actions.

- **Start bathroom muzak when door closes** — **Automation ID:** [1744750292983](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7079)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `on` to `off`. Runs only if not (`media_player.mass_bathroom_vpe_speaker` is `playing`). Actions: call `media_player.shuffle_set` on `media_player.mass_bathroom_vpe_speaker` (shuffle=True); play media via `music_assistant.play_media` on `media_player.mass_bathroom_vpe_speaker` (media_id='Bathroom Muzak', media_type='playlist'); +1 more actions.

- **Start music when maxi returns home** — **Automation ID:** [1741877702709](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6091)

  Triggered when `person.maximiliano` changes from `not_home` to `home` for 3m. Runs only if `person.maximiliano` is `home`; `media_player.group_mass_home_group_speakers` is `off`; +1 more conditions. Actions: play media via `music_assistant.play_media` on `media_player.group_mass_home_group_speakers` (media_id='All favorited tracks').

- **Start rain sounds where maxi is sleeping** — **Automation ID:** [1717285150530](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2474)

  Triggered when `sensor.where_is_maxi_sleeping` changes. Runs only if `person.maximiliano` is `home`; not (`sensor.where_is_maxi_sleeping` is `hotbox`, `unavailable`); +2 more conditions. Actions: compute variables; if template `{{ sleeper_speaker != '' and state_attr(sleeper_speaker,'media_title') != 'thunderstorm' }}` is true, call `media_player.unjoin` on `{{ sleeper_speaker_mass }}
  
`; wait 5s; otherwise raise volume on `{{ sleeper_speaker }}`; set volume on `{{ sleeper_speaker }}` (volume_level=0.42).

- **Start visuals on all inactive TVs** — **Automation ID:** [1713723493265](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L903)

  Triggered when `input_button.start_visuals` changes. Actions: run `script.start_visual_playlist_on_plex`.

- **Stop bedroom speaker when bedroom sleeper awake** — **Automation ID:** [1708472370553](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L55)

  Triggered when `input_boolean.sleeper_in_bedroom` changes from `on` to `off` for 6m. Actions: clear playlist on `media_player.mass_bedroom_wiim_speaker`; turn off `media_player.mass_bedroom_wiim_speaker`.

- **Stop home group when maxi leaves** — **Automation ID:** [1751674442304](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9874)

  Triggered when `person.maximiliano` changes from `home` for 5m. Runs only if not (`media_player.group_mass_home_group_speakers` is `off`). Actions: clear playlist on `media_player.group_mass_home_group_speakers`; turn off `media_player.group_mass_home_group_speakers`.

- **Stop thunderstorm sound when bedroom awakes** — **Automation ID:** [1749266986349](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8931)

  Triggered when `input_boolean.sleeper_in_bedroom` changes from `on` to `off` for 5m. Runs only if template `{{ 'thunderstorm' in state_attr('media_player.mass_bedroom_wiim_speaker','media_title')}}` is true. Actions: clear playlist on `media_player.mass_bedroom_wiim_speaker`; turn off `media_player.mass_bedroom_wiim_speaker`.

- **Trigger initial Speaker volume script when Following Music turns on** — **Automation ID:** [1748032160039](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8026)

  Triggered when `input_boolean.following_music` changes from `off` to `on`. Actions: compute variables; run `script.speaker_automatic_volume_adjustment_script`.

- **Turn off Jukebox Access during Live Concert** — **Automation ID:** [1761366615315](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12324)

  Triggered when `input_boolean.live_concert` changes from `off` to `on` for 10s. Actions: run `script.reset_tablet_endpoint_interface`; run `script.reset_tablet_endpoint_interface`; +2 more actions.

- **Turn off speakers after 15 minutes of pause** — **Automation ID:** [1714151063007](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1276)

  Triggered when `media_player.mass_lounge_wiim_speaker`, `media_player.mass_hallway_wiim_speaker`, `media_player.mass_workshop_wiim_speaker`, `media_player.mass_salon_wiim_speaker` +3 more changes to `paused`, `idle` for 15m. Actions: clear playlist on `{{ trigger.entity_id }}`; turn off `{{ trigger.entity_id }}`.

- **Turn on Jukebox Public queue when party mode turns on** — **Automation ID:** [1759518333967](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12035)

  Triggered when `input_boolean.party_mode` changes from `off` to `on` for 0s; `input_boolean.live_concert` changes from `on`, `off`. Runs only if `input_boolean.live_concert` is `off`; `input_boolean.party_mode` is `on`. Actions: run `script.launch_party_mode_on_all_kiosks`.

- **Turn on volume control automations when Following speakers goes to on** — **Automation ID:** [1748225209386](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8049)

  Triggered when `input_boolean.following_music` changes from `off` to `on`. Actions: turn on `automation.speaker_automatic_volume_v4`, `automation.mute_speakers_when_the_room_is_empty`.

- **Unmute Bathroom Muzak on Door Close** — **Automation ID:** [1774308533764](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L16110)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `on` to `off`. Actions: unmute `media_player.mass_bathroom_vpe_speaker`; set volume on `media_player.mass_bathroom_vpe_speaker` (volume_level=0.4).

- **UnMute chromecasts when no unmuted speakers in the same room** — **Automation ID:** [1748927820689](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8492)

  Triggered when `media_player.universal_workshop_chromecast`, `media_player.universal_hotbox_top_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast` +1 more changes from `off`; attribute `is_volume_muted` on `media_player.mass_workshop_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_bedroom_wiim_speaker` changes; +3 more triggers. Runs only if template `{{ area_id(trigger.entity_id) | replace('_top','') | replace('_down','') not in states('sensor.active_sleeper_rooms') | ` is true; template `{{ area_id(trigger.entity_id) | replace('_top','') | replace('_down','') | lower in states('sensor.active_motion_rooms')` is true; +1 more conditions. Actions: compute variables; if template `{{ unmuted_playing_speakers == [] }}` is true, unmute `{{playing_chromecasts | list}}`; run `script.chromecast_automatic_volume_adjustment_script`.

- **Unmute playing bathroom speaker when someone enters** — **Automation ID:** [1743452959837](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6590)

  Triggered when `binary_sensor.group_bathroom_motion_occupancy` changes from `off` to `on`. Runs only if `media_player.mass_bathroom_vpe_speaker` is `playing`; not (`media_player.mass_kitchen_wiim_speaker` is `playing`; `media_player.mass_workshop_wiim_speaker` is `playing`; +1 more). Actions: compute variables; if time is after `08:45:00` and before `11:30:00` and weekday in fri, thu, wed, tue, mon, sat, sun, set volume on `{{speaker_id}}` (volume_level=0.3); unmute `{{speaker_id}}`; if time is after `11:30:00` and before `21:30:00` and weekday in fri, thu, wed, tue, mon, sun, sat, set volume on `{{speaker_id}}` (volume_level=0.35); unmute `{{speaker_id}}`; otherwise set volume on `{{speaker_id}}` (volume_level=0.2); unmute `{{speaker_id}}`; +4 more branches; +1 more actions.

- **UnMute speakers when no unmuted chromecast in the same room** — **Automation ID:** [1748928181744](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8610)

  Triggered when `media_player.universal_workshop_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast`, `media_player.universal_bedroom_chromecast` +1 more changes from `playing` for 2s; `media_player.mass_workshop_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_bedroom_wiim_speaker` changes to `playing` for 2s; attribute `is_volume_muted` on `media_player.universal_workshop_chromecast`, `media_player.universal_patio_chromecast`, `media_player.universal_salon_chromecast`, `media_player.universal_bedroom_chromecast` changes. Runs only if template `{{ trigger.id == 'speakers' and ((now() - trigger.from_state.last_changed) > timedelta(seconds=600)) or trigger.id != 's` is true; template `{{ area_id(trigger.entity_id) | string | lower not in states('sensor.active_sleeper_rooms') | string | lower }}` is true; +1 more conditions. Actions: compute variables; if template `{{ unmuted_player_chromecasts == [] }}` is true, unmute `{{playing_speakers | list}}`.

<a id="lighting-ambience-60"></a>
## Lighting & Ambience (60)

[Back to top](#top)

- ** Trippy Light Random Colors Effect automation (Script call)** — **Automation ID:** [1774297006546](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15940)

  Triggered when `input_boolean.trippy_lights_effect` changes from `off`, `on`; `input_number.trippy_lights_effect_speed` changes for 2s. Runs only if `input_boolean.trippy_lights_effect` is `on`; `input_boolean.trippy_warm_colors_effect` is `off`. Actions: repeat compute variables; repeat run `script.llm_random_dissimilar_colors_script`; turn on `{{repeat.item}}` (transition="{{ states('input_number.trippy_lights_effect_speed')}}") every 00:00:{{ states('input_number.trippy_lights_effect_speed')}}; +1 more actions.

- **Automatically turn off bathroom light** — **Automation ID:** [1740935035535](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5888)

  Triggered when `binary_sensor.group_bathroom_motion_occupancy` changes from `on` to `off` for 7m. Runs only if `binary_sensor.shower_in_usage` is `off`; `binary_sensor.sensor_bathroom_door_contact` is `on`. Actions: turn off `light.group_bathroom_lights` (transition=30).

- **Avoid trippy lights dual activation** — **Automation ID:** [1766790345547](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14905)

  Triggered when `input_boolean.trippy_lights_effect` changes from `off` to `on`; `input_boolean.trippy_warm_colors_effect` changes from `off` to `on`. Actions: if trigger id `random_effect_on` matched, turn off `input_boolean.trippy_warm_colors_effect`; if trigger id `warm_effect_on` matched, turn off `input_boolean.trippy_lights_effect`.

- **Bedroom Hue Dial Automation** — **Automation ID:** [1749521213239](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9093)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets, stop media, clear playlists on `light.group_bedroom_lights` plus 3 other targets.

- **Bedroom hue tap remote automation ** — **Automation ID:** [1721278758149](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3127)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets on `light.group_bedroom_lights`.

- **Dial Bedroom automation** — **Automation ID:** [1763156136004](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12364)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial hallway automation new** — **Automation ID:** [1772565586854](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15593)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Kitchen Automation New** — **Automation ID:** [1752639024754](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10836)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Lounge Automation New** — **Automation ID:** [1752639270370](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11038)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Lounge Tree Automation New** — **Automation ID:** [1756568623181](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11669)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Salon Automation New** — **Automation ID:** [1752638710619](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10088)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Salon window Automation New** — **Automation ID:** [1752638946576](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10646)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Workshop Automation New** — **Automation ID:** [1752638811178](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10278)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dial Workshop Wall Automation New** — **Automation ID:** [1752638839619](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10462)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

- **Dim budha if someone is watching TV in salon from bed** — **Automation ID:** [1769570875542](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15344)

  Triggered when `binary_sensor.salon_radar_occupancy_zone_2_occupancy` changes to `on`; `media_player.universal_salon_chromecast` changes to `playing`; attribute `app_id` on `media_player.salon_roku_tv` changes to `tvinput.hdmi1`. Runs only if `light.group_salon_buddha_statue_lights` is `on`; (not (`media_player.universal_salon_chromecast` is `off`) or attribute `app_id` on `media_player.salon_roku_tv` is `tvinput.hdmi1`); +1 more conditions. Actions: turn on `light.group_salon_buddha_statue_lights` (brightness_pct=1, transition=2).

- **Dim Wall lights when workshop TV is playing and Beige couch or matress is occupied** — **Automation ID:** [1728311156670](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3646)

  Triggered when `binary_sensor.workshop_radarr_occupancy_sensor_radar_zone_2_occupancy` changes to `on`; `media_player.universal_workshop_chromecast` changes to `playing`. Runs only if `light.workshop_wall_light` is `on`; not (`media_player.universal_workshop_chromecast` is `off`); +1 more conditions. Actions: turn on `light.workshop_wall_light` (brightness_pct=1, transition=2).

- **execute Sleep actions when "sleeper in bedroom" is turned on** — **Automation ID:** [1748845278340](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8335)

  Triggered when `input_boolean.sleeper_in_bedroom` changes from `off` to `on`. Actions: run in parallel: turn off `switch.bedroom_tablet_screen`, `switch.bedroom_tablet_motion_detection`; turn off area `bedroom` (transition=10); turn off `light.home_assistant_voice_09e4a7_led_ring`.

- **Execute sleep actions when "Sleeper in closet" turns on** — **Automation ID:** [1740935638351](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5946)

  Triggered when `input_boolean.sleeper_in_closet` changes from `off` to `on`. Actions: turn off `light.group_closet_lights`, `light.closet_string_2`, `light.closet_string_1` (transition=2); turn off `switch.closet_tablet_screen`, `switch.closet_tablet_motion_detection`.

- **Hallway hue dial automation** — **Automation ID:** [1763655791601](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L13198)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets on `light.group_hallway_lights` plus 1 other targets.

- **Hotbox Hue Dial Automation** — **Automation ID:** [1763157128192](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12557)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets, stop media, clear playlists on `light.group_hotbox_lights` plus 3 other targets.

- **Hue Dial lounge automation** — **Automation ID:** [1763653396906](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12897)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets, stop media, clear playlists on `light.group_lounge_lights` plus 2 other targets.

- **Kitchen hue dial automation** — **Automation ID:** [1763657214344](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L13792)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets, stop media, clear playlists on `light.group_kitchen_lights` plus 2 other targets.

- **Lower lights to 30% when a movie is played** — **Automation ID:** [1752801417034](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11099)

  Triggered when attribute `media_content_type` on `media_player.universal_bedroom_chromecast`, `media_player.universal_workshop_chromecast`, `media_player.universal_salon_chromecast` changes to `movie` for 4s; `media_player.universal_bedroom_chromecast`, `media_player.universal_workshop_chromecast`, `media_player.universal_salon_chromecast` changes to `playing` for 2s. Runs only if template `{{ state_attr(state_attr(trigger.entity_id,'active_child'),'media_library_title') == 'Movies'}}` is true; template `{{ not trigger.to_state.attributes.media_series_title }}` is true. Actions: compute variables; repeat turn on `{{ lights_over_30_percent[repeat.index - 1 ] }}` (brightness_pct=30, color_temp_kelvin=3308, transition=3).

- **Nanoleaf actions control automation** — **Automation ID:** [1773975525008](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15921)

  Triggered when attribute `event_type` on `event.kitchen_nanoleaf_light_touch_gesture` changes to `swipe_up`. Actions: run `script.apply_random_existing_effect_to_a_light_entity`.

- **Partial sleep actions in hotbox when Lylou falls asleep** — **Automation ID:** [1756344417569](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11648)

  Triggered when `sensor.is_lylou_asleep` changes from `awake` to `asleep`. Runs only if time is after `20:00:00` and before `07:00:00`. Actions: turn off `remote.hotbox_roku`.

- **Raise lights to 60% brightness when a movie is paused and the brightness is below 30%** — **Automation ID:** [1713630060902](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L754)

  Triggered when `media_player.universal_bedroom_chromecast`, `media_player.universal_workshop_chromecast`, `media_player.universal_salon_chromecast` changes to `paused` for 0s. Runs only if template `{{ state_attr(state_attr(trigger.entity_id,'active_child'),'media_library_title') == 'Movies'}}` is true; template `{{ not trigger.from_state.attributes.media_series_title }}` is true. Actions: compute variables; repeat turn on `{{ repeat.item }}` (brightness=153, transition=2).

- **Remote Phillip Dimmer Automation** — **Automation ID:** [1753137946773](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11170)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to turn lights off, turn lights on with presets on `light.group_hotbox_lights`.

- **revert Lounge Door Arch effect to solid color when All residents Present Sleeping is on for 4 hours** — **Automation ID:** [1754715205053](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11517)

  Triggered when `binary_sensor.all_present_sleeping` changes from `off` to `on` for 4h; `binary_sensor.all_present_sleeping` changes from `on` to `off` for 20m. Runs only if `input_boolean.sleeper_in_lounge` is `off`; `input_boolean.sleeper_in_salon` is `off`. Actions: turn on `light.lounge_door_arch_light` (brightness=1); turn off `light.lounge_door_arch_light`.

- **Salon hue dial automation** — **Automation ID:** [1763657573280](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14101)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets, stop media, clear playlists on `light.group_salon_lights` plus 2 other targets.

- **Set Wallpaper engine colors to match the lights** — **Automation ID:** [1733790164217](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4789)

  Triggered when attribute `rgb_color` on `light.group_salon_lights` changes for 2s. Runs only if template `{{ trigger.to_state.state != 'off' }}` is true. Actions: compute variables; call `mqtt.publish` on; +7 more actions.

- **Sleep actions when "sleeper in hotbox" is turned on** — **Automation ID:** [1739631004694](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5530)

  Triggered when `input_boolean.sleeper_in_hotbox` changes from `off` to `on`. Actions: turn off `remote.hotbox_roku`; close `cover.cover_hotbox_blinds`; +1 more actions.

- **Sleep actions when "sleeper in patio" is turned on** — **Automation ID:** [1731515407976](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4200)

  Triggered when `input_boolean.sleeper_in_patio` changes from `off` to `on`. Actions: turn off `light.group_patio_lights` (transition=2).

- **Sleeper actions when salon sleeper turns on** — **Automation ID:** [1749333714992](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8961)

  Triggered when `input_boolean.sleeper_in_salon` changes from `off` to `on`. Actions: turn off `light.group_salon_lights` (transition=10); close `cover.cover_salon_blinds`; +6 more actions.

- **Trippy Light Warm Random Colors Effect automation (Script call)** — **Automation ID:** [1774297474995](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L16029)

  Triggered when `input_boolean.trippy_warm_colors_effect` changes from `off`, `on`; `input_number.trippy_lights_effect_speed` changes for 5s. Runs only if `input_boolean.trippy_warm_colors_effect` is `on`; `input_boolean.trippy_lights_effect` is `off`. Actions: repeat compute variables; repeat run `script.llm_ranged_dissimilar_colors_generation_script` every 00:00:{{ states('input_number.trippy_lights_effect_speed')}}; +1 more actions.

- **Turn Assist screens on when listening** — **Automation ID:** [1728390311720](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3728)

  Triggered when `assist_satellite.esp32_s3_box_kitchen_assistant_assist_satellite`, `assist_satellite.saloon_assistant_assist_satellite`, `assist_satellite.lv_assistant_assist_satellite`, `assist_satellite.bedroom_assistant_assist_satellite` +5 more changes to `listening`. Runs only if template `{{ area_id(trigger.entity_id) | lower | replace('_top','')| replace('_down','') not in states('sensor.active_sleeper_roo` is true. Actions: turn on `{{device_entities(device_id(trigger.entity_id)) | select("match", ".*light.*") | first }}` (brightness_pct=100).

- **Turn light on when someone is detected in the front** — **Automation ID:** [1714688757969](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1506)

  Triggered when `sensor.front_door_camera_person_count` goes above `0`. Actions: if `light.front_door_light` is `on`, compute variables; turn on `light.front_door_light` (brightness=255, color_temp_kelvin=4628, transition=2); if `light.front_door_light` is `off`, turn on `light.front_door_light` (brightness=255, color_temp_kelvin=4628, transition=2); wait for `sensor.front_door_camera_person_count` goes below `1` for 3m (timeout 1h).

- **Turn lights back to normal when UV lamps are turned off** — **Automation ID:** [1754680196397](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11435)

  Triggered when `light.group_hallway_uv_lamps`, `light.group_kitchen_uv_lamps`, `light.group_lounge_uv_lamps`, `light.group_patio_uv_lamps` +2 more changes from `on` to `off`. Actions: compute variables; if (`input_boolean.party_mode` is `on`), run `script.changes_selected_lights_to_warm_pale_colors`; if (`input_boolean.kink_party` is `on`), run `script.changes_selected_light_group_to_single_warm_colors`; otherwise call `adaptive_lighting.apply` on (transition='1'); +1 more branches.

- **Turn lights to white when we stop a movie** — **Automation ID:** [1713574201448](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L670)

  Triggered when `media_player.universal_salon_chromecast`, `media_player.universal_workshop_chromecast` changes from `idle` to `off`; `media_player.universal_salon_chromecast`, `media_player.universal_workshop_chromecast` changes from `paused` to `off`; `media_player.universal_salon_chromecast`, `media_player.universal_workshop_chromecast` changes from `playing` to `off`. Runs only if template `{{ state_attr(state_attr(trigger.entity_id,'active_child'),'media_library_title') == 'Movies'}}` is true; template `{{ not trigger.from_state.attributes.media_series_title }}` is true; +4 more conditions. Actions: compute variables; repeat turn on `{{ repeat.item }}`; call `adaptive_lighting.apply` on `switch.adaptive_lighting_adaptive_lighting` (transition='2').

- **Turn off 'Sleeper in X' switch when lights are on and bright in the room for 7 minutes** — **Automation ID:** [1755459606104](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11566)

  Triggered when attribute `brightness` on `light.group_salon_lights` goes above `60` for 7m; attribute `brightness` on `light.group_hotbox_lights` goes above `60` for 7m; +4 more triggers. Runs only if template `{{([area_id(trigger.entity_id)] | map('area_entities') | sum(start=[]) | select('search', 'input_boolean.sleeper_in') | ` is true. Actions: turn off `{{([area_id(trigger.entity_id)]  | map('area_entities') | sum(start=[]) |
 select('search', 'input_boolean.sleeper_in') | select('is_state','on')  |  unique | list ) 
 }}`.

- **Turn off Assists screens when done listening** — **Automation ID:** [1728390398785](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3762)

  Triggered when `assist_satellite.esp32_s3_box_kitchen_assistant_assist_satellite`, `assist_satellite.saloon_assistant_assist_satellite`, `assist_satellite.lv_assistant_assist_satellite`, `assist_satellite.bedroom_assistant_assist_satellite` +1 more changes from `processing`; `assist_satellite.esp32_s3_box_kitchen_assistant_assist_satellite`, `assist_satellite.saloon_assistant_assist_satellite`, `assist_satellite.lv_assistant_assist_satellite`, `assist_satellite.bedroom_assistant_assist_satellite` +1 more changes to `idle`. Runs only if template `{{ area_id(trigger.entity_id) | replace('_top','') | replace('_down','') | lower in states('sensor.active_sleeper_rooms'` is true. Actions: turn off `{{device_entities(device_id(trigger.entity_id)) | select("match", ".*light.*") | first }}`.

- **Turn off lights automatically when room is empty and sleep mode is on** — **Automation ID:** [1753418041554](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11295)

  Triggered when `binary_sensor.group_patio_motion_occupancy`, `binary_sensor.group_closet_motion_occupancy`, `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_server_motion_occupancy` +6 more changes to `off` for 1m. Runs only if template `{% if states('sensor.maxi_location_by_petro') != 'unknown' %} {{ states('sensor.maxi_location_by_petro') | lower not in ` is true; template `{{ states('sensor.myriam_location_by_petro') | lower not in [area_id(trigger.entity_id)] | lower }}` is true; +6 more conditions. Actions: turn off area `{{ area_id(trigger.entity_id) }}` (transition=60).

- **Turn off lights colors adaptation when an event is ongoing** — **Automation ID:** [1714668230697](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1459)

  Triggered when `input_boolean.kink_party`, `input_boolean.party_mode`, `input_boolean.acid_time` changes to `on`. Actions: turn off `switch.adaptive_lighting_adapt_color_adaptive_lighting`.

- **Turn off lights when a room is empty and sleep mode is off V2** — **Automation ID:** [1747973723106](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7962)

  Triggered when `binary_sensor.group_patio_motion_occupancy`, `binary_sensor.group_closet_motion_occupancy`, `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_server_motion_occupancy` +6 more changes to `off` for 6m. Runs only if template `{% if states('sensor.maxi_location_by_petro') != 'unknown' %} {{ states('sensor.maxi_location_by_petro') | lower not in ` is true; template `{% if (states('binary_sensor.tablet_myriam_interactive') == 'on' and states('sensor.myriam_talet_location_compounded') |` is true; +5 more conditions. Actions: turn off area `{{ area_id(trigger.entity_id) }}`.

- **Turn off lights when the house is empty** — **Automation ID:** [1714497353040](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1439)

  Triggered when `zone.home` goes below `0.9` for 5m. Actions: turn off `light.group_all_inside_lights`.

- **Turn off patio lights at sunset when patio is empty** — **Automation ID:** [1768863734220](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15324)

  Triggered when sun event `sunset`. Runs only if `binary_sensor.group_patio_motion_occupancy` is `off`. Actions: turn off `light.group_patio_lights`.

- **Turn off sleeper in bedroom when lights turns on** — **Automation ID:** [1717285030468](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2452)

  Triggered when attribute `brightness` on `light.bedroom_chandelier_light` goes above `80` for 2m. Actions: turn off `input_boolean.sleeper_in_bedroom`.

- **Turn off trippy lights after 16 continous hours** — **Automation ID:** [1766792030896](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14950)

  Triggered when `input_boolean.trippy_lights_effect` changes from `off` to `on` for 16h; `input_boolean.trippy_warm_colors_effect` changes from `off` to `on` for 16h 16m. Actions: if trigger id `random_effect_on` matched, turn off `input_boolean.trippy_lights_effect`; if trigger id `warm_effect_on` matched, turn off `input_boolean.trippy_warm_colors_effect`.

- **Turn off workshop lights when bedroom Sleeper turns on ** — **Automation ID:** [1723530969899](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3308)

  Triggered when `binary_sensor.sleeper_in_bedroom` changes from `off` to `on`. Runs only if `binary_sensor.group_workshop_motion_occupancy` is `off`. Actions: turn off `light.group_workshop_lights` (transition=20).

- **Turn on Blue Light with UV Lamps in Same Area** — **Automation ID:** [1754679794287](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11401)

  Triggered when `light.group_hallway_uv_lamps`, `light.group_kitchen_uv_lamps`, `light.group_lounge_uv_lamps`, `light.group_patio_uv_lamps` +2 more changes from `off` to `on`. Runs only if `input_boolean.live_concert` is `off`. Actions: turn on area `{{area_id(trigger.entity_id)}}` (brightness_pct=100, rgb_color=[0, 0, 255], transition=1).

- **Turn on color adaptation when events end** — **Automation ID:** [1714668294513](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1478)

  Triggered when `input_boolean.kink_party`, `input_boolean.party_mode`, `input_boolean.acid_time` changes to `off`. Runs only if `input_boolean.acid_time` is `off`; `input_boolean.party_mode` is `off`; +1 more conditions. Actions: turn on `switch.adaptive_lighting_adapt_color_adaptive_lighting`.

- **Turn on lights automatically** — **Automation ID:** [1713824382107](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L946)

  Triggered when `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy`, `binary_sensor.group_kitchen_motion_occupancy`, `binary_sensor.group_workshop_motion_occupancy` +7 more changes to `on` for 0s. Runs only if template `{{ area_id(trigger.entity_id) | lower | replace('_top','')| replace('_down','') not in states('sensor.active_sleeper_roo` is true; not (`alarm_control_panel.home_alarm` is `triggered`); +1 more conditions. Actions: compute variables; repeat turn on `{{off_lights_in_room[repeat.index - 1]    }}`.

- **Undim budha lights when movie stops or people raise** — **Automation ID:** [1769570986649](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15400)

  Triggered when `binary_sensor.salon_radar_occupancy_zone_2_occupancy` changes to `off`; `media_player.universal_salon_chromecast`, `media_player.salon_roku_tv` changes to `off`. Runs only if `light.group_salon_buddha_statue_lights` is `on`; ((`binary_sensor.salon_radar_occupancy_zone_2_occupancy` is `off`) or `media_player.universal_salon_chromecast` is `off`); +1 more conditions. Actions: turn on `light.group_salon_buddha_statue_lights` (brightness='{{ state_attr("light.group_salon_main_lights", \'brightness\') / 3 }}\n', transition=5).

- **unDim Wall lights when workshop TV is stopped ** — **Automation ID:** [1728311332778](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3686)

  Triggered when `binary_sensor.workshop_radarr_occupancy_sensor_radar_zone_2_occupancy` changes to `off`; `media_player.universal_workshop_chromecast` changes to `off`. Runs only if `light.workshop_wall_light` is `on`; ((`binary_sensor.workshop_radarr_occupancy_sensor_radar_zone_2_occupancy` is `off`) or `media_player.universal_workshop_chromecast` is `off`). Actions: turn on `light.workshop_wall_light` (brightness='{{ state_attr("light.group_workshop_lights", \'brightness\') / 2 }}\n', transition=5).

- **Undo Sleep actions when "sleeper in bedroom" is turned off** — **Automation ID:** [1748845212701](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8286)

  Triggered when `input_boolean.sleeper_in_bedroom` changes from `on` to `off` for 5s. Actions: run in parallel: turn on `switch.bedroom_tablet_screen`, `switch.bedroom_tablet_motion_detection`; turn on `light.bedroom_chandelier_light` (brightness_pct=100, color_temp_kelvin=5126, transition=45); turn on `climate.bedroom_ac`.

- **Undo Sleep actions when "sleeper in patio" is turned off** — **Automation ID:** [1731515445191](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4220)

  Triggered when `input_boolean.sleeper_in_patio` changes from `on` to `off` for 5s. Actions: turn on `light.group_patio_lights` (transition=45).

- **undo Sleeper action in salon when sleeper turns off** — **Automation ID:** [1749333829084](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9074)

  Triggered when `input_boolean.sleeper_in_salon` changes from `on` to `off`. Actions: turn on `light.group_salon_lights` (transition=30).

- **Undo sleeper actions when Sleeper in closet turns off** — **Automation ID:** [1740935693012](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5974)

  Triggered when `input_boolean.sleeper_in_closet` changes from `on` to `off`. Actions: turn on `switch.closet_tablet_screen`, `switch.closet_tablet_motion_detection`; turn on `light.group_closet_lights`, `light.closet_string_2`, `light.closet_string_1` (brightness_pct=1, rgb_color=[255, 0, 255], transition=20); +1 more actions.

- **Vibrating color effect automation** — **Automation ID:** [1766817510039](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15003)

  Triggered when `input_boolean.vibrating_colors_effect` changes from `off`, `on`. Runs only if `input_boolean.trippy_warm_colors_effect` is `off`; `input_boolean.trippy_lights_effect` is `off`. Actions: repeat compute variables; repeat compute variables; turn on `{{repeat.item}}` (transition="{{ states('input_number.vibrating_color_effect_speed')}}"); wait 00:00:{{ states('input_number.vibrating_color_effect_speed') | int + 1}}; compute variables; call `adaptive_lighting.apply` on (transition='3').

- **Workshop Desk Hue Dial automation** — **Automation ID:** [1763775560173](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14403)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets on `light.group_workshop_lights` plus 1 other targets.

- **Workshop Hue dial automation** — **Automation ID:** [1763656472816](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L13483)

  Triggered by multiple remote/button/dial actions. It branches by trigger ID to toggle lights, turn lights on with presets, stop media, clear playlists on `light.group_workshop_lights` plus 2 other targets.

<a id="climate-covers-environment-26"></a>
## Climate, Covers & Environment (26)

[Back to top](#top)

- **close bedroom blind in relation to bedroom ac** — **Automation ID:** [1748838408630](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8251)

  Triggered when attribute `current_position` on `cover.cover_bedroom_blinds` changes for 2s; `switch.zooz_bedroom_plug` changes to `off`. Runs only if `switch.zooz_bedroom_plug` is `off`; attribute `current_position` on `cover.cover_bedroom_blinds` is below `20`. Actions: set cover position on `cover.cover_bedroom_blinds` (position=0).

- **Close bedroom blinds when sun sets** — **Automation ID:** [1749669750052](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9406)

  Triggered when `sun.sun` changes from `above_horizon` to `below_horizon` for 10m. Actions: close `cover.cover_bedroom_blinds`.

- **Close bedroom cover when sleeper turns on** — **Automation ID:** [1719366196396](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3105)

  Triggered when `input_boolean.sleeper_in_bedroom` changes to `on` for 1m. Actions: close `cover.cover_bedroom_blinds`.

- **Close blinds when the house is empty** — **Automation ID:** [1713452550061](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L388)

  Triggered when `zone.home` goes below `0.9` for 5m. Actions: close `cover.group_all_blinds`, `cover.cover_bedroom_blinds`.

- **Close hotbox blinds when children leaves for 10 minutes** — **Automation ID:** [1749057091718](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8872)

  Triggered when `person.lylou` changes from `home` to `not_home` for 10m. Actions: close `cover.cover_hotbox_blinds`.

- **Close Kitchen curtains when sun sets** — **Automation ID:** [1764362706933](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14688)

  Triggered when `sun.sun` changes from `above_horizon` to `below_horizon` for 10m. Actions: close `cover.cover_kitchen_curtains`.

- **Close salon blind in relationship to ac** — **Automation ID:** [1747423540817](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7643)

  Triggered when attribute `current_position` on `cover.cover_salon_blinds` changes for 5s; `switch.zooz_salon_ac` changes to `off`. Runs only if `switch.zooz_salon_ac` is `off`; attribute `current_position` on `cover.cover_salon_blinds` is below `30`. Actions: set cover position on `cover.cover_salon_blinds` (position=0).

- **Close salon blind sometimes after sun closes** — **Automation ID:** [1747632525749](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7741)

  Triggered when `sun.sun` changes from `above_horizon` to `below_horizon` for 45m. Actions: wait for `binary_sensor.group_salon_motion_occupancy` changes to `off` for 10m; close `cover.cover_salon_blinds`.

- **Close salon blind upon sunset** — **Automation ID:** [1752195988065](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10066)

  Triggered when `sun.sun` changes from `above_horizon` to `below_horizon` for 10m. Actions: close `cover.cover_salon_blinds`.

- **Close workshop Blind in relation to AC** — **Automation ID:** [1747682690257](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7872)

  Triggered when attribute `current_position` on `cover.cover_workshop_blinds` changes for 2s; `switch.zooz_workshop_ac` changes to `off`. Runs only if `switch.zooz_workshop_ac` is `off`; attribute `current_position` on `cover.cover_workshop_blinds` is below `27`. Actions: set cover position on `cover.cover_workshop_blinds` (position=0).

- **Control Bedroom Thermostat in relation to Bedroom sleeper** — **Automation ID:** [1750445065859](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9625)

  Triggered when `input_boolean.sleeper_in_bedroom` changes from `off` to `on` for 2m. Actions: turn off `climate.bedroom_ac`; run in parallel: wait for sun event `sunrise` (offset `600`); turn off `switch.zooz_bedroom_plug`; wait for `input_boolean.sleeper_in_bedroom` changes from `on` to `off` for 10m; turn on `climate.bedroom_ac`; +2 more actions.

- **Isolate front salon when active smoking is present** — **Automation ID:** [1772244438943](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15537)

  Triggered when `binary_sensor.active_smoking_in_the_salon` changes to `on`. Runs only if not ((`sensor.roborock_qrevo_curv_series_current_room` is `Salon` or `sensor.roborock_qrevo_curv_series_current_room` is `Lounge`)). Actions: close `cover.cover_lounge_door`.

- **Open bedroom blind in relation to bedroom ac** — **Automation ID:** [1748750771802](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8220)

  Triggered when attribute `current_position` on `cover.cover_bedroom_blinds` changes; `switch.zooz_bedroom_plug` changes to `on`. Runs only if `switch.zooz_bedroom_plug` is `on`; attribute `current_position` on `cover.cover_bedroom_blinds` is below `19`. Actions: set cover position on `cover.cover_bedroom_blinds` (position=19).

- **Open bedroom covers when the person awakes** — **Automation ID:** [1719366131642](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3074)

  Triggered when `input_boolean.sleeper_in_bedroom` changes from `on` to `off` for 0s. Runs only if `binary_sensor.sensor_bedroom_door_contact` is `on` for 5m. Actions: open `cover.cover_bedroom_blinds`.

- **Open front salon when smoke is gone** — **Automation ID:** [1772328254331](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15570)

  Triggered when `sensor.apollo_air_1_7b76cc_pm_1mm_weight_concentration` goes below `10`. Runs only if `input_boolean.sleeper_in_salon` is `off`. Actions: open `cover.cover_lounge_door`.

- **Open Salon Blind in relation to AC** — **Automation ID:** [1747423390465](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7595)

  Triggered when attribute `current_position` on `cover.cover_salon_blinds` changes; `switch.zooz_salon_ac` changes to `on`; +3 more triggers. Runs only if attribute `current_position` on `cover.cover_salon_blinds` is below `29`; not (`switch.zooz_salon_ac` is `off`). Actions: set cover position on `cover.cover_salon_blinds` (position=29).

- **Open workshop blind in relation to AC** — **Automation ID:** [1747682528206](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7840)

  Triggered when attribute `current_position` on `cover.cover_workshop_blinds` changes; `switch.zooz_workshop_ac` changes to `on`. Runs only if `switch.zooz_workshop_ac` is `on`; attribute `current_position` on `cover.cover_workshop_blinds` is below `26`. Actions: set cover position on `cover.cover_workshop_blinds` (position=26).

- **Raise volume when the AC starts in a room** — **Automation ID:** [1717287475672](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2572)

  Triggered when `switch.zooz_workshop_ac`, `switch.zooz_bedroom_plug` changes from `off` to `on`. Actions: raise volume on `{{ area_entities(area_id(trigger.entity_id))|select('search', 'media_player.universal')|select('is_state_attr', 'is_volume_muted', false)|select('is_state', 'playing')| list }}`.

- **Sleep actions when "all resident sleeping" is turned on** — **Automation ID:** [1713451307697](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L297)

  Triggered when `binary_sensor.all_residents_sleeping` changes to `on` for 5m. Runs only if `input_boolean.kink_party` is `off`; `input_boolean.acid_time` is `off`; +2 more conditions. Actions: turn on `switch.adaptive_lighting_sleep_mode_adaptive_lighting`; close `cover.cover_workshop_blinds`, `cover.cover_salon_blinds`.

- **Sleep actions when "sleeper in workshop" is turned on** — **Automation ID:** [1728309825943](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3549)

  Triggered when `input_boolean.sleeper_in_workshop` changes from `off` to `on`. Actions: mute `media_player.mass_workshop_wiim_speaker`, `media_player.mass_hallway_wiim_speaker`; close `cover.cover_workshop_blinds`; +4 more actions.

- **Start bathroom fan when Shower in Usage turns on** — **Automation ID:** [1713536741103](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L597)

  Triggered when `binary_sensor.shower_in_usage` changes to `on` for 30s. Actions: turn on `switch.bathroom_fan`.

- **Turn ACs on and Off  depending on house occupancy** — **Automation ID:** [1713534223873](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L462)

  Triggered when `binary_sensor.user_home` changes. Actions: if `binary_sensor.user_home` is `on`, set preset mode on `climate.workshop_ac`, `climate.salon_ac` (preset_mode='none'); if `binary_sensor.user_home` is `off`, set preset mode on `climate.workshop_ac`, `climate.salon_ac` (preset_mode='away').

- **Turn down volume when AC goes off** — **Automation ID:** [1717290684944](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2593)

  Triggered when `switch.zooz_workshop_ac`, `switch.zooz_bedroom_plug` changes from `on` to `off`. Actions: lower volume on `{{ area_entities(area_id(trigger.entity_id))|select('search', 'media_player.universal')|select('is_state_attr', 'is_volume_muted', false)|select('is_state', 'playing')| list }}`.

- **Turn off air purifier in salon when smoke is gone** — **Automation ID:** [1772575096130](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15671)

  Triggered when `sensor.apollo_air_1_7b76cc_pm_1mm_weight_concentration` goes below `10`. Actions: turn off `switch.plug_salon_air_purifier`.

- **Turn off Bathroom fan when humidity falls** — **Automation ID:** [1734912412449](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5165)

  Triggered when `sensor.sensor_bathroom_temperature_humidity` goes below `sensor.average_house_humidity`. Actions: turn off `switch.bathroom_fan`.

- **Turn off Closet fan after 5 minutes of non occupancy** — **Automation ID:** [1713631370622](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L806)

  Triggered when `binary_sensor.group_closet_motion_occupancy` changes from `on` to `off` for 10m. Actions: turn off `switch.plug_closet_fan`.

<a id="sleep-presence-daily-routines-14"></a>
## Sleep, Presence & Daily Routines (14)

[Back to top](#top)

- **Automatically Turn off sleeper in patio** — **Automation ID:** [1740934578627](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5848)

  Triggered when `input_boolean.sleeper_in_patio` changes from `off` to `on` for 50m. Actions: wait for `binary_sensor.group_patio_motion_occupancy` changes from `off` to `on` for 25m (timeout 3h); turn off `input_boolean.sleeper_in_patio`.

- **Disable Sleeper in children room when children leaves** — **Automation ID:** [1748898158918](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8467)

  Triggered when `person.lylou` changes from `home` to `not_home` for 10m. Runs only if `input_boolean.sleeper_in_hotbox` is `on`. Actions: turn off `input_boolean.sleeper_in_hotbox`.

- **mute patio assistant when people are absent** — **Automation ID:** [1775146849950](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L16686)

  Triggered when `binary_sensor.group_patio_motion_occupancy` changes from `on`, `off`; `sensor.patio_camera_person_count` changes to `0`; `switch.patio_assistant_mute` changes from `on`, `off`. Runs only if (`binary_sensor.group_patio_motion_occupancy` is `off` and `sensor.patio_camera_person_count` is `0`). Actions: turn on `switch.patio_assistant_mute`.

- **Toggle sleeper in bedroom on button press** — **Automation ID:** [1749014947859](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8854)

  Triggered when `sensor.remote_patio_quiet_button_action` changes to `off`. Actions: toggle `input_boolean.sleeper_in_bedroom`.

- **Trigger party script when party mode turns on** — **Automation ID:** [1739316122149](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5482)

  Triggered when `input_boolean.party_mode` changes from `off` to `on`. Actions: run `script.set_whole_house_to_party_ambiance`.

- **Turn off hotbox sleeper when lylou sleeping bayesian turns off** — **Automation ID:** [1757861158274](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11888)

  Triggered when `binary_sensor.lylou_is_asleep_bayesian` changes from `on` to `off` for 20m. Runs only if `input_boolean.sleeper_in_hotbox` is `on`. Actions: turn off `input_boolean.sleeper_in_hotbox`.

- **Turn off sleep mode for adaptative lighting when myriam awakes** — **Automation ID:** [1750443660133](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9605)

  Triggered when `sensor.is_myriam_asleep` changes from `asleep` to `awake` for 5m. Actions: turn off `switch.adaptive_lighting_sleep_mode_adaptive_lighting`.

- **Turn on Bedroom mirror smart plug when myriam awakes** — **Automation ID:** [1750437590850](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9579)

  Triggered when `sensor.is_myriam_asleep` changes from `asleep` to `awake`; trigger `template`. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 1800}}` is true. Actions: turn on `switch.plug_bedroom_mirror_lamp`.

- **Turn on party mode when over 5 guests are over** — **Automation ID:** [1713451981404](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L367)

  Triggered when `sensor.chilling_hotspot_client_count` goes above `5`. Runs only if `input_boolean.kink_party` is `off`; `input_boolean.acid_time` is `off`. Actions: turn on `input_boolean.party_mode`.

- **Turn on sleeper in bedroom boolean when sensor turns to on** — **Automation ID:** [1717284957489](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2434)

  Triggered when `binary_sensor.sleeper_in_bedroom` changes from `off` to `on`. Actions: turn on `input_boolean.sleeper_in_bedroom`.

- **Turn on Sleeper in Hotbox on button press** — **Automation ID:** [1747642993124](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7823)

  Triggered when `sensor.remote_hotbox_sleeper_button_action` changes to `off`. Actions: toggle `input_boolean.sleeper_in_hotbox`.

- **Turn on sleeper in hotbox when Lylou falls asleep** — **Automation ID:** [1757805301673](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11826)

  Triggered when `binary_sensor.lylou_is_asleep_bayesian` changes from `off` to `on` for 5m. Runs only if condition `sun`. Actions: turn on `input_boolean.sleeper_in_hotbox`.

- **Undo Sleep actions when "sleeper in workshop" is turned off** — **Automation ID:** [1728310438747](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3605)

  Triggered when `input_boolean.sleeper_in_workshop` changes from `on` to `off`. Actions: compute variables; turn on `{{off_lights_in_room}}` (transition=120); +2 more actions.

- **Unmute patio assistant when people are present** — **Automation ID:** [1775146719220](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L16657)

  Triggered when `sensor.patio_camera_person_count` goes above `0`; `binary_sensor.group_patio_motion_occupancy` changes from `on`. Runs only if `binary_sensor.group_patio_motion_occupancy` is `on`; `sensor.patio_camera_person_count` is above `0`. Actions: turn off `switch.patio_assistant_mute`.

<a id="voice-assistants-commands-16"></a>
## Voice Assistants & Commands (16)

[Back to top](#top)

- **Adjust volume of voice assistants on answering** — **Automation ID:** [1751909529397](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9907)

  Triggered when `assist_satellite.esp32_s3_box_kitchen_assistant_assist_satellite`, `assist_satellite.saloon_assistant_assist_satellite`, `assist_satellite.lv_assistant_assist_satellite`, `assist_satellite.bathroom_assistant_assist_satellite` +8 more changes to `responding`. Actions: compute variables; set volume on `{{ mediaplayers}}
` (volume_level=1).

- **Assist Equalize volume in the house** — **Automation ID:** [1732990123635](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4611)

  Triggered when conversation command `equalize the volume`, `normalize the volume`. Actions: compute variables; if `input_boolean.following_music` is `off`, set volume on `media_player.mass_hallway_wiim_speaker`, `media_player.mass_kitchen_wiim_speaker`, `media_player.mass_salon_wiim_speaker`, `media_player.mass_lounge_wiim_speaker` +1 more (volume_level='{{average_volume}}'); if `input_boolean.following_music` is `on`, repeat set volume on `{{ "media_player.universal_" + unmuted_speakers[repeat.index - 1] + "_speakers" }}
` (volume_level='{{average_volume}}').

- **Assist identify the device** — **Automation ID:** [1738198899369](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5385)

  Triggered when conversation command `who are you`, `what is your name`, `what is your identity`. Actions: call `assist_satellite.announce` on.

- **Assist mute ESP32 endpoints when room is unoccupied** — **Automation ID:** [1734282372322](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4938)

  Triggered when `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy`, `binary_sensor.group_kitchen_motion_occupancy`, `binary_sensor.group_hotbox_top_motion_occupancy` +9 more changes from `on` to `off` for 2m. Actions: compute variables; repeat turn on `{{esp32_mute_entities[repeat.index - 1]    }}`.

- **Assist play next song** — **Automation ID:** [1716566146304](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1968)

  Triggered when conversation command `[Play] [the] next song`, `Skip [this/the] song`. Actions: skip to next track on `media_player.group_mass_home_group_speakers`.

- **Assist Play random music** — **Automation ID:** [1727273501229](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3532)

  Triggered when conversation command `play random music`. Actions: play media via `media_player.play_media` on `media_player.group_mass_home_group_speakers`.

- **Assist quick color change phrase** — **Automation ID:** [1733533239694](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4660)

  Triggered when conversation command `[set] [turn] [the] lights [to] {color}`, `{color} lights`. Actions: compute variables; turn on area `{{room }}`.

- **Assist Restart Music Assistant** — **Automation ID:** [1711565389493](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L82)

  Triggered when conversation command `Restart Music Assistant`. Actions: call `hassio.addon_restart` on (addon='d5369777_music_assistant_beta').

- **assist Stop home group music** — **Automation ID:** [1722263369374](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3226)

  Triggered when conversation command `stop [the] music`. Actions: stop media on `media_player.group_mass_home_group_speakers`.

- **Assist Trigger find my phone** — **Automation ID:** [1732911162102](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4595)

  Triggered when conversation command `where is my phone`, `find my phone`, `phone now`. Actions: run `script.find_phone`.

- **Assist turn off ESP32 endpoint screens when room is unoccupied** — **Automation ID:** [1734282532788](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4982)

  Triggered when `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy`, `binary_sensor.group_kitchen_motion_occupancy`, `binary_sensor.group_hotbox_top_motion_occupancy` +10 more changes to `off` for 1m 45s. Actions: compute variables; repeat turn off `{{esp32_screens_entities[repeat.index - 1]    }}`.

- **Assist turn on ESP32 endpoint screens when room is occupied** — **Automation ID:** [1734282569650](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5025)

  Triggered when `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy`, `binary_sensor.group_kitchen_motion_occupancy`, `binary_sensor.group_hotbox_top_motion_occupancy` +10 more changes from `off` to `on`. Runs only if template `{{ area_id(trigger.entity_id) | replace('_top','') | replace('_down','') not in states('sensor.active_sleeper_rooms') | ` is true. Actions: compute variables; repeat turn on `{{esp32_screens_entities[repeat.index - 1]    }}`.

- **Assist Unmute ESP32 endpoints when room is occupied** — **Automation ID:** [1734282252470](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4892)

  Triggered when `binary_sensor.group_lounge_motion_occupancy`, `binary_sensor.group_hallway_motion_occupancy`, `binary_sensor.group_kitchen_motion_occupancy`, `binary_sensor.group_hotbox_top_motion_occupancy` +10 more changes to `on` for 0s. Runs only if template `{{ area_id(trigger.entity_id) | replace('_top','') | replace('_down','') not in states('sensor.active_sleeper_rooms') | ` is true. Actions: compute variables; repeat turn off `{{esp32_mute_entities[repeat.index - 1]    }}`.

- **Change light to random colors on press** — **Automation ID:** [1714077180427](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1169)

  Triggered when `input_boolean.random_colors` changes to `on`; conversation command `random colors lights`, `random color lights`, `random colored lights`. Actions: run `script.changes_all_on`.

- **Change light to random pastel colors on press** — **Automation ID:** [1714077215807](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1189)

  Triggered when `input_boolean.random_pastel_colors` changes to `on`; conversation command `pastel colors lights`, `pastel color lights`, `pastel colored lights`. Actions: compute variables; repeat run `script.change_received_lights_to_random_pastel_colors`.

- **Mute all voice assistants while a concert is happening** — **Automation ID:** [1761366181983](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12230)

  Triggered when `input_boolean.live_concert` changes from `off` to `on` for 5m. Actions: turn on `switch.assistants_mute_group`; wait for `input_boolean.live_concert` changes from `on` to `off` for 5m (timeout 10h); +1 more actions.

<a id="notifications-personal-devices-15"></a>
## Notifications & Personal Devices (15)

[Back to top](#top)

- **Alert house-wide when there is smoke in the kitchen** — **Automation ID:** [1731611005918](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4297)

  Triggered when `binary_sensor.sensor_kitchen_smoke_detector_smoke` changes to `on` for 0s. Actions: repeat run `script.broadcast_alert_in_the_house`; send notification via `notify.maxi_notification_group` ('Smoke detected in the kitchen') every 4m.

- **Alert maxi in the room where he is when his phone rings** — **Automation ID:** [1740953881544](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6025)

  Triggered when `sensor.phone_maxi_phone_state` changes to `ringing`. Actions: compute variables; call `media_player.unjoin` on `{{ speaker }} 
`; +2 more actions.

- **Alert maxi that his laptop battery is low** — **Automation ID:** [1739201293361](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5427)

  Triggered when `sensor.laptop_maxi_msi_battery_charge_remaining_percentage` goes below `25`. Actions: if `person.maximiliano` is `home`, compute variables; announce via `tts.speak` ('Charge your laptop!'); if `person.maximiliano` is `not_home`, send notification via `notify.maxi_notification_group` ('LOW BATTERY!').

- **Alert maxi when people are too noisy on the patio during a party ** — **Automation ID:** [1731514999187](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4135)

  Triggered when `binary_sensor.patio_camera_yell_sound`, `binary_sensor.patio_camera_cheering_sound`, `binary_sensor.patio_camera_crowd_sound` changes from `off` to `on` for 10s; `sensor.patio_camera_sound_level` goes above `-30`. Runs only if `binary_sensor.group_patio_motion_occupancy` is `on`; `input_boolean.party_mode` is `on`. Actions: if `person.maximiliano` is `home` and `input_boolean.party_mode` is `on`, send notification via `notify.maxi_notification_group` ('People are being too noisy on the patio'); if `person.maximiliano` is `not_home` and `input_boolean.party_mode` is `on`, run `script.broadcast_alert_in_the_house`; send notification via `notify.maxi_notification_group` ('People are being told to reduce noise on the patio').

- **Alert myriam when lylou nears home** — **Automation ID:** [1759251903732](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12006)

  Triggered when `person.lylou` changes from `not_home` to `home`. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 240 and trigger.from_state.sta` is true; not (`person.myriam` is `home`). Actions: send notification via `notify.myriam_notification_group` ('Lylou is nearing home!').

- **Notify maxi when myriam arrives at home** — **Automation ID:** [1759251745280](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11980)

  Triggered when `person.myriam` changes from `not_home` to `home`. Runs only if template `{{ ((as_timestamp((now()) ) - (as_timestamp(trigger.from_state.last_changed)))) | float > 240 and trigger.from_state.sta` is true. Actions: send notification via `notify.maxi_notification_group` ('Myriam is nearing home!').

- **Open phone to 911 number when smoke "call 911" notification is pressed** — **Automation ID:** [1735277359392](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5182)

  Triggered when event `mobile_app_notification_action` with action=`open_phone_app_911`. Actions: send notification via `notify.maxi_notification_group` ('command_activity').

- **Sleep actions when "maxi is sleeping" is turned on anywhere** — **Automation ID:** [1732291871329](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4432)

  Triggered when `binary_sensor.maxi_sleeping` changes to `on` for 5m. Actions: send notification via `notify.maxi_notification_group` ('command_dnd'); send notification via `notify.maxi_notification_group` ('command_screen_brightness_level').

- **Tell maxi about the weather when he awakes** — **Automation ID:** [1715696933725](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1675)

  Triggered when `sensor.is_maxi_asleep` changes to `awake` for 5m. Runs only if `person.maximiliano` is `home`; time is after `06:45:00` and before `10:00:00`. Actions: send notification via `notify.maxi_notification_group` ('It is "{{ states(\'weather.weather_montreal_forecast\') }}" and the temperature is "{{ states(\'sensor.outside_temperature_with_fallback\') }}"').

- **Tell maxi when his watch is charged** — **Automation ID:** [1715788063807](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1834)

  Triggered when `sensor.maxi_watch_battery_level` changes to `100`. Runs only if `sensor.maxi_watch_battery_state` is `charging`. Actions: send notification via `notify.maxi_notification_group` ('Your watch is charged!').

- **Trigger all home defense script through notification** — **Automation ID:** [1717359724027](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2995)

  Triggered when event `mobile_app_notification_action` with action=`trigger_all_alerts`. Actions: run `script.in_home_defense_script`.

- **Trigger In home defense script through phone notification** — **Automation ID:** [1717797397935](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3055)

  Triggered when event `mobile_app_notification_action` with action=`trigger_inside_alert`. Actions: run `script.inside_only_home_defense_script`.

- **Turn off Phone Bluetooth emitter when maxi leaves home** — **Automation ID:** [1732294106703](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4512)

  Triggered when `person.maximiliano` changes to `not_home`. Actions: send notification via `notify.maxi_notification_group` ('command_ble_transmitter').

- **Turn on Phone Bluetooth emitter when maxi arrives home** — **Automation ID:** [1732294056055](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4493)

  Triggered when `person.maximiliano` changes to `home`. Actions: send notification via `notify.maxi_notification_group` ('command_ble_transmitter').

- **Undo sleeping actions for Maxi when he awakes** — **Automation ID:** [1732293937510](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4463)

  Triggered when `binary_sensor.maxi_sleeping` changes to `off` for 5m. Actions: send notification via `notify.maxi_notification_group` ('command_dnd'); send notification via `notify.maxi_notification_group` ('command_auto_screen_brightness').

<a id="tablets-screens-device-power-22"></a>
## Tablets, Screens & Device Power (22)

[Back to top](#top)

- **Adjust Bedroom Tablet brightness to match the lights** — **Automation ID:** [1763283441940](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12871)

  Triggered when `light.group_bedroom_lights` changes for 3s. Actions: compute variables; call `number.set_value` on `number.bedroom_tablet_screen_brightness`.

- **Adjust closet tablet screen brightness to match the lights** — **Automation ID:** [1740935205313](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5918)

  Triggered when `light.group_closet_lights` changes for 2s. Runs only if `sensor.closet_tablet_battery` is above `50`. Actions: compute variables; call `number.set_value` on `number.closet_tablet_screen_brightness`.

- **Adjust hallway tablet screen brightness to match the lights** — **Automation ID:** [1741317422533](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6062)

  Triggered when attribute `brightness` on `light.group_hallway_lights` changes for 1s. Runs only if `sensor.hallway_tablet_battery` is above `30`. Actions: compute variables; call `number.set_value` on `number.hallway_tablet_screen_brightness`.

- **Adjust Kitchen Tablet brightness to match the lights** — **Automation ID:** [1753302685762](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L11270)

  Triggered when `light.group_kitchen_lights` changes for 1s. Actions: compute variables; call `number.set_value` on `number.tablet_kitchen_s6_lite_screen_brightness`.

- **Adjust workshop tablet screen brightness to match the lights** — **Automation ID:** [1747973281617](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L7929)

  Triggered when attribute `brightness` on `light.group_workshop_lights` changes for 1s. Runs only if `sensor.workshop_tablet_battery` is above `30`; `input_boolean.sleeper_in_workshop` is `off`. Actions: compute variables; call `number.set_value` on `number.workshop_tablet_screen_brightness`.

- **Auto close closet tablet screen when someone is sleeping** — **Automation ID:** [1715664801848](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1649)

  Triggered when `switch.closet_tablet_screen` changes to `on`. Runs only if `input_boolean.sleeper_in_closet` is `on`. Actions: wait 1m; turn off `switch.closet_tablet_screen`.

- **Control bedroom tablet screen to avoid full discharge** — **Automation ID:** [1765731514294](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14809)

  Triggered when `sensor.bedroom_tablet_battery` goes above `95`; `sensor.bedroom_tablet_battery` goes below `50`. Actions: if `sensor.bedroom_tablet_battery` is below `15` and `switch.bedroom_tablet_screen` is `on`, call `number.set_value` on `number.bedroom_tablet_screen_brightness`; turn off `switch.bedroom_tablet_screen`; if `sensor.bedroom_tablet_battery` is below `50` and `switch.bedroom_tablet_screen` is `on`, call `number.set_value` on `number.bedroom_tablet_screen_brightness`.

- **Control closet tablet screen to avoid full discharge** — **Automation ID:** [1765730839407](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14757)

  Triggered when `sensor.closet_tablet_battery` goes above `50` and below `55`; `sensor.closet_tablet_battery` goes below `15`. Actions: if `sensor.closet_tablet_battery` is below `15` and `switch.closet_tablet_screen` is `on`, call `number.set_value` on `number.closet_tablet_screen_brightness`; turn off `switch.closet_tablet_screen`; if `sensor.closet_tablet_battery` is below `50` and `switch.closet_tablet_screen` is `on`, call `number.set_value` on `number.closet_tablet_screen_brightness`.

- **Control Hallway Tablet charging** — **Automation ID:** [1713538197256](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L618)

  Triggered when `sensor.hallway_tablet_battery` changes. Actions: if `sensor.hallway_tablet_battery` is above `90`, turn off `switch.plug_hallway_tablet`; if `sensor.hallway_tablet_battery` is below `15`, turn on `switch.plug_hallway_tablet`; call `number.set_value` on `number.hallway_tablet_screen_brightness`; +1 more branches.

- **Control workshop secondary tablet screen to avoid full discharge** — **Automation ID:** [1711648800000](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L16402)

  Triggered when `sensor.workshop_secondary_tablet_battery` goes above `50` and below `55`; `sensor.workshop_secondary_tablet_battery` goes below `15`; `sensor.workshop_secondary_tablet_battery` goes above `70`. Actions: if `sensor.workshop_secondary_tablet_battery` is below `15` and `switch.workshop_secondary_tablet_screen` is `on`, call `number.set_value` on `number.workshop_secondary_tablet_screen_brightness`; turn off `switch.workshop_secondary_tablet_screen`; if `sensor.workshop_secondary_tablet_battery` is below `50` and `switch.workshop_secondary_tablet_screen` is `on`, call `number.set_value` on `number.workshop_secondary_tablet_screen_brightness`; +1 more branches.

- **Control workshop Tablet charging** — **Automation ID:** [1715103309573](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1603)

  Triggered when `sensor.workshop_tablet_battery` changes. Runs only if (`sensor.workshop_tablet_battery` is above `90` or `sensor.workshop_tablet_battery` is below `30`). Actions: if `sensor.workshop_tablet_battery` is above `90`, turn off `switch.plug_workshop_tablet`; if `sensor.workshop_tablet_battery` is below `30`, turn on `switch.plug_workshop_tablet`; call `number.set_value` on `number.workshop_tablet_screen_brightness`.

- **Display room color  on TVs when mimiclights mode is triggered** — **Automation ID:** [1729462034192](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L3796)

  Triggered when `input_boolean.mimic_lights_on_screens` changes. Actions: if `input_boolean.mimic_lights_on_screens` is `on`, compute variables; repeat compute variables; call `cast.show_lovelace_view` on `{{repeat.item}}`; if `input_boolean.mimic_lights_on_screens` is `off`, compute variables; repeat turn off `{{repeat.item}}`.

- **kitchen google display automation** — **Automation ID:** [1733597890338](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4682)

  Triggered when attribute `app_id` on `media_player.kitchen_google_hub` changes for 1m; `media_player.kitchen_google_hub` changes from `playing` to `paused` for 10m. Runs only if not (attribute `app_id` on `media_player.kitchen_google_hub` is `A078F6B0`); not ((attribute `app_id` on `media_player.kitchen_google_hub` is `9AC194DC` and `media_player.kitchen_google_hub` is `playing`)). Actions: turn off `media_player.kitchen_google_hub`; call `cast.show_lovelace_view` on `media_player.kitchen_google_hub`; +1 more actions.

- **Mute speakers in room when Myriam's tablet starts jellyfin playback** — **Automation ID:** [1751301093156](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9780)

  Triggered when `sensor.myriam_tablet_location_compounded` changes for 2s. Runs only if `input_boolean.following_music` is `on`; (`media_player.universal_tablet_myriam_s8` is `playing`). Actions: compute variables; mute `{{playing_speakers}}
`.

- **Reload Hallway tablet start URL when occupancy of hallway goes to off for 5  minutes** — **Automation ID:** [1748703259809](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8140)

  Triggered when `binary_sensor.group_hallway_motion_occupancy` changes from `on` to `off` for 5m. Runs only if not (`sensor.maxi_location_by_petro` is `hallway`); not (attribute `full_url` on `sensor.hallway_tablet_current_page` is `http://192.168.0.15:8123/tablet-entrance-dashboard/main-view?kiosk`; attribute `full_url` on `sensor.hallway_tablet_current_page` is `http://192.168.0.15:8095/#/party`). Actions: press `button.hallway_tablet_load_start_url`.

- **Reload tablet page when motion is detected in the kitchen** — **Automation ID:** [1717291435116](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L2614)

  Triggered when `binary_sensor.group_kitchen_motion_occupancy` changes to `off` for 5m. Actions: press `button.tablet_kitchen_s6_lite_load_start_url`.

- **Reload workshop tablet start URL when occupancy of workshop goes to off for 5 minutes** — **Automation ID:** [1748703480244](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8180)

  Triggered when `binary_sensor.group_workshop_motion_occupancy` changes from `on` to `off` for 10m. Runs only if not (`sensor.maxi_location_by_petro` is `workshop`); not (`sensor.workshop_tablet_current_page` is `http://192.168.0.15:8123/tablet-workshop-dashboard/main-view?kiosk=1`; `sensor.workshop_tablet_current_page` is `http://192.168.0.15:8095/#/party`). Actions: press `button.workshop_tablet_load_start_url`.

- **Show main screen when music stops on Hallway tablet** — **Automation ID:** [1730910881566](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4063)

  Triggered when `media_player.group_mass_home_group_speakers` changes to `off` for 1m. Runs only if (attribute `full_url` on `sensor.hallway_tablet_current_page` is `http://192.168.0.15:8123/tablets-subviews/subview-music#camera_motion_detected` or attribute `full_url` on `sensor.hallway_tablet_current_page` is `http://192.168.0.15:8123/tablets-subviews/subview-music`). Actions: press `button.hallway_tablet_load_start_url`.

- **Turn on Listening Chime on assist endpoints when screen turns off** — **Automation ID:** [1752076862818](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L10034)

  Triggered when `light.bedroom_assistant_lcd_backlight`, `light.desktop_assistant_lcd_backlight`, `light.esp32_s3_box_kitchen_assistant_lcd_backlight`, `light.lv_assistant_lcd_backlight` +2 more changes from `on` to `off`. Actions: compute variables; turn on `{{esp32_wake_sound_entities}}
`.

- **Turns on maintenance mode for all tablets During Live Concert** — **Automation ID:** [1761366394577](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L12276)

  Triggered when `input_boolean.live_concert` changes from `off` to `on` for 5m. Actions: turn on `switch.maintenance_mode_all_tablets`; wait for `input_boolean.live_concert` changes from `on` to `off` for 5m (timeout 5h); +1 more actions.

- **Update Universal Myriam's Tablet Entity area when it changes room** — **Automation ID:** [1751305945586](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L9822)

  Triggered when `sensor.myriam_tablet_location_compounded` changes for 2s. Actions: call `homeassistant.add_entity_to_area` on `media_player.universal_tablet_myriam_s8`.

- **Workshop Tablet dial automation** — **Automation ID:** [1774748550086](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L16473)

  No triggers or actions are defined, so this appears to be incomplete or placeholder-like.

<a id="household-appliances-utility-alerts-9"></a>
## Household Appliances & Utility Alerts (9)

[Back to top](#top)

- **Alert that the vacuum is stuck** — **Automation ID:** [1713399927983](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L242)

  Triggered when `vacuum.roborock_qrevo_curv_series` changes to `error` for 5m. Actions: repeat run `script.non_important_broadcast_alert_in_the_house` every 10m.

- **Alert when Airfryer has finished** — **Automation ID:** [1764891085092](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L14710)

  Triggered when `sensor.plug_kitchen_airfryer_electric_consumption_w` goes above `100`. Runs only if `binary_sensor.maxi_sleeping` is `off`; `binary_sensor.all_present_sleeping` is `off`; +1 more conditions. Actions: wait for `sensor.plug_kitchen_airfryer_electric_consumption_w` goes below `3` for 3m (timeout 20m); run `script.non_important_broadcast_alert_in_the_house`; +1 more actions.

- **Alert when Dishwasher finishes and no one is in the kitchen** — **Automation ID:** [1772205343426](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15451)

  Triggered when `sensor.plug_kitchen_dishwasher_electric_consumption_w` goes above `100`. Runs only if `binary_sensor.maxi_sleeping` is `off`; `binary_sensor.all_present_sleeping` is `off`. Actions: wait for `sensor.plug_kitchen_dishwasher_electric_consumption_w` goes below `5` for 5s (timeout 20m); run `script.non_important_broadcast_alert_in_the_house`.

- **Alert when dryer finishes!** — **Automation ID:** [1714054125770](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1049)

  Triggered when `sensor.dryer_current_status` changes from `running` to `end`. Runs only if `binary_sensor.maxi_sleeping` is `off`; `binary_sensor.all_present_sleeping` is `off`. Actions: run `script.non_important_broadcast_alert_in_the_house`; repeat  every 30m.

- **Alert when Microwave finishes** — **Automation ID:** [1714058481200](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1092)

  Triggered when `sensor.plug_kitchen_microwave_electric_consumption_w` goes above `100`. Runs only if `binary_sensor.maxi_sleeping` is `off`; `binary_sensor.all_present_sleeping` is `off`; +1 more conditions. Actions: wait for `sensor.plug_kitchen_microwave_electric_consumption_w` goes below `5` for 5s (timeout 20m); if `binary_sensor.group_kitchen_motion_occupancy` is `off`, run `script.non_important_broadcast_alert_in_the_house`.

- **Alert when the fridge remains open** — **Automation ID:** [1731610964007](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4266)

  Triggered when `binary_sensor.sensor_fridge_door_contact` changes to `on` for 4m. Actions: repeat run `script.broadcast_alert_in_the_house` every 4m.

- **Alert when the stove water is boiling.** — **Automation ID:** [1731610772963](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4243)

  Triggered when `sensor.sensor_stove_temperature_humidity` goes above `75`. Runs only if `input_boolean.cooking_mode` is `off`. Actions: run `script.non_important_broadcast_alert_in_the_house`; wait 15m.

- **Alert when Toaster finishes** — **Automation ID:** [1772205443339](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15489)

  Triggered when `sensor.plug_kitchen_toaster_electric_consumption_w` goes above `50`. Runs only if `binary_sensor.maxi_sleeping` is `off`; `binary_sensor.all_present_sleeping` is `off`; +1 more conditions. Actions: wait for `sensor.plug_kitchen_toaster_electric_consumption_w` goes below `2` for 5s (timeout 20m); if `binary_sensor.group_kitchen_motion_occupancy` is `off`, run `script.non_important_broadcast_alert_in_the_house`.

- **Alert when Washer finishes** — **Automation ID:** [1714058675432](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L1137)

  Triggered when `sensor.plug_kitchen_washer_electric_consumption_a` goes above `1`. Runs only if `binary_sensor.maxi_sleeping` is `off`; `binary_sensor.all_present_sleeping` is `off`. Actions: wait for `sensor.plug_kitchen_washer_electric_consumption_a` goes below `0.01` for 5m; run `script.non_important_broadcast_alert_in_the_house`.

<a id="system-network-integrations-8"></a>
## System, Network & Integrations (8)

[Back to top](#top)

- **Auto-timeout safety net for temporary input booleans** — **Automation ID:** [1740934326306](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5734)

  Triggered when `input_boolean.waiting_someone` changes from `off` to `on` for 1h; `input_boolean.sleeper_in_closet` changes from `off` to `on` for 10h; +8 more triggers. Actions: turn off `{{ trigger.entity_id }}`.

- **Reload tablets 4 minutes after HA restarts** — **Automation ID:** [1713813890992](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L919)

  Triggered when `sensor.bootminutes` goes above `3` and below `5`. Actions: press `button.hallway_tablet_load_start_url`, `button.workshop_tablet_load_start_url`, `button.closet_tablet_load_start_url`, `button.tablet_kitchen_s6_lite_load_start_url` +5 more.

- **Update cloudflare when IP changes** — **Automation ID:** [1743869107759](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L6885)

  Triggered when attribute `ip` on `device_tracker.ucg_ultra` changes. Actions: call `cloudflare.update_records` on.

- **Vibe Arc - Enable/Disable Handler** — **Automation ID:** `vibe_arc_enable_disable`
  Triggered when `input_boolean.vibe_arc_enabled` changes to `off`. Actions: set option on `input_select.vibe_arc_phase` (option='Off'); turn off `input_boolean.vibe_arc_paused`, `input_boolean.vibe_arc_acting`; +1 more actions.

- **Vibe Arc - Override Detection** — **Automation ID:** `vibe_arc_override_detection`
  Triggered when `input_boolean.party_mode`, `input_boolean.kink_party`, `input_boolean.acid_time` changes. Runs only if `input_boolean.vibe_arc_enabled` is `on`; `input_boolean.vibe_arc_acting` is `off`. Actions: turn on `input_boolean.vibe_arc_paused`; call `timer.start` on `timer.vibe_arc_override`.

- **Vibe Arc - Override Release** — **Automation ID:** `vibe_arc_override_release`
  Triggered when event `timer.finished`. Runs only if `input_boolean.vibe_arc_enabled` is `on`. Actions: turn off `input_boolean.vibe_arc_paused`.

- **Vibe Arc - Phase Controller** — **Automation ID:** `vibe_arc_phase_controller`
  Triggered when `input_boolean.vibe_arc_enabled` changes to `on`; `sensor.total_of_persons_in_the_appartment` changes; +6 more triggers. Runs only if `input_boolean.vibe_arc_enabled` is `on`; `input_boolean.vibe_arc_paused` is `off`. Actions: if trigger id `people_change`, `noise_high`, `crowd`, `yell` matched, wait 3m; compute variables; +1 more actions.

- **Vibe Arc - Phase Executor** — **Automation ID:** `vibe_arc_phase_executor`
  Triggered when `input_select.vibe_arc_phase` changes. Runs only if `input_boolean.vibe_arc_enabled` is `on`; not (`input_select.vibe_arc_phase` is `Off`). Actions: if `input_select.vibe_arc_phase` is `Gathering`, run `script.vibe_arc_gathering`; if `input_select.vibe_arc_phase` is `Warming Up`, run `script.vibe_arc_warming_up`; +4 more branches.

<a id="other-mixed-10"></a>
## Other / Mixed (10)

[Back to top](#top)

- **Alert Housewide when Kitchen timer is finished** — **Automation ID:** [1731611467781](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L4344)

  Triggered when `timer.cooking` changes from `active` for 1s. Actions: repeat run `script.non_important_broadcast_alert_in_the_house` every 5m.

- **Hibernate Maxi Desktop when he leaves the Salon for more than 20 minutes** — **Automation ID:** [1748938888873](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8770)

  Triggered when `sensor.maxi_location_by_petro` changes from `salon` for 20m. Runs only if `sensor.maxis_current_game` is `idle`. Actions: press `button.desktop_maxi_hibernate`.

- **Hibernate Maxi Desktop when he sleeps for over 40 minutes** — **Automation ID:** [1748938722404](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L8748)

  Triggered when `binary_sensor.maxi_sleeping` changes from `off` to `on` for 40m. Actions: press `button.desktop_maxi_hibernate`.

- **Purify Salon air when someone is smoking** — **Automation ID:** [1772575048674](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15653)

  Triggered when `binary_sensor.active_smoking_in_the_salon` changes to `on`. Actions: turn on `switch.plug_salon_air_purifier`.

- **Raise bathroom volume when door closes** — **Automation ID:** [1735682831028](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5230)

  Triggered when `binary_sensor.sensor_bathroom_door_contact` changes from `on` to `off` for 0s. Actions: run `script.speaker_automatic_volume_adjustment_script`.

- **Trigger kinky ambiance script when kinky party turns on** — **Automation ID:** [1739373870986](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5514)

  Triggered when `input_boolean.kink_party` changes from `off` to `on`. Actions: run `script.set_whole_house_to_kinky_ambiance`.

- **Trigger psychedelic script when acid mode turns on** — **Automation ID:** [1739373811238](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5498)

  Triggered when `input_boolean.acid_time` changes from `off` to `on`. Actions: run `script.set_whole_house_to_acid_time`.

- **Turn off acid time when everyone goes to bed** — **Automation ID:** [1740934084849](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L5707)

  Triggered when `binary_sensor.all_residents_sleeping` changes from `off` to `on` for 30m. Runs only if `input_boolean.acid_time` is `on`. Actions: turn off `input_boolean.acid_time`.

- **Turn off Sleeping mode for Adaptative lighting on "all resident sleeping" turns off** — **Automation ID:** [1713451780517](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L336)

  Triggered when `binary_sensor.all_residents_sleeping` changes to `off` for 20m. Runs only if time is after `10:00:00` and before `22:00:00` and weekday in fri, thu, wed, tue, mon, sat, sun. Actions: turn off `switch.adaptive_lighting_sleep_mode_adaptive_lighting`.

- **Turn on Waiting Pakidge when Intelcom emails me and off when it arrives** — **Automation ID:** [1768062570998](https://github.com/maxi1134/Home-Assistant-Config/blob/675eae0e9f0285d878ce37849a32dfd213f80903/automations.yaml#L15282)

  Triggered when event `imap_content`. Actions: if template `{{ 'ici une heure' in trigger.event.data.subject }}` is true, turn on `input_boolean.waiting_package`; if template `{{ 'Votre colis est arriv' in trigger.event.data.subject }}` is true, wait 10m; turn off `input_boolean.waiting_package`.
