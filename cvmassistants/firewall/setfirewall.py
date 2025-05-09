import subprocess
import os 
subprocess.run(f'modprobe ip_tables', shell=True)
subprocess.run(f'ufw enable',input="y".encode(), shell=True)
ports = os.getenv('allowPorts')
if ports:
    print("allow ports", ports)
    port_list = ports.split(',')
    for port in port_list:
        if ':' in port:
            start, end = port.split(':')
            subprocess.run(f'ufw allow {start}:{end}/tcp', shell=True)
        else:
            subprocess.run(f'ufw allow {port}/tcp', shell=True)
else :
    print("no need allow prots skip")
