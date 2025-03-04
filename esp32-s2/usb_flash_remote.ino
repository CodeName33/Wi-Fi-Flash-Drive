#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else
#include "USB.h"
#include "USBMSC.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "FS.h"
#include "SPIFFS.h"

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
USBCDC USBSerial;
#endif

#define PRINT(TEXT) Serial.print(TEXT); USBSerial.print(TEXT); USBSerial.flush();
#define PRINTLN(TEXT) Serial.println(TEXT); USBSerial.println(TEXT); USBSerial.flush();

namespace Settings
{
	String SSID = "SET_YOUR_WIFI";
	String SSIDPassword = "SET_YOUR_PASSWORD";
	String remote_server = "192.168.1.2"; //SET_YOUR_SERVER_IP
	int remote_port = 8085;

	const String SettingsFileName = "/settings.txt";
}

struct remote_request
{
	uint64_t position;
	uint32_t length;
} __attribute__((packed));

struct init_answer
{
	unsigned int sectors_count = 0;
	unsigned int clusters_count = 0;
	unsigned int files_count = 0;
	unsigned int fat_first_sector = 0;
	unsigned int reserved[4] = {};
} __attribute__((packed));

struct psram_file_sectors_buffer
{
	int32_t* buffer = nullptr;
	uint32_t size = 0;
};

USBMSC MSC;
WiFiClient client;

uint32_t DISK_SECTOR_COUNT = 0;//2 * 8; // 8KB is the smallest size that windows allow to mount
static const uint16_t DISK_SECTOR_SIZE = 512;    // Should be 512
static const uint16_t DISC_SECTORS_PER_TABLE = 1; //each table sector can fit 170KB (340 sectors)
static const byte LED_PIN = 2;


static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
	//HWSerial.printf("MSC WRITE: lba: %u, offset: %u, bufsize: %u\n", lba, offset, bufsize);
	//memcpy(msc_disk[lba] + offset, buffer, bufsize);
	return bufsize;
}




enum BUFFER_STATUSES
{
	BUFFER_STATUS_NONE = 0,
	BUFFER_STATUS_NEED_REQUEST,
	BUFFER_STATUS_IN_REQUEST,
	BUFFER_STATUS_ERROR,
	BUFFER_STATUS_DONE
};

const int BUFFER_SIZE = 32768;//38912 * 2;
struct sectors_buffer
{
	BUFFER_STATUSES status = BUFFER_STATUS_NONE;
	int64_t lba = 0;
	uint32_t offset = 0;
	uint32_t bufsize = BUFFER_SIZE;
	uint32_t recv_offset = 0;
	uint32_t recv_len = 0;
	byte buffer[BUFFER_SIZE];
};

init_answer answer = {};
sectors_buffer g_buffer = {};
psram_file_sectors_buffer psram_file_sectors = {};

bool check_psram(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
	if (psram_file_sectors.size > 0)
	{
		if (lba >= answer.fat_first_sector && lba + bufsize / DISK_SECTOR_SIZE <= answer.fat_first_sector + answer.clusters_count * 4 / DISK_SECTOR_SIZE)
		{
			unsigned int first_gen_sector = (lba - answer.fat_first_sector) * (DISK_SECTOR_SIZE / 4) + (offset / 4);

			if (first_gen_sector > psram_file_sectors.buffer[0])
			{
				unsigned int gen_sector_count = bufsize / 4;
				unsigned int* buffer4 = (unsigned int*)buffer;
				bool inside = false;

				

				unsigned int current_file_index = 0;
				for (unsigned int i = 1; i < psram_file_sectors.size; i++)
				{
					if (psram_file_sectors.buffer[i] > first_gen_sector)
					{
						current_file_index = i;
						unsigned int next_end = psram_file_sectors.buffer[current_file_index];
						while (gen_sector_count)
						{
							if (first_gen_sector == next_end)
							{
								*buffer4++ = 0x0FFFFFF8;
								current_file_index++;
								if (current_file_index < psram_file_sectors.size)
								{
									next_end = psram_file_sectors.buffer[current_file_index];
									
								}
								else
								{
									first_gen_sector++;
									gen_sector_count--;
									while (gen_sector_count)
									{
										*buffer4++ = 0;
										gen_sector_count--;
									}
									return true;
								}
							}
							else
							{
								*buffer4++ = first_gen_sector + 1;
							}
							first_gen_sector++;
							gen_sector_count--;
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
	//HWSerial.printf("MSC READ: lba: %u, offset: %u, bufsize: %u\n", lba, offset, bufsize);
	//memcpy(buffer, msc_disk[lba] + offset, bufsize);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}


	uint64_t pos = (uint64_t)lba * DISK_SECTOR_SIZE + offset;

	uint64_t buffer_pos = (uint64_t)g_buffer.lba * DISK_SECTOR_SIZE + g_buffer.offset;

	//HWSerial.printf("Check Cache: %u, pos: %u, buffer_pos: %u, bufsize: %u, buffers[i].bufsize: %u, status: %u \n", (uint32_t)i, (uint32_t)buffer_pos, (uint32_t)pos, bufsize, buffers[i].bufsize, buffers[i].status);

	if (buffer_pos <= pos && buffer_pos + g_buffer.bufsize >= pos + bufsize)
	{
		if (g_buffer.status == BUFFER_STATUS_IN_REQUEST || g_buffer.status == BUFFER_STATUS_NEED_REQUEST)
		{
			delay(1);
		}

		if (g_buffer.status == BUFFER_STATUS_DONE)
		{
			memcpy(buffer, g_buffer.buffer + (pos - buffer_pos), bufsize);
			return bufsize;
		}
	}

	uint32_t ps_begin = micros();
	static uint ps_count = 0;
	static uint ps_time = 0;
	static uint ps_size = 0;
	/*
	if (check_psram(lba, offset, buffer, bufsize))
	{
		ps_count++;
		ps_time += micros() - ps_begin;
		ps_size += bufsize;
		static uint last_psram_message = 0;
		if (millis() - last_psram_message > 1000)
		{
			Serial.printf("psraddr: %u*1000, avg: %umcs, cnt: %u, sz: %u\r\n", (uint)(((uint64_t)lba * DISK_SECTOR_SIZE + offset) / 1000), ps_time / ps_count, ps_count, ps_size);
			last_psram_message = millis();
			ps_count = 0;
			ps_time = 0;
			ps_size = 0;
		}
		return bufsize;
	}
	//*/
	

	while (g_buffer.status == BUFFER_STATUS_IN_REQUEST || g_buffer.status == BUFFER_STATUS_NEED_REQUEST)
	{
		delay(1);
	}

	if (g_buffer.status == BUFFER_STATUS_NONE || g_buffer.status == BUFFER_STATUS_ERROR || g_buffer.status == BUFFER_STATUS_DONE)
	{
		g_buffer.lba = lba;
		g_buffer.status = BUFFER_STATUS_NEED_REQUEST;
		g_buffer.bufsize = BUFFER_SIZE;
		//HWSerial.printf("Req Curr LBA: %u,%u,%u,%u\n", lba, (uint32_t)buffers[0].status, (uint32_t)buffers[1].status, buffers[1].lba);
	}

	//uint32_t timeWiFi = millis();
	//HWSerial.printf("%u WiFi wait...\n", millis());
	while (g_buffer.status == BUFFER_STATUS_IN_REQUEST || g_buffer.status == BUFFER_STATUS_NEED_REQUEST)
	{
		delay(1);
	}
	//HWSerial.printf("%u WiFi Done (%u)\n", millis(), millis() - timeWiFi);

	if (g_buffer.status == BUFFER_STATUS_DONE)
	{
		memcpy(buffer, g_buffer.buffer, bufsize);

		return bufsize;
	}


	return 0;
}


static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
	HWSerial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\r\n", power_condition, start, load_eject);
	return true;
}

void printWifiStatus() {
	// print the SSID of the network you're attached to:
	Serial.print(F("SSID: "));
	Serial.println(WiFi.SSID());

	// print your board's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print(F("IP Address: "));
	Serial.println(ip);

	// print the received signal strength:
	long rssi = WiFi.RSSI();
	Serial.print(F("signal strength (RSSI):"));
	Serial.print(rssi);
	Serial.println(F(" dBm"));
}

static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == ARDUINO_USB_EVENTS) {
		arduino_usb_event_data_t* data = (arduino_usb_event_data_t*)event_data;
		switch (event_id) {
		case ARDUINO_USB_STARTED_EVENT:
			HWSerial.println(F("USB PLUGGED"));
			break;
		case ARDUINO_USB_STOPPED_EVENT:
			//HWSerial.println("USB UNPLUGGED");
			break;
		case ARDUINO_USB_SUSPEND_EVENT:
			HWSerial.printf("USB SUSPENDED: remote_wakeup_en: %u\r\n", data->suspend.remote_wakeup_en);
			break;
		case ARDUINO_USB_RESUME_EVENT:
			HWSerial.println(F("USB RESUMED"));
			break;

		default:
			break;
		}
	}
}

void indicator()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		digitalWrite(LED_PIN, (millis() / 300) % 2 ? HIGH : LOW);
	}
	else if (!client.connected())
	{
		digitalWrite(LED_PIN, (millis() / 1000) % 2 ? HIGH : LOW);
	}
	else
	{
		digitalWrite(LED_PIN, g_buffer.status == BUFFER_STATUS_IN_REQUEST ? LOW : HIGH);
	}
	//if (client)
}

void receiveInit()
{
	remote_request request = { 0, 0 };
	if (client.write((byte*)&request, sizeof(request)) != sizeof(request))
	{
		client.stop();
	}
	client.flush();

	while (client.connected())
	{
		if (client.available())
		{
			uint32_t DISK_SECTOR_COUNT_PREV = DISK_SECTOR_COUNT;
			client.setTimeout(0);
			if (client.readBytes((byte*)&answer, sizeof(answer)) == sizeof(answer))
			{
				uint32_t psram = ESP.getPsramSize();
				if (psram)
				{
					Serial.print("PSRAM: "); Serial.println(ESP.getPsramSize());
					Serial.print("clusters_count: "); Serial.println(answer.clusters_count);
					Serial.print("fat_first_sector: "); Serial.println(answer.fat_first_sector);
					Serial.print("files_count: "); Serial.println(answer.files_count);
					Serial.print("sectors_count: "); Serial.println(answer.sectors_count);
					//delay(1000);
					if (psram_file_sectors.buffer)
					{
						free(psram_file_sectors.buffer);
						psram_file_sectors.buffer = nullptr;
					}
					psram_file_sectors.buffer = (int32_t*)ps_malloc(answer.files_count * 4 + 4);
					if (psram_file_sectors.buffer != nullptr)
					{
						psram_file_sectors.size = answer.files_count + 1;
					}
					Serial.print("psram_file_sectors.buffer: "); Serial.println((uint32_t)psram_file_sectors.buffer);
					//delay(1000);
				}
				for (int i = 0; i < psram_file_sectors.size; i++)
				{
					uint32_t addr;
					if (client.readBytes((byte*)&addr, 4) != 4)
					{
						break;
					}
					if (psram_file_sectors.size)
					{
						psram_file_sectors.buffer[i] = addr - 1;
					}
				}
				DISK_SECTOR_COUNT = answer.sectors_count;

				if (client.readBytes((byte*)&answer, sizeof(answer)) == sizeof(answer))
				Serial.printf("Received drive sectors count: %u, psram: %u\r\n", (uint32_t)DISK_SECTOR_COUNT, psram_file_sectors.size);
				if (DISK_SECTOR_COUNT != DISK_SECTOR_COUNT_PREV && DISK_SECTOR_COUNT_PREV != 0)
				{
					PRINTLN(F("Drive sectors count changed, rebooting..."));
					delay(300);
					ESP.restart();
				}
				break;
			}

			client.stop();
			break;
		}
		indicator();
	}
}

void writeText(File& f, const String &text)
{
	f.write((const uint8_t*)text.c_str(), text.length());
}

void saveSettings()
{
	if (SPIFFS.totalBytes() == 0)
	{
		SPIFFS.format();
		PRINTLN("SPIFFS created. Total bytes: "); Serial.println(SPIFFS.totalBytes());
		delay(300);
		ESP.restart();
	}
	
	if (File f = SPIFFS.open(Settings::SettingsFileName, "w"))
	{
		writeText(f, Settings::SSID.c_str()); writeText(f, ";");
		writeText(f, Settings::SSIDPassword.c_str()); writeText(f, ";");
		writeText(f, Settings::remote_server.c_str()); writeText(f, ";");
		writeText(f, String(Settings::remote_port).c_str()); writeText(f, ";");
		f.close();
	}
}

void printSettings()
{
	String passwordSafe = Settings::SSIDPassword;
	for (int i = 2; i < passwordSafe.length(); i++)
	{
		passwordSafe[i] = '*';
	}

	PRINTLN(F("Current Settings:"));
	PRINT(F("\tSSID: ")); PRINTLN(Settings::SSID);
	PRINT(F("\tSSIDPassword: ")); PRINTLN(passwordSafe);
	PRINT(F("\tRemote Server: ")); PRINTLN(Settings::remote_server);
	PRINT(F("\tRemote Port: ")); PRINTLN(Settings::remote_port);
}

void loadSettings()
{
	
	if (SPIFFS.exists(Settings::SettingsFileName))
	{
		if (File f = SPIFFS.open(Settings::SettingsFileName, "r"))
		{
			Settings::SSID = f.readStringUntil(';');
			Settings::SSIDPassword = f.readStringUntil(';');
			Settings::remote_server = f.readStringUntil(';');
			Settings::remote_port = f.readStringUntil(';').toInt();

			printSettings();
		}
	}
	else
	{
		saveSettings();
	}
	//exists("/settings.txt");

}

void processCommand(const String& command)
{
	if (command == "?" || command == "help" || command == "/?" || command == "--help")
	{
		PRINTLN("Commands:");
		PRINTLN("\tset ssid YOUR_SSID_NAME");
		PRINTLN("\tset password YOUR_SSID_PASSWORD");
		PRINTLN("\tset server YOUR_SERVER_IP");
		PRINTLN("\tset port YOUR_SERVER_PORT");
		PRINTLN("\treboot/restart - restart device");
		PRINTLN("\tsave - save settings");
		PRINTLN("\tload - load settings");
	}
	else if (command.startsWith("set "))
	{
		String parameter = command.substring(4);
		parameter.trim();
		if (parameter.startsWith("ssid "))
		{
			Settings::SSID = parameter.substring(5);
			Settings::SSID.trim();
			PRINTLN(F("Settings changed"));
			printSettings();
		}
		else if (parameter.startsWith("password "))
		{
			Settings::SSIDPassword = parameter.substring(9);
			Settings::SSIDPassword.trim();
			PRINTLN(F("Settings changed"));
			printSettings();
		}
		else if (parameter.startsWith("server "))
		{
			Settings::remote_server = parameter.substring(7);
			Settings::remote_server.trim();
			PRINTLN(F("Settings changed"));
			printSettings();
		}
		else if (parameter.startsWith("port "))
		{
			String port = parameter.substring(5);
			port.trim();
			Settings::remote_port = port.toInt();
			PRINTLN(F("Settings changed"));
			printSettings();
		}
	}
	else if (command == "reboot" || command == "restart")
	{
		PRINTLN(F("Restarting..."));
		delay(300);
		ESP.restart();
	}
	else if (command == "save")
	{
		saveSettings();
		PRINTLN(F("Settings saved"));
	}
	else if (command == "load")
	{
		PRINTLN(F("Settings loaded"));
		loadSettings();
	}
}

char serialBuffer[64] = {};
int serialBufferIndex = 0;
void serial()
{
	Stream* serials[2] = { &Serial, &USBSerial };

	for (int i = 0; i < 2; i++)
	{
		Stream& serial = *(serials[i]);
		if (serial.available())
		{
			char c = serial.read();

			/*
			Serial.println("char:");
			Serial.print(c);
			Serial.print(", code:");
			Serial.println((byte)c);*/
			if (c != 255)
			{
				serialBuffer[serialBufferIndex] = c;
				if (serialBuffer[serialBufferIndex] == '\n' || serialBuffer[serialBufferIndex] == '\r')
				{
					serialBuffer[serialBufferIndex] = 0;
					String command = serialBuffer;
					command.trim();
					serialBufferIndex = 0;

					processCommand(command);
				}
				else
				{
					serialBufferIndex++;
				}

				if (serialBufferIndex > 63)
				{
					PRINTLN("Error in command (too long)");
					serialBufferIndex = 0;
				}
			}
		}

	}
}

void setup() {
	pinMode(2, OUTPUT);
	

	SPIFFS.begin();

	HWSerial.begin(115200);
	HWSerial.setDebugOutput(true);
	Serial.begin(115200);

	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB port only
	}


	uint32_t timeStart = millis();
	loadSettings();
	// attempt to connect to Wifi network:
	Serial.print(F("Attempting to connect to SSID: "));
	Serial.println(Settings::SSID);

	WiFi.mode(WIFI_STA);
	WiFi.setSleep(WIFI_PS_NONE);
	//esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
	//esp_err_t e = esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_MCS7_SGI);
	WiFi.setTxPower(WIFI_POWER_19_5dBm);
	//Serial.printf("esp_wifi_config_80211_tx_rate: %u\n", (uint32_t)e);
	WiFi.begin(Settings::SSID.c_str(), Settings::SSIDPassword.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		indicator();
		serial();
		Serial.print(".");

		if (millis() - timeStart > 10000)
		{
			break;
		}
	}

	Serial.println("");
	Serial.println(F("Connected to WiFi"));
	printWifiStatus();

	client.setNoDelay(true);
	while (!client.connected())
	{
		if (client.connect(Settings::remote_server.c_str(), Settings::remote_port))
		{
			if (client.connected())
			{
				Serial.println(F("Connected to server"));
				receiveInit();
			}
		}

		if (client.connected() && DISK_SECTOR_COUNT != 0)
		{
			break;
		}
		indicator();
		serial();

		if (millis() - timeStart > 10000)
		{
			break;
		}
	}
	client.setNoDelay(true);

	USB.onEvent(usbEventCallback);
	MSC.vendorID("ESP32");//max 8 chars
	MSC.productID("USB_MSC");//max 16 chars
	MSC.productRevision("1.0");//max 4 chars
	MSC.onStartStop(onStartStop);
	MSC.onRead(onRead);
	MSC.onWrite(onWrite);
	MSC.mediaPresent(true);
	MSC.begin(DISK_SECTOR_COUNT, DISK_SECTOR_SIZE);
	USBSerial.begin(115200);
	USB.begin();

	Serial.print("PSRAM Total: "); Serial.println(ESP.getPsramSize());
	Serial.print("PSRAM  Free: "); Serial.println(ESP.getFreePsram());
	
}


uint32_t lbaDebug = 0;
uint32_t debugTime = 0;

bool connected = true;


void loop()
{
	indicator();
	serial();

	/*
	if (lbaDebug < 10 && (buffers[0].status != BUFFER_STATUS_IN_REQUEST && buffers[0].status != BUFFER_STATUS_NEED_REQUEST))
	{
		//HWSerial.printf("%u WiFi wait...\n", millis());
		debugTime = millis();

		buffers[0].lba = lbaDebug;
		buffers[0].status = BUFFER_STATUS_NEED_REQUEST;
		lbaDebug++;
	}*/

	while (!client.connected())
	{
		if (connected)
		{
			connected = false;
			PRINTLN(F("Disconnected from server"));
		}
		if (client.connect(Settings::remote_server.c_str(), Settings::remote_port))
		{
			if (client.connected())
			{
				//client.setNoDelay(true);
				PRINTLN(F("Connected to server"));
				g_buffer.status = BUFFER_STATUS_NONE;
				receiveInit();
				connected = true;
				break;
			}
		}
		serial();
		indicator();
	}

	if (g_buffer.status == BUFFER_STATUS_NEED_REQUEST)
	{
		remote_request request = { g_buffer.lba * DISK_SECTOR_SIZE + g_buffer.offset, g_buffer.bufsize };
		debugTime = millis();
		if (client.write((byte*)&request, sizeof(request)) != sizeof(request))
		{
			client.stop();
		}
		else
		{
			client.flush();
			
			g_buffer.status = BUFFER_STATUS_IN_REQUEST;
			g_buffer.recv_offset = 0;
			g_buffer.recv_len = g_buffer.bufsize;
			
		}
	}

	indicator();

	if (g_buffer.status == BUFFER_STATUS_IN_REQUEST)
	{
		uint32_t recv = client.readBytes(g_buffer.buffer + g_buffer.recv_offset, g_buffer.recv_len);
		if (recv == g_buffer.recv_len)
		{
			g_buffer.status = BUFFER_STATUS_DONE;
		}
		else if (recv < g_buffer.recv_len)
		{
			g_buffer.recv_len -= recv;
			g_buffer.recv_offset += recv;
		}
		else
		{
			g_buffer.status = BUFFER_STATUS_ERROR;
			PRINTLN(F("Error read"));
		}

		
		
		//HWSerial.printf("WiFi Done (%u)\r\n", millis() - debugTime);

		//Serial.printf("buffers[%u] done on lba = %u\n", (uint32_t)i, (uint32_t)buffers[i].lba);
	}
	indicator();
}
#endif /* ARDUINO_USB_MODE */
