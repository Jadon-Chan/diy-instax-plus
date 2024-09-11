#define USE_TFT_ESPI_LIBRARY

#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "WiFi.h"
#include <TFT_eSPI.h>
#include "HTTPClient.h"
#include <lvgl.h>
#include "lv_xiao_round_screen.h"
#include <malloc.h>
#include <TJpg_Decoder.h>
#include <SoftwareSerial.h>

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#include "camera_pins.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

#define SD_CS_PIN 21 // ESP32S3 Sense SD Card Reader
// #define SD_CS_PIN D2 // XIAO Round Display SD Card Reader
#define CHSC6X_I2C_ID 0x2e
#define CHSC6X_READ_POINT_LEN 5
#define TOUCH_INT D7

#define DIY_RX 3
#define DIY_TX 1

#define BUTTON_WIDTH_LARGE 80

#define BUTTON_WIDTH_SMALL 40

#define BUTTON_HEIGHT 40

#define CENTER 120

#define BAR_1 60

#define BAR_2 120

#define BAR_3 180

#define minimum(a, b) (((a) < (b)) ? (a) : (b))

typedef enum
{
  MAIN_PAGE,
  RESULTS_INIT,
  RESULTS_LISTEN,
  INITS_INIT,
  INITS_LISTEN,
  TAKE_PHOTO,
  CONFIRM_SEND_INIT,
  CONFIRM_SEND_LISTEN,
  WAIT_SERVER,
  CONFIRM_PRINT,
  SEND_BIN
} state_t;

SoftwareSerial DIYSerial(DIY_RX, DIY_TX, false, 256);

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
const char img2img_server[] = "Set this as your server address and port";
String serverName = "Set this as your server address";
uint16_t serverPort = 8000; // Change to your server port
WiFiClient client;

int curr_num_inits = 0;
int curr_num_results = 0;

state_t state = MAIN_PAGE;
state_t last_state = INITS_INIT;
int request_id = -1;

lv_obj_t *shot_btn;
lv_obj_t *shot_label;
lv_obj_t *inits_btn;
lv_obj_t *inits_label;
lv_obj_t *results_btn;
lv_obj_t *results_label;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height())
    return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}

uint32_t change_size(uint8_t *buf, uint32_t len, uint8_t *new_buf)
{
  for (int x = 60; x < 1020; x += 4)
  {
    for (int y = 480; y < 1440; y += 4)
    {
      uint32_t r = 0, g = 0, b = 0;
      for (int i = 0; i < 4; i++)
      {
        for (int j = 0; j < 4; j++)
        {
          r += buf[(4 * y + j) * 1080 * 2 + (4 * x + i) * 2] >> 3;
          g += ((buf[(4 * y + j) * 1080 * 2 + (4 * x + i) * 2] & 0b111) << 3) + (buf[(4 * y + j) * 1080 * 2 + (4 * x + i) * 2] >> 5);
          b += buf[(4 * y + j) * 1080 * 2 + (4 * x + i) * 2 + 1] & 0b11111;
        }
      }
      r /= 16;
      g /= 16;
      b /= 16;
      new_buf[y * 240 * 2 + x * 2] = (((uint8_t)r) << 3) + (((uint8_t)g & 0b111000) >> 5);
      new_buf[y * 240 * 2 + x * 2 + 1] = ((uint8_t)b) + (((uint8_t)g & 0b111) << 5);
    }
  }
  return 240 * 240 * 2;
}

// avail API of img2img_server
bool avail()
{
  int httpCode;
  Serial.println("Sending GET to avail API");
  http.setReuse(true);
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
int generate(int num)
{
  // prepare httpclient
  Serial.println("Starting connection to server...");
  Serial.println(serverName.c_str());

  char fileName[20];
  sprintf(fileName, "/inits/%d.jpg", num);

  char shortName[20];
  sprintf(shortName, "%d.jpg", num);

  uint8_t *fb = NULL;
  fb = (uint8_t *)malloc(240 * 240 * 2);
  uint32_t imageLen = readFile(SD, fileName, fb);

  String getAll;
  String getBody;

  String serverPath = "/generate";

  Serial.println("Connecting to server: " + serverName);

  if (client.connect(serverName.c_str(), serverPort))
  {
    Serial.println("Connection successful!");
    char head_c_str[200];
    sprintf(head_c_str, "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: image/jpeg\r\n\r\n", shortName);
    String head = head_c_str;
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    Serial.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    Serial.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    Serial.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    Serial.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    Serial.println();
    client.print(head);
    Serial.print(head);

    uint8_t *fbBuf = fb;
    size_t fbLen = imageLen;
    for (size_t n = 0; n < fbLen; n = n + 1024)
    {
      if (n + 1024 < fbLen)
      {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0)
      {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);
    Serial.print(tail);

    int timoutTimer = 20000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + timoutTimer) > millis())
    {
      Serial.print(".");
      delay(100);
      while (client.available())
      {
        char c = client.read();
        if (c == '\n')
        {
          if (getAll.length() == 0)
          {
            state = true;
          }
          getAll = "";
        }
        else if (c != '\r')
        {
          getAll += String(c);
        }
        if (state == true)
        {
          getBody += String(c);
        }
        startTimer = millis();
      }
      if (getBody.length() > 0)
      {
        break;
      }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);
    int dataStart = getBody.indexOf("{\"id\":") + strlen("{\"id\":");
    int dataEnd = getBody.indexOf("}");
    String id_str = getBody.substring(dataStart, dataEnd);
    int id = id_str.toInt();
    request_id = id;
  }
  else
  {
    getBody = "Connection to " + serverName + " failed.";
    Serial.println(getBody);
  }
  free(fb);
  // return getBody;
  return 0;
}

bool check(int image_id)
{
  int httpCode;
  Serial.println("Sending GET to check API");
  Serial.printf("id = %d\n", image_id);
  http.setReuse(true);
  char query[25];
  sprintf(query, "/check?id=%d", image_id);
  String query_str = query;
  if (http.begin(String(img2img_server) + query_str))
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
    int dataStart = payload.indexOf("{\"finished\":") + strlen("{\"finished\":");
    int dataEnd = payload.indexOf("}");
    String finished = payload.substring(dataStart, dataEnd);
    Serial.printf("Call check succeed! Finished status is %s\n", finished);
    if (finished == "true")
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
    Serial.printf("Call check failed... return code: %d\n", httpCode);
    return false;
  }
}

bool fetch_bin(int id)
{
  char fileName[20];
  sprintf(fileName, "/results/%d.bin", id);
  int httpCode;
  Serial.println("Sending GET to avail API");
  http.setReuse(true);
  char path[25];
  sprintf(path, "/fetch_bin?id=%d", id);
  if (http.begin(String(img2img_server) + path))
  {
    httpCode = http.GET();
  }
  else
  {
    Serial.println("[HTTP] Unable to connect");
    delay(1000);
  }

  if (httpCode == HTTP_CODE_OK)
  {
    int len = http.getSize();

    char* buff = (char*) malloc(40*400);
    size_t index = 0;

    // get tcp stream
    WiFiClient * stream = http.getStreamPtr();

    Serial.println("Before read loop");
    // read all data from server
    while(http.connected() && (len > 0 || len == -1)) {
      // get available data size
      size_t size = stream->available();

      if(size) {
          // read up to 128 byte
          int c = stream->readBytes(buff+index, size);

          if(len > 0) {
              len -= c;
          }
          index += size;
      }
      delay(1);
    }

    Serial.println("Save to SD..");
    writeFile(SD, fileName, (uint8_t*)buff, index);
    Serial.println("Finishing writing!");
    free(buff);
    return true;
  }
  else
  {
    Serial.printf("Call avail failed... return code: %d\n", httpCode);
    return false;
  }
  // // prepare httpclient
  // Serial.println("Starting connection to server...");
  // Serial.println(serverName.c_str());

  // char fileName[20];
  // sprintf(fileName, "/results/%d.bin", id);

  // char shortName[20];
  // sprintf(shortName, "%d.png", id);

  // uint8_t *file_buffer = NULL;
  // file_buffer = (uint8_t *)malloc(40 * 400);

  // String getAll;
  // String getBody;
  // char path[20];
  // sprintf(path, "/fetch_bin?id=%d", id);
  // String serverPath = String(path);

  // Serial.println("Connecting to server: " + serverName);

  // if (client.connect(serverName.c_str(), serverPort))
  // {
  //   Serial.println("Connection successful!");

  //   client.println("GET " + serverPath + " HTTP/1.1");
  //   Serial.println("GET " + serverPath + " HTTP/1.1");
  //   client.println("Host: " + serverName);
  //   Serial.println("Host: " + serverName);
  //   client.println("Accept: application/json");
  //   Serial.println("Accept: application/json");
  //   client.println();
  //   Serial.println();

  //   int timoutTimer = 20000;
  //   long startTimer = millis();
  //   boolean state = false;

  //   size_t file_len = 0;

  //   while ((startTimer + timoutTimer) > millis())
  //   {
  //     Serial.print(".");
  //     delay(100);
  //     while (client.available())
  //     {
  //       char c = client.read();
  //       file_buffer[file_len++] = c;
  //       startTimer = millis();
  //     }
  //   }
  //   Serial.println();
  //   client.stop();
  //   writeFile(SD, fileName, file_buffer, file_len);
  //   return true;
  // }
  // else
  // {
  //   getBody = "Connection to " + serverName + " failed.";
  //   Serial.println(getBody);
  //   return false;
  // }
}

bool display_is_pressed(void)
{
  if (digitalRead(TOUCH_INT) != LOW)
  {
    delay(3);
    if (digitalRead(TOUCH_INT) != LOW)
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
  esp_err_t ret = frame2jpg(fb, 63, &out_buf, &out_len);
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
int32_t readFile(fs::FS &fs, const char *path, uint8_t *data)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return -1;
  }

  Serial.print("Read from file: ");
  int32_t index = 0;
  while (file.available())
  {
    data[index++] = file.read();
  }
  file.close();

  return index;
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

camera_config_t config;

void setup()
{
  Serial.begin(115200);
  DIYSerial.begin(115200);
  if (!DIYSerial) { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid EspSoftwareSerial pin configuration, check config"); 
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  } 
  // while (!Serial)
  ; // When the serial monitor is turned on, the program starts to execute

  // set up camera
  // camera_config_t config;
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
  // config.frame_size = FRAMESIZE_P_FHD; // 1080x1920
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
  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

  // Jpeg decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

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

static void shot_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    Serial.println("SHOT button is pressed");
    state = TAKE_PHOTO;
  }
}

static void results_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    Serial.println("RESULTS button is pressed");
    state = RESULTS_INIT;
  }
}

static void inits_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    Serial.println("INITS button is pressed");
    state = INITS_INIT;
  }
}

void main_page_init()
{
  // Display initialization
  lv_obj_t *scr = lv_obj_create(NULL);

  Serial.println("Entering main_page_init");
  // shot button
  shot_btn = lv_btn_create(scr); /*Add a button the current screen*/
  // lv_obj_set_pos(shot_btn, CENTER - BUTTON_WIDTH_LARGE / 2, BAR_1 - BUTTON_HEIGHT / 2); /*Set its position*/
  lv_obj_set_pos(shot_btn, CENTER - BUTTON_WIDTH_LARGE / 2, CENTER - BUTTON_HEIGHT / 2); /*Set its position*/
  lv_obj_set_size(shot_btn, BUTTON_WIDTH_LARGE, BUTTON_HEIGHT);                          /*Set its size*/
  lv_obj_add_event_cb(shot_btn, shot_btn_event_cb, LV_EVENT_ALL, NULL);                  /*Assign a callback to the button*/

  shot_label = lv_label_create(shot_btn); /*Add a label to the button*/
  lv_label_set_text(shot_label, "SHOT");  /*Set the labels text*/
  lv_obj_center(shot_label);

  // results button
  // results_btn = lv_btn_create(scr);                                                        /*Add a button the current screen*/
  // lv_obj_set_pos(results_btn, CENTER - BUTTON_WIDTH_LARGE / 2, BAR_2 - BUTTON_HEIGHT / 2); /*Set its position*/
  // lv_obj_set_size(results_btn, BUTTON_WIDTH_LARGE, BUTTON_HEIGHT);                         /*Set its size*/
  // lv_obj_add_event_cb(results_btn, results_btn_event_cb, LV_EVENT_ALL, NULL);              /*Assign a callback to the button*/

  // results_label = lv_label_create(results_btn); /*Add a label to the button*/
  // lv_label_set_text(results_label, "RESULTS");  /*Set the labels text*/
  // lv_obj_center(results_label);

  // inits button
  // inits_btn = lv_btn_create(scr);                                                        /*Add a button the current screen*/
  // lv_obj_set_pos(inits_btn, CENTER - BUTTON_WIDTH_LARGE / 2, BAR_3 - BUTTON_HEIGHT / 2); /*Set its position*/
  // lv_obj_set_size(inits_btn, BUTTON_WIDTH_LARGE, BUTTON_HEIGHT);                         /*Set its size*/
  // lv_obj_add_event_cb(inits_btn, inits_btn_event_cb, LV_EVENT_ALL, NULL);                /*Assign a callback to the button*/

  // inits_label = lv_label_create(inits_btn); /*Add a label to the button*/
  // lv_label_set_text(inits_label, "INITS");  /*Set the labels text*/
  // lv_obj_center(inits_label);

  tft.setRotation(3);

  lv_scr_load(scr);
  state = MAIN_PAGE;
}

void main_page_listen()
{
  lv_timer_handler();
  delay(5);
}

void results_init()
{
  //  lv_obj_clean(lv_scr_act());
  lv_obj_t *scr_tmp = lv_obj_create(NULL);
  lv_scr_load(scr_tmp);
  // show_image(curr_num_results, false);

  state = RESULTS_LISTEN;
}

void results_listen()
{
  if (chsc6x_is_pressed())
  {
    lv_indev_data_t data;
    chsc6x_read(&indev_drv, &data);
    if (data.point.x <= SCREEN_WIDTH / 3)
    {
      Serial.println("Left button is pressed");
      state = RESULTS_INIT;
      if (curr_num_results == 0)
      {
        curr_num_results = imageCount - 1;
      }
      else
      {
        curr_num_results--;
      }
    }
    else if (data.point.x >= SCREEN_WIDTH / 3 * 2)
    {
      Serial.println("Right button is pressed");
      state = RESULTS_INIT;
      if (curr_num_results == imageCount - 1)
      {
        curr_num_results = 0;
      }
      else
      {
        curr_num_results++;
      }
    }
    else
    {
      Serial.println("Back to main page");
      state = MAIN_PAGE;
    }
  }
  delay(5);
}

void results()
{
  return;
}

void show_image(uint32_t num, bool inits)
{
  //  images
  char fileName[20];
  if (inits)
  {
    Serial.println("Enter inits_image");
    sprintf(fileName, "/inits/%d.jpg", num);
  }
  else
  {
    Serial.println("Enter results_image");
    sprintf(fileName, "/results/%d.jpg", num);
  }

  tft.setSwapBytes(true);

  uint16_t w = 0, h = 0;
  uint32_t t = millis();
  TJpgDec.getSdJpgSize(&w, &h, fileName);
  Serial.print("Width = ");
  Serial.print(w);
  Serial.print(", height = ");
  Serial.println(h);

  // Draw the image, top left at 0,0
  TJpgDec.drawSdJpg(0, 0, fileName);

  // How much time did rendering take
  t = millis() - t;
  Serial.print(t);
  Serial.println(" ms");

  tft.setSwapBytes(false);
  // Wait before drawing again
  delay(100);
}

static void left_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    Serial.println("Left button is pressed");
    state = INITS_INIT;
    if (curr_num_inits == 0)
    {
      curr_num_inits = imageCount - 1;
    }
    else
    {
      curr_num_inits--;
    }
  }
}

static void right_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    Serial.println("Right button is pressed");
    state = INITS_INIT;
    if (curr_num_inits == imageCount - 1)
    {
      curr_num_inits = 0;
    }
    else
    {
      curr_num_inits++;
    }
  }
}

void inits_init()
{
  lv_obj_t *scr_tmp = lv_obj_create(NULL);
  lv_scr_load(scr_tmp);

  // lv_obj_clean(lv_scr_act());
  show_image(curr_num_inits, true);

  state = INITS_LISTEN;
}

void inits_listen()
{
  if (chsc6x_is_pressed())
  {
    lv_indev_data_t data;
    chsc6x_read(&indev_drv, &data);
    if (data.point.x <= SCREEN_WIDTH / 3)
    {
      Serial.println("Left button is pressed");
      state = INITS_INIT;
      if (curr_num_inits == 0)
      {
        curr_num_inits = imageCount - 1;
      }
      else
      {
        curr_num_inits--;
      }
    }
    else if (data.point.x >= SCREEN_WIDTH / 3 * 2)
    {
      Serial.println("Right button is pressed");
      state = INITS_INIT;
      if (curr_num_inits == imageCount - 1)
      {
        curr_num_inits = 0;
      }
      else
      {
        curr_num_inits++;
      }
    }
    else
    {
      state = MAIN_PAGE;
      Serial.println("Back to main page");
    }
  }
  delay(5);
}

bool curr_pressed = false;
bool prev_pressed = false;

void take_photo()
{
  tft.setRotation(1);
  // when button is pressed, save it in SD card.
  curr_pressed = display_is_pressed();
  if (curr_pressed && !prev_pressed)
  {
    Serial.println("Screen is touched");
    // reconfig camera
    config.frame_size = FRAMESIZE_P_HD;

    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK)
    {
      Serial.printf("Camera deinit failed with error 0x%x, when turning to high res", err);
      return;
    }

    err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
      Serial.printf("Camera reconfig failed with error 0x%x, when turning to high res", err);
      return;
    }

    // Take a photo
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Failed to get camera frame buffer");
      return;
    }

    state = CONFIRM_SEND_LISTEN;
    Serial.println("Shutter is pressed");
    char filename[32];
    sprintf(filename, "/inits/%d.jpg", imageCount);
    photo_save(fb, filename);

    // reconfig camera back
    config.frame_size = FRAMESIZE_240X240; // 1080x1920

    err = esp_camera_deinit();
    if (err != ESP_OK)
    {
      Serial.printf("Camera deinit failed with error 0x%x, when turning back into low res", err);
      return;
    }
    err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
      Serial.printf("Camera reconfig failed with error 0x%x, when turning back into low res", err);
      return;
    }
  }
  else
  {
    // Take a photo
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Failed to get camera frame buffer");
      return;
    }

    //  images
    uint8_t *buf = fb->buf;
    uint32_t len = fb->len;
    // uint32_t new_len;
    // uint8_t *new_buf = (uint8_t*) malloc(240 * 240 * 2 + 5);
    // new_len = change_size(buf, len, new_buf);

    tft.startWrite();
    tft.setAddrWindow(0, 0, camera_width, camera_height);
    tft.pushColors(buf, len);
    tft.endWrite();

    // Release image buffer
    esp_camera_fb_return(fb);
  }
  prev_pressed = curr_pressed;

  tft.setRotation(3);

  delay(10);
}

void confirm_send_init()
{
  show_image(imageCount - 1, true);
  state = CONFIRM_SEND_LISTEN;
  delay(10);
}

void confirm_send_listen()
{
  if (chsc6x_is_pressed())
  {
    lv_indev_data_t data;
    chsc6x_read(&indev_drv, &data);
    if (data.point.x <= SCREEN_WIDTH / 2)
    {
      char fileName[20];
      sprintf(fileName, "/inits/%d.jpg", imageCount - 1);
      if (!SD.remove(fileName))
      {
        Serial.println("Remove last photo failed");
      }
      imageCount--;
      state = TAKE_PHOTO;
    }
    else
    {
      bool ret = avail();
      if (ret)
      {
        Serial.println("GPU device is available");
      }
      else
      {
        Serial.println("GPU device is not available");
      }
      generate(imageCount - 1);

      state = WAIT_SERVER;
    }
  }
  delay(5);
}

void wait_server()
{
  if (check(request_id)){
    Serial.println("Finished!");
    fetch_bin(request_id);
    state = SEND_BIN;
  }
  else{
    Serial.println("Not finished yet!");
  }
  delay(3000);
}

void send_bin(int id)
{
  char path[25];
  sprintf(path, "/results/%d.bin", id);
  uint8_t* buff = (uint8_t*) malloc(40 * 400);
  size_t len = readFile(SD, path, buff);
  Serial.printf("Writing to UNO len is %d\n", len);
  for (int i = 0; i < 10; i++){
    Serial.println("Write One Part");
    DIYSerial.write(buff+i*1520, 1520);
    delay(10000);
  }
  free(buff);
  state = TAKE_PHOTO;
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
      if (last_state != MAIN_PAGE)
      {
        main_page_init();
      }
      else
      {
        main_page_listen();
      }
      break;
    case RESULTS_INIT:
      results_init();
      break;
    case RESULTS_LISTEN:
      results_listen();
      break;
    case INITS_INIT:
      inits_init();
      break;
    case INITS_LISTEN:
      inits_listen();
      break;
    case TAKE_PHOTO:
      take_photo();
      break;
    case CONFIRM_SEND_INIT:
      confirm_send_init();
      break;
    case CONFIRM_SEND_LISTEN:
      confirm_send_listen();
      break;
    case WAIT_SERVER:
      wait_server();
      break;
    case CONFIRM_PRINT:
      confirm_print();
      break;
    case SEND_BIN:
      send_bin(request_id);
      break;
    }
    last_state = state;
  }
  delay(10);
}
