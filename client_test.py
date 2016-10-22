#UDP serving float64 (double) 
from SocketServer import DatagramRequestHandler as DRH, UDPServer as UDP
import array
import numpy as np

HOST = "192.168.0.199"
PORT = 18001

class MyRequestHandler(DRH):
    def handle(self):
        
        data, socket = self.request
        #d_arr = array.array('d', data)
        if len(data)>0:
            print "Recved from ", self.client_address
            print type(data), len(data), data 

s = UDP((HOST, PORT), MyRequestHandler)
s.serve_forever()