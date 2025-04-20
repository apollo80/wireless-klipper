#!/usr/bin/python3

import sys
sys.path.append('/home/apollo/.espressif/python_env/idf5.3_py3.12_env/lib/python3.12/site-packages')

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


minPackSize  = 6
maxPackSize  = 70
maxPackCount = 7

tcp2serial_data = bytes()
tcp2serial_data_size = 0


class MCUProtocol(asyncio.Protocol):
    def __init__(self) -> None:
        super().__init__()
        self.recv_data = bytes()
        self.mcu_recvBytes = bytes()

    def connection_made(self, transport):
        self.transport = transport
        test_logger.info("mcu: port opened {} : {}".format(self.transport.serial.port, self.transport.serial.baudrate))
        transport.serial.rts = True # You can manipulate Serial object via transport
        test_logger.info('mcu: ---> Hello, World <---')
        ##transport.write(b'Hello, World!\n')  # Write serial data via transport

    def data_received(self, data):
        global tcp2serial_data
        global tcp2serial_data_size

        if len(data) == 2 and len(self.recv_data) < tcp2serial_data_size :
            tmp = self.recv_data + data
            if tmp == tcp2serial_data[:len(tmp)]:
                self.recv_data += data
            else:
                self.recv_data += bytes([ data[0] ])
                test_logger.info('mcu: -- DEBUG: fix serial two bytes received')
        else:
            self.recv_data += data

        #test_logger.info("mcu: --> recv data {} (+{}) <--".format(len(self.recv_data), len(data)))
        #test_logger.info("mcu: --> {} <--".format(self.recv_data))

        while True:
            log_start = self.recv_data.find(b'I (')
            if log_start < 0:
                log_start = self.recv_data.find(b'E (')

            if log_start < 0:
                log_start = self.recv_data.find(b'W (')

            if log_start < 0:
                break

            log_end = self.recv_data.find(b'\n', log_start)
            #test_logger.info("mcu: -- DEBUG: log_start {}, log_end {}, len(self.recv_data) {}".format(log_start, log_end, len(self.recv_data)))
            if log_end < 0:
                #log_msg = self.recv_data[log_start:]
                #test_logger.info("mcu: Debug - is_not_ending - {}".format(log_msg.decode()))
                return

            log_msg = self.recv_data[log_start:log_end]
            new_data = self.recv_data[:log_start] + self.recv_data[log_end+1:]
            self.recv_data = new_data
            test_logger.info("mcu: -- {}".format(log_msg.decode()))

        if len(self.recv_data) < 4:
            return

        test_logger.info('mcu: -- recv  bytes[{}] - {}'.format(len(self.recv_data), self.recv_data.hex(' ')))

        if len(self.recv_data) < tcp2serial_data_size:
            test_logger.info('mcu: -- wait {} bytes, received {} bytes -> need more'.format(tcp2serial_data_size, len(self.recv_data)))
            #test_logger.info('mcu: ---')
            return

        if self.recv_data[:tcp2serial_data_size] != tcp2serial_data:
            test_logger.info('mcu: data corrupted'.format())
            test_logger.info('mcu:     generate {}'.format(tcp2serial_data.hex(' ')))
            test_logger.info('mcu:     received {}'.format(self.recv_data[:tcp2serial_data_size].hex(' ')))
            test_logger.info('mcu:     received {}'.format(self.recv_data.hex()))
            test_logger.info('mcu:     received {}'.format(self.recv_data.decode()))
            self.transport.loop.stop()
            return

        #delay = random.randrange(2, 100)
        #time.sleep(delay / 1000)

        write_data = self.recv_data[:tcp2serial_data_size]
        self.transport.write(write_data)
        test_logger.info('mcu: -- write bytes[{}] - {}'.format(len(write_data), write_data.hex(' ')))
        self.recv_data = self.recv_data[tcp2serial_data_size:]
        if len(self.recv_data):
            test_logger.info('mcu: -- DEBUG: exist data [{}] {}'.format(len(self.recv_data), self.recv_data.decode()))

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
        self.recv_data = bytes()
        self.recv_data_size = 0
        self.mcu_recvBytes = bytes()

    def connection_made(self, transport):
        global tcp2serial_data
        global tcp2serial_data_size

        self.transport = transport
        test_logger.info("serial: port opened {} : {}".format(self.transport.serial.port, self.transport.serial.baudrate))
        ##transport.serial.rts = False  # You can manipulate Serial object via transport
        ##transport.write(b'Hello, World!\n')  # Write serial data via transport

        test_logger.info("serial: generate new sequence".format(tcp2serial_data_size))
        for idx in range (1, random.randrange(2, maxPackCount)):
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
        pkg_count = random.randrange(2, maxPackCount)
        for idx in range (1, pkg_count):
            inSeqSize = random.randrange(minPackSize, maxPackSize)
            inSeqBin = random.randbytes(inSeqSize)

            tmp = bytearray(base64.b64encode(inSeqBin))
            tmp[0] = len(tmp)
            tmp[-1] = 0x7e
            tcp2serial_data += bytes(tmp)
            test_logger.info("serial:     {} pkg[{}] - {}".format(idx, len(tmp), tmp.hex(' ')))

        if pkg_count % 2:
            tmp =  bytes()
            tmp += b'\x7e'
            tmp += tcp2serial_data
            tcp2serial_data = tmp

        tcp2serial_data_size = len(tcp2serial_data)

        self.recv_data = bytes()
        self.recv_data_size = 0

        test_logger.info("serial: send msg {} bytes".format(tcp2serial_data_size))
        test_logger.info("serial: {}".format(tcp2serial_data.hex(' ')))
        self.transport.write(tcp2serial_data)
        test_logger.info("serial: ---")



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


# получение пользовательского логгера и установка уровня логирования
test_logger = logging.getLogger("test_serial.debug")
test_logger.setLevel(logging.INFO)

# настройка обработчика и форматировщика в соответствии с нашими нуждами
test_handler = GzipRotatingFileHandler(f"test_serial.debug.log", mode='w', maxBytes=10*1024*1024, backupCount=5)
test_formatter = logging.Formatter("%(asctime)s %(message)s")

# добавление форматировщика к обработчику 
test_handler.setFormatter(test_formatter)

# добавление обработчика к логгеру
test_logger.addHandler(test_handler)

test_logger.info(f"Testing the custom logger for module {__name__}...")

loop = asyncio.new_event_loop()
#coro_mcu    = serial_asyncio.create_serial_connection(loop, MCUProtocol,    '/dev/ttyUSB0', baudrate=74880)
#coro_serial = serial_asyncio.create_serial_connection(loop, SerialProtocol, '/home/apollo/virtual_pty', baudrate=250000)
coro_mcu    = serial_asyncio.create_serial_connection(loop, MCUProtocol,    '/dev/ttyUSB0', baudrate=250000)
coro_serial = serial_asyncio.create_serial_connection(loop, SerialProtocol, '/tmp/virtual_pty', baudrate=250000)

transport_ser, protocol_ser = loop.run_until_complete(coro_mcu)
transport_soc, protocol_soc = loop.run_until_complete(coro_serial)

loop.run_forever()
loop.close()
