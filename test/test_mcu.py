import sys
sys.path.append('/home/apollo/.platformio/penv/lib/python3.10/site-packages')

import asyncio
import base64
import logging
import random
import socket
import serial_asyncio


minPackSize = 50
maxPackSize = 200

tcp2serial_data = b''
tcp2serial_data_size = 0
wustmo_started = 0

iteration_max = 10


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
        global wustmo_started

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

                if line == ":wustmo".encode():
                    if wustmo_started == 0:
                        test_logger.info("serial: data line: {}".format(line.decode()))

                    wustmo_started += 1
                    continue
                else:
                    if wustmo_started > 1:
                        test_logger.info("serial: data line: :wustmo ({})".format(wustmo_started))

                    wustmo_started = 0

                test_logger.info('serial: data line: {}'.format(line.decode()))

                if line[:6] == "data: ".encode():
                    self.mcu_recvBytes += line[6:]
                    test_logger.info("serial: pkg recv {} from {}".format(len(self.mcu_recvBytes), tcp2serial_data_size))

                    if self.mcu_recvBytes != tcp2serial_data[:len(self.mcu_recvBytes)]:
                        test_logger.info("serial: pkg recv failed".format(len(self.mcu_recvBytes), tcp2serial_data_size))
                        test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
                        test_logger.info("    serial recv ({}): {}".format(len(self.mcu_recvBytes), self.mcu_recvBytes))
                        test_logger.info("")
                        self.transport.loop.stop()

                    if len(self.mcu_recvBytes) < tcp2serial_data_size:
                        return

                    if tcp2serial_data != self.mcu_recvBytes:
                        test_logger.info()
                        test_logger.info("serial failed recv data:")
                        test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
                        test_logger.info("    serial recv ({}): {}".format(len(self.mcu_recvBytes), self.mcu_recvBytes))
                        test_logger.info()
                        self.transport.loop.stop()
                        return

                    test_logger.info('serial: recv package: {}'.format(self.mcu_recvBytes))
                    self.transport.write(self.mcu_recvBytes)
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


class ClientProtocol(asyncio.Protocol):
    def __init__(self, loop) -> None:
        super().__init__()
        self.iteration = 0
        self.tcp_recvBytes = b''
        self.loop = loop

    def connection_made(self, transport):
        global tcp2serial_data
        global tcp2serial_data_size

        self.transport = transport
        client_socket = self.transport.get_extra_info('socket')
        client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_QUICKACK, 1)

        client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1024)
        client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)

        test_logger.info(f'socket: connection_mode: {self.transport}')

        inSeqSize = random.randrange(minPackSize, maxPackSize)
        inSeqBin = random.randbytes(inSeqSize)

        tcp2serial_data = base64.b64encode(inSeqBin)
        tcp2serial_data_size = len(tcp2serial_data)

        #self.socket = self.transport.get_extra_info('socket')
        self.transport.write(tcp2serial_data)
        test_logger.info('socket: data sent({}): {}'.format(tcp2serial_data_size, tcp2serial_data))

    def data_received(self, data):
        global tcp2serial_data
        global tcp2serial_data_size

        self.tcp_recvBytes += data
        test_logger.info('socket: recv data {} bytes, size of recv_data {} bytes'.format(len(data), len(self.tcp_recvBytes)))

        if len(self.tcp_recvBytes) < tcp2serial_data_size:
            return

        test_logger.info('socket: data received: {}'.format(self.tcp_recvBytes))
        self.iteration += 1

        if tcp2serial_data != self.tcp_recvBytes:
            test_logger.info("")
            test_logger.info("socket - failed recv data:")
            test_logger.info("    socket send ({}): {}".format(tcp2serial_data_size, tcp2serial_data))
            test_logger.info("    serial recv ({}): {}".format(len(self.tcp_recvBytes), self.tcp_recvBytes))
            test_logger.info("")
            self.loop.stop()
            return

        self.tcp_recvBytes = b''

        if self.iteration == iteration_max:
            test_logger.info("socket: regenerate data")
            inSeqSize = random.randrange(minPackSize, maxPackSize)
            inSeqBin = random.randbytes(inSeqSize)

            tcp2serial_data = base64.b64encode(inSeqBin)
            tcp2serial_data_size = len(tcp2serial_data)
            self.iteration = 0

        self.transport.write(tcp2serial_data)
        test_logger.info('socket: data sent({}): {}'.format(tcp2serial_data_size, tcp2serial_data))

    def connection_lost(self, exc):
        test_logger.info('socket: the server closed the connection')
        #self.on_con_lost.set_result(True)

    def pause_writing(self):
        test_logger.info('socket: pause writing')
        #test_logger.info(self.transport.get_write_buffer_size())

    def resume_writing(self):
        #test_logger.info(self.transport.get_write_buffer_size())
        test_logger.info('socket: resume writing')


# получение пользовательского логгера и установка уровня логирования
test_logger = logging.getLogger(__name__)
test_logger.setLevel(logging.INFO)

# настройка обработчика и форматировщика в соответствии с нашими нуждами
test_handler = logging.FileHandler(f"{__name__}.log", mode='w')
test_formatter = logging.Formatter("%(asctime)s %(message)s")

# добавление форматировщика к обработчику 
test_handler.setFormatter(test_formatter)

# добавление обработчика к логгеру
test_logger.addHandler(test_handler)

test_logger.info(f"Testing the custom logger for module {__name__}...")

loop = asyncio.get_event_loop()
coro_ser  = serial_asyncio.create_serial_connection(loop, SerialProtocol, '/dev/ttyUSB0', baudrate=250000)
coro_sock = loop.create_connection(lambda: ClientProtocol(loop), host="esp8266.local", port=8888)


transport_ser, protocol_ser = loop.run_until_complete(coro_ser)
transport_soc, protocol_soc = loop.run_until_complete(coro_sock)

loop.run_forever()
loop.close()


