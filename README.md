# Wi-Fi-Flash-Drive
Pseudo USB Flash Drive that gets it contents from PC by WiFi (Based on ESP32-S2)

# How it works
This is ESP32-S2 based USB device that will be recognized as USB Drive but all drive contents getting from server by wifi. You can share folder on servet and it will create pseudo FAT32 image (this is not real image, it will not be written on disk or memory, it can be huge size, 500gb or etc. and it will be created very fast).

# Problems
ESP32-S2 WiFi is not fast, i can reach only 2-2.5Mbits/sec max. So it's slow :(

# How To Create Device
You need ESP32-S2 board and male USB connector. Connect  
- USB DATA+ with ESP GPIO20 
- USB DATA- with ESP GPIO19
- USB GND with ESP GND 
- USB 5v with ESP 5v  

Change configuration in firmware source "usb_flash_remote.ino":
``` c++
namespace Settings
{
	String SSID = "SET_YOUR_WIFI";
	String SSIDPassword = "SET_YOUR_PASSWORD";
	String remote_server = "192.168.1.2"; //SET_YOUR_SERVER_IP
	int remote_port = 8085;

	const String SettingsFileName = "/settings.txt"; //DONT CHANGE
}
```
(WiFi SSID, Password, ServerIP, ServerPort). It can be changed later using COM port commands.  
Upload firmware to your ESP32-S2 over board's build in UART Micro/TypeC USB using Arduino studio with ESP32-S2 SDK or other software.

# Starting server
Change settings on server "**remote-flash-server.config**":  
> path=YOUR_SHARING_PATH  
> port=PORT (must be same as on board config)


run **remote-flash-server.exe** on server

# Connecting device
Connect your new male USB connector from ESP32-S2 (not board's built in UART USB, but you can connect it to for debug, but i recommeend in this case disconnect 5v and GND from new USB connector) to your target device. If you use ESP-12K with built-in led in GPIO2 it will display status:
- fast blink (3 times per second) - trying to connect WiFi
- slow blink (1 times per second) - WiFi connected, trying to connect server
- led is always on - connected to server, no data transfer
- superfast blink (8-10 times per second) - all ok, data transfer goes (you can see data transfer in server application console)

# Changing configuration on ESP device
Device will store settings on internal SPIFFS after first boot, so updating it in firmware source and reflashing will has no effect. So you can format internat fs during flashing or change it using COM port commands. You can connect COM port 2 ways: using build-in UART USB or using new USB connector that add COM port too (but only after connecting wifi/server or after 10 sec trying). Connect to COM port (using Putty for example) and enter command "help". All available commands will be displayed:
```
Commands:
  set ssid YOUR_SSID_NAME
  set password YOUR_SSID_PASSWORD
  set server YOUR_SERVER_IP
  set port YOUR_SERVER_PORT
  reboot/restart - restart device
  save - save settings
  load - load settings
```
So you can use this commands to change settings. After all don't forget to exec **save** command and **reboot** or PLUG-OUT/IN USB.
