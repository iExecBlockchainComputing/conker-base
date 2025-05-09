import requests
import os

key = os.getenv('local-key')
print(key)
url = "http://127.0.0.1:9090/secret"
payload={"key":"wrapkey","value":key}
files=[
]
headers = {
}
response = requests.request("POST", url, headers=headers, data=payload, files=files)
print(response.text)
