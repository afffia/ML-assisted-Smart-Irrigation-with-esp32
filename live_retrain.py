import serial, csv, os
import pandas as pd
from sklearn.tree import DecisionTreeRegressor
from micromlgen import port

PORT = "COM3"  # Update this on your teammate's laptop
CSV_FILE = "agrisense_live_data.csv"

if not os.path.exists(CSV_FILE):
    with open(CSV_FILE, "w", newline="") as f:
        csv.writer(f).writerow(["soil_moisture", "temperature", "humidity", "hour", "pump_seconds"])

ser = serial.Serial(PORT, 115200, timeout=2)
print("Listening to AgriSense over USB...")

while True:
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    if not line or line.startswith("soil_") or line.startswith("Timestamp"):
        continue

    parts = line.split(",")
    
    # Handles 6-column format: soil,temp,hum,hour,secs,status
    if len(parts) == 6 and parts[0].isdigit():
        ts = "LIVE"
        soil = int(parts[0])
        temp = float(parts[1])
        hum = float(parts[2])
        hour = int(parts[3])
        secs = int(parts[4])
        status = parts[5]
    # Handles 7-column format: Timestamp,Soil_%,Temp_C,Hum_%,Hour,Run_Sec,Status
    elif len(parts) == 7 and ":" in parts[0]:
        ts = parts[0]
        soil = int(parts[1].replace("%", ""))
        temp = float(parts[2].replace("C", ""))
        hum = float(parts[3].replace("%", ""))
        hour = int(parts[4])
        secs = int(parts[5].replace("s", ""))
        status = parts[6]
    else:
        continue

    # Only log & retrain on genuine ML decisions (ignore safety-locked rows)
    if status in ["WATERING", "HOLD"]:
        with open(CSV_FILE, "a", newline="") as f:
            csv.writer(f).writerow([soil, temp, hum, hour, secs])

        df = pd.read_csv(CSV_FILE)
        if len(df) >= 5:
            model = DecisionTreeRegressor(max_depth=4, random_state=42)
            model.fit(df[["soil_moisture", "temperature", "humidity", "hour"]], df["pump_seconds"])
            with open("IrrigationModel.h", "w") as h:
                h.write(port(model))
        print(f"[{ts}] LOGGED & RETRAINED ({len(df)} rows) -> Soil:{soil}% Temp:{temp}C Hr:{hour} => {secs}s [{status}]")
    else:
        print(f"[{ts}] LIVE MONITOR ONLY (Not added to dataset) -> Soil:{soil}% Temp:{temp}C Hr:{hour} => {secs}s [{status}]")
