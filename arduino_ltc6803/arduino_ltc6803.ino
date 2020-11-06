#include <SPI.h>

byte WRCFG[2] {0x01,0xc7}; // Write Configuration Register
byte RDCFG[2] {0x02,0xce}; // Read Configuration Register
byte RDCV[2]  {0x04,0xdc}; // Read all cell voltages
byte RDCVA[2] {0x06,0xd2};
byte RDCVB[2] {0x08,0xf8};
byte RDTMP[2] {0x0e,0xea}; // Read all temperatures, page 22
byte STCVDC[2] {0x60,0xe7};  //STCVDC, page 23
byte STCVAD[2] {0x10,0xb0};  //STCVAD, page 23
byte DAGN[2] {0x52,0x79};
byte RDDGNR[2] {0x54,0x6b};

#define VLSB .0015 // 1.5 mV

#define LTC1 0x80 // Board Address for 1st LTC6803G-2
#define LTC2 0x81 // Board Address for 2nd LTC6803G-2
#define LTC3 0x82 // Board Address for 3rd LTC6803G-2

byte byteHolder;
// configuration for 1st module of LTC6803-4 (negate LTC6803-4)
//byte CFFMVMC[6] = {0x09, 0x00, 0x00, 0x00, 0x00, 0x00};
// configuration for 1st module to assert LTC6803-4
//byte asLTC6I803[6] = {0x29, 0x00, 0x00, 0xFC, 0xA4, 0xBA};
 // TODO mask cells if less than 12 in MCxI
byte asLTC6803[6] = {0x29, 0x00, 0b11110000, 0b00001111, 0xA4, 0xBA};

byte WRFG[1] = {0x01};
byte addr0[1] = {LTC1};
byte addr1[1] = {LTC2};
byte addr2[1] = {LTC3};

void setup() {
 // put your setup code here, to run once:
 
 pinMode(10, OUTPUT);
 pinMode(11, OUTPUT);
 pinMode(12, INPUT);
 pinMode(13, OUTPUT);
 digitalWrite(10, HIGH);
 Serial.begin(9600);
 Serial.println("##################################################################");
 Serial.println("#                                                                #");
 Serial.println("# LTC6803 Interrogator                                           #");
 Serial.println("#                                                                #");
 Serial.println("##################################################################");
 
 SPI.begin();
 SPI.setClockDivider(SPI_CLOCK_DIV64); // as low as possible, 128 probabbly the lowest value for Nano
 SPI.setDataMode(SPI_MODE3);
 SPI.setBitOrder(MSBFIRST);
 
  writeConfig(addr0);
  
  writeConfig(addr1);
  
  writeConfig(addr2);
  
}

void loop() {
  int count = 10;


  if (readConfig(addr0,false) == 0) {
    if ( count == 0 ) {
      doSelfTest(addr2);
    } else {
      getAll(addr0);
    }
  }
  
  if (readConfig(addr1,false) == 0) {
    if ( count == 0 ) {
      doSelfTest(addr2);
    } else {
      getAll(addr1);
    }
  }
  
  if (readConfig(addr2,false) == 0) {
    if ( count == 0 ) {
      doSelfTest(addr2);
    } else {
      getAll(addr2);
    }
  }
  
  delay(2000);
  if ( count == 0 ) {
    count = 10;
  } else {
    count--;
  }
    
    Serial.println("##################################################################");
  //}
//}

//  spiStart();
//  SPI.transfer(getPEC(WRFG,1));
//  spiEnd();
//Serial.println("Loop done..");
 delay(2000);
}

void getAll(byte * ltc_addr) {
  readConfig(ltc_addr, true);
  readVoltages(ltc_addr);
  readTemperatures(ltc_addr);
}






// Calculate PEC, n is  size of DIN
byte getPEC(byte *din, int n) {
byte pec, in0, in1, in2;
pec = 0x41;
for(int j=0; j<n; j++) {
  for(int i=0; i<8; i++) {
    in0 = ((din[j] >> (7 - i)) & 0x01) ^ ((pec >> 7) & 0x01);
    in1 = in0 ^ ((pec >> 0) & 0x01);
    in2 = in0 ^ ((pec >> 1) & 0x01);
    pec = in0 | (in1 << 1) | (in2 << 2) | ((pec << 1) & ~0x07);
  }
}
return pec;
}

void spiStart(byte *ltc_addr) {
 delay(10);
 SPI.begin();
 if(digitalRead(10) == LOW) {
   digitalWrite(10,HIGH);
   delayMicroseconds(5); //Ensure a valid pulse
 }
 digitalWrite(10, LOW);
 //delayMicroseconds(20);
  SPI.transfer(ltc_addr[0]);
  SPI.transfer(getPEC(ltc_addr,1));
}

void spiEnd(uint8_t flag) { //Wait poll Status
 SPI.end();
 if(flag > 0) {
   delayMicroseconds(10);
   digitalWrite(10, HIGH);
 }
}

// Send multiple data to LTC
void sendMultiplebyteSPI(byte * data, int n){
 for (int i = 0; i<n; i++){
   SPI.transfer(data[i]);
   }
 }

void spiEnd(void) {
 spiEnd(1);
}


/*
 * read a number of bytes on the selected register and verify checksum
 * 
 * RDCV and reg seem to be equivalent beyond the first sedMultiplebyteSPI() call.
 */
bool readBytes(byte *ltc_addr, byte *reg, byte *res, int num_bytes, bool verbose = false) {
  byte pec;
  //Serial.println("Reading "+String(num_bytes,DEC)+"bytes + PEC");
  
  spiStart(ltc_addr);
  sendMultiplebyteSPI(reg, 2);
   for(int i = 0; i < num_bytes; i++){
    //res[i] = SPI.transfer(reg[0]);
    res[i] = SPI.transfer(RDCV[0]);
  }
  //pec = SPI.transfer(reg[0]);
  pec = SPI.transfer(RDCV[0]);
  spiEnd();
  
  if (getPEC(res,num_bytes) != pec) { 
    if ( verbose) { Serial.println("PEC Mismatch: read 0x"+String(pec,HEX)+" but computed 0x"+String(getPEC(res,num_bytes),HEX)); }
    return false;
   } else {
    //Serial.println("PEC MATCH!!!");
    return true;
  }
}


// run ADC converter self test. see page 16
int doSelfTest(byte *ltc_addr) {
  int err_count = 0;
  
  Serial.println("Running diagnose on 0x"+String(ltc_addr[0],HEX));
  spiStart(ltc_addr);
  // start diagnose and poll status
  sendMultiplebyteSPI(DAGN, 2);
  spiEnd();
  
  // RDDGNR read diagnose register, 2 bytes
  byte res[2];
  readBytes(ltc_addr,RDDGNR,res,2);
  Serial.println(res[0],HEX);
  Serial.println(res[1],HEX);
  
  // TODO PLINT poll ADC converter status
  
  byte voltages[18];

  if (readBytes(ltc_addr,RDCV,voltages,18)) {
    for (int i = 0 ; i < 18 ; i++ ) {
      Serial.println("Cell "+String(i,DEC)+": "+String(voltages[i],DEC)+" [V]");
    }
  } else { err_count += 1; }
 
   return err_count;
}

// Write Configuration Registers for LTC6803-4 using Broadcast Write
void writeConfig(byte *ltc_addr){
 Serial.println("Writing configuration register on 0x"+String(ltc_addr[0],HEX));spiStart(ltc_addr);
 sendMultiplebyteSPI(WRCFG, 2);
 sendMultiplebyteSPI(asLTC6803, 6);
 SPI.transfer(getPEC(asLTC6803,6));
 spiEnd();
}


//// Read Cell Configuration Registers for LTC6803-4 using Addressable Read
int readConfig(byte *ltc_addr, bool verbose){
  if (verbose){ Serial.println("Reading configuration register on 0x"+String(ltc_addr[0],HEX));}
  byte res[6];
  if (readBytes(ltc_addr,RDCFG,res,6) ) {
    for (int i = 0 ; i < 6 ; i++ ) {
      if (verbose){ Serial.println("CFGR"+String(i,DEC)+": 0x"+String(res[i],HEX)); }
    }
    return 0;
  } else {
    return 1;
  }
 
 }


int readVoltages(byte * ltc_addr){
 Serial.println("Reading voltages on 0x"+String(ltc_addr[0],HEX));
 // enable polling/conversion
 spiStart(ltc_addr);
 sendMultiplebyteSPI(STCVAD, 2);
 //byte result;
 //result = SPI.transfer(RDCV[0]);
 //Serial.println("Reading voltages on 0x"+String(ltc_addr[0],HEX)+" result: "+String(result,HEX));
 spiEnd();
 delay(15); // see table on page 4

 int err_count = 0;
 byte res[6];

  if (readBytes(ltc_addr,RDCVA,res,6)) {
    for (int i = 0 ; i < 2 ; i++ ) {
      //for (int j = 0 ; j < 3 ; j++ ) {
      //  Serial.println("Byte "+String(3*i+j,DEC)+": "+String(res[3*i+j],HEX));
      //}
      byte vaM = (res[3*i+1]&0x0f) ;
      byte vaL = res[3*i];
      //Serial.println("Double "+String(i,DEC)+": "+String(vaM,HEX)+"'"+String(vaL,HEX));
      double va = ((vaM*0xff + vaL)-512)*VLSB;
      Serial.println("+++++> "+String(va)+" [V]");
      //byte vbM = res[3*i+2];
      //byte vbL = (res[3*i+1]&0xf0)>>4;
      byte vbM = res[3*i+2]>>4;
      byte vbL = (res[3*i+2]&0x0f)<<4 | (res[3*i+1]&0xf0)>>4;
      //Serial.println("Double "+String(i+1,DEC)+": "+String(vbM,HEX)+"'"+String(vbL,HEX)+"   "+vbM*255+"+"+vbL);
      double vb = ((vbM*0xff + vbL)-512)*VLSB;
      Serial.println("-----> "+String(vb)+" [V]");
    }
  } else { err_count += 1; }
 
  if (readBytes(ltc_addr,RDCVB,res,6)) {
    for (int i = 0 ; i < 2 ; i++ ) {
      //for (int j = 0 ; j < 3 ; j++ ) {
      //  Serial.println("Byte "+String(3*i+j,DEC)+": "+String(res[3*i+j],HEX));
      //}
      byte vaM = (res[3*i+1]&0x0f) ;
      byte vaL = res[3*i];
      //Serial.println("Double "+String(i+4,DEC)+": "+String(vaM,HEX)+" "+String(vaL,HEX));
      double va = ((vaM*0xff + vaL)-512)*VLSB;
      Serial.println("+++++> "+String(va)+" [V]");
      byte vbM = res[3*i+2]>>4;
      byte vbL = (res[3*i+2]&0x0f)<<4 | (res[3*i+1]&0xf0)>>4;
      //byte vbL = (res[3*i+1]&0xf0)>>4;
      //Serial.println("Double "+String(i+1,DEC)+": "+String(vbM,HEX)+"'"+String(vbL,HEX)+"   "+vbM*255+"+"+vbL);
      double vb = ((vbM*0xff + vbL)-512)*VLSB;
      Serial.println("-----> "+String(vb)+" [V]");
    }
  } else { err_count += 1; }
 
  return err_count;
  spiEnd();
}


int readTemperatures(byte * ltc_addr){
 Serial.println("Reading temperatures on 0x"+String(ltc_addr[0],HEX));
 spiStart(ltc_addr);
 SPI.transfer(0x30);  //STTMPAD, page 22
 SPI.transfer(0x50);
 spiEnd();
 delay(5); // see table on page 4

  byte res[5];
  if (readBytes(ltc_addr,RDTMP,res,5)) {
    Serial.println("External 1:  "+String((float)res[0]/10.)+" [°C]");
    Serial.println("External 2:  "+String((float)res[1]/10.)+" [°C]");
    Serial.println("Internal:    "+String((float)res[2]/10.)+" [°C]");
    Serial.println("Self Test 1: "+String((float)res[3]/10.)+" [°C]");
    Serial.println("Self Test 2: "+String((float)res[4]/10.)+" [°C]");
    return 0;
  } else {
    return 1;
  }
}
