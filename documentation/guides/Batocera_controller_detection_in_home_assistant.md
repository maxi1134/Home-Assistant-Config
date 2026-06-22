Here is how you can achieve this, be wary that you will need to SSH onto the Batocera machine:

### 0: Create the required folders:
```
mkdir -p /userdata/system/configs/bt-mqtt
mkdir -p /userdata/system/configs/emulationstation/scripts/controller-connected
mkdir -p /userdata/system/configs/emulationstation/scripts/controller-disconnected
```

### 1: Configure your MQTT server onto the Batocera machine
 -    `nano /userdata/system/configs/bt-mqtt/bt-mqtt.conf`
 - Then fill in your MQTT server informations
 ```
 # ===== FILL THESE IN =====
MQTT_HOST="192.168.1.10"        # IP of your MQTT broker (e.g. HA's Mosquitto add-on)
MQTT_PORT="1883"
MQTT_USER="USERNAME"
MQTT_PASSWORD="PASSSWORD"
# Base topic. Keep this identical on the Home Assistant side.
MQTT_BASE_TOPIC="batocera/arcade"
```
---
### 2: Create the `publish-controller.sh` script file
 - `nano /userdata/system/configs/bt-mqtt/publish-controller.sh`
 - Then copy this entire blob into it and save it
 ```
 #!/bin/bash
# Args: <connected|disconnected> <device path, e.g. /dev/input/js0>
# Runs as root from udev's minimal env, so set PATH and use absolute fallbacks.
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

CONF="/userdata/system/configs/bt-mqtt/bt-mqtt.conf"
LOG="/userdata/system/logs/controller-mqtt.log"
mkdir -p /userdata/system/logs
[ -f "$CONF" ] && . "$CONF"

action="$1"
dev="$2"
ts="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"

MP="$(command -v mosquitto_pub 2>/dev/null)"
[ -z "$MP" ] && [ -x /usr/bin/mosquitto_pub ] && MP=/usr/bin/mosquitto_pub

# Best-effort controller name from sysfs (jsN -> device/name)
js="$(basename "$dev" 2>/dev/null)"
name="unknown"
[ -n "$js" ] && [ -r "/sys/class/input/$js/device/name" ] && \
  name="$(cat "/sys/class/input/$js/device/name" 2>/dev/null)"

# Count joysticks present right now (the removed one is already gone on disconnect)
count=0
for n in /dev/input/js*; do [ -e "$n" ] && count=$((count+1)); done
[ "$count" -ge 1 ] && state="ON" || state="OFF"

base="${MQTT_BASE_TOPIC:-batocera/$(hostname)}"
json="{\"action\":\"$action\",\"device\":\"$dev\",\"name\":\"$name\",\"count\":$count,\"state\":\"$state\",\"ts\":\"$ts\"}"

echo "$ts action=$action dev=$dev name=\"$name\" count=$count state=$state" >> "$LOG"

if [ -z "$MP" ]; then
  echo "$ts ERROR: mosquitto_pub not found" >> "$LOG"; exit 1
fi

pub() { "$MP" -h "$MQTT_HOST" -p "${MQTT_PORT:-1883}" -u "$MQTT_USER" -P "$MQTT_PASSWORD" "$@" >> "$LOG" 2>&1; }

# Retained connectivity state -> drives the binary_sensor
pub -t "$base/controller/connected"  -m "$state" -q 1 -r
# Retained attributes -> controller name / device / count
pub -t "$base/controller/attributes" -m "$json"  -q 1 -r
# NON-retained edge pulse -> what the automation triggers on
#   (not retained, so it never replays and yanks your TV input on HA restart)
[ "$action" = "connected" ] && pub -t "$base/controller/event" -m "connected" -q 1
 ```
 - Finally, make it executable with `chmod +x /userdata/system/configs/bt-mqtt/publish-controller.sh`
---
### 3: Create the controller-connected and controller-disconnected sttate scripts

- Create the `controller-connected` script
    - `nano /userdata/system/configs/emulationstation/scripts/controller-connected/mqtt.sh`
    - Then copy `#!/bin/bash
exec /userdata/system/configs/bt-mqtt/publish-controller.sh connected "$1"` into it and save it
    - and make it executable: `chmod +x /userdata/system/configs/emulationstation/scripts/controller-connected/mqtt.sh`

- Create the `controller-disconnected` script
    - `nano /userdata/system/configs/emulationstation/scripts/controller-disconnected/mqtt.sh`
    - Then copy `#!/bin/bash
exec /userdata/system/configs/bt-mqtt/publish-controller.sh disconnected "$1"` into it and save it
    - and make it executable: `chmod +x /userdata/system/configs/emulationstation/scripts/controller-disconnected/mqtt.sh`

------

# On the Home-Assistant side:

Create an MQTT sensor under your configuration.yaml with the following value:

```
mqtt:
  - binary_sensor:
      name: "Batocera controller"
      unique_id: batocera_arcade_controller
      state_topic: "batocera/arcade/controller/connected"
      payload_on: "ON"
      payload_off: "OFF"
      device_class: connectivity
      json_attributes_topic: "batocera/arcade/controller/attributes"
```


Voila!
Your Home-Assistant sensor "Batocera controller" should now turn on when a controller is actively connected to the Batocera machine!