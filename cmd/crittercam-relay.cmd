# Provide a relay to a network stream source.

new SocketRelay sr
new SocketServer ss

connect sr RawData_buffer ss

# Source.
config sr input 192.168.2.77:6666
config sr retry 1

# Listen port.
config ss v4port 7130
config ss enable 1
config ss notify sr

ignoreeof 1
