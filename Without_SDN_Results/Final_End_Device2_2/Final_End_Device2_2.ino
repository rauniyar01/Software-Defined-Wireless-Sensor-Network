/*
Date   :      14/01/2016
Author :      DIPAK MAJHI.

&03*02#       Sensor change (Temperature)
&03*01#       Sensor change (Gyroscope)
!03*10#       Delay
$03*00#       Destination
@03*01#       Drop Packet
@03*00#       Forward Packet
*/

#include <ArduinoJson.h>
#include <Arduino.h>
#include<Wire.h>
#include <Time.h>

const int MPU=0x68;
int GyX,GyY,GyZ;
float pitch=0,roll=0,yaw=0,v_pitch,v_roll,v_yaw,a_pitch,a_roll,a_yaw; 
int i=0;

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false; 
String rinputString = ""; 
unsigned long int timeout;
unsigned char rfdata[200];  
unsigned int sourceadd16b,myadd16b,len=0;
unsigned char MY[10];
unsigned static int dest=0,del=900;  // setting Destination address as 0 and Delay of 1 sec

boolean rfsendok,rfok;
unsigned char RFFailure,count,maxresponse,myadd[3];
unsigned char check;
unsigned char sent=0;
float temp;
unsigned char flag=0;
String nodeID,state,activity,destADD,sensorID,startID,delaytime,checkst;
unsigned static int flagch=0, amtsend=2, reqcount=0;
String hr="",minit="",sec="", jsonString="";


void setup() {                                //setupcode to run only once
   
  myadd16b = 3;                               //Setting its own address as 3
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  Serial.begin(9600);                        // Initialize serial and wait for port to open for communication with baudrate of 9600
  delay(100);                                //delay of 0.1 sec
  Serial1.begin(9600);                       // Initialize serial1 and wait for port to open for communication with baudrate of 9600
   delay(100);                               //delay of 0.1 sec
   inputString.reserve(500);                 //allocate a buffer of size 500 bytes in memory for manipulating strings
  Initialize_RF();                           //Function to enable API format in zigbee 
  delay(100);                                //delay of 0.1 sec
  Set_My_Addr();                             // Function to set my address as 3
  
}



void loop() {                                     //main code to run repeatedly

    if(stringComplete) 
    {                            //Indicates that we received a string/command that needs to be processed.
     inputString = inputString.substring(9,70); 
     Serial.println(inputString);
     len = inputString.length(); 
     len+=10;
      char charBuff[len];
      pkt_broadcast(&charBuff[0]);
      inputString.toCharArray(charBuff,len-1);                    //converting string to array of characters
      sent=Send_data(0,&charBuff[0]);                         //sending destiation address with data packet to form a API packet
      inputString = "";                                 // clear the string:
      stringComplete = false;                           //setting flag to indicate new string could be accepted.
    }
    hr=String(hour());
    minit=String(minute());
    sec=String(second());

    Wire.beginTransmission(MPU);
    Wire.write(0x41); 
    Wire.endTransmission(false);
    Wire.requestFrom(MPU,6,true); 
    temp=Wire.read()<<8|Wire.read();                      //reading sensor vaue
    temp=(temp/340)+36.53;
    
    String s1 = "${"+String(amtsend)+","+String(reqcount)+", \"ID\":" + String(myadd16b);           //forming a Json string of data
    String s2 = ",4, "+hr+":"+minit+":"+sec+", \"Data\":["+String(temp)+"]}##";
    String jsonString = s1+s2;                            //concatenating the individual strings
    len = jsonString.length();                        //finding string length
    len+=10;
    char charBuff[len];
    pkt_broadcast(&charBuff[0]);
    jsonString.toCharArray(charBuff,len-1);               //converting string to array of characters
    
    sent=Send_data(0xFFFF,&charBuff[0]);                    //sending destiation address with data packet to form a API packet
    amtsend++;
    Serial.println(jsonString);
    delay(del); 
}


// Function for broadcast operation

void pkt_broadcast(char* mypkt_all)
{
    unsigned char i;
    
    for(i=0;i<15;i++) mypkt_all[i]=0;
    i=0;
    mypkt_all[i]=myadd16b>>8;i++;
    mypkt_all[i]=myadd16b;i++;
    mypkt_all[i]=0xFF;i++;
    mypkt_all[i]=0xFF;i++;

    mypkt_all[i]=1;i++;          //association not required
    mypkt_all[i]=0;i++;           //reserved
    
}


void serialEvent1() {                           //Function to receive a character or string as input
  while (Serial1.available()) {                 //if any incoming data is available in Serial1
                                                
    char inChar = (char)Serial1.read();           // get the new byte:
    //if(inChar=='&'||inChar=='@'||inChar=='!'||inChar=='$'||inChar=='0'||inChar=='1'||inChar=='2'||inChar=='3'||inChar=='4'||inChar=='*')    //if we receive any character in {@,#,$,!,&,0,1,2,3} we store it
    if(inChar!='#')
    inputString += inChar;                        // add it to the inputString:
    
    if (inChar == '#') {         
      inputString += '#';                         //concatinate a '#' with the string
      stringComplete = true;                      // if the incoming character is a newline, set a flag so the main loop can do something about it:
       reqcount++;
    }
  }
}

//Formation of API packets with Header, Data and Checksum

char Send_data(unsigned int dest_addr,char* mypacket)
{
     unsigned char i,length;
     unsigned int checksum;
     //Serial.println("Data sending.........");
     //delay(1000);
     for(i=9;i<=100;i++) rfdata[i]=0;                             //initialize total packet to zero.
     rfdata[1]=126;                                               //Hex form for 7E i.e starting delimiter
     rfdata[2]=0;                                                 //MSB of length
     rfdata[4]=1;                                                 //API identifier
     rfdata[5]='M';                                               //Frame ID
     rfdata[6]=dest_addr>>8;                                      //MSB and LSB of destnation address
     rfdata[7]=dest_addr;
     rfdata[8]=0;
     
     for(i=0;i<70;i++)
     {
          if(mypacket[i]=='#')                                      
         {
           rfdata[9+i]=mypacket[i];
           i=i+1;
           break;
         }
          rfdata[9+i]=mypacket[i];                                  //Store data in API packet untill we receive '#'
     }
     length=i+5;   
     rfdata[3]=length;                                              //storing length of the packet
     checksum=0;
     for(i=4;i<(length+4);i++)  checksum=checksum+rfdata[i];        //Finding Checksum
      check=checksum;
     rfdata[length+4]=0xFF-check;                                   //Storing Checksum
     for(i=1;i<=(length+4);i++) Serial1.write(rfdata[i]);           //Sending the API packet to controller through zigbee
     //Serial.println("");
      //sending finally the TX packet
      timeout=0;
      rfsendok=0;
       while(timeout<500 && rfsendok==0) timeout++;                  //Condition for Time out
       if(timeout>=500) {RFFailure++;return(0);}
       else  return(rfsendok);
     
}




// RF configuration

void Initialize_RF(void)
{
     //xbee_status_dir_input;
     //xbee_status_pullup_on;
     //xbee_reset_dir_output;
     //xbee_reset_high;
     //Delay_ms(500);
     Serial.println("**********Start xbee initialization**************");
     delay(1500);Serial1.print("+++");delay(1500);
      //Guard Time to enter into command mode
     Serial1.print("ATAP 1\r"); delay(100);
     // Configure in API mode
     Serial1.print("ATWR\r"); delay(100);
     // Write permanently in NV memory
     Serial1.print("ATCN\r"); delay(100);
     // Out of command mode
     Serial.println("**********End xbee initialization**************");
}



//Set it's own address

void Set_My_Addr(void)
{
     Serial.println("**********Start set address**************");
     unsigned char i;
     unsigned int checksum;
     MY[0]=0x7E;MY[1]=0x00;MY[2]=0x06;MY[3]=0x08;MY[4]=0x4D;
     MY[5]=0x4D;MY[6]=0x59;
     MY[7]=myadd16b>>8;MY[8]=myadd16b;
     checksum=0;
     for(i=3;i<=8;i++) checksum=checksum+MY[i];
     checksum=0xFF-(unsigned char)checksum;
     MY[9]=checksum;
     rfok=0;
     for(i=0;i<10;i++) Serial1.print(MY[i]);   //set MY address
     delay(50);
     if(rfok==0) RFFailure++;
     MY[0]=0x7E;MY[1]=0x00;MY[2]=0x04;MY[3]=0x08;MY[4]=0x4D;
     MY[5]=0x57;MY[6]=0x52;MY[7]=0x01;   //ATWR
     for(i=0;i<8;i++) Serial1.print(MY[i]);
     delay(50);
     if(rfok==0)   RFFailure++;
     Serial.println("**********End set address**************");
}
