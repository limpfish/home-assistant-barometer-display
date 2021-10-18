/*
 *  Displays temperature/pressure readings from MQTT
 * 
 * 
 *  The D1 Mini:
 *  info :  https://www.az-delivery.uk/collections/mikrocontroller/products/d1-mini-v3
 * 
 *  Libraries used : 
 * 
 *  wifimanager by tzapu
 *  PubSubClient by Nick O'Leary
 *  ArduinoJson by Benoit Blanchon
 *  Adafruit GFX library
 *  Adafruit ST7735 tft library
 * 
 * Setting up Wifi:
 * On first run it will create a Wifi Access Point called 'ESP Setup', connect to that from a phone to setup wifi and password
 * It will save and use this next time it runs.
 * 
 */
 

// PINS :

#define BUTTON    0

// TFT: TFT uses hardware SPI; SCK: pin D5/14, MOSI: pin D7/13, 
#define TFT_CS    15
#define TFT_RST   4
#define TFT_DC    16  

#define MOTION     2

// CONTROL TFT BACKLIGHT :
#define TFT_SCREEN 5


// THE HOME ASSISTANT SENSORS TO READ

 char* mqtt_sub_topics[4][3] = 
 {
    // See also : mqtt_handle_result function 
    //name            // mqtt topic           // unit (unused here)
    { "Outside",         "shed/info",    ""},
    { "Living Room",     "tele/sonoff5/SENSOR",    ""},
    { "Attic",           "tele/sonoff4/SENSOR",    ""},
    // 4th must be pressure
    { "Pressure",        "tele/sonoff1/SENSOR",    ""}
 };


// mqtt
const char* mqtt_server = "192.168.0.160"; 
const char* mqtt_user = "DVES_USER";
const char* mqtt_pass = "DVES_PASS";


#include "barometer_gfx.c"

#include <WiFiManager.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/Picopixel.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <SPI.h>


StaticJsonDocument<256> doc;

char values[20][20];
bool mqtt_found=false;
int mqtt_total_topics = 4;


int min_value = 99999;  
int max_value = 0;



unsigned long currentMillis;
unsigned long previousMillis = 1000 * 60 * 11;
const long interval = 1000 * 60 * 10;  // ten minutes

unsigned long button_pressed_timer =0;
unsigned long button_pressed_start =0;

unsigned long screen_off_countdown, screen_off_countdown_start;
unsigned long screen_off_interval = 1000 * 60 * 2; //5 minutes before screen off.
unsigned long milli_seconds_elapsed;
unsigned long prev_milli_seconds_elapsed = 0;
/*
 * 
 *  150 entries to plot. The screen is 160 wide so will fit with 5 pixels either side. 
 *  150 / 25 =  6, 6 pixels every hour. 25 hours worth shown on screen.
 *  
 */
#define total_plot 150

int plot_values[total_plot];
int plot_index_tail = total_plot-1;
int plot_index_head = 0;
int time_for_reading = 0;
int pressure_val = 0;
int pressure_val_o = 0;

bool movement = true;
bool screen_on = true;


bool button_status, last_button_status;

int needleval = 0; // 0 to 120; //100;  // 160 points, 80 is dead bottom. 100 is lowest tick
int oldneedleval = 0;
int displaymode = 0;

WiFiClient espClient;
PubSubClient client(espClient);

//serial read
const int BUFF_SIZE = 128;
static char buffer[BUFF_SIZE+1];
static int length = 0;


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

int connected_ok = 0;


// floating point map function
float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/*
 * Called when new MQTT data, extracts data and stores in values array.
 */
void mqtt_handle_result(int i)
{
      float tmpval, corrected_val; 
      const char* tmpstr; 

switch(i)
{
  case 0: // Outside
    tmpval  = doc["shed"]["outsidetemp"];
    dtostrf(tmpval, 6, 1, values[i]); 
    break;

  case 1:   // Living Room
    //incoming value from sensors sending to Home Assistant.
    tmpval  = doc["DS18B20"]["Temperature"];
    dtostrf(tmpval, 6, 1, values[i]);
  break;

  case 2: // Attic
    tmpval  = doc["SI7021"]["Temperature"];
    dtostrf(tmpval, 6, 1, values[i]);
    break;

  case 3: // Pressure
    tmpval  = doc["BMP180"]["Pressure"];      
    pressure_val = tmpval * 10;
    dtostrf(tmpval, 6, 1, values[i]);
    break; 
}


}

/*
 * When an MQTT message is received from one of the subscribed topics this function is callled.
 * parses JSON data etc.
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {

  char inData[160];
  
  Serial.println();
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.println("payload: ");
  for(int i =0; i<length; i++)
  {
    Serial.print((char)payload[i]);
    inData[i] = (char)payload[i];
  }
  Serial.println();
  deserializeJson(doc, payload, length);

  // Is this in our subscribed topics list?
  for (byte i=0;i<mqtt_total_topics;i++) 
  {
    // In the list? 
    if (strcmp(topic,mqtt_sub_topics[i][1])==0)
    {
      // Flag that we've got something new!
      mqtt_found=true;
      Serial.println(mqtt_sub_topics[i][0]);
      // Do something with the new data...
      mqtt_handle_result(i);
    }
  }
  Serial.println("end of payload");
  displaymode%=2;

}

/*
 * Connects to the MQTT server and subscribes to MQTT topics.
 */
void mqtt_connect()
{
   //connect to MQTT
  int failed = 0;
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);
  // loop until we're connected.
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ArduinoClient2", mqtt_user, mqtt_pass)) 
    {
      Serial.println("MQTT connected");
        if (connected_ok!=1)
        {
        tft.setCursor(0, 33);
        tft.print("Connected to MQTT");
        tft.setCursor(0, 50);
        tft.print("Waiting for data...");
        connected_ok = 1;
        }
    } 
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      failed++;
      if (failed>5)
      {
        tft.setCursor(0, 33);
        tft.print("MQTT failed");
      }
    }
  }
  // subscribe to topics
  Serial.println();
  Serial.println("Subscribing to topics:");
  char* previous = "x";
  for(byte i=0;i<mqtt_total_topics;i++)
  {
     Serial.print(mqtt_sub_topics[i][0]);
     Serial.print("  ");
     //Serial.print(previous);
      
    // avoid subscribing to same topic twice, assuming we're listing the topics together in array
    if (strcmp(previous,mqtt_sub_topics[i][1])!=0)
    {
     
      Serial.println(mqtt_sub_topics[i][1]);
      client.subscribe(mqtt_sub_topics[i][1]);
      previous = mqtt_sub_topics[i][1];
    }
    else
    {
      Serial.print("already subscribed to ");
      Serial.println(mqtt_sub_topics[i][1]);
    }
  }

 Serial.println();
}


// For debugging the pressure data I added these routines to read from serial.
// Enter DATA in the serial console and it dumps the array.
void readSerial()
{
  // check anything on serial...
  while(Serial.available() > 0 ) 
  {
    char c = Serial.read();
    if((c == '\r') || (c == '\n')) // eol
    {
      if(length > 0)
      {
        handleReceivedMessage(buffer);
      }
      length=0;
      
    }
    else
    {
      if(length < BUFF_SIZE)
      {
        buffer[length++] = c; // append the received character to the array
        buffer[length] = 0; // terminate string 
      }
      else
      {
        // buffer full
      
      }
    
    }
  }  // end of serial read
}

// Entered DATA, Dump array
void handleReceivedMessage(char *msg)
{
  char tmp[128];

  if(strncmp(msg, "DATA",10) == 0)
  {
    
Serial.println();
Serial.println("-----------");
Serial.println("Graph data");
Serial.print("Maximum value +1 : ");
Serial.println(max_value);
Serial.print("Minimum value -1 : ");
Serial.println(min_value);

Serial.print("plot index tail : ");
Serial.print(plot_index_tail);
Serial.println(" (latest value inserted here)");

Serial.print("plot index head : ");
Serial.print(plot_index_head);
Serial.println(" (plot start from here)");

Serial.print("Needle value : ");
Serial.println((needleval+940) *10);
Serial.print("Old needle value : ");
Serial.println((oldneedleval+940) * 10);


int drawindex;
 for (int x = 0; x<total_plot; x++)  // total_plot is 150
  {
  // plot_index_head
    drawindex = (plot_index_head + x) % total_plot;
    if (x<10) Serial.print(" ");
    if (x<100) Serial.print(" ");
    Serial.print(x);
    Serial.print("/");
    if (drawindex<10) Serial.print(" ");
    if (drawindex<100) Serial.print(" ");
    Serial.print(drawindex);
    Serial.print(":");
    Serial.print( plot_values[drawindex]);
    Serial.print(", ");
    if (((x+1)%10)==0) Serial.println();
  }
  Serial.println();
  Serial.println("-----------");
  }
  
}



/* 
 *  DRAW NEEDLE
 *  
 */

void drawNeedle(int val, int oldval)
{
  int centrey=63;
  int centrex=79;
  int x,y,x2,y2,ox,oy,ox2,oy2;
  float pi = 3.14159267;

  int tx,ty,tx2,ty2,tx3,ty3,cx,cy, otx,oty,otx2,oty2,otx3,oty3;
  
  int needlelength = 38;
  
  val-=60;
  oldval -=60;
  //y= (needlelength * cos(pi-(2*pi)/60* val ))+centrey;
  //x =(needlelength * sin(pi-(2*pi)/60* val ))+centrex;
  // point to tick


  // main needle coords
  y= (needlelength * cos(pi-(2*pi)/ 160* val ))+centrey;
  x =(needlelength * sin(pi-(2*pi)/ 160* val ))+centrex;
  // through centre...
  y2= ( ( - 22) * cos(pi-(2*pi)/ 160* val ))+centrey;
  x2 =( (  - 22) * sin(pi-(2*pi)/ 160* val ))+centrex;

  // thicker triangle needle main coords
  ty= (needlelength * cos(pi-(2*pi)/ 160* val ))+centrey;
  tx =(needlelength * sin(pi-(2*pi)/ 160* val ))+centrex;
  ty2= (-23 * cos(pi-(2*pi)/ 160* (val-1) ))+centrey;
  tx2 =(-23 * sin(pi-(2*pi)/ 160* (val-1) ))+centrex;
  ty3= (-23 * cos(pi-(2*pi)/ 160* (val+1) ))+centrey;
  tx3 =(-23 * sin(pi-(2*pi)/ 160* (val+1) ))+centrex;

  

  // old value needle coords
  oy= (needlelength * cos(pi-(2*pi)/ 160* oldval ))+centrey;
  ox =(needlelength * sin(pi-(2*pi)/ 160* oldval ))+centrex;
  // through centre...
  oy2= (( - 13) * cos(pi-(2*pi)/ 160* oldval ))+centrey;
  ox2 =(( - 13) * sin(pi-(2*pi)/ 160* oldval ))+centrex;

  // thicker triangle needle main coords
  oty= (needlelength * cos(pi-(2*pi)/ 160* oldval ))+centrey;
  otx =(needlelength * sin(pi-(2*pi)/ 160* oldval ))+centrex;
  oty2= (-13 * cos(pi-(2*pi)/ 160* (oldval-0.9) ))+centrey;
  otx2 =(-13 * sin(pi-(2*pi)/ 160* (oldval-0.9) ))+centrex;
  oty3= (-13 * cos(pi-(2*pi)/ 160* (oldval+0.9) ))+centrey;
  otx3 =(-13 * sin(pi-(2*pi)/ 160* (oldval+0.9) ))+centrex;


  // circle coords
  cy= (-23 * cos(pi-(2*pi)/ 160* val ))+centrey;
  cx =(-23 * sin(pi-(2*pi)/ 160* val ))+centrex;

  // centre circle
  tft.fillCircle(centrex,centrey,5,0xDEFB); // light grey
  tft.fillCircle(centrex,centrey,4,0xD62F); // centre circle
  
  // previous value needle
  tft.fillTriangle(otx,oty,otx2,oty2,otx3,oty3,0x9D13); // 0xDEFB LIGHT GREY
  tft.drawLine(ox,oy,ox2,oy2,0x738E);  
  tft.drawLine(ox,oy,ox2,oy2,0xE680); 
  

  // main needle
  // dark grey triangle, overlaid with black line for better definition
  tft.fillTriangle(tx,ty,tx2,ty2,tx3,ty3,0x9D13); // 0x9D13 light grey. 0x7BEF DARK GREY
  tft.drawLine(tx,ty,x2,y2,ST77XX_BLACK);
  tft.fillCircle(cx,cy,2,0x7BEF);
  tft.fillCircle(cx,cy,1,ST77XX_BLACK);

  // brass centre thing
  tft.fillCircle(centrex,centrey,3,0x7BEF); //  dark grey
  tft.fillCircle(centrex,centrey,2,0xFFB1); // yellow
  tft.fillCircle(centrex-1,centrey,1,0xFFFB); // highlight

}
 

void display_barometer()
{

  int    i =0;

  // Set TFT address window to clipped image bounds
  tft.startWrite();

  tft.setAddrWindow(0, 0,160, 128);
  tft.endWrite();
  i = (160*128); // number of pixels to plot
  // dump image data to display
  while(i--) tft.pushColor(pgm_read_word(image + i));

  needleval = plot_values[plot_index_tail] / 10;
  oldneedleval  = plot_values[plot_index_head] / 10;

//Serial.print(needleval);
//Serial.print(",");
//Serial.println(oldneedleval);
  
  if (needleval < 940) needleval = 940;
  if (needleval > 1060) needleval = 1060;
  needleval = needleval - 940;

  if (oldneedleval < 940) oldneedleval = 940;
  if (oldneedleval > 1060) oldneedleval = 1060;
  oldneedleval = oldneedleval - 940;

Serial.print(needleval);
Serial.print(",");
Serial.println(oldneedleval);
  
  drawNeedle( needleval, oldneedleval ); 
  //drawNeedle( 1055-940, 983-940 ); 
}




/*
 * 
 * SETUP
 * 
 */
void setup() 
{
   
  pinMode(LED_BUILTIN, OUTPUT);

  //back light for screen
  pinMode(TFT_SCREEN, OUTPUT);
  digitalWrite(TFT_SCREEN,1);

  pinMode(BUTTON, INPUT_PULLUP);


  // Set up TFT screen
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);
  //tft.setFont(&FreeMono9pt7b);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextWrap(false);
   tft.setTextSize(0);
  tft.setTextColor( ST77XX_WHITE);
 

  // Set up Wifi
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  Serial.begin(115200);
    
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //reset settings - wipe credentials for testing
   //wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name
    tft.setCursor(0, 16);
    tft.print("Setup wifi");
    bool res;
    res = wm.autoConnect("ESP8266 Setup"); // anonymous ap

    if(!res) 
    {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else 
    {
        tft.setCursor(0, 16);
        tft.setTextColor( ST77XX_BLACK);
        tft.print("Setup wifi");
        tft.setCursor(0, 16);
        tft.setTextColor( ST77XX_WHITE);
        tft.print("Connected to wifi");
        //if you get here you have connected to the WiFi    
        Serial.println("Wifi connected!");
        //Serial.print("MQTT MAX PACKET SIZE : ");
        //Serial.println(MQTT_MAX_PACKET_SIZE);
  
        Serial.println("Connecting to MQTT");
        mqtt_connect();
     
    }

  
  pinMode(MOTION, INPUT);

  
   
  // clear values
  for (int i =0;i<20;i++)  
  {
    for(int ii=0;ii<2;ii++) values[i][ii]=' ';
    values[i][3]=0;
  }
  // init graph data
  
  pressure_val = 1000 * 10;
   for (int i=0;i<total_plot;i++) plot_values[i]=pressure_val;  // init plot values with mid value

  
}

/*
 * 
 * Draws everything to the screen
 * 
 */
void redraw()
{
Serial.print("REDRAW! MODE: ");
Serial.println(displaymode);
  if (displaymode==0)
  {
    // clear the screen.
    tft.fillScreen(ST77XX_BLACK);

    // set up the font
    //tft.setFont(&FreeMono9pt7b);
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(0);
    tft.setTextColor( ST77XX_WHITE);

    // draw the text/value for each of the MQTT topics
    //for (byte i=0;i<mqtt_total_topics;i++) // find in our list
    for (byte i=0;i<3;i++) // find in our list
    {
      tft.setFont(&FreeSans9pt7b);
       tft.setCursor(0, (i)*20 + 15);
       tft.print(mqtt_sub_topics[i][0]); //Name
       tft.setFont(&FreeMono9pt7b);
       
       tft.setCursor(92, (i)*20  + 15);
       tft.print(values[i]);
       tft.setCursor(91, (i)*20  + 15); // twice -1 x for bolder
       tft.print(values[i]);
       //tft.print(mqtt_sub_topics[i][2]); //Units
    }

    // draw the graph
    draw_graph();
  }
  else
  {
     display_barometer();
  }

}

void draw_graph()
{

  int drawindex;
  int drawy;

 // find max an min values so we can scale graph to fit
 max_value=0;
 min_value=999999;
 
  for (int x = 0; x<total_plot; x++)  // total_plot is 150
  {
    if (plot_values[x]<min_value) min_value = plot_values[x];
    if (plot_values[x]>max_value) max_value = plot_values[x];
  }
  max_value+=1;
  min_value-=1; // breathing space, also map() crashes if both same value

  // axis
  // screen size : 0 - 159   
  // data is 150 items, draw data from 5 to 155 
  
  //tft.drawLine(4, 127, 4,60, 0xf800); // left
  //tft.drawLine(155, 127, 155,60, 0xf800); // right
  //tft.drawLine(4, 127, 155,127, 0xf800); // bottom

  // clear:
  tft.fillRect(5, 60, 150, 66, 0x0000);

  // draw hour markers every 6 pixels
  for (int hourx = 4;hourx<156;hourx+=6) 
  //for (int hourx = 10;hourx<150;hourx+=6) 
  {
    tft.drawLine(hourx, 126, hourx,60, 0xc000 );  //0xf800 full red  0xc000 off red...  0x8000 DARK RED 16 bit (5,6,5 RGB)
  }

  // previous values for line
  int px;
  int py;

  // draw
  for (int x = 0; x<total_plot; x++)  // total_plot is 150
  {
  // plot_index_head
    drawindex = (plot_index_head + x) % total_plot;
    drawy = plot_values[drawindex];

   // scale for the display

   //int scaled_value = map(drawy, min_value, max_value, 60, 125); // screen coords 60 - 125
   
   int scaled_value = map(drawy, min_value, max_value, 125, 60); // screen coords 60 - 125
   
   // flip it
   //scaled_value =(scaled_value - 60)+125;
   // Serial.print(" scaled:");
   // Serial.println(scaled_value);

    // no previous
    if (x==0) 
    {
      px = x+5; py = scaled_value;
    }
   tft.drawLine(px, py, x+5, scaled_value, ST77XX_YELLOW);


   int col =0x0700;

   if ((x+1) %6== 0) col = 0x0640; // maintain hour markers, darker green
   tft.drawLine(x+5, scaled_value, x+5, 154, col);

   

px = x+5;
py = scaled_value;
   //tft.drawPixel(x+5,  scaled_value, ST77XX_YELLOW); // plot
   // tft.drawPixel(x+5,  scaled_value+1, ST77XX_YELLOW); // plot
    

  }


  tft.setFont(NULL);  // defaults to an 8 pixel high font?
  tft.setCursor(124, 88);
  float pressure_x =(float) pressure_val_o / 10;
  tft.print(pressure_x,1);
  

}


void baro_plot(int pressure)
{
  //Serial.println("time to insert");
  // Serial.print("inserting ");
  // Serial.print(pressure);
  // Serial.print(" at ");
  // Serial.println(plot_index_tail);
  plot_index_tail++;
  plot_index_tail%=total_plot;
  plot_index_head++;
  plot_index_head%=total_plot;
  plot_values[plot_index_tail] = pressure;

  if (displaymode==0)
  {
    draw_graph();
  }
  else
  {
    display_barometer();
  }
}


void loop() 
{
  
  // MQTT, MQTT is the protocol used in HomeAssistant to send/receive temperatures etc.
  if (!client.connected()) 
  {
    Serial.println("MQTT not connected.");
    mqtt_connect();
  }

  // New data from MQTT?
  if (mqtt_found) 
  {
    
     redraw();
    
    mqtt_found =  false;  //reset flag
  }
  
  client.loop();

  /*
  * Check for serial messages asking for data, for debugging
  */
  
   readSerial();



/* 
 *  
 *  Check the Button...
 *  
 */

  button_status = digitalRead(BUTTON);

  if(button_status != last_button_status)
  {

    if( button_status == false ) // pressed
    {
         
          displaymode++;
          displaymode%=2;
          Serial.println(displaymode);
          redraw();
          
    }
   
  }

 
  
  last_button_status = button_status;
  





  /* 
  *  Take pressure reading and plot it 
  *  
  *   does this every 10 minutes, or whatever interval is set up above.
  */
    currentMillis = millis();
    // time to take reading?
    if (abs(currentMillis - previousMillis) >= interval) 
    {
      //Serial.print("Time to take pressure reading : ");
      
      //Serial.println(pressure_val);
      pressure_val_o = pressure_val;
      // store this in the graph array...
      baro_plot(pressure_val);
      //set previous millis to now / reset timer
      previousMillis = currentMillis;
    }




    /*
     * Check for motion!
     * 
     * checks for motion on the sensor
     * if no motion for 5 minutes it turns off the screen
     * if there is any motion it resets the 5 minute countdown
     * 
     */
     
    movement = digitalRead(MOTION);  // either 1 or 0
    if (movement)  // motion detected?
    {
        // turn screen on
        if (!screen_on)
        {
          screen_on = true;
          //tft.enableDisplay(true);
          //Serial.println("Motion Detected! Screen on!");
          digitalWrite(TFT_SCREEN,1);
        }
        
        // Movement so reset the countdown.
        screen_off_countdown_start = millis();
        prev_milli_seconds_elapsed = millis(); // temp

    }  
    else  // no movement detected
    {
      if (screen_on)
      {
          screen_off_countdown = millis(); // now

          // get time elapsed since last motion
          milli_seconds_elapsed = abs(screen_off_countdown - screen_off_countdown_start);
         
        // 5 mins without movement? Turn it off.
        if ( (milli_seconds_elapsed  > screen_off_interval) )
        {
          //Serial.println("Screen Off!!");
          screen_on = false;
          digitalWrite(TFT_SCREEN,0);
        }
      }
    }




}
