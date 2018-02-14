#include <RTClib.h>
#include <math.h>
#include <Wire.h>
#include <SoftwareSerial.h>

//The circuit:
// * RX is digital pin 10 (connect to TX of other device)
// * TX is digital pin 11 (connect to RX of other device)
SoftwareSerial mySerial(10, 11, true); // RX, TX  //Do not refactor this line. DOn't know what this does and don't know how to hid it in the code
RTC_DS1307 RTC;


#define SCALES_WAIT_SEC  5
#define AVER_PTS  3 //used for tests
#define MAXLEN 128  //double buffer for average points
#define RHO 1.

uint32_t getSeconds(void) {
  DateTime now = RTC.now();
  return now.unixtime();
}

void setupRTC(void) {
  Wire.begin();
  RTC.begin();
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
}

void setupSerial(void) {
  // Open serial communications and wait for port to open:
  Serial.begin(57600);
  while (!Serial) {
  }
  Serial.println("Serial is on, babe");
}

class Scales {
  private:

    long prevRecTime = 0;

    byte mesLeng = 0; //todo:rename
  public:    void setupSWSerial() {
      // set the data rate for the SoftwareSerial port
      mySerial.begin(4800);
    }
    int waitGetReading() {
      int result = -1;
      //      Serial.println("Reading scales");
      long callTime = millis();
      bool nok = true;
      while (millis() - callTime < SCALES_WAIT_SEC * 1000 || nok) //I cannot rely on scales messages
      {
        if (mySerial.available()) {

          if (millis() - prevRecTime > 3000) {  //this line is magic but nobody cares.. for now.. watch it
            //            Serial.print("Mes leng: ");
            //            Serial.println(mesLeng);  //I will not delete this line. It was used to confirm that datasheet is wrong and message length is variable. Brilliant, devs!
            mesLeng = 0;
          } else {
            mesLeng++;
            if (mesLeng > 7) {
              nok = false;
            }
          }
          prevRecTime = millis();

          byte input = mySerial.read();
          //          Serial.print(mesLeng);
          //          Serial.print(" - ");
          //          Serial.println(input);  //used for debug only
          if (mesLeng == 2 || mesLeng == 7) {
            result = input;
            Serial.print("lower mass: ");
            Serial.println(result);
          } else if (mesLeng == 3 || mesLeng == 8) {
            int intput = input;
            result += 256 * intput;
            Serial.print ("Got upper mass: ");
            Serial.println(intput);
          }


          //Useless block:
          if (Serial.available()) { //this block handles USB communication. Basically useless
            int input = Serial.read();
            Serial.print("Received this: ");
            Serial.print(input);
            Serial.println("But have no idea what to do with it. Thanks for support anyway.");
          } else {
            digitalWrite(LED_BUILTIN, HIGH);
          }
          //Endof useless block
        }
      }
      Serial.print("Got mass: ");
      Serial.println(result);
      return (result);
    }

};


String getDateTimeS(void) {
  DateTime now = RTC.now();
  String dateTimeString = \
                          String(now.year(), DEC) + \
                          String('/') + \
                          String(now.month(), DEC) + \
                          String('/') + \
                          String(now.day(), DEC) + \
                          String(' ') + \
                          String(now.hour(), DEC) + \
                          String(':') + \
                          String(now.minute(), DEC) + \
                          String(':') + \
                          String(now.second(), DEC) + \
                          String('\n');
  return dateTimeString;
}

/*
   A class to get time readings from RTC that basically does the standsrd millis() but with seconds and RTC-based
*/
class Chronometer { //DO NOT MOVE
  private:
    uint32_t startTime = 0;
  public:
    Chronometer() {
      reset();
    }
    void reset(void) {
      startTime = getSeconds();
    }
    uint32_t curTime() {
      uint32_t ct ;
      ct = getSeconds() - startTime;
      return (ct);
    }
};


class LinReg {
  private:
    //    int maxLen = 128; #defined
    int n;
    float x[MAXLEN], y[MAXLEN];
  public:
    LinReg() {
      reset();
    }

    void dump(int from, int to) {
      Serial.print("dumping from ");
      Serial.print(from);
      Serial.print(" to ");
      Serial.println(to);
      for (int i = from; i < to; i++) {
        Serial.print(x[i], 0);
        Serial.print(" - ");
        Serial.print(y[i], 0);
        Serial.println('\t');
      }
    }
    void dump() {
      int nw = n % (MAXLEN / 2);
      int imin = 0;
      int imax = n;
      if (n > MAXLEN / 2) {
        imax = nw + MAXLEN / 2;
        imin = nw + 1;
      }
      dump(imin, imax);
    }
    void reset() {
      Serial.println("Resetting regressor because weights grew");
      n = 0;
      for (int i = 0; i < MAXLEN; i++) {
        x[i] = 0;
        y[i] = 0;
      }
    }

    void add (float xn, float yn) {
      int nw = n % (MAXLEN / 2);
      x[nw] = xn;
      y[nw] = yn;
      x[MAXLEN / 2 + nw] = xn;
      y[MAXLEN / 2 + nw] = yn;
      n++;
    }

    float getCoeff(int points) {
      Serial.print("Getting coeff by points: ");
      Serial.println(points);
      if (points > MAXLEN / 2) {
        Serial.println("ERROR: bad points came to average");
        points = MAXLEN / 2;
      }
      // initialize variables
      float xbar = 0;
      float ybar = 0;
      float xybar = 0;
      float xsqbar = 0;
      float lrCoef[2] = {0, 0};

      // calculations required for linear regression
      int imin = 0;
      int imax = n;
      if (n > points) { //or is it >=? todo think
        imin = imax - points;
      }
      if (n > MAXLEN / 2) {
        int nw = n % (MAXLEN / 2);
        imax = nw + MAXLEN / 2;
        imin = imax - points;
      }
      dump(imin, imax);
      float t = x[imax - 1] - x[imin];
      Serial.print("Averaging period seconds ");
      Serial.println(t);
      Serial.print ("cur n");
      Serial.println(n);

      for (int i = imin; i < imax; i++) {
        xbar = xbar + x[i];
        ybar = ybar + y[i];
        xybar = xybar + x[i] * y[i];
        xsqbar = xsqbar + x[i] * x[i];
      }
      int npts = imax - imin;
      xbar = xbar / npts;
      ybar = ybar / npts;
      xybar = xybar / npts;
      xsqbar = xsqbar / npts;

      //simple, stupid
      float dx = 0;
      float dy = 0;
      float numM = 0;
      float denM = 0;
      for (int i = imin; i < imax; i++) {
        dx = x[i] - xbar;
        dy = y[i] - ybar;
        numM += dx * dy;
        denM += dx * dx;
      }

      return ( numM / denM);
    }
};

Scales *scales ; //DO NOT MOVE. Or it will not init
Chronometer *chr;
LinReg *lr;
int prevMass = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setupSerial();
  scales = new Scales();
  scales->setupSWSerial();
  setupRTC();
  chr = new Chronometer();
  lr = new LinReg();
  prevMass = scales->waitGetReading();
  Serial.println("Setup is over");
}

float grSecToMlHr(float grSec) {
  float ans = grSecToMlMin(grSec) * 60;
  return ans;
}
float grSecToMlMin(float grSec) {
  Serial.println("WARNING: FILL THE LIQUID DENSITY FOR TRANSLATION TO WORK");
  float ans = grSec / RHO * 60;
  return ans;
}

void loop() { // run over and over
  for (int i = 0; i < AVER_PTS; i++) {
    int curMass = scales->waitGetReading();
    if (curMass != -1) {
      if (curMass > prevMass) {
        lr->reset();
        chr->reset();
      }
      Serial.print(chr->curTime());
      Serial.print(':');
      Serial.println (curMass);
      lr->add(chr->curTime(), curMass);
      prevMass = curMass;
    }
    delay(1000);
  }

  //  lr->dump();

  float fast = lr->getCoeff(MAXLEN / 32);
  //  float mid = lr->getCoeff(MAXLEN / 8);
  //  float slow = lr->getCoeff(MAXLEN / 2);
  Serial.print("Coeff is [gramm/sec] fast,mid,slow:");
  Serial.println(fast, 4);
  //  Serial.println(mid, 4);
  //  Serial.println(slow, 4);
  //  Serial.print("Coeff is [gramm/min] fast,mid,slow:");
  //  Serial.println(grSecToMlMin(fast), 4);
  //  Serial.println(grSecToMlMin(mid), 4);
  //  Serial.println(grSecToMlMin(slow), 4);
  //  Serial.print("Coeff is [gramm/hour] fast,mid,slow:");
  //  Serial.println(grSecToMlHr(fast), 4);
  //  Serial.println(grSecToMlHr(mid), 4);
  //  Serial.println(grSecToMlHr(slow), 4);
}


