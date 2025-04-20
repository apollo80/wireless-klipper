#!/usr/bin/env python3

import socket
import datetime
import gzip
import os
import sys

import logging
from logging.handlers import RotatingFileHandler

sys.path.append('/home/apollo/.espressif/python_env/idf5.3_py3.12_env/lib/python3.12/site-packages')

class GzipRotatingFileHandler(RotatingFileHandler):
     def doRollover(self):
        super(GzipRotatingFileHandler, self).doRollover()

        # Compress the old log.
        for idx in range(9, 0, -1):
            oldfilename = self.baseFilename + '.'+ str(idx) + '.gz'
            newfilename = self.baseFilename + '.'+ str(idx+1) + '.gz'
            if os.path.isfile(oldfilename):
                print("rename {} -> {}".format(oldfilename, newfilename))
                os.rename(oldfilename, newfilename)

        old_log = self.baseFilename + ".1"
        with open(old_log, "rb") as f_in, gzip.open(old_log + '.gz', 'wb') as f_out:
            while True:
                content = f_in.read(8192)
                if not content:
                    break
                f_out.write(content)

        os.remove(old_log)



UDP_IP = "0.0.0.0"
UDP_PORT = 1345

sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
sock.bind( (UDP_IP, UDP_PORT) )


# получение пользовательского логгера и установка уровня логирования
udp_logger = logging.getLogger("udp_logger")
udp_logger.setLevel(logging.INFO)

# настройка обработчика и форматировщика в соответствии с нашими нуждами
test_handler = GzipRotatingFileHandler(f"udp_logger.log", mode='w', maxBytes=10*1024*1024, backupCount=5)
test_formatter = logging.Formatter("%(asctime)s %(message)s")

# добавление форматировщика к обработчику
test_handler.setFormatter(test_formatter)

# добавление обработчика к логгеру
udp_logger.addHandler(test_handler)

udp_logger.info(f"Testing the custom logger for module {__name__}...")


udp_logger.info("+============================+")
udp_logger.info("|  ESP32 UDP Logging Server  |")
udp_logger.info("+============================+")
udp_logger.info("")

while True:
    data, addr = sock.recvfrom(512)
    if len(data) > 2:
        data_log = data[:-2]
        data = data_log
    udp_logger.info(data.decode())
    #print(datetime.datetime.now(), " -- ", data.hex(' '))