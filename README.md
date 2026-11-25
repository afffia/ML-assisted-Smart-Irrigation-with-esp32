1. For training with dataset:
   Upload IrrigationModel.h to the ESP32
1. Open Arduino IDE on the computer and save a new sketch named SmartIrrigationML (this creates a folder named SmartIrrigationML).
2. Move the downloaded IrrigationModel.h file into that exact SmartIrrigationML folder.
3. Install these three libraries via Sketch > Include Library > Manage Libraries:
	• Adafruit SSD1306
	• Adafruit GFX Library
	• DHT sensor library (by Adafruit)
Create SmartIrrigationML.ino
Plug the ESP32 into the PC via USB, select Tools > Board > DOIT ESP32 DEVKIT V1 and the COM Port, and click Upload.
