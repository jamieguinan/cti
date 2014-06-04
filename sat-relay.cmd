# Provide a relay to a network stream source.

new SocketRelay sr
new SocketServer ss

connect sr RawData_buffer ss

# Source.
config sr input 192.168.2.174:11301
config sr retry 1

# Listen port.
config ss v4port 7132
config ss enable 1
config ss notify sr

ignoreeof 1
