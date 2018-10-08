import socket
import struct
import unittest

BETSERVER_OPEN = 1
BETSERVER_ACCEPT = 2
BETSERVER_BET = 3
BETSERVER_RESULT = 4


class TestBetServerConnection(unittest.TestCase):
    def test_connect(self):
        s = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
        s.connect(('localhost', 2222))
        s.close()


class TestBetServer(unittest.TestCase):
    BUFFER_SIZE = 100

    connected_socket = None

    def createHeader(self, version, size, type, id):
        return int(version) | int(size) << 3 | int(type) << 8 | int(id) << 16

    def setUp(self):
        self.connected_socket = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
        self.connected_socket.connect(('localhost', 2222))

    def tearDown(self):
        self.connected_socket.close();

    def test_valid_connection(self):
        self.assertTrue(self.connected_socket)

    def test_header_open(self):
        """
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |  1  |    4    |      1        |              0                |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        """
        header = self.createHeader(1, 4, BETSERVER_OPEN, 0)
        print("Open header: %d" % header)
        self.assertEqual(header, 289, "header is not valid")
        data = struct.pack("!I", header)
        print(self.connected_socket.send(data))

    def test_header_bet(self):
        """
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |  1  |    8    |      3        |              34567            |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        """
        header_open = self.createHeader(1, 4, BETSERVER_OPEN, 0)
        self.connected_socket.send(struct.pack("!I", header_open))

        header_bet = self.createHeader(1, 8, BETSERVER_BET, 34567)
        print("Bet header: %d" % header_bet)
        self.assertEqual(header_bet, 2265383745, "header is not valid")
        self.connected_socket.send(struct.pack("!I", header_bet))
        self.connected_socket.send(struct.pack("!I", int(5)))

    def test_bet(self):
        """
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |  1  |    8    |      3        |              34567            |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        """
        header_open = self.createHeader(1, 4, BETSERVER_OPEN, 0)
        self.connected_socket.send(struct.pack("!I", header_open))

        header_bet = self.createHeader(1, 8, BETSERVER_BET, 34567)
        self.connected_socket.send(struct.pack("!I", header_bet))

        bet = 3774873510
        self.connected_socket.send(struct.pack("!I", bet))
        print("Betting: %d" % bet)
        reply = self.connected_socket.recv(self.BUFFER_SIZE)
        print("Got reply: %s" % reply)
        self.assertIn("winner is", reply)


if __name__ == '__main__':
    unittest.main()
