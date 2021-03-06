/*
This sketch continually listens to a 433 or 315mhz receiver connected to rxpin. 
It listens for 4, 8, 16, or 32 byte packets, sent 2ms after the training burst with 0.65ms low between bits, 1.1ms high for 1, 0.6ms for 0.




Description of packet format:

The first byte of packets is:

| SZ1 | SZ0 | A5 | A4 | A3 | A2 | A1 | A0 |

A5~0: Address bits - 6 bit address of the device, compared to MyAddress. Only packets matching address are processed. 

SZ1~SZ0: Size setting

SZ1=0, SZ0=0: 4 bytes
SZ1=0, SZ0=1: 8 bytes
SZ1=1, SZ0=0: 16 bytes
SZ1=1, SZ0=1: 32 bytes

The next two bytes go into MyCmd and MyParam, respectively, if the transmission is accepted (ie, device address matches and checksum is OK)

In a 4 byte packet, the first 4 bits of the last byte goes into MyExtParam (extended parameter), and the last 4 bits are the checksum. 

For all longer packets, the whole fourth byte goes into MyExtParam, and the final byte is the checksum. 

For 4 byte packets, checksum is calculated by XORing the first three bytes, then XORing the first 4 bits of the result with the last 4 bits, and the first 4 bits of the fourth byte. 

For longer packets, checksum is calculated by XORing all bytes of the packet. 


How to extend this:


Most of the heavy lifting has been done with regards to receiving, transmitting, and ignoring repeated commands. Ideally, you should only have to edit onCommandST() to handle your new commands, and loop() if you want to add new states, poll sensors, etc. 

Receiving:

On receiving a valid packet, MyState will be set to CommandST, and MyCmd, MyParam, and MyExtParam will be populated.

Add a test for the new command to onCommandST() that calls code to handle your new command. 
If you're receiving a long packet, you can get bytes beyond the first 4 from txrxbuffer[] (these are not stored as a separate variable in order to save on SRAM)


Transmitting:

Populate txrxbuffer with the bytes you want to send (bytes beyond that are ignored), including the size/address byte.  
set TXLength to the length of the packet you are sending

Call doTransmit(rep) to send it, where rep is the number of times you want to repeat the transmission to ensure that it arrives. This blocks until it finishes sending.


The example commands are:

0xF2 - Respond with a 4-byte packet containing the values at 2 locations on the EEPROM, at the addresses (MyParam) and (MyParam+MyExtParam). The first byte is the address of this device, and the first half of the fourth byte is the checksum of the received command (likely not useful here). 

0xF4 - Respond with a packet (MyParam) bytes long, repeated (MyExtParam) times. For testing reception. This calculates the checksum, but doesn't set the size and address byte to anything meaningful.

*/

#define ListenST 1
#define CommandST 2
#include <EEPROM.h>

#define rxpin 9
#define txpin 10
#define CommandForgetTime 10000

//These set the parameters for transmitting. 
#define txOneLength 1100 //length of a 1
#define txZeroLength 600 //length of a 0
#define txLowTime 650 //length of the gap between bits
#define txTrainRep 30 //number of pulses in training burst
#define txSyncTime 2000 //length of sync
#define txTrainLen 400 //length of each pulse in training burst

//These set the parameters for receiving; any packet where these criteria are not met is discarded. 
#define rxSyncMin 1900 //minimum valid sync length
#define rxSyncMax 2100 //maximum valid sync length
#define rxZeroMin 475 //minimum length for a valid 0
#define rxZeroMax 650 //maximum length for a valid 0
#define rxOneMin 1000 //minimum length for a valid 1
#define rxOneMax 1150 //maximum length for a valid 1
#define rxLowMax 800 //longest low before packet discarded



unsigned char MyAddress=31;


byte lastPinState;
unsigned long lastPinHighTime;
unsigned long lastPinLowTime;
unsigned long lastTempHighTime=0;
unsigned long lastTempLowTime=0;
unsigned char rxdata;
byte lastTempPinState;
int bitsrx;
int rxing;
int MyState;
unsigned char MyCmd;
unsigned char MyParam;
unsigned char MyExtParam;
unsigned long curTime;
int count=0;
int badcsc=0;
int pksize=32;
char rxaridx;
unsigned char txrxbuffer[32];
byte TXLength;
unsigned long lastChecksum; //Not the same as the CSC - this is our hack to determine if packets are identical
unsigned long forgetCmdAt; 

void setup() {
	lastPinState=0;
	lastPinLowTime=0;
	lastPinHighTime=0;
	rxdata=0;
	bitsrx=0;
	rxing=0;
	MyState=ListenST;
	//Serial.begin(9600);
	pinMode(txpin, OUTPUT);
	pinMode(rxpin, INPUT);
	pinMode(8, OUTPUT);//for debug
	pinMode(7, OUTPUT);//for debug
	pinMode(6, OUTPUT);//for debug
	digitalWrite(6,1);
	digitalWrite(7,1);
	digitalWrite(8,1);
}


void loop() {
	curTime=micros();
	if (MyState==ListenST) {
		ClearCMD();
		onListenST();
	} else if (MyState==CommandST){ 
		onCommandST();
	} else {
		MyState=ListenST; //in case we get into a bad state somehow.
	}
}



void onCommandST() {

	if (MyCmd==0xF2) {
		//Serial.println("Starting transmit info");
		prepareEEPReadPayload();
		delay(500);
		doTransmit(5);
		MyState=ListenST;
	} else if (MyCmd==0xF4) {
		//Serial.println("Starting transmit");
		//Serial.print(MyParam);
		//Serial.print(" byte payload");
		prepareTestPayload();
		delay(500);
		doTransmit(MyExtParam);
		MyState=ListenST;
	} else {
		//Serial.println("Invalid command type");
		MyState=ListenST;
	}
}


void prepareEEPReadPayload() {
	unsigned char Payload1=EEPROM.read(MyParam);
	unsigned char Payload2=EEPROM.read(MyParam+MyExtParam);
	unsigned char oldcsc=((MyAddress&0xF0)>>4)^(MyAddress&0x0F)^(0x0F)^(0x02)^((MyParam&0xF0)>>4)^(MyParam&0x0F)^(MyExtParam&0x0F);
	txrxbuffer[0]=MyAddress;
	txrxbuffer[1]=EEPROM.read(MyParam);
	txrxbuffer[2]=EEPROM.read(MyParam+MyExtParam);
	unsigned char csc=((MyAddress&0xF0)>>4)^(MyAddress&0x0F)^((MyCmd&0xF0)>>4)^(MyCmd&0x0F)^((MyParam&0xF0)>>4)^(MyParam&0x0F)^(oldcsc&0x0F);
	txrxbuffer[3]=oldcsc<<4+(csc&0x0F);
	TXLength=4;
}

void prepareTestPayload() { //MyParam sets the size of the payload to send.
	//Serial.println("prepareTestPayload called...");
	txrxbuffer[MyParam-1]=0;
	for (byte i=1;i<MyParam;i++) { //if we wanted, we could pick a return address, and start with i=2, and then set up address/etc in txrxbuffer[0]. But this is just a demo.
		txrxbuffer[i-1]=100+i; 
		txrxbuffer[MyParam-1]=txrxbuffer[MyParam-1]^(100+i);
		//Serial.print("B");
		//Serial.print(i);
		//Serial.print(": ");
		//Serial.println(txrxbuffer[i]);
	}
	//Serial.println("Payload generated.");
	TXLength=MyParam;
}

void doTransmit(int rep) { //rep is the number of repetitions
	for (byte r=0;r<rep;r++) {;
		for (byte j=0; j < txTrainRep; j++) {
			delayMicroseconds(txTrainLen);
			digitalWrite(txpin, 1);
			delayMicroseconds(txTrainLen);
			digitalWrite(txpin, 0);
		}
		delayMicroseconds(txSyncTime);
		for (byte k=0;k<TXLength;k++) {
			//send a byte
			for (int m=7;m>=0;m--) {
				digitalWrite(txpin, 1);
				if ((txrxbuffer[k]>>m)&1) {
					delayMicroseconds(txOneLength);
				} else {
					delayMicroseconds(txZeroLength);
				}
				digitalWrite(txpin, 0);
				delayMicroseconds(txLowTime);
			}
			//done with that byte
		}
		//done with sending this packet;
		digitalWrite(txpin, 0); //make sure it's off;
		delayMicroseconds(2000); //wait 2ms before doing the next round. 
	}
	//Serial.println("Transmit done");
	TXLength=0;
}



void onListenST() {
	byte pinState=digitalRead(rxpin);
	if (pinState==lastPinState) {
		return;
	} else {
		lastPinState=pinState;
	}
	if (pinState==0) {
		lastPinLowTime=curTime;
		unsigned long bitlength=lastPinLowTime-lastPinHighTime;
		if (bitlength > 1500) {
			resetListen();
			return;
		} else if (rxing==1) {
			if (bitlength > rxZeroMin && bitlength < rxZeroMax) {
				rxdata=rxdata<<1;
			} else if (bitlength > rxOneMin && bitlength < rxOneMax ) {
				rxdata=(rxdata<<1)+1;
			} else {
				resetListen();
				return;
			}
			bitsrx++;
			if (bitsrx==2) {
				pksize=32<<rxdata;
			} else if (bitsrx==8*(1+rxaridx)) {
				txrxbuffer[rxaridx]=rxdata;
				rxdata=0;
				rxaridx++;
				if (rxaridx*8==pksize) {
					//Serial.println("Receive done");
					parseRx();
					parseRx2(txrxbuffer,pksize/8);
					resetListen();
				}
			}
			return;
		}   
	} else {
		lastPinHighTime=curTime;
		if (lastPinHighTime-lastPinLowTime > rxSyncMin && lastPinHighTime-lastPinLowTime <rxSyncMax && rxing==0) {
			rxing=1;
			return;
		}
		if (lastPinHighTime-lastPinLowTime > rxLowMax && rxing==1) {
			//Serial.println(bitsrx);
			resetListen();
			return;
		}
	}
}




void parseRx() { //uses the globals. 
	//Serial.println("Parsing");
	unsigned char calccsc=0;
	unsigned char rcvAdd=txrxbuffer[0]&0x3F;
	if (rcvAdd==MyAddress) {
		if (lastChecksum!=calcBigChecksum(byte(pksize/8))) {
			lastChecksum=calcBigChecksum(byte(pksize/8));
		    if (pksize==32) { //4 byte packet
		    	calccsc=txrxbuffer[0]^txrxbuffer[1]^txrxbuffer[2];
		    	calccsc=(calccsc&15)^(calccsc>>4)^(txrxbuffer[3]>>4);
		    	if (calccsc==(txrxbuffer[3]&15)) {
		    		MyCmd=txrxbuffer[1];
		    		MyParam=txrxbuffer[2];
		    		MyExtParam=txrxbuffer[3]>>4;
		    		MyState=CommandST;
		    		//Serial.println(MyCmd);
		    		//Serial.println(MyParam);
		    		//Serial.println(MyExtParam);
		    		//Serial.println("Valid transmission received");
	    		} else {
	    			//Serial.println("Bad CSC on 4 byte packet");
	    		}
			} else {
				for (byte i=1;i<(pksize/8);i++) {
					calccsc=calccsc^txrxbuffer[i-1];
				}
				if (calccsc==txrxbuffer[(pksize/8)-1]) {
					MyCmd=txrxbuffer[1];
					MyParam=txrxbuffer[2];
					MyExtParam=txrxbuffer[3];
					MyState=CommandST; //The command state needs to handle the rest of the buffer if sending long commands.  
					//Serial.println(MyCmd);
					//Serial.println(MyParam);
					//Serial.println(MyExtParam);
					//Serial.println("Valid transmission received");
					if (pksize==64) {
						digitalWrite(8,1);
						digitalWrite(7,0);
					} else if (pksize==128) {
						digitalWrite(8,0);
						digitalWrite(7,1);
					} else if (pksize==256){
						digitalWrite(8,0);
						digitalWrite(7,0);
					}
				} else {
					//Serial.println("Bad CSC on long packet");
				}  
			}
		} else {
			//Serial.println("Already got it");
		} 
	} else {
		//Serial.println("Not for me");
	}
}

unsigned long calcBigChecksum(byte len) {
	unsigned long retval=0;
	for (byte i=0;i<len;i++) {
		retval+=(txrxbuffer[i]<<(i>>1));
	}
	return retval;
}

void resetListen() {
    bitsrx=0;
    rxdata=0;
    rxing=0;
    rxaridx=0;
}

//Just for test/debug purposes;
void parseRx2(unsigned char rxd[],byte len) {

	//Serial.println("Parsing long packet");
	for (byte i=0;i<len;i++) {
		//Serial.println(rxd[i]);
	}
	//Serial.println("Done");
}

void ClearCMD() {  //This handles clearing of the commands, and also clears the lastChecksum value, which is used to prevent multiple identical packets received in succession from being processed.
	if (MyCmd) {
		forgetCmdAt=millis()+10000;
		MyParam=0;
		MyExtParam=0;
		MyCmd=0;
	} else if (millis()>forgetCmdAt) {
		forgetCmdAt=0;
		lastChecksum=0;
		digitalWrite(8,1);
		digitalWrite(7,1);
		digitalWrite(6,1);
	}
}
