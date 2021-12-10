// v2 - eth control added

#include <UIPEthernet.h> // Used for Ethernet

// mode 0 idle
// mode 1 down
// mode 2 up
// mode 3 down perm
// mode 4 up perm
// mode 5 blocked 

// pins 22 to 29 piny pro zakaz  ovladani
// pins 30 to 45 to control releys
// pins 46 + 47 GND relay modul
// pins 48 Ucc, 49 GND ETH modul
// pins 50 to 53 to control ETH modul

  #define level_1       271     
  #define level_2       390     
  #define level_3       475     
  #define level_4       654
  #define tolerance     40

  const long interval_click  = 500;           // interval for button click
  const long interval_move   = 55000;          // interval for move down
  const int  interval_flip   = 15;            // interval for button flick

  #define input_count                     8   // number of inputs 
  
  #define rele_offset                     30  // ofset rele   // output pins 30 to 45 to control releys
  #define central_control_block_offset    22  // piny pro zakaz  ovladani 

  int sensorPin_offset    = A0;               // select first input pin 
  int disablePin_offset   = A8;               // select first  pin for disable central control

  int mode[8]                             = {0, 0, 0, 0, 0, 0, 0, 0} ;
  unsigned long timestamp[8]              = {0, 0, 0, 0, 0, 0, 0, 0} ;
  unsigned long timestamp_perm[8]         = {0, 0, 0, 0, 0, 0, 0, 0} ;
  unsigned long timestamp_perm_stop[8]    = {0, 0, 0, 0, 0, 0, 0, 0} ;
  
  int Action = 0;

  // **** ETHERNET SETTING ****
  byte mac[] = { 0x54, 0x34, 0x41, 0x30, 0x30, 0x31 };                                       
  IPAddress ip(192, 168, 111, 80);                        
  EthernetServer server(80);
  EthernetClient client; 

  String HTTPget = "";
  boolean reading = false;

//  ---------------------------------------    check enable control   ------------------------------
boolean IsEnabledGroupControl(int device)
{
 return digitalRead(central_control_block_offset+device) ==  1;
 /*
 if (digitalRead(central_control_block_offset+device) ==  1)  
    {  return true;
    }
    else         {   
      return false;
    }
    */
}

//  ---------------------------------------    check enable group control   ------------------------------
boolean IsEnabledControl(int device)
{
  return analogRead(disablePin_offset+device) > 5 ;
  /*
 if (analogRead(disablePin_offset+device) > 5)  
    {  return true;
    }
    else         {   
      return false;
    }*/
}

//  ---------------------------------------------------    kontrola      --------------------------------------------------
boolean TimeStampIsIn(int device)
{
    unsigned long currentMillis = millis();
    if ((currentMillis - timestamp[device] <= interval_click ) )   { // kontrola uplynuti 0.5 sec       
      return true;
    }
    else         {   
      return false;
    }
}

//  ---------------------------------------------------          --------------------------------------------------
boolean PermTimeStampIsIn(int device)
{
    unsigned long currentMillis = millis();
    if ((currentMillis - timestamp_perm[device] <= interval_move) )  {  // kontrola uplynuti 50 sec
      return true;
    }
    else         {   
      return false;
    }
    
}

//  ---------------------------------------------------          --------------------------------------------------
boolean PermStopTimeStampIsIn(int device)
{
    unsigned long currentMillis = millis();
    if ((currentMillis - timestamp_perm_stop[device] <= interval_click) )  {  // kontrola uplynuti 20 sec
      return true;
    }
    else         {   
      return false;
    }
}

//  ---------------------------------------------------          --------------------------------------------------
int Check_action(int sensorPin_int)
{
int     status_int, status_int1;
int     sensorValue = analogRead(sensorPin_offset + sensorPin_int);
  
 if ((sensorValue >  level_1 - tolerance) and (sensorValue <  level_1 + tolerance)) 
    {status_int = 1;         //down
    }
    else if ((sensorValue >  level_2 - tolerance) and (sensorValue <  level_2 + tolerance)) 
    {status_int = 0;          // idle
    }
    else if ((sensorValue >  level_3 - tolerance) and (sensorValue <  level_3 + tolerance)) 
    {status_int = 3;          // both
    }
    else if ((sensorValue >  level_4 - tolerance) and (sensorValue <  level_4 + tolerance)) 
    {status_int = 2;          // up
    }

if (status_int > 0)  // kontrola na zakmitnuti
{ 
  delay(interval_flip);
  sensorValue = analogRead(sensorPin_offset + sensorPin_int);
  if ((sensorValue >  level_1 - tolerance) and (sensorValue <  level_1 + tolerance)) 
    {status_int1 = 1;         //down
    }
    else if ((sensorValue >  level_2 - tolerance) and (sensorValue <  level_2 + tolerance)) 
    {status_int = 0;          // idle
    }
    else if ((sensorValue >  level_3 - tolerance) and (sensorValue <  level_3 + tolerance)) 
    {status_int1 = 3;          // both
    }
    else if ((sensorValue >  level_4 - tolerance) and (sensorValue <  level_4 + tolerance)) 
    {status_int1 = 2;          // up
    }
  }
if (status_int == status_int1) return status_int; else return 0;
}

//  ----------------------------------------------------------------------------------------------------
void set_rele_int(int device_int, int state_int)
{
if (state_int == 1) { 
      digitalWrite(rele_offset + device_int*2, LOW);            // set reley DOWN  
      digitalWrite(rele_offset + device_int*2 + 1, HIGH);        
  }
  else if (state_int == 2) { 
      digitalWrite(rele_offset + device_int*2, HIGH);            // set reley UP
      digitalWrite(rele_offset + device_int*2 + 1, LOW);        
  }
  else if (state_int == 0)   { 
      digitalWrite(rele_offset + device_int*2, HIGH);             
      digitalWrite(rele_offset + device_int*2 + 1, HIGH);         
  }  
}

void set_rele(int device, int state)
{
 /*    Serial.print(" device : "); Serial.print(device);  
     Serial.print(" state : "); Serial.print(state);  
     Serial.println();  
*/
  if (IsEnabledControl(device)and device < input_count-1){  // check if control is enabled by DIP switch  
    set_rele_int(device,state);
    if (device == input_count-2){set_rele_int(device+1,state);  // zapni i druhy motor pro obyvak
    }  
  }
}


// ========================================   read commands from HTTP  ============================================  
void readHTTPline()
{
  // listen for incoming clients
  client = server.available();
  if (client) 
  {
    // send http reponse header
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    // process request.
    processClient(client);
  }
}


// ========================================  list page ============================================  
void show_list_HTTP()  
  {
    client.print("-------------------------------------------------------------\n");
    client.print("|           List of rooms for Arduino Zaluzie             |\n");
    client.print("-------------------------------------------------------------\n");
    client.print("rele 1          2np. Loznice \n");
    client.print("rele 2          2np. Zapad \n");
    client.print("rele 3          2np. Vychod \n");
    client.print("rele 4          2np. Chodba \n");
    client.print("rele 5          1np. Pracovna\n");
    client.print("rele 6          1np. Kuchyne  \n");
    client.print("rele 7          1np. Obyvak I. \n");
    client.print("rele 8          1np. Obyvak II. \n");
  }


// ========================================  manual page ============================================  
void show_help_HTTP()
  {
    client.print("-------------------------------------------------------------\n");
    client.print("|           List of commands for Arduino Zaluzie             |\n");
    client.print("-------------------------------------------------------------\n");
    client.print("config         get config \n");
    client.print("getCBS         get central block DIP settings \n");
    client.print("getDIS         get disable DIP settings \n");
    client.print("up             move up \n");
    client.print("down           move down \n");
    client.print("stop           stop all \n");
    client.print("list           list of all rooms \n");
    client.print("man            show this manual \n ");
  }


// ========================================   process commands from HTTP  ============================================  

void processClient(EthernetClient client)
{
  // http request will end with a blank line
  boolean lineIsBlank = true;
 
  while (client.connected())
  {
    if (client.available())
    {
      char c = client.read();
 
      if(reading && c == ' ') reading = false;
      if(c == '?') reading = true; // ? in GET request was found, start reading the info
 
      //check that we are reading, and ignore the '?' in the URL, then append the get parameter into a single string
      if(reading && c != '?') HTTPget += c;
 
      if (c == '\n' && lineIsBlank)  break;
 
      if (c == '\n')
      {
        lineIsBlank = true;
      }
      else if (c != '\r')
      {
        lineIsBlank = false;
      }
     }
  }

  int divider_position = HTTPget.indexOf('%');
  String part1=HTTPget.substring(0,divider_position);
  String part2=HTTPget.substring(divider_position+1);

    if (part1.equalsIgnoreCase("getCBS"))  {
         send_central_control_block_setting_to_HTTP();
    }

    else if (part1.equalsIgnoreCase("getDIS")) {
        send_disable_setting_to_HTTP();
    }
    
    else if (part1.equalsIgnoreCase("config")) {
        send_config_to_HTTP();
    }

    else if (part1.equalsIgnoreCase("down")) {
      int device_id = part2.toInt();
        moveUpDownURL(device_id-1,1);
    }

    else if (part1.equalsIgnoreCase("up")) {
      int device_id = part2.toInt();
        moveUpDownURL(device_id-1,2);
    }

    else if (part1.equalsIgnoreCase("stop")) {
        for (int i = 0; i < input_count; i++)     {              // initial settings of all relays to LOW       
        set_rele(i, 0);       // switch on/off releys for all motors
        }
    }
       
   else if (part1.equalsIgnoreCase("man"))
      {
        show_help_HTTP();
      }   

   else if (part1.equalsIgnoreCase("list"))
      {
        show_list_HTTP();
      }   
      
  delay(1); // give the web browser a moment to recieve
  client.stop(); // close connection
  HTTPget = ""; // clear out the get param we saved
}

// ========================================   sent data to HTTP ============================================  
void sent_data_to_HTTP(void)
{
  client.print("counter_2_post:"+String(11)+"\n");
  client.print((char)4); // EOF character
}

// ========================================   sent data to HTTP ============================================  
void send_central_control_block_setting_to_HTTP(void)
{
      for (int i = 0; i < input_count-1; i++)     {              // initial settings of all relays to LOW       
          client.print("CBS DIP " +String(1 + i)+" set:" +String(digitalRead(central_control_block_offset + i))+"\n");
      } 

          client.print("CBS DIP " +String(input_count)+" set:" +String(digitalRead(central_control_block_offset + input_count))+" < --- Debug mode \n");
      
      client.print((char)4); // EOF character
}


// ========================================   sent data to HTTP ============================================  
void send_disable_setting_to_HTTP(void)
{
      for (int i = 0; i < input_count; i++)     {              // initial settings of all relays to LOW       
        int res =  (analogRead(disablePin_offset + i) > 5) ? 0:1;
        client.print("DIS DIP " +String(1 + i)+" set:" + String(res)+"\n");
      } 
      client.print((char)4); // EOF character
}

// ========================================   move URL   ============================================  
void moveUpDownURL(int device,int Action)
{
   mode[device]= Action+2;
   timestamp_perm[device]= millis();
   debug_message(device,"set perm up/down mode from url");
   set_rele(device, Action);       // switch on/off releys for all motors
  }

// ========================================   move URL   ============================================  
void send_config_to_HTTP()
{
  client.print("IP Address: ");   
  client.println(Ethernet.localIP());
  String debug_status =IsEnabledGroupControl(input_count-1) ? "On":"Off";
  client.print("Debug mode: ");    
  client.println(debug_status);
}
//  ----------------------------------------------------------------------------------------------------
void debug_message(int device, String note)
{
  if (IsEnabledGroupControl(input_count-1))  {
     Serial.print(" time: ");     Serial.print(millis());  
     Serial.print(" i: ");        Serial.print(device);  
     Serial.print(" mode: ");     Serial.print(mode[device]); 
     Serial.print(" TS: ");       Serial.print(timestamp[device]); 
     Serial.print(" TS perm: ");  Serial.print(timestamp_perm[device]); 
     Serial.print(" ");           Serial.println(note);
  }
}
 
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);

  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Debug mode: ");
  String debug_status =IsEnabledGroupControl(input_count-1) ? "On":"Off";
  Serial.println(debug_status);

  debug_message(100, "Test debug modu");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();

 for (int i = rele_offset; i < rele_offset+2*input_count; i++)     {              // initial settings of all relays to LOW       
         pinMode(i, OUTPUT);                // set pin to input
         digitalWrite(i, HIGH);             // turn on pullup resistors  
 }

 for (int i = 0; i < 8; i++)     {                                        // initial settings of pins 22- 29  as input for perm_block_inputs_offset      
         pinMode(central_control_block_offset+i, INPUT);                  // set pin to input
         digitalWrite(central_control_block_offset+i, HIGH);              // turn on pullup resistors  
}
}

void loop() {
  readHTTPline();

  if (IsEnabledGroupControl(input_count-1))  {
   Serial.print(analogRead(sensorPin_offset + 0)); Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 1));Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 2));Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 3));Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 4));Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 5));Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 6));Serial.print(" ");
   Serial.print(analogRead(sensorPin_offset + 7));Serial.print(" ");
   Serial.println();
  }
  
 for (int i = 0; i < input_count; i++)
  {         
    if ((mode[i] == 3 or mode[i] == 4) and !PermTimeStampIsIn(i)) {   // moving up/down in perm mode if time exceeds then stop motors
                      if (i == input_count-1) {   // central control is used
                          for (int j = 0; j <  input_count-1; j++) {
                            mode[i]= 0;
                            set_rele(j, 0);       // switch off motors for all motors
                          }
                          debug_message(i,"release group perm mode timeout");                      
                      }
                      else{                                        
                        mode[i]= 0;
                        set_rele(i, 0);
                        debug_message(i,"release perm mode timeout");
                      }
    }

    Action = Check_action(i);                         
    
    if (Action == 0 ) {                                                             // button released
         if (mode[i] == 1 or mode[i] == 2 ) {                                       // moving up/down
             if (TimeStampIsIn(i) and (mode[i] != 3 or mode[i] != 4 )) {            // pokud je tlacitko uvolneno do .5 s tak se zapne pernament mode
                      if (i == input_count - 1) {                                   // central control is used
                          for (int j = 0; j <  input_count ; j++) {
                              if (IsEnabledGroupControl(j)) {                       // check DIP switch if group control is enabled
                                mode[j]= mode[j]+2;                                 // change mode to perm
                                timestamp_perm[j]= millis();
                              }
                          }
                      debug_message(i,"set group perm up/down mode");                      
                      }
                      else
                      {
                          mode[i]= mode[i]+2;                                        // change mode to perm
                          timestamp_perm[i]= millis();
                          debug_message(i,"set perm up/down mode");
                      }
             }
             else {                              
                      if (i==input_count-1) {   // central control is used
                          for (int j = 0; j <  input_count-1; j++) {
                            mode[i]= 0;
                            set_rele(j,0);       // switch off motors for all motors
                          }
                      debug_message(i,"clear up/down group mode");                      
                      }
                      else {
                        mode[i]= 0;
                        set_rele(i, 0);           // switch off motors
                        debug_message(i,"clear up/down mode");
                      }
                  }
        }    
    }
    
    if (Action == 1 or Action == 2) {                                           // button pressed
         if (mode[i] == 3 or mode[i] == 4 ) {                                   // moving up/down in perm mode
                    mode[i]= Action;                                            // stop perm mode and set up/down               
                    debug_message(i,"clear perm up/down mode");
                    timestamp_perm_stop[i]= millis();                           // po zruseni perm modu nelze 0.5s zapnout up/dwn
         }
         if (!PermStopTimeStampIsIn(i) and mode[i] != 1 and mode[i] != 2 ) {    // pokud neni zapnuto up/down tak se tento mod zapne
                      if (i==input_count-1) {                                   // central control is used
                            debug_message(i,"set perm up/down mode - begin");
                            for (int j = 0; j < input_count-1; j++) {
                              if (IsEnabledGroupControl(j)) {
                                set_rele(j, Action);                            // switch on/off releys for all motors
                                mode[j]= Action;
                                timestamp[j]= millis();
                              }
                            }
                            debug_message(i,"set up/down group mode - end");                      
                      }
                      else {
                          set_rele(i, Action);
                          mode[i]= Action;            
                          timestamp[i]= millis();
                          debug_message(i,"set up/down mode");
                      }
        }          
    }
  } // for (int i = 0; i < input_count; i++)
}
