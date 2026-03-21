import appdaemon.plugins.hass.hassapi as hass
import datetime
import requests


class GetToken(hass.Hass):
    def initialize(self):
        start = datetime.datetime.now() + datetime.timedelta(seconds=15)
        self.run_hourly(self.fetch_token, start=start)

    def fetch_token(self, kwargs):
        user = self.args["user"]
        password = self.args["password"]
        login_url = self.args["url"]
        params = [{"cmd": "Login", "action": 0, "param": {"User": {"userName": user, "password": password}}}]

        try:
            response = requests.post(url=login_url, json=params, timeout=15)
            response.raise_for_status()
            payload = response.json()
            token = payload[0]["value"]["Token"]["name"]
            # TODO: This preserves the existing sensor-based token flow for PTZ commands,
            # but it leaks a live token into Home Assistant state and should be replaced.
            self.set_state("sensor.reolink_token", state=token)
            self.log("Fetched a fresh Reolink token and published it to Home Assistant state.", level="WARNING")
        except (requests.RequestException, KeyError, IndexError, TypeError, ValueError) as err:
            self.log(f"Failed to refresh Reolink token: {err}", level="WARNING")
