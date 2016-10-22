from socket import *
import time
import numpy as np
import array

host = '192.168.0.199'        #broadcast
all = '0.0.0.0'
port = 18001            #server port
addr = (host,port)
client = socket(AF_INET,SOCK_DGRAM)
client.connect(addr)
#client.bind(addr)

while True:
    #data = np.array([2342.3400000000001, 34.1, 2, 2, 3.3, 0.00001], dtype="float64")
    #print "sending ", array.array('d', data.tobytes())
    #client.send(data.tobytes())
    data = 'on'
    print "sending", data
    client.send(data)
    time.sleep(1)

client.close()