#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "WiFi.h"
#include <TFT_eSPI.h>
#include "HTTPClient.h"
#include "esp_http_client.h"

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#include "camera_pins.h"

#define SD_CS_PIN 21 // ESP32S3 Sense SD Card Reader
// #define SD_CS_PIN D2 // XIAO Round Display SD Card Reader
#define TOUCH_INT D7

typedef enum
{
  MAIN_PAGE,
  RESULTS,
  INITS,
  TAKE_PHOTO,
  CONFIRM_SEND,
  WAIT_SERVER,
  CONFIRM_PRINT
} state_t;

// Replace with your network credentials
const char *ssid = "SSID";
const char *password = "PASSWORD";

// Width and height of round display
const int camera_width = 240;
const int camera_height = 240;

unsigned long lastCaptureTime = 0; // Last shooting time
int imageCount = 0;                // File Counter
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status
bool wifi_sign = false;            // Check sd status

HTTPClient http;
const char img2img_server[] = "http://101.6.161.43:8000";
const char host[] = "101.6.161.43";

TFT_eSPI tft = TFT_eSPI();

state_t state = TAKE_PHOTO;

// avail API of img2img_server
bool avail()
{
  int httpCode;
  Serial.println("Sending GET to avail API");
  if (http.begin(String(img2img_server) + "/avail"))
  {
    httpCode = http.GET();
  }
  else
  {
    Serial.println("[HTTP] Unable to connect");
    delay(1000);
  }
  String payload = http.getString();

  if (httpCode == HTTP_CODE_OK)
  {
    int dataStart = payload.indexOf("{\"available\":") + strlen("{\"available\":");
    int dataEnd = payload.indexOf("}");
    String avail = payload.substring(dataStart, dataEnd);
    Serial.printf("Call avail succeed! availability is %s\n", avail);
    if (avail == "true")
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    Serial.printf("Call avail failed... return code: %d\n", httpCode);
    return false;
  }
}

// generate API of img2img_server
int generate(const char fileName[])
{
  return 0;
  // // prepare httpclient
  // Serial.println("Starting connection to server...");
  // Serial.println(host);

  // WiFiClient client;
  // delay(1000);
  // // start http sending
  // if (client.connect(host, 8000))
  // {
  //   // open file
  //   File myFile;
  //   myFile = SD_MMC.open(fileName); // change to your file name
  //   int filesize = myFile.size();
  //   Serial.print("filesize=");
  //   Serial.println(filesize);
  //   String fileName = myFile.name();
  //   String fileSize = String(myFile.size());

  //   Serial.println("reading file");
  //   if (myFile)
  //   {
  //     String boundary = "CustomizBoundarye----";
  //     String contentType = "image/jpeg"; // change to your file type

  //     // prepare http post data(generally, you dont need to change anything here)
  //     String postHeader = "POST " + Sendurl + " HTTP/1.1\r\n";
  //     postHeader += "Host: " + Sendhost + ":80 \r\n";
  //     postHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  //     postHeader += "Accept-Charset: utf-8;\r\n";
  //     String keyHeader = "--" + boundary + "\r\n";
  //     keyHeader += "Content-Disposition: form-data; name=\"key\"\r\n\r\n";
  //     String requestHead = "--" + boundary + "\r\n";
  //     requestHead += "Content-Disposition: form-data; name=\"\"; filename=\"" + fileName + "\"\r\n";
  //     requestHead += "Content-Type: " + contentType + "\r\n\r\n";
  //     // post tail
  //     String tail = "\r\n--" + boundary + "--\r\n\r\n";
  //     // content length
  //     int contentLength = keyHeader.length() + requestHead.length() + myFile.size() + tail.length();
  //     postHeader += "Content-Length: " + String(contentLength, DEC) + "\n\n";

  //     // send post header
  //     char charBuf0[postHeader.length() + 1];
  //     postHeader.toCharArray(charBuf0, postHeader.length() + 1);
  //     client.write(charBuf0);
  //     Serial.print("send post header=");
  //     Serial.println(charBuf0);

  //     // send key header
  //     char charBufKey[keyHeader.length() + 1];
  //     keyHeader.toCharArray(charBufKey, keyHeader.length() + 1);
  //     client.write(charBufKey);
  //     // Serial.print("send key header=");
  //     // Serial.println(charBufKey);

  //     // send request buffer
  //     char charBuf1[requestHead.length() + 1];
  //     requestHead.toCharArray(charBuf1, requestHead.length() + 1);
  //     client.write(charBuf1);
  //     // Serial.print("send request buffer=");
  //     // Serial.println(charBuf1);

  //     // create file buffer
  //     const int bufSize = 4096;
  //     byte clientBuf[bufSize];
  //     int clientCount = 0;

  //     // send myFile:
  //     Serial.println("Send file");
  //     clientBuf[clientCount] = myFile.read();
  //     if (clientCount > 0)
  //     {
  //       client.write((const uint8_t *)clientBuf, clientCount);
  //       // Serial.println("Sent LAST buffer");
  //     }

  //     // send tail
  //     char charBuf3[tail.length() + 1];
  //     tail.toCharArray(charBuf3, tail.length() + 1);
  //     client.write(charBuf3);
  //     Serial.println("send tail");
  //     // Serial.print(charBuf3);
  //   }
  // }
  // else
  // {
  //   Serial.println("Connecting to server error");
  // }
  // // print response
  // unsigned long timeout = millis();
  // while (client.available() == 0)
  // {
  //   if (millis() - timeout > 10000)
  //   {
  //     Serial.println(">>> Client Timeout !");
  //     client.stop();
  //     return;
  //   }
  // }
  // while (client.available())
  // {
  //   String line = client.readStringUntil('\r');
  //   Serial.print(line);
  // }
  // // myFile.close();
  // Serial.println("closing connection");
  // client.stop();
}

bool display_is_pressed(void)
{
    if(digitalRead(TOUCH_INT) != LOW) {
        delay(3);
        if(digitalRead(TOUCH_INT) != LOW)
        return false;
    }
    return true;
}

// WiFi Connection
void WiFiConnect(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Save pictures to SD card
void photo_save(camera_fb_t *fb, const char *fileName)
{
  // Save photo to file
  size_t out_len = 0;
  uint8_t *out_buf = NULL;
  esp_err_t ret = frame2jpg(fb, 12, &out_buf, &out_len);
  if (ret == false)
  {
    Serial.printf("JPEG conversion failed");
  }
  else
  {
    // Save photo to file
    writeFile(SD, fileName, out_buf, out_len);
    Serial.printf("Saved picture: %s\n", fileName);
    imageCount++;
    free(out_buf);
  }
}

// SD card write file
void writeFile(fs::FS &fs, const char *path, uint8_t *data, size_t len)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.write(data, len) == len)
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

// SD card read file
void readFile(fs::FS &fs, const char *path, uint8_t *data, size_t len)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path, FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }
  if (file.read(data, len) == len)
  {
    Serial.println("File read");
  }
  else
  {
    Serial.println("Read failed");
  }
  file.close();
}

// SD card create directory
bool createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Directory created");
    return true;
  }
  else
  {
    Serial.println("Mkdir failed");
    return false;
  }
}

// SD card count # files in a directory with depth 1
int countFile(fs::FS &fs, const char *path)
{
  Serial.printf("Counting Dir: %s\n", path);
  int ret = 0;

  File root = fs.open(path);

  if (!root)
  {
    Serial.println("Failed to open directory");
    return -1;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return -1;
  }

  File file;
  while (file = root.openNextFile())
  {
    ret++;
  }
  return ret;
}

void setup()
{
  Serial.begin(115200);
  // while (!Serial)
    ; // When the serial monitor is turned on, the program starts to execute

  // set up camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  // config.frame_size = FRAMESIZE_UXGA;
  config.frame_size = FRAMESIZE_240X240;
  // config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.pixel_format = PIXFORMAT_RGB565;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG)
  {
    if (psramFound())
    {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else
    {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }
  else
  {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  camera_sign = true; // Camera initialization check passes

  // Display initialization
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  // Determine if the type of SD card is available
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  // create SD file structure
  if (!createDir(SD, "/inits") || !createDir(SD, "/results"))
  {
    return;
  }

  if ((imageCount = countFile(SD, "/inits")) == -1)
  {
    Serial.println("Error when counting images in /inits");
    return;
  }
  else
  {
    Serial.printf("Already %d images in /inits\n", imageCount);
  }

  sd_sign = true; // sd initialization check passes

  WiFiConnect();

  wifi_sign = true; // wifi initailization check passes

  if (camera_sign && sd_sign && wifi_sign)
  {
    Serial.println("DIY Instax Plus is ready for use! Enjoy it!");
  }
}

void main_page()
{
  return;
}

void results()
{
  return;
}

void inits()
{
  return;
}

void take_photo()
{
  // Take a photo
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Failed to get camera frame buffer");
    return;
  }
  // when button is pressed, save it in SD card.
  if (display_is_pressed())
  {
    Serial.println("Shutter is pressed");
    char filename[32];
    sprintf(filename, "/inits/%d.jpg", imageCount);

    photo_save(fb, filename);
  }

  //  images
  uint8_t* buf = fb->buf;
  uint32_t len = fb->len;
  tft.startWrite();
  tft.setAddrWindow(0, 0, camera_width, camera_height);
  tft.pushColors(buf, len);
  tft.endWrite();
      
  // Release image buffer
  esp_camera_fb_return(fb);

  delay(10);
}

void confirm_send()
{
  return;
}

void wait_server()
{
  return;
}

void confirm_print()
{
  return;
}

void loop()
{
  // Camera & SD available, start checking button state
  if (camera_sign && sd_sign && wifi_sign)
  {
    switch (state)
    {
    case MAIN_PAGE:
      main_page();
      break;
    case RESULTS:
      results();
      break;
    case INITS:
      inits();
      break;
    case TAKE_PHOTO:
      take_photo();
      break;
    case CONFIRM_SEND:
      confirm_send();
      break;
    case WAIT_SERVER:
      wait_server();
      break;
    case CONFIRM_PRINT:
      confirm_print();
      break;
    }
  }
}