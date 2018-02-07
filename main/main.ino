#include <Wire.h>

#include <SoftwareSerial.h>

//The circuit:
// * RX is digital pin 10 (connect to TX of other device)
// * TX is digital pin 11 (connect to RX of other device)
SoftwareSerial mySerial(10, 11, true); // RX, TX  //Do not refactor this line. DOn't know what this does and don't know how to hid it in the code


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


