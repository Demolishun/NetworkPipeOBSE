import socket
import select
from time import sleep

UDP_IP="127.0.0.1"
#UDP_IP="0.0.0.0"
UDP_PORT=12345


"""
[{"test1":"data","test2":"more data","form":"reference:00000014","numdata":"12"},{"other object":"data"}]
"""

"""{"name":"Joe","id":"number:12","object":"reference:0000000F"}"""

MESSAGE= \
"""{"one":12,"two":"ref:0000000F","three":1.12,"four":true,"five":false,"six":0000000f}"""

# defining obse variables
""" "<value>" """ # string
""" "str:<value>" """ # string
""" "int:<value>" """ # integer
""" "flt:<value>" """ # float
""" "ref:<value>" """ # ref
""" {} """ # string map
""" [] """ # array
""" "<name>":"<value>" """ # map entry

# implemented in plugin
#define SIG_STRING_VAR "string:"
#define SIG_NUMBER_VAR "number:"
#define SIG_REFERENCE_VAR "reference:"

print "UDP target IP:", UDP_IP
print "UDP target port:", UDP_PORT
print "message:", MESSAGE

sock = socket.socket( socket.AF_INET, # Internet
                      socket.SOCK_DGRAM ) # UDP
#sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

sock.setblocking(0)
#sock.settimeout(1)

count = 30
while(1):
    print "sending message"
    port = sock.sendto( MESSAGE, (UDP_IP, UDP_PORT) )

    sleep(1)

    try:
        ready = select.select([sock], [], [], 1)
        if ready[0]:
            response = sock.recv(65535)
            print response
    except Exception,e:
        print e