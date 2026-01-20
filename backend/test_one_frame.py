import socket
import base64
import os
import struct
import time

def create_masked_frame(data):
    # Opcode 1 (text), FIN 1 -> 0x81
    header = b'\x81'
    payload_len = len(data)
    
    # Client frames must be masked (bit 7 set)
    # 0x80 | length
    if payload_len < 126:
        header += struct.pack('B', 0x80 | payload_len)
    elif payload_len < 65536:
        header += struct.pack('!BH', 0x80 | 126, payload_len)
    else:
        header += struct.pack('!BQ', 0x80 | 127, payload_len)
        
    masking_key = os.urandom(4)
    header += masking_key
    
    masked_data = bytearray(payload_len)
    for i in range(payload_len):
        masked_data[i] = data[i] ^ masking_key[i % 4]
        
    return header + masked_data

def run():
    host = '127.0.0.1'
    port = 8080
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
        
        # Handshake
        key = base64.b64encode(os.urandom(16)).decode('utf-8')
        request = (
            f"GET / HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"\r\n"
        )
        s.sendall(request.encode('utf-8'))
        
        response = s.recv(4096)
        if b"101 Switching Protocols" not in response:
            print("Handshake failed")
            return

        print("Handshake successful")
        
        # Send one frame request
        # payload format: x_min,y_min,x_scale,y_scale,width,height,iterations
        payload = "-2.0,-1.0,0.01,0.01,100,100,100".encode('utf-8')
        frame = create_masked_frame(payload)
        s.sendall(frame)
        
        # Wait a bit for server to process and log
        time.sleep(1)
        
        s.close()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    run()
