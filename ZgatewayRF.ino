/*  
  OpenMQTTGateway  - ESP8266 or Arduino program for home automation 

   Act as a wifi or ethernet gateway between your 433mhz/infrared IR signal  and a MQTT broker 
   Send and receiving command by MQTT
 
  This gateway enables to:
 - receive MQTT data from a topic and send RF 433Mhz signal corresponding to the received MQTT data
 - publish MQTT data to a different topic related to received 433Mhz signal

    Copyright: (c)Florian ROBERT
  
    This file is part of OpenMQTTGateway.
    
    OpenMQTTGateway is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenMQTTGateway is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 Example MQTT to RF command
 sudo mosquitto_pub -t home/OpenMQTTGateway/commands/CODE_8233372/UNIT_0/PERIOD_272 -m 0
 sudo mosquitto_pub -t home/OpenMQTTGateway/commands/CODE_8233372/toMQTT/UNIT_0/PERIOD_272 -m 2599032
 sudo mosquitto_pub -t home/OpenMQTTGateway/commands/MQTTto433/433_1/PLSL_123/RFBITS_272 -m 2599028
 sudo mosquitto_pub -t home/OpenMQTTGateway/commands/MQTTto433/433_1/PLSL_123 -m 2599028
 sudo mosquitto_pub -t home/OpenMQTTGateway/commands/MQTTto433 -m here
*/
#ifdef ZgatewayRF
#include <ELECHOUSE_CC1101_RCS_DRV.h>
#include <RCSwitch.h> // library for controling Radio frequency switch

RCSwitch mySwitch = RCSwitch();

void setupRF(){
  #ifdef IS_C1101
    //CC1101 Settings:                (Settings with "//" are optional!)
    ELECHOUSE_cc1101.setESP8266(CC1101_ESP);    // esp8266 & Arduino SPI pin settings. Don´t change this line!
    //ELECHOUSE_cc1101.setRxBW(16);       // set Receive filter bandwidth (default = 812khz) 1 = 58khz, 2 = 67khz, 3 = 81khz, 4 = 101khz, 5 = 116khz, 6 = 135khz, 7 = 162khz, 8 = 203khz, 9 = 232khz, 10 = 270khz, 11 = 325khz, 12 = 406khz, 13 = 464khz, 14 = 541khz, 15 = 650khz, 16 = 812khz.
    //ELECHOUSE_cc1101.setChannel(1);    // set channel. steps from Channle spacing.0 - 255 default channel number is 1 for basic frequency.
    //ELECHOUSE_cc1101.setChsp(50);     // set Channle spacing (default = 50khz) you can set 25,50,80,100,150,200,250,300,350,405.
    ELECHOUSE_cc1101.setMHZ(433.92); // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
    ELECHOUSE_cc1101.Init(PA10);    // must be set to initialize the cc1101! set TxPower  PA10, PA7, PA5, PA0, PA_10, PA_15, PA_20, PA_30.

    mySwitch.enableTransmit(CC1101_TX_PIN);
    ELECHOUSE_cc1101.SetTx();
    trc(F("RF_EMITTER_PIN "));
    trc(CC1101_TX_PIN);
        
    mySwitch.enableReceive(CC1101_RX_PIN);  // Receiver on
    ELECHOUSE_cc1101.SetRx();  // set Recive on
    trc(F("RF_RECEIVER_PIN "));
    trc(CC1101_RX_PIN);
    
    trc(F("ZgatewayRF setup done "));
  #else
    //RF init parameters
    mySwitch.enableTransmit(RF_EMITTER_PIN);
    trc(F("RF_EMITTER_PIN "));
    trc(RF_EMITTER_PIN);
    
    mySwitch.setRepeatTransmit(RF_EMITTER_REPEAT); 
    mySwitch.enableReceive(RF_RECEIVER_PIN); 
    trc(F("RF_RECEIVER_PIN "));
    trc(RF_RECEIVER_PIN);
  trc(F("RF setup ok"));
  #endif
}

void RFtoMQTT(){

  if (mySwitch.available()){
    const int JSON_MSG_CALC_BUFFER = JSON_OBJECT_SIZE(4);
    StaticJsonBuffer<JSON_MSG_CALC_BUFFER> jsonBuffer;
    JsonObject& RFdata = jsonBuffer.createObject();
    trc(F("Rcv. RF"));
    #ifdef ESP32
      String taskMessage = "RF Task running on core ";
      taskMessage = taskMessage + xPortGetCoreID();
      trc(taskMessage);
    #endif
    RFdata.set("value", (unsigned long)mySwitch.getReceivedValue());
    RFdata.set("protocol",(int)mySwitch.getReceivedProtocol());
    RFdata.set("length", (int)mySwitch.getReceivedBitlength());
    RFdata.set("delay", (int)mySwitch.getReceivedDelay());
    mySwitch.resetAvailable();
    
    unsigned long MQTTvalue = RFdata.get<unsigned long>("value");
    if (!isAduplicate(MQTTvalue) && MQTTvalue!=0) {// conditions to avoid duplications of RF -->MQTT
        #ifdef ZmqttDiscovery //component creation for HA
            RFtoMQTTdiscovery(MQTTvalue);
        #endif
        pub(subjectRFtoMQTT,RFdata);
        trc(F("Store to avoid duplicate"));
        storeValue(MQTTvalue);
        if (repeatRFwMQTT){
            trc(F("Pub RF for rpt"));
            pub(subjectMQTTtoRF,RFdata);
        }
    } 
  }
}

#ifdef ZmqttDiscovery
void RFtoMQTTdiscovery(unsigned long MQTTvalue){//on the fly switch creation from received RF values
  char val[11];
  sprintf(val, "%lu", MQTTvalue);
  trc(F("switchRFDiscovery"));
  char * switchRF[8] = {"switch", val, "", "","",val, "", ""};
     //component type,name,availability topic,device class,value template,payload on, payload off, unit of measurement

   trc(F("CreateDiscoverySwitch"));
   trc(switchRF[1]);
    createDiscovery(switchRF[0],
                    subjectRFtoMQTT, switchRF[1], (char *)getUniqueId(switchRF[1], switchRF[2]).c_str(),
                    will_Topic, switchRF[3], switchRF[4],
                    switchRF[5], switchRF[6], switchRF[7],
                    0,"","",true,subjectMQTTtoRF);
}
#endif

#ifdef simpleReceiving
void MQTTtoRF(char * topicOri, char * datacallback) {

  unsigned long data = strtoul(datacallback, NULL, 10); // we will not be able to pass values > 4294967295

  // RF DATA ANALYSIS
  //We look into the subject to see if a special RF protocol is defined 
  String topic = topicOri;
  int valuePRT = 0;
  int valuePLSL  = 0;
  int valueBITS  = 0;
  int pos = topic.lastIndexOf(RFprotocolKey);       
  if (pos != -1){
    pos = pos + +strlen(RFprotocolKey);
    valuePRT = (topic.substring(pos,pos + 1)).toInt();
    trc(F("RF Protocol:"));
    trc(valuePRT);
  }
  //We look into the subject to see if a special RF pulselength is defined 
  int pos2 = topic.lastIndexOf(RFpulselengthKey);
  if (pos2 != -1) {
    pos2 = pos2 + strlen(RFpulselengthKey);
    valuePLSL = (topic.substring(pos2,pos2 + 3)).toInt();
    trc(F("RF Pulse Lgth:"));
    trc(valuePLSL);
  }
  int pos3 = topic.lastIndexOf(RFbitsKey);       
  if (pos3 != -1){
    pos3 = pos3 + strlen(RFbitsKey);
    valueBITS = (topic.substring(pos3,pos3 + 2)).toInt();
    trc(F("Bits nb:"));
    trc(valueBITS);
  }
  if ((topic == subjectMQTTtoRF) && (valuePRT == 0) && (valuePLSL  == 0) && (valueBITS == 0)){
    trc(F("MQTTtoRF dflt"));
    mySwitch.setProtocol(1,350);
    mySwitch.send(data, 24);
    // Acknowledgement to the GTWRF topic
    pub(subjectGTWRFtoMQTT, datacallback);  
  } else if ((valuePRT != 0) || (valuePLSL  != 0)|| (valueBITS  != 0)){
    trc(F("MQTTtoRF usr par."));
    if (valuePRT == 0) valuePRT = 1;
    if (valuePLSL == 0) valuePLSL = 350;
    if (valueBITS == 0) valueBITS = 24;
    trc(valuePRT);
    trc(valuePLSL);
    trc(valueBITS);
    mySwitch.setProtocol(valuePRT,valuePLSL);
    mySwitch.send(data, valueBITS);
    // Acknowledgement to the GTWRF topic 
    pub(subjectGTWRFtoMQTT, datacallback);// we acknowledge the sending by publishing the value to an acknowledgement topic, for the moment even if it is a signal repetition we acknowledge also
  } 
}
#endif

#ifdef jsonReceiving
  void MQTTtoRF(char * topicOri, JsonObject& RFdata) { // json object decoding
  
   if (strcmp(topicOri,subjectMQTTtoRF) == 0){
      trc(F("MQTTtoRF json"));
      unsigned long data = RFdata["value"];
      if (data != 0) {
        int valuePRT =  RFdata["protocol"]|1;
        int valuePLSL = RFdata["delay"]|350;
        int valueBITS = RFdata["length"]|24;
        int valueRPT = RFdata["repeat"]|RF_EMITTER_REPEAT;
        mySwitch.setRepeatTransmit(valueRPT);
        mySwitch.setProtocol(valuePRT,valuePLSL);
        mySwitch.send(data, valueBITS);
        trc(F("MQTTtoRF OK"));
        pub(subjectGTWRFtoMQTT, RFdata);// we acknowledge the sending by publishing the value to an acknowledgement topic, for the moment even if it is a signal repetition we acknowledge also
        mySwitch.setRepeatTransmit(RF_EMITTER_REPEAT); // Restore the default value
      }else{
        trc(F("MQTTtoRF Fail json"));
      }
    }
  }
#endif
#endif
