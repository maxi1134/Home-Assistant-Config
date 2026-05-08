# mDNS Discovery Component for ESPHome

Automatic discovery of devices on the local network using mDNS (Bonjour/Avahi).

## Features

- **Service Discovery**: Find devices advertising specific mDNS services
- **Automatic Updates**: Periodic scanning with configurable interval
- **Peer Tracking**: Maintains list of discovered peers with name, IP, port
- **Event Triggers**: Automations when peers appear or disappear
- **Manual Scan**: Force immediate discovery via lambda

## Use Cases

This component is useful beyond intercom systems:

- **Smart Home**: Discover ESPHome devices, Home Assistant instances
- **Printers/Scanners**: Find network printing devices
- **Media Devices**: Locate Chromecast, AirPlay, Spotify Connect devices
- **IoT Networks**: Build dynamic device meshes without static IPs
- **Monitoring**: Track when devices come online/offline
- **Service Discovery**: Find any mDNS-advertised service

## Installation

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n-IA-hane/esphome-intercom
      ref: main
      path: components
    components: [mdns_discovery]
```

## Configuration

```yaml
mdns_discovery:
  id: discovery
  service_type: "_http._tcp"      # Service to discover
  scan_interval: 30s              # How often to scan (default: 60s)
  on_peer_found:                  # Triggered when new peer appears
    - logger.log:
        format: "Found: %s at %s:%d"
        args: ["name.c_str()", "ip.c_str()", "port"]
  on_peer_lost:                   # Triggered when peer disappears
    - logger.log:
        format: "Lost: %s"
        args: ["name.c_str()"]
```

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `id` | ID | Required | Component ID for referencing |
| `service_type` | string | Required | mDNS service type (e.g., `_http._tcp`) |
| `scan_interval` | time | 10s | How often to issue an mDNS query for the configured service. |
| `peer_timeout` | time | 60s | How long a previously-seen peer is kept in the registry without a fresh response before it is considered lost. Trigger `on_peer_lost` fires when the timeout elapses. |
| `on_peer_found` | automation | - | Actions when a new peer is discovered. Lambda args: `name` (string), `ip` (string), `port` (uint16). |
| `on_peer_lost` | automation | - | Actions when a peer disappears (no response within `peer_timeout`). Lambda arg: `name` (string). |
| `on_scan_complete` | automation | - | Actions after each scan cycle finishes. Lambda arg: `count` (int) of currently known peers. |

## Common Service Types

| Service Type | Description |
|--------------|-------------|
| `_http._tcp` | Web servers |
| `_https._tcp` | Secure web servers |
| `_printer._tcp` | Network printers |
| `_ipp._tcp` | Internet Printing Protocol |
| `_airplay._tcp` | Apple AirPlay |
| `_googlecast._tcp` | Chromecast devices |
| `_spotify-connect._tcp` | Spotify Connect |
| `_esphomelib._tcp` | ESPHome devices |
| `_homeassistant._tcp` | Home Assistant |
| `_mqtt._tcp` | MQTT brokers |
| `_ssh._tcp` | SSH servers |

## Sensors

```yaml
sensor:
  - platform: mdns_discovery
    mdns_discovery_id: discovery
    peer_count:
      name: "Discovered Devices"

text_sensor:
  - platform: mdns_discovery
    mdns_discovery_id: discovery
    peers_list:
      name: "Device List"    # Comma-separated list of names
```

## Lambda Access

```cpp
// Get all discovered peers
auto& peers = id(discovery).get_peers();

for (const auto& peer : peers) {
  ESP_LOGI("mdns", "Peer: %s at %s:%d",
           peer.name.c_str(),
           peer.ip.c_str(),
           peer.port);
}

// Force immediate scan
id(discovery).scan_now();

// Get peer count
int count = peers.size();
```

## Advertising Your Own Service

To make your device discoverable by others:

```yaml
mdns:
  services:
    - service: "_myservice"
      protocol: "_tcp"
      port: 8080
      txt:
        version: "1.0"
        device: "My Device"
```

## Complete Example: Network Monitor Dashboard

```yaml
mdns_discovery:
  id: network_scanner
  service_type: "_esphomelib._tcp"
  scan_interval: 60s
  on_peer_found:
    - homeassistant.event:
        event: esphome.device_online
        data:
          device: !lambda 'return name;'
          ip: !lambda 'return ip;'
    - lambda: |-
        ESP_LOGI("network", "Device online: %s (%s)",
                 name.c_str(), ip.c_str());
  on_peer_lost:
    - homeassistant.event:
        event: esphome.device_offline
        data:
          device: !lambda 'return name;'

sensor:
  - platform: mdns_discovery
    mdns_discovery_id: network_scanner
    peer_count:
      name: "ESPHome Devices Online"

text_sensor:
  - platform: mdns_discovery
    mdns_discovery_id: network_scanner
    peers_list:
      name: "Online ESPHome Devices"

button:
  - platform: template
    name: "Scan Network"
    on_press:
      - lambda: 'id(network_scanner).scan_now();'
```

## Troubleshooting

### No Peers Found
1. Verify service type is correct (include underscore prefix: `_http._tcp`)
2. Check target devices are advertising the service
3. Ensure devices are on same network/VLAN
4. Some routers block mDNS multicast - check settings

### Peers Disappear/Reappear
1. Normal behavior as mDNS cache expires
2. Increase scan_interval for less frequent updates
3. Check WiFi signal strength on both devices

### High CPU Usage
1. Increase scan_interval (60s or more recommended)
2. mDNS queries are lightweight but frequent scans add up

## Requirements

- ESPHome `network` and `mdns` components (declared as dependencies)
- Network connectivity
- mDNS support on the network (standard on most routers; some VLAN setups block multicast)

## License

MIT License
