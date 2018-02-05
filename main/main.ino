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
        if (millis() - prevRecTime > 1000) {  //this line is magic but nobody cares.. for now.. watch it
          //          Serial.println(mesLeng);  //I will not delete this line. It was used to confirm that datasheet is wrong and message length is variable. Brilliant, devs!
          mesLeng = 0;
        } else {
          mesLeng++;
        }
        prevRecTime = millis();

        byte input = mySerial.read();

        if (mesLeng == 2 || mesLeng == 7) {
          result = input;

        } else if (mesLeng == 3 || mesLeng == 8) {
          result += 256 * input;
          Serial.print ("Got mass: ");
          Serial.println(result);
        }

        if (Serial.available()) {
          int input = Serial.read();
          Serial.print("Received this: ");
          Serial.print(input);
          Serial.println("But have no idea what to do with it. Thanks for support anyway.");
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
  scales->listenScales();
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

