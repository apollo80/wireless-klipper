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

from logging.handlers import RotatingFileHandler


minPackSize = 10
maxPackSize = 500

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


class SerialProtocol(asyncio.Protocol):
    def __init__(self) -> None:
        super().__init__()
        self.recv_data = b''
        self.mcu_recvBytes = b''


    def connection_made(self, transport):
        self.transport = transport
        test_logger.info("serial: port opened {}".format(self.transport))
        transport.serial.rts = False  # You can manipulate Serial object via transport
        ##transport.write(b'Hello, World!\n')  # Write serial data via transport

    def data_received(self, data):
        global tcp2serial_data
        global tcp2serial_data_size

        self.recv_data += data
        #test_logger.info('serial: recv data {} bytes, size of recv_data {} bytes'.format(len(data), len(self.recv_data)))

        if b'\n' in self.recv_data:
            lines = self.recv_data.splitlines()
            lines_count = len(lines)

            # self.recv_data[-1] == b'\n':
            if self.recv_data[-1] == 10:
                self.recv_data = b''
            else:
                self.recv_data = lines[-1]
                lines_count -= 1

            for idx in range(lines_count):
                line = lines[idx]


                test_logger.info('serial: {}'.format(line.decode()))

                #if line[:6] == "data: ".encode():
                if (line[:3] != "I (".encode()) and (line[:3] != "E (".encode()) and (line[:3] != "W (".encode()):
                    self.mcu_recvBytes += line
                    test_logger.info("serial: pkg recv {} from {}".format(len(self.mcu_recvBytes), tcp2serial_data_size))

                    if self.mcu_recvBytes != tcp2serial_data[:len(self.mcu_recvBytes)]:
                        test_logger.info("serial: pkg recv pkg failed".format(len(self.mcu_recvBytes), tcp2serial_data_size))
                        test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
                        test_logger.info("    serial recv ({}): {}".format(len(self.mcu_recvBytes), self.mcu_recvBytes))
                        test_logger.info("")
                        self.transport.loop.stop()

                    if len(self.mcu_recvBytes) < tcp2serial_data_size:
                        return

                    if self.mcu_recvBytes != tcp2serial_data:
                        test_logger.info("")
                        test_logger.info("serial failed recv data:")
                        test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
                        test_logger.info("    serial recv ({}): {}".format(len(self.mcu_recvBytes), self.mcu_recvBytes))
                        test_logger.info("")
                        self.transport.loop.stop()
                        return
                    
                    test_logger.info("serial: send to back {} bytes".format(len(self.mcu_recvBytes)))
                    self.transport.write(self.mcu_recvBytes)
                    #test_logger.info('serial: recv package: {}'.format(self.mcu_recvBytes))
                    self.mcu_recvBytes = b''



        #test_logger.info('serial: self.recv_data {}'.format(self.recv_data))
        #test_logger.info('serial: ---')


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

class ClientProtocol:
    def __init__(self, loop) -> None:
        super().__init__()
        self.iteration = 0
        self.tcp_recvBytes = b''
        self.loop = loop
        self.netIndex = 0
        self.uartIndex = -1
        self.timeout_sec = 0.1

        #self.recv_msg_prefix = 0
        #self.recv_msg_index = 0
        #self.recv_msg_size = 0
        #self.prefix_uartMsgIndex_is_not_set = True

    def connection_made(self, transport):
        global tcp2serial_data
        global tcp2serial_data_size
        global prefix_netStart
        
        self.transport = transport
        self.timer = Timer(transport)

        test_logger.info('socket: connection_mode: {}'.format(self.transport))

        inSeqSize = random.randrange(minPackSize, maxPackSize)
        inSeqBin = random.randbytes(inSeqSize)

        tcp2serial_data = base64.b64encode(inSeqBin)
        tcp2serial_data_size = len(tcp2serial_data)

        msg = b''
        msg += socket.htonl(prefix_netStart).to_bytes(4, "little")
        msg += socket.htonl(self.netIndex).to_bytes(4, "little")
        msg += socket.htonl(0).to_bytes(4, "little")
        msg += socket.htonl(tcp2serial_data_size).to_bytes(4, "little")
        msg += tcp2serial_data

        self.transport.sendto(msg)
        self.timer.start(self.timeout_sec, msg, self.netIndex)
        test_logger.info('socket: send start msg data({}): {}'.format(tcp2serial_data_size, tcp2serial_data))

    def datagram_received(self, data, addr):
        global tcp2serial_data
        global tcp2serial_data_size

        global prefix_netConfirm
        global prefix_uartConfirm
        global prefix_uartData

        self.udp_recvBytes = data
        #test_logger.info('socket: recv msg {} bytes'.format(len(self.udp_recvBytes)))

        # разбираем заголовок пакета
        self.recv_msg_prefix    = socket.ntohl(int.from_bytes(self.udp_recvBytes[ 0:4],  "little"))
        self.recv_netmsg_index  = socket.ntohl(int.from_bytes(self.udp_recvBytes[ 4:8],  "little"))
        self.recv_uartmsg_index = socket.ntohl(int.from_bytes(self.udp_recvBytes[ 8:12], "little"))
        self.recv_msg_size      = socket.ntohl(int.from_bytes(self.udp_recvBytes[12:16], "little"))

        if (self.recv_msg_size + 16) != len(self.udp_recvBytes):
            test_logger.info('socket: incorrect msg header - data size: {}, msg size: {}'.format(self.recv_msg_size, len(self.udp_recvBytes)))

        #test_logger.info('socket: header: prefix    - {:x}'.format(self.recv_msg_prefix))
        #test_logger.info('socket: header: net index  - {}'.format(self.recv_netmsg_index))
        #test_logger.info('socket: header: uart index - {}'.format(self.recv_uartmsg_index))
        #test_logger.info('socket: header: data size  - {}'.format(self.recv_msg_size))

        # проверяем префикс полученного сообщения
        if self.recv_msg_prefix == prefix_netConfirm:
            test_logger.info('socket: recv net_confirm msg: net index - {}; uart index - {}'.format(
                self.recv_netmsg_index, self.recv_uartmsg_index))

            if self.netIndex == self.recv_netmsg_index:
                self.timer.cancel()
                test_logger.info('socket: recv confirm msg_net_index {}, store_netIndex {} - ok'.format(self.recv_netmsg_index, self.netIndex))
                self.netIndex += 1

            else:
                test_logger.info('socket: recv confirm msg_net_index {}, store_netIndex {} - skip'.format(self.recv_netmsg_index, self.netIndex))
            return

        elif self.recv_msg_prefix == prefix_uartData:

            test_logger.info('socket: recv uart_data msg: net index - {}; uart index - {}; data size  - {}'
                .format(self.recv_netmsg_index, self.recv_uartmsg_index, self.recv_msg_size))

            # проверяем, является ли данное сообщение дублирующим
            if self.uartIndex and self.uartIndex >= self.recv_uartmsg_index:
                test_logger.info("socket: recv data msg - uart_index({}) - dublicate".format(self.recv_uartmsg_index))
                return

            self.uartIndex = self.recv_uartmsg_index

            # отправляем подтверждение получения
            msg = b''
            msg += socket.htonl(prefix_uartConfirm).to_bytes(4, "little")
            msg += socket.htonl(self.recv_netmsg_index).to_bytes(4, "little")
            msg += socket.htonl(self.recv_uartmsg_index).to_bytes(4, "little")
            msg += socket.htonl(0).to_bytes(4, "little")

            self.transport.sendto(msg)
            test_logger.info('socket: send uart_confirm msg: net index - {}; uart index - {};'
                .format(self.recv_netmsg_index, self.recv_uartmsg_index))

            if self.netIndex == self.recv_netmsg_index:
                self.timer.cancel()
                test_logger.info('socket: recv data msg confirm: msg_net_index {}, store_netIndex {} - ok'.format(self.recv_netmsg_index, self.netIndex))
                self.netIndex += 1

            # удяляем заголовок
            uartIndex = self.recv_uartmsg_index
            tmp = self.udp_recvBytes
            self.udp_recvBytes = tmp[16:]
 
            # если данных больше или равно, чем указано в заголовке
            # т.е. получили сразу несколько пакетов
            if self.udp_recvBytes != tcp2serial_data:
                test_logger.info("")
                test_logger.info("socket - failed recv pkg data:")
                test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
                test_logger.info("    socket recv ({}): {}".format(self.recv_msg_size, self.udp_recvBytes))
                test_logger.info("")
                self.loop.stop()
                return

            self.iteration += 1
            if self.iteration == iteration_max:
                test_logger.info("socket: regenerate data")
                inSeqSize = random.randrange(minPackSize, maxPackSize)
                inSeqBin = random.randbytes(inSeqSize)

                tcp2serial_data = base64.b64encode(inSeqBin)
                tcp2serial_data_size = len(tcp2serial_data)
                self.iteration = 0

            msg = b''
            msg += socket.htonl(prefix_netData).to_bytes(4, "little")
            msg += socket.htonl(self.netIndex).to_bytes(4, "little")
            msg += socket.htonl(uartIndex).to_bytes(4, "little")
            msg += socket.htonl(tcp2serial_data_size).to_bytes(4, "little")
            msg += tcp2serial_data

            self.transport.sendto(msg)
            self.timer.start(self.timeout_sec, msg, self.netIndex)
            test_logger.info('socket: data sent(size {}, net_index {}, uart_index {}): {}'
                .format(tcp2serial_data_size, self.netIndex, uartIndex, tcp2serial_data))

        else:
            test_logger.warning('socket: incorrect msg prefix = 0x{:x}', self.recv_msg_prefix)
            test_logger.warning('socket: waiting prefix 0x{:x}', prefix_uartData)
            return


    def error_received(self, exc):
        test_logger.info('socket: the server closed the connection')

    def connection_lost(self, exc):
        test_logger.info('socket: the server closed the connection')
        #self.on_con_lost.set_result(True)

    # def pause_writing(self):
    #     test_logger.info('socket: pause writing')
    #     #test_logger.info(self.transport.get_write_buffer_size())

    # def resume_writing(self):
    #     #test_logger.info(self.transport.get_write_buffer_size())
    #     test_logger.info('socket: resume writing')


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
coro_ser  = serial_asyncio.create_serial_connection(loop, SerialProtocol, '/dev/ttyUSB0', baudrate=74880)
#coro_sock = loop.create_datagram_endpoint(lambda: ClientProtocol(loop), remote_addr=('esp8266.local', 8888))
coro_sock = loop.create_datagram_endpoint(lambda: ClientProtocol(loop), remote_addr=('192.168.1.114', 8888))

transport_ser, protocol_ser = loop.run_until_complete(coro_ser)
transport_soc, protocol_soc = loop.run_until_complete(coro_sock)

loop.run_forever()
loop.close()
