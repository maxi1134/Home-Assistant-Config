"""

Credits to:

https://github.com/glucee/Multilateration/blob/master/Python/example.py

Uses:

https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.minimize.html
https://docs.scipy.org/doc/scipy/reference/optimize.minimize-neldermead.html#optimize-minimize-neldermead

"""

import appdaemon.plugins.hass.hassapi as hass
from scipy.optimize import minimize
import numpy as np
import json

class ESPresenseIps(hass.Hass):
    def initialize(self):
        self.devices = {}
        for device in self.args["devices"]:
          self.devices.setdefault(device["id"],{})["name"]=device["name"]
        self.mqtt = self.get_plugin_api("MQTT")
        for room, pos in self.args["rooms"].items():
            t = f"{self.args.get('rooms_topic', 'espresense/rooms')}/{room}"
            self.log(f"Subscribing to topic {t}")
            self.mqtt.mqtt_unsubscribe(t)
            self.mqtt.mqtt_subscribe(t)
            self.mqtt.listen_event(self.mqtt_message, "MQTT_MESSAGE", topic=t)

    def mqtt_message(self, event_name, data, *args, **kwargs):
        """Process a message sent on the MQTT Topic."""
        topic = data.get("topic")
        payload = data.get("payload")

        topic_path = topic.split("/")
        room = topic_path[-1].lower()

        payload_json = {}
        try:
            payload_json = json.loads(payload)
        except ValueError:
            pass

        id = payload_json.get("id")
        name = payload_json.get("name")
        distance = payload_json.get("distance")
        self.log(f"{id} {room} {distance}", level="DEBUG")

        device = self.devices.setdefault(id,{})
        device["measures"] = device.get("measures", 0) + 1

        if (room in self.args["rooms"]):
            dr = device.setdefault("rooms",{}).setdefault(room,{"pos":self.args["rooms"][room]})
            dr["distance"] = distance

            distance_to_stations=[]
            stations_coordinates=[]
            for r in device["rooms"]:
                if "distance" in device["rooms"][r]:
                    distance_to_stations.append(device["rooms"][r]["distance"])
                    stations_coordinates.append(device["rooms"][r]["pos"])

            name = device.get("name", name)
            if (name) and len(distance_to_stations)>2:
                device["x0"] = position_solve(distance_to_stations, np.array(stations_coordinates), device.get("x0", None))
                pos = device["x0"].tolist()
                #self.call_service("device_tracker/see", dev_id = id + "_see", gps = [self.config["latitude"]+(pos[1]/111111), self.config["longitude"]+(pos[0]/111111)], location_name="home")
                #self.log(f"{room} {id}: {pos}")
                roomname = room_solve(self,round(pos[0],2,),round(pos[1],2))
                self.mqtt.mqtt_publish(f"{self.args.get('ips_topic', 'espresense/ips')}/{id}", json.dumps({"name":name, "x":round(pos[0],2),"y":round(pos[1],2),"z":round(pos[2],2), "fixes":len(distance_to_stations),"measures":device["measures"],"currentroom":roomname}))
                self.mqtt.mqtt_publish(f"{self.args.get('location_topic', 'espresense/location')}/{id}", json.dumps({"name":name, "longitude":(self.config["longitude"]+(pos[0]/111111)),"latitude":(self.config["latitude"]+(pos[1]/111111)),"elevation":(self.config.get("elevation","0")+pos[2]), "fixes":len(distance_to_stations),"measures":device["measures"]}))

def position_solve(distances_to_station, stations_coordinates, last):
    def error(x, c, r):
        return sum([(np.linalg.norm(x - c[i]) - r[i]) ** 2 for i in range(len(c))])

    l = len(stations_coordinates)
    S = sum(distances_to_station)
    # compute weight vector for initial guess
    W = [((l - 1) * S) / (S - w) for w in distances_to_station]
    # get initial guess of point location
    x0 = last if last is not None else sum([W[i] * stations_coordinates[i] for i in range(l)])
    # optimize distance from signal origin to border of spheres
    return minimize(
        error,
        x0,
        args=(stations_coordinates, distances_to_station),
        method="Nelder-Mead",
        options={'xatol': 0.001, 'fatol': 0.001, 'adaptive': True}
    ).x

def room_solve(self, xpos, ypos):
    if "roomplans" in self.args:
        for rooms in self.args["roomplans"]:
            if rooms["x1"] < float(xpos) < rooms["x2"] and rooms["y1"] < float(ypos) < rooms["y2"]:
                return rooms["name"]
    return "none"
