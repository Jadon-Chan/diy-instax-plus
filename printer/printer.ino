/*
HPD482模块测试主函数v3.6
热敏打印机模块测试打印。上电后将打印5条虚线，打印小、中、大号英文字符串，以及打印小图片。
测试型号：Arduino UNO
测试步骤：
1.使用Arduino软件将本程序下载到Arduino UNO，并断开电源。
2.将HPD482板和Arduino UNO相连，具体接线如下：（接线引脚定义可以根据自己喜好，在程序HPD482.h里自定义，所用IO都应该是普通IO）
接线说明：
Arduino<--->HPD482
2<--------->MNB
3<--------->MB
4<--------->MNA
5<--------->MA
6<--------->LAT
7<--------->DI
8<--------->CLK
9<--------->STBA
10<-------->STBB
GND<------->GND（一定要接上）
3.将HPD482板和打印头连接，打印头装上热敏纸和胶辊。
4.先将12V电源插到HPD482板的DC供电口，再给Arduino UNO上电，将2次打印5条虚线，打印小、中、大号英文字符串，以及打印小图片。
5.去掉打印机模块时，应先断开arduino的电源，再断开12V适配器电源。

注意：
1.本程序没有对打印头进行温度检测，不要长时间频繁打印，防止高温损坏
2.详细介绍及函数说明参考“使用手册”
///////////////////////////////////////////////////////////////////////////////////////////////
作者：有立diy
淘宝店铺：有立diy
淘宝链接：https://shop587795831.taobao.com/
原创声明：本例程为热敏打印机模块HPD482配套例程，仅供参考，不得用于商业等用途，其他后果概不负责。
		  如需使用，注明出处。
///////////////////////////////////////////////////////////////////////////////////////////////
*/
#include "HPD482.h"//打印机头文件
#include "picture.h"//待打印的图片数据
#include "SoftwareSerial.h"

HPD482 printer=HPD482();//实例化打印机模块

SoftwareSerial mySerial(12, 13);

int state = 0;
// char * buff;
char buff[38*40];

char message_buff[20]; 
void setup() {
  //*********************以下部分调用打印函数开始打印**********************************//
  // printer.Print_Lines(5,2);//打印5条虚线，虚线间距2mm
  // printer.Motor_Run(24,0);//步进电机旋转进纸24/8=3mm
  // printer.Print_SStr((u8*)"HPD482 test.YOU LI diy.https:\n//shop587795831.taobao.com/",12,6);//打印12号英文字符串
  // printer.Print_SStr((u8*)"HPD482 test.YOU LI diy.https://shop587795831.taobao.com/",16,8);//打印16号英文字符串
  // printer.Print_SStr((u8*)"HPD482 test.YOU LI diy.https://shop587795831.taobao.com/",24,16);//打印24号英文字符串
  // printer.Print_SetDeep(70);//设置打印颜色深度（用于修改打印颜色深度，不需要每次都设置，初始化里已有，此句可不要）
  // printer.Print_Img2Lcd(0,(u8*)Image,1);//在坐标0位置打印图片
  // printer.Print_SetDeep(35);//设置打印颜色深度（用于修改打印颜色深度，不需要每次都设置，初始化里已有，此句可不要）
  // printer.Print_Img2Lcd(10,(u8*)Image,1);//在坐标为20位置打印图片
  // printer.Print_SetDeep(1);//设置打印颜色深度（用于修改打印颜色深度，不需要每次都设置，初始化里已有，此句可不要）
  // printer.Print_Img2Lcd(20,(u8*)Image,1);//在坐标为44位置打印图片（超出打印范围部分会被裁掉）
  //************************************打印结束**************************************//
  // for (int j = 0; j < 40; j++){
  //   for (int i = 0; i < 38; i++){
  //     if (i <= 16){
  //       buff[j*38+i] = '\xf0';
  //     }
  //     else if ( i >= 32){
  //       buff[j*38+i] = '\xff';
  //     }
  //     else{
  //       buff[j*38+i] = '\x0f';
  //     }
  //   }
  // }
  // printer.Print_Test(buffer);
  Serial.begin(115200);
  // while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  // }
  mySerial.begin(115200);
  // buff = (char *) malloc(40 * 400);
  Serial.println("Setup finished.");
}

int len = 0;
int message_len = 0;

void loop() {
    while (mySerial.available())
    {
        buff[len++] = mySerial.read();
        if (len >= 38*40 ){
          Serial.println("read a part");
          char mes[20];
          sprintf(mes, "len is %d", len);
          Serial.println(mes);
          Serial.println("Begin print");
          printer.Print_Picture(0, buff, 304, 40, 0);
          Serial.println("Swith to state 0");
          len = 0;
          state = 0;
        }
    }
}
