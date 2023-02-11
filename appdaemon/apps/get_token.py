import appdaemon.plugins.hass.hassapi as hass
import requests
import datetime

class GetToken(hass.Hass):
    def initialize(self):
        start = datetime.datetime.now()
        self.run_hourly(self.fetch_token, start = (datetime.datetime.now() + datetime.timedelta(seconds=15)))

    def fetch_token(self, kwargs):
        user = self.args["user"]
        password = self.args["password"]
        login_url = self.args["url"]
        params = [{"cmd":"Login","action":0,"param":{"User":{"userName": user,"password": password }}}]
        request = requests.post(url = login_url, json=params)
        self.log(request.json())
        token = request.json()[0]["value"]["Token"]["name"]
        self.log("fetch token result: {}".format(request.status_code))
        self.set_state("sensor.reolink_token",state=token)