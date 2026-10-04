import time
import serial

PORT = "COM8"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=0.2)
ser.dtr = False
ser.rts = True
time.sleep(0.1)
ser.rts = False

start = time.time()
lines = []
while time.time() - start < 30:
    chunk = ser.read(4096)
    if chunk:
        lines.append(chunk)

ser.close()
data = b"".join(lines)
with open(r"D:\02-Projects\Smart-Home\tools\boot_log.txt", "wb") as f:
    f.write(data)
text = data.decode("utf-8", errors="replace")
print("TOTAL BYTES:", len(data))
print("TOTAL LINES:", text.count("\n"))
print("---- TAIL ----")
print(text[-3000:])
