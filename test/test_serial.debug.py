#!/usr/bin/python3

import sys
sys.path.append('/home/apollo/.platformio/penv/lib/python3.10/site-packages')
sys.path.append('/home/apollo/.espressif/python_env/rtos3.4_py3.10_env/lib/python3.10/site-packages')

import asyncio
import base64
import gzip
import logging
import os
import random
import shutil
import socket
import serial_asyncio
import time

from logging.handlers import RotatingFileHandler


minPackSize = 6
maxPackSize = 30

tcp2serial_data = b''
tcp2serial_data_size = 0

iteration_max = 10

prefix_netStart     = 0x0A1B2C0D
prefix_uartStart    = 0x0A2B3C0D

prefix_netData      = 0x0A4B5C0D
prefix_uartData     = 0x0A6B7C0D

prefix_netConfirm   = 0x30405060
prefix_uartConfirm  = 0x40506070

prefix_recvMsgIndex_is_not_set = True


class MCUProtocol(asyncio.Protocol):
    def __init__(self) -> None:
        super().__init__()
        self.recv_data = b''
        self.mcu_recvBytes = b''

    def connection_made(self, transport):
        self.transport = transport
        test_logger.info("mcu: port opened {} : {}".format(self.transport.serial.port, self.transport.serial.baudrate))
        transport.serial.rts = True # You can manipulate Serial object via transport
        test_logger.info('mcu: ---> Hello, World <---')
        ##transport.write(b'Hello, World!\n')  # Write serial data via transport

    def data_received(self, data):
        global tcp2serial_data
        global tcp2serial_data_size

        self.recv_data += data
        #test_logger.info("mcu: --> recv data {} (+{}) <--".format(len(self.recv_data), len(data)))
        #test_logger.info("mcu: --> {} <--".format(self.recv_data))

        log_data = bytes()
        is_log_message = True
        while(is_log_message):
            if len(self.recv_data) == 0:
                return

            lines = self.recv_data.splitlines(keepends=True)
            if not lines:
                test_logger.info("mcu: --> not lines: {} <--".format(self.recv_data.hex(' ')))
                self.recv_data = b''
                return

            if lines[0][:3] == b'I (' or lines[0][:3] == b'E (' or lines[0][:3] == b'W (':
                #test_logger.info("mcu: -- last symbol {}".format(hex(lines[0][-1])))
                if lines[0][-1] == 0x0a:
                    test_logger.info("mcu: -- {}".format(lines[0][:-1].decode() ))
                    self.recv_data = self.recv_data[len(lines[0]):]
                else:
                    #test_logger.info("mcu: --> {} <--".format(lines[0].decode()))
                    #test_logger.info("mcu: -- need more")
                    return
            else:
                if len(self.recv_data) < 5:
                    return

                if len(lines[0]) > tcp2serial_data_size:
                    test_logger.info("mcu: recv data too big - checking")
                    log_data = self.recv_data[tcp2serial_data_size:]
                    self.recv_data = self.recv_data[:tcp2serial_data_size];
                    test_logger.info("mcu: -- data[{}] - {}".format(len(self.recv_data), self.recv_data.hex(' ')))
                    test_logger.info("mcu: -- logs after data:")
                    test_logger.info("mcu: -- {}".format(log_data.decode()))

                    #if (lines[0][tcp2serial_data_size:3] == b'I (') or (lines[0][tcp2serial_data_size:3] == b'E (') or (lines[0][tcp2serial_data_size:3] == b'W ('):

                is_log_message = False
                break

        test_logger.info('mcu: recv bytes[{}] (+{}) {}: '.format(len(self.recv_data), len(data), self.recv_data.hex(' ')))

        if len(self.recv_data) < tcp2serial_data_size:
            test_logger.info('mcu: wait {} bytes, received {} bytes -> need more'.format(tcp2serial_data_size, len(self.recv_data)))
            #test_logger.info('mcu: ---')
            return

        delay = random.randrange(2, 100)
        time.sleep(delay / 1000)

        self.transport.write(self.recv_data)
        test_logger.info('mcu: write back {} bytes'.format(len(self.recv_data)))
        self.recv_data = b''

        if len(log_data):
            test_logger.info('mcu: post log ...')
            while(len(log_data)):
                lines = log_data.splitlines(keepends=True)

                if lines[0][:3] == b'I (' or lines[0][:3] == b'E (' or lines[0][:3] == b'W (':
                    if lines[0][-1] == 0x0a:
                        test_logger.info("mcu: -- {}".format(lines[0][:-1].decode() ))
                        log_data = log_data[len(lines[0]):]
                    else:
                        self.recv_data = lines[0]
                        test_logger.info("mcu: -- err1 {}".format(self.recv_data.hex(' ')))
                        break;
                else:
                    self.recv_data = lines[0]
                    test_logger.info("mcu: -- err2 {}".format(self.recv_data.hex(' ')))
                    break;

        test_logger.info('mcu: ---')

    def connection_lost(self, exc):
        test_logger.info('mcu: port closed')
        self.transport.loop.stop()

    def pause_writing(self):
        test_logger.info('mcu: pause writing')
        test_logger.info(self.transport.get_write_buffer_size())

    def resume_writing(self):
        test_logger.info(self.transport.get_write_buffer_size())
        test_logger.info('mcu: resume writing')


class SerialProtocol(asyncio.Protocol):
    def __init__(self) -> None:
        super().__init__()
        self.recv_data = b''
        self.recv_data_size = 0
        self.mcu_recvBytes = b''

    def connection_made(self, transport):
        global tcp2serial_data
        global tcp2serial_data_size

        self.transport = transport
        test_logger.info("serial: port opened {} : {}".format(self.transport.serial.port, self.transport.serial.baudrate))
        ##transport.serial.rts = False  # You can manipulate Serial object via transport
        ##transport.write(b'Hello, World!\n')  # Write serial data via transport

        test_logger.info("serial: generate new sequence".format(tcp2serial_data_size))
        for idx in range (1, random.randrange(2, 4)):
            inSeqSize = random.randrange(minPackSize, maxPackSize)
            inSeqBin = random.randbytes(inSeqSize)

            tmp = bytearray(base64.b64encode(inSeqBin))
            tmp[0] = len(tmp)
            tmp[-1] = 0x7e
            tcp2serial_data += bytes(tmp)
            test_logger.info("serial:     {} pkg[{}] - {}".format(idx, len(tmp), tmp.hex(' ')))
        tcp2serial_data_size = len(tcp2serial_data)

        test_logger.info("serial: send start msg {} bytes".format(tcp2serial_data_size))
        test_logger.info("serial: {}".format(tcp2serial_data.hex(' ')))
        self.transport.write(tcp2serial_data)


    def data_received(self, data):
        global tcp2serial_data
        global tcp2serial_data_size

        self.recv_data += data
        self.recv_data_size = len(self.recv_data)

        if self.recv_data_size < tcp2serial_data_size:
            test_logger.info("serial: recv {} bytes from {} -> need more".format(self.recv_data_size, tcp2serial_data_size))
            return

        if self.recv_data != tcp2serial_data:
            test_logger.info("serial: pkg recv pkg failed".format(self.recv_data_size, tcp2serial_data_size))
            test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
            test_logger.info("    serial recv ({}): {}".format(self.recv_data_size, self.recv_data))
            test_logger.info("")
            self.transport.loop.stop()
        else:
            test_logger.info("serial: recv {} bytes - ok".format(self.recv_data_size))

        test_logger.info("serial: generate new sequence".format(tcp2serial_data_size))
        tcp2serial_data = bytes()
        for idx in range (1, random.randrange(2, 5)):
            inSeqSize = random.randrange(minPackSize, maxPackSize)
            inSeqBin = random.randbytes(inSeqSize)

            tmp = bytearray(base64.b64encode(inSeqBin))
            tmp[0] = len(tmp)
            tmp[-1] = 0x7e
            tcp2serial_data += bytes(tmp)
            test_logger.info("serial:     {} pkg[{}] - {}".format(idx, len(tmp), tmp.hex(' ')))
        tcp2serial_data_size = len(tcp2serial_data)

        self.recv_data = b''
        self.recv_data_size = 0

        test_logger.info("serial: send msg {} bytes".format(tcp2serial_data_size))
        test_logger.info("serial: {}".format(tcp2serial_data.hex(' ')))
        self.transport.write(tcp2serial_data)



    def connection_lost(self, exc):
        test_logger.info('serial: port closed')
        self.transport.loop.stop()

    def pause_writing(self):
        test_logger.info('serial: pause writing')
        test_logger.info(self.transport.get_write_buffer_size())

    def resume_writing(self):
        test_logger.info(self.transport.get_write_buffer_size())
        test_logger.info('serial: resume writing')


class Timer:
    def __init__(self, transport):
        self._transport = transport

    def __del__(self):
        self.cancel()

    async def _job(self, timeout, msg, net_index):
        await asyncio.sleep(timeout)
        self._transport.sendto(msg)
        test_logger.info('socket: timeout - data sent(net_index:{}): {}'.format(net_index, msg[16:]))
        self.start(timeout, msg, net_index)

    def start(self, timeout, msg, net_index):
        self._task = asyncio.ensure_future(self._job(timeout, msg, net_index))

    def cancel(self):
        self._task.cancel()


class GzipRotatingFileHandler(RotatingFileHandler):
     def doRollover(self):
        super(GzipRotatingFileHandler, self).doRollover()

        # Compress the old log.
        for idx in range(10, 0, -1):
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


# получение пользовательского логгера и установка уровня логирования
test_logger = logging.getLogger(__name__)
test_logger.setLevel(logging.INFO)

# настройка обработчика и форматировщика в соответствии с нашими нуждами
test_handler = GzipRotatingFileHandler(f"{__name__}.log", mode='w', maxBytes=10*1024*1024, backupCount=5)
test_formatter = logging.Formatter("%(asctime)s %(message)s")

# добавление форматировщика к обработчику 
test_handler.setFormatter(test_formatter)

# добавление обработчика к логгеру
test_logger.addHandler(test_handler)

test_logger.info(f"Testing the custom logger for module {__name__}...")

loop = asyncio.get_event_loop()
#coro_mcu    = serial_asyncio.create_serial_connection(loop, MCUProtocol,    '/dev/ttyUSB0', baudrate=74880)
#coro_serial = serial_asyncio.create_serial_connection(loop, SerialProtocol, '/home/apollo/virtual_pty', baudrate=250000)
coro_mcu    = serial_asyncio.create_serial_connection(loop, MCUProtocol,    '/dev/ttyUSB0', baudrate=250000)
coro_serial = serial_asyncio.create_serial_connection(loop, SerialProtocol, '/home/apollo/Development/github/apollo80/wireless-klipper/pty2udp-proxy/cmake-build-debug/mcu.virtual.serial', baudrate=460800)

transport_ser, protocol_ser = loop.run_until_complete(coro_mcu)
transport_soc, protocol_soc = loop.run_until_complete(coro_serial)

loop.run_forever()
loop.close()
