import socket
import threading

def get_response_data():
    """Convert the hex string from the comments into a bytes object"""
    hex_data = """00 00 01 EF 52 E8 03 52 EC 02 52 E9 02 52 60 86
5F A5 A1 48 0D 81 CF 4F A8 BD A0 42 AC 58 FB E0
AA 97 A0 CF 9D 1B 3D F6 10 29 38 8C 2E 31 60 3C
DC B8 CD B9 06 3E CA 24 E2 8D C9 79 CD BD 1D 32
AC EF 1C 7C C2 3A D6 E2 DD 61 E6 D1 E6 EC 44 BD
5F DF 19 99 E3 D2 8F 89 71 AA 6C AE 49 0B 5E 20
C4 AA C5 FD 4F E7 57 F3 CA 63 94 DA 00 3C 10 A0
01 00 F2 01 80 02 3D 63 41 13 0C D9 32 63 2A 40
8B E3 15 ED BA 02 8B BF A9 8E BB 75 CC 27 30 94
4E C5 B5 C6 79 17 FF 3B E6 ED B6 66 E1 41 59 8D
52 C8 43 21 4A C3 31 90 00 B9 B1 B5 84 10 B7 0D
43 EF EE 65 85 21 D0 05 C5 E1 26 D3 23 1E 8B DE
12 CC 16 67 B0 88 73 FC E6 E3 BD D3 DF 67 D7 FA
22 B0 43 40 C4 7C 2E F7 35 1A E4 D4 24 89 EF 42
54 D3 E0 8D 6A 5B 2C B6 23 EC D3 37 4B 95 6A 41
4F 8A 7D 80 C7 C2 6C 9F FD E4 66 92 59 1F 19 0E
4C F7 BB 47 82 66 F6 13 93 55 C1 54 6F A0 93 EB
92 6E A2 1C D6 DF C3 C7 0D DD A5 44 11 15 40 B7
27 8A 81 22 21 99 7E 2E DC BD 8B EB C6 32 BA 84
D3 5D 95 0E 2D A2 8F B4 AC AB 1B 6A D5 20 51 25
DC 0D E4 8E 88 85 6A F5 A1 BD 47 1B 55 AF EB 94
1D 09 F9 75 91 25 C3 56 EB DE 96 CF 7E A8 0A D5
A1 98 0B CE 10 0C 51 40 52 F6 12 74 87 A1 E1 C5
4B 3C D2 82 4D 18 A2 01 00 F2 01 00 C2 02 02 52
00 92 03 10 F4 F6 39 58 53 87 3E 15 9E 0D DD 10
66 52 89 FA E2 03 58 8C 96 E2 57 DE CD 1D C0 E9
5E 84 D2 8A 8B C6 80 C4 1E D3 4B 5C E8 EA 6A C6
FA D0 18 83 CA E1 0F 60 C3 67 3E 91 84 FE 7A E2
83 4C 6C 0E 12 EC D2 30 BF 1D 8D A8 07 F7 6E 01
C7 86 84 91 67 94 F1 2A FB 30 BB 7F 2E 35 61 B1
81 CD BF 93 B9 91 C4 78 AF 51 20 B6 48 8E B8"""
    # Remove whitespace and convert to bytes
    hex_bytes = bytes.fromhex(''.join(hex_data.split()))
    return hex_bytes

class TCPServer:
    def __init__(self, host='0.0.0.0', port=8080):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.response_data = get_response_data()

    def start(self):
        self.sock.bind((self.host, self.port))
        self.sock.listen(5)
        print(f"Server listening on {self.host}:{self.port}")

        while True:
            client, address = self.sock.accept()
            client_thread = threading.Thread(target=self.handle_client, args=(client, address))
            client_thread.daemon = True
            client_thread.start()

    def handle_client(self, client_socket, address):
        print(f"Connection from {address}")
        try:
            while True:
                data = client_socket.recv(1024)
                if not data:
                    break

                print(f"Received data from {address}: {len(data)} bytes")

                # Send the predefined response data
                client_socket.send(self.response_data)
                print(f"Sent response data ({len(self.response_data)} bytes) to {address}")

        except Exception as e:
            print(f"Error handling client {address}: {e}")
        finally:
            client_socket.close()
            print(f"Connection with {address} closed")

def main():
    server = TCPServer()
    try:
        server.start()
    except KeyboardInterrupt:
        print("Server shutting down")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()