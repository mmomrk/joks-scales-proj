#include <SoftwareSerial.h>

//The circuit:
// * RX is digital pin 10 (connect to TX of other device)
// * TX is digital pin 11 (connect to RX of other device)
SoftwareSerial mySerial(10, 11, true); // RX, TX
class Scales {
  private:
    
    long prevRecTime = 0;
    int result = 0;
    byte mesLeng = 0; //todo:rename
  public:    void setupSerials() {
      // Open serial communications and wait for port to open:
      Serial.begin(57600);
      while (!Serial) {
      }
      Serial.println("Serial is on, babe");
      // set the data rate for the SoftwareSerial port
      mySerial.begin(4800);
    }
    void listenScales() {
      if (mySerial.available()) {
        if (millis() - prevRecTime > 1000) {
          Serial.println(mesLeng);
          mesLeng = 0;
        } else {
          mesLeng++;
        }
        prevRecTime = millis();
        
     

        byte input = mySerial.read();
        //    raw = sprintf("08D
        byte revput = (input);
        unsigned int uint = revput & 255;

        serPrintLeadingZ(input);
        Serial.print(input, BIN);
        Serial.print("\t");
        serPrintLeadingZ(uint);
        Serial.print(uint, BIN);
        Serial.print("\t");

        if (mesLeng == 2 || mesLeng == 7) {
          result = uint;
          Serial.print(uint);
        } else if (mesLeng == 3 || mesLeng == 8) {
          result += 256 * uint;
          Serial.print (result);
        } else {
          Serial.print("0x");
          Serial.print(uint, HEX);
        }
        Serial.println();
        if (Serial.available()) {
          int input = Serial.read();
          Serial.print("Received this: ");
          Serial.print(input);
          Serial.println("But have no idea what to do with it");
//          mySerial.write(b);
        }
      }
    }
  

};


Scales *scales ;

void setup() {
  scales = new Scales();
  scales->setupSerials();

}





void loop() { // run over and over

}

byte bitswap (byte x)
{
  byte result;

  asm("mov __tmp_reg__, %[in] \n\t"
      "lsl __tmp_reg__  \n\t"   /* shift out high bit to carry */
      "ror %[out] \n\t"  /* rotate carry __tmp_reg__to low bit (eventually) */
      "lsl __tmp_reg__  \n\t"   /* 2 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 3 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 4 */
      "ror %[out] \n\t"

      "lsl __tmp_reg__  \n\t"   /* 5 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 6 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 7 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 8 */
      "ror %[out] \n\t"
      : [out] "=r" (result) : [in] "r" (x));
  return (result);
}

void serPrintLeadingZ(byte questioned) {
  for (int i = 0; i < 8; i++)
  {
    if (questioned < pow(2, i)) {
      Serial.print(B0);
    }
  }
}

