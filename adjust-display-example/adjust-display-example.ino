/*
 * ADJUSTABLE DISPLAY EXAMPLE FOR ARDUINO
 * The goal of this example is to make a display with text and graphic output, in which the display can be toggled by the user via a joystick.
 * The Arduino uses time and climate data for the output. The data type selected by the user for text display is indicated on the graph.
 * The user can also close the graph if they prefer to make the text bigger.
 * 
 * Basic use:
 * Push joystick up to check time
  * Push joystick down to check climate
  * Push joystick right to hide graphic
  * Push joystick left to show graphic
 */

/*
 * HARDWARE LIST
 * - Screen is a 1.3" White-pixel OLED with SH1106 chip from Sainsmart, currently out-of-stock with them;
 * needs a 5th pin for reset.
 * - Real-time clock with SD2403 chip from DFRobot. Doesn't have a dedicated library, but the devs brewed
 * up some code.
 * - DHT11 (temp/humidity sensor) from DFRobot. Using the Adafruit DHT library.
 * - Keyes-style PS2 joystick.
 * - Sainsmart MEGA2560 (Arduino-equivalent) dev board.
 * 
 * Comments that I created below are marked with j
 * Some math for the display grabbed from this other example: http://www.instructables.com/id/DS3231-OLED-clock-with-2-button-menu-setting-and-t/step4/step4/
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h> //j https://github.com/wonho-maker/Adafruit_SH1106
 #include <Joystick.h> //j https://github.com/fmilburn3/Joystick  Does math that significantly smooths out joystick operation
 #include <DHT.h> //j https://learn.adafruit.com/dht/downloads

#define OLED_RESET 4
Adafruit_SH1106 display(OLED_RESET);

//j Joystick
 const int xPin = A14;               // x direction potentiometer pin (analog)
 const int yPin = A15;               // y direction potentiometer pin (analog)
 const int pushPin = 53;            // Push button pin (digital)
 const int analogRes = 1023;       // Highest analog reading for microcontroller 

 // Instantiate the joystick
 Joystick joy(xPin, yPin, pushPin, analogRes);

//j DHT
#define DHTPIN 41  
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//j RTC SD2403
#define RTC_Address   0x32  //RTC_Address 
unsigned char   date[7];
/*j FYI this code is not smart enough to set the time yet; 
 * the time will just tick forward from it's default values.
 */

//j Timing trickery
/*
 * I have the variable comparisons/adjusts system included 
 * so the Arduino has room to think about things besides
 * using the Real-Time Clock.
 */

unsigned long prvRtcM = 1001;
long intRtcM = 1000;
unsigned long curTime = millis();

//j Variables to control screen modes
int toggle1 = 0;
int toggle2 = 0;

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  joy.begin();
  Serial.begin(9600);
  dht.begin();
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
      
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
 // put your main code here, to run repeatedly:

 I2CWriteDate();//Write the Real-time Clock
 delay(100);

 if(curTime - prvRtcM > intRtcM)
  {  
    /* j
    This "while loop" will totally mind-control your Arduino.
    Anything you want to do besides tell time should be inside the while loop.
    */
   while(1)
   {
    I2CReadDate();  //Read the Real-time Clock  
    Data_process();//Process the data    
    loopContents();
    display.display();
    display.clearDisplay();
   }
   prvRtcM=curTime;
  }
}


//RTC FUNCTIONS
//Read the Real-time data register of SD2403 
void I2CReadDate(void)
{
  unsigned char n=0;

  Wire.requestFrom(RTC_Address,7); 
  while(Wire.available())
  {  
    date[n++]=Wire.read();
  }
  delayMicroseconds(1);
  Wire.endTransmission();
}

//Write the Real-time data register of SD2403 
void I2CWriteDate(void)
{   
  WriteTimeOn();

  Wire.beginTransmission(RTC_Address);        
  Wire.write(byte(0));//Set the address for writing       
  Wire.write(0x59);//second:59     
  Wire.write(0x01);//minute:1      
  Wire.write(0x95);//hour:15:00(24-hour format)       
  Wire.write(0x03);//weekday:Wednesday      
  Wire.write(0x26);//day:26th      
  Wire.write(0x12);//month:December     
  Wire.write(0x12);//year:2012      
  Wire.endTransmission();

  Wire.beginTransmission(RTC_Address);      
  Wire.write(0x12);   //Set the address for writing       
  Wire.write(byte(0));            
  Wire.endTransmission(); 

  WriteTimeOff();   
}

//The program for allowing to write to SD2400
void WriteTimeOn(void)
{   
  Wire.beginTransmission(RTC_Address);       
  Wire.write(0x10);//Set the address for writing as 10H       
  Wire.write(0x80);//Set WRTC1=1      
  Wire.endTransmission();

  Wire.beginTransmission(RTC_Address);    
  Wire.write(0x0F);//Set the address for writing as OFH       
  Wire.write(0x84);//Set WRTC2=1,WRTC3=1      
  Wire.endTransmission();   
}

//The program for forbidding writing to SD2400
void WriteTimeOff(void)
{   
  Wire.beginTransmission(RTC_Address);   
  Wire.write(0x0F);   //Set the address for writing as OFH        
  Wire.write(byte(0));//Set WRTC2=0,WRTC3=0      
  Wire.write(byte(0));//Set WRTC1=0  
  Wire.endTransmission(); 
}

//Process the time_data
void Data_process(void)
{
 unsigned char i;

 for(i=0;i<7;i++)
 {
  if(i!=2)
      date[i]=(((date[i]&0xf0)>>4)*10)+(date[i]&0x0f);
    else
    {
     date[2]=(date[2]&0x7f);
     date[2]=(((date[2]&0xf0)>>4)*10)+(date[2]&0x0f);
    }
 }
}

void loopContents(void)
{
 //DHT
 // Wait a few seconds between measurements.
 // Reading temperature or humidity takes about 250 milliseconds!
 // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
 float h = dht.readHumidity();
 // Read temperature as Celsius (the default)
 float t = dht.readTemperature();
 // Read temperature as Fahrenheit (isFahrenheit = true)
 float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
 if (isnan(h) || isnan(t) || isnan(f)) 
 {
  Serial.println("Failed to read from DHT sensor!");
  return;
 }

 // Compute heat index in Fahrenheit (the default)
 float hif = dht.computeHeatIndex(f, h);
 // Compute heat index in Celsius (isFahreheit = false)
 float hic = dht.computeHeatIndex(t, h, false);

 /*j HINTS
  * Time info: date[0]=secs,[1]=mins,[2]=hrs,[3]=wks,[4]=day,[5]=mo,[6]=yr
  * joy.readX(), 515-1023 is left, 490-0 is right
  * joy.readY(), 515-1023 is up, 490-0 is down
  * joy.readPushButton(), 0 is pressed
  */

//xi and yi stand for "inner" (center) coordinates for the circular graph.
 int xi=(display.width()*0.75);
 int yi=(display.height()/2);
 int clockWidth=22;

 if(toggle2==0)
 {
  display.drawFastVLine(xi,yi-clockWidth,clockWidth,WHITE);

  //SECONDS
  //j The angle multiplier is 360/(max num of the range)
  //so in this case, 360/60 (as in seconds) = 6
  float     angle = date[0] * 6; //Retrieve stored minutes and apply
  angle=(angle/57.29577951) ; //Convert degrees to radians  
  int        xo=(xi+(sin(angle)*(clockWidth))); 
  int   yo=(yi-(cos(angle)*(clockWidth)));

  if(toggle1==0) //j These toggle1 statements will show empty/filled circles based on whether time or climate is selected
  {
   display.fillCircle(xo,yo,2,WHITE);
  }
  else
  {
   display.drawCircle(xo,yo,2,WHITE);
  }
  
  //MINUTES
  angle = date[1] * 6; //Retrieve stored minutes and apply
  angle=(angle/57.29577951) ; //Convert degrees to radians  
  xo=(xi+(sin(angle)*(clockWidth*0.8))); 
  yo=(yi-(cos(angle)*(clockWidth*0.8)));

  if(toggle1==0)
  {
   display.fillCircle(xo,yo,2,WHITE);
  }
  else
  {
   display.drawCircle(xo,yo,2,WHITE);
  }
                 
   //HOUR
   angle = date[2] * 30 + int((date[1] / 12) * 6);
   angle=(angle/57.29577951) ; //Convert degrees to radians  
   xo=(xi+(sin(angle)*(clockWidth*0.6))); 
   yo=(yi-(cos(angle)*(clockWidth*0.6)));
   //display.drawLine(xi,yi,xo,yo,WHITE);
   if(toggle1==0)
   {
    display.fillCircle(xo,yo,2,WHITE);
   }
   else
   {
    display.drawCircle(xo,yo,2,WHITE);
   }

   display.drawCircle(xi,yi,clockWidth*0.5,WHITE); //j This circle separates the time and climate indicators
  
   //TEMP
   /*j Based on Adafruit description, 
    * DHT11 temp range is 0-50C (translates to 32-122F)
    * So like above, 360/90=4, and use that as the multiplier
    */
   angle = f * 4;
   angle=(angle/57.29577951) ; //Convert degrees to radians  
   xo=(xi+(sin(angle)*(clockWidth*0.4))); 
   yo=(yi-(cos(angle)*(clockWidth*0.4)));
   //display.drawLine(xi,yi,xo,yo,WHITE);
   if(toggle1==1)
   {
    display.fillCircle(xo,yo,2,WHITE);
   }
   else
   {
    display.drawCircle(xo,yo,2,WHITE);
   }

   //HUMIDITY
   //j Based on Adafruit description, DHT11 humidity range is 20-80%
   angle = h * 3.6;
   angle=(angle/57.29577951) ; //Convert degrees to radians  
   xo=(xi+(sin(angle)*(clockWidth*0.2))); 
   yo=(yi-(cos(angle)*(clockWidth*0.2)));

   if(toggle1==1)
   {
    display.fillCircle(xo,yo,2,WHITE);
   }
   else
   {
    display.drawCircle(xo,yo,2,WHITE);
   }
 }

 display.setCursor(1,1);
 if(toggle2==0)
 {
  display.setTextSize(1);
 }
 else
 {
  display.setTextSize(2);
 }
 
 display.setTextColor(WHITE);

 if(toggle1==0)
 {
  display.println("-TIME-");
  display.print(date[5]); //j month
  display.print("/"); 
  display.print(date[4]); //day
  display.print("/");
  display.println(date[6]); //year
  if(date[2]>12) //j this section restricts display to 12-hour mode
  {
   display.print(date[2]-12);
  }
  else
  {
   display.print(date[2]);
  }
  display.print(":");
  display.print(date[1]); //j minutes
  display.print(":");
  display.print(date[0]); //seconds
  if(date[2]>12)
  {
   display.print("PM"); //dark
  }
  else
  {
   display.print("AM"); //bright
  }
 }

 if(toggle1==1)
 {
   display.println("-CLIMATE-");
   display.print("Temp:");
   display.print(int(f));
   display.println("F");
   display.print("Humid:");
   display.print(int(h));
   display.println("%");
 }

 //j HINTS
 //joy.readX(), 515-1023 is left, 490-0 is right
 //joy.readY(), 515-1023 is up, 490-0 is down
 //joy.readPushButton(), 0 is pressed

 /*j Adjust the selection highlight: time or climate
  * Push joystick up to check time
  * Push joystick down to check climate
  * Push joystick right to hide graphic
  * Push joystick left to show graphic
  */
  
 if(joy.readY()>515)
 {
   if(toggle1==1)
   {
    toggle1=0;
   }
 }

 if(joy.readY()<490)
 {
  if(toggle1==0)
  {
   toggle1=1;
  }
 }

 if(joy.readX()<490)
 {
  if(toggle2==0)
  {
   toggle2=1;
  }
 }
 
 if(joy.readX()>515)
 {
  if(toggle2==1)
  {
   toggle2=0;
  }
 }
}
