#include <RTClib.h>
#include <math.h>
#include <Wire.h>
#include <SoftwareSerial.h>

//The circuit:
// * RX is digital pin 10 (connect to TX of other device)
// * TX is digital pin 11 (connect to RX of other device)
SoftwareSerial mySerial(10, 11, true); // RX, TX  //Do not refactor this line. DOn't know what this does and don't know how to hid it in the code
RTC_DS1307 RTC;


#define SCALES_POLL_PERIOD  4
#define AVER_PTS  3 //used for tests
#define MAXLEN 128  //full buffer length. This is double the number of stored points for averageing
#define RHO 1.  //Liquid density. With good precision
int REFILL_DELAY_SEC = 30;  //seconds
int MIN_TRUSTED_AVERAGE_TIME_SEC = 30;

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
  Serial.println("Serial is on, babe. Let's get to work");
}

class Scales {
  private:

    long prevRecTime = 0;
    int resultI = -1; //this is the variable where actual mass is stored during two passes of the method
    byte mesLeng = 0; //todo:rename
  public:
    void setupSWSerial() {
      // set the data rate for the SoftwareSerial port
      mySerial.begin(4800);
    }
    int waitGetReading() {


      int result = -1;  //intermediate result that is return value
      //      Serial.println("Reading scales");
      //      long callTime = millis();
      if (millis() - prevRecTime > SCALES_POLL_PERIOD * 1000 ) //DEBUG
      {
        //        Serial.println("Waiting 10 seconds before sending poll command");
        //        delay(10000);
        byte command = 74;
        mySerial.write(command);

      }
      if (mySerial.available()) {

        if (millis() - prevRecTime > 1000) {  //this line is magic but nobody cares.. for now.. watch it
          //            Serial.print("Mes leng: ");
          //            Serial.println(mesLeng);  //I will not delete this line. It was used to confirm that datasheet is wrong and message length is variable. Brilliant, devs!
          mesLeng = 0;
          resultI = -1;

        } else {
          mesLeng++;
        }
        prevRecTime = millis();

        byte input = mySerial.read();
        Serial.print(mesLeng);
        Serial.print(" * ");
        Serial.println(input);  //used for debug only
        if (mesLeng == 2 || mesLeng == 7) {
          resultI = input;
          Serial.print("lower mass: ");
          Serial.println(resultI);
        } else if (mesLeng == 3 || mesLeng == 8) {
          int intput = input;

          result = 256 * intput + resultI;
          if (mesLeng == 8) {
            result = -1; //because the message is repeated two times. I could have inserted check here but not now. TODO later
          }

          Serial.print ("Got upper mass: ");
          Serial.println(intput);
          Serial.print("Got mass: ");
          Serial.println(result);
        }
      }
      //      }

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
      Serial.println("CHR RESET");
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

    int getImax(int points) { //todo refactor
      // calculations required for linear regression
      int imin = 0;
      int imax = n;
      //      if (n > points) { //or is it >=? todo think
      //        imin = imax - points;
      //      }
      if (n > MAXLEN / 2) {
        int nw = n % (MAXLEN / 2);
        imax = nw + MAXLEN / 2;
      }
      return (imax);
    }

    int getImin(int points ) { //todo refactor
      // calculations required for linear regression
      int imin = getImax(points) - points;
      if (imin < 0) {
        imin = 0;
      }
      return (imin);
    }
  public:
    LinReg() {
      reset();
      Serial.println("LR RESET DONE");
      dump();
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
      Serial.println("DUMP CURRENT n");
      Serial.println(n);

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
      Serial.println("Resetting regressor");
      Serial.print ("BEFORE RESET CURRENT n = ");
      Serial.println(n);
      n = 0;
      Serial.print ("DONE RESET CURRENT n = ");
      Serial.println(n);
      for (int i = 0; i < MAXLEN; i++) {
        x[i] = 0;
        y[i] = 0;
      }
      Serial.print ("AFTER RESET CURRENT n = ");
      Serial.println(n);

    }

    void add (float xn, float yn) {
      Serial.print(n);
      int nw = n % (MAXLEN / 2);
      x[nw] = xn;
      y[nw] = yn;
      x[MAXLEN / 2 + nw] = xn;
      y[MAXLEN / 2 + nw] = yn;
      n++;
      Serial.println(" ADD CURRENT n, nw");
      Serial.println(n);
      Serial.println(nw);
      Serial.println("I HAVE PUT TIME AND MASS:");
      Serial.println(x[nw]);
      Serial.println(y[nw]);
    }

    bool canBeTrusted(int points) { //this method checks if there is enough data for average value to be correct  //warning: with certain points variable this may always return false because of small time for integration. See TRUSTED_AVERAGE_TIME
      return canBeTrusted(getImin(points), getImax(points));
    }

    bool canBeTrusted(int imin, int imax) {
      Serial.print(imin);
      Serial.print (" can be trusted ");
      Serial.println(imax);
      dump(imin, imax);
      float t = x[imax - 1] - x[imin];
      Serial.print("Averaging period seconds ");
      Serial.println(t);
      if (t < MIN_TRUSTED_AVERAGE_TIME_SEC) {
        //        Serial.print("Time too smal ");
        //        Serial.println(MIN_TRUSTED_AVERAGE_TIME_SEC);
        return (false);
      } else {
        return (true);
      }
    }

    float getCoeff(int points) {  //don't forget to call canBeTrusted method before using this one. It may return nan and not bother about it
      Serial.print("Getting coeff by points: ");
      Serial.println(points);
      if (points > MAXLEN / 2) {
        Serial.println("ERROR: bad points count came to average");
        points = MAXLEN / 2;
      }
      // initialize variables
      float xbar = 0;
      float ybar = 0;
      float xybar = 0;
      float xsqbar = 0;
      float lrCoef[2] = {0, 0};

      int imin = getImin(points);
      int imax = getImax(points);
      //      dump(imin, imax); //debug

      //      Serial.print ("cur n ");
      //      Serial.println(n);

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


//void refillDelay() {  //not good. blocking. Makes scales class return stupid things that cause more delay. Stupid
//  Serial.print("Refill detected. Waiting ");
//  Serial.print(REFILL_DELAY);
//  Serial.println(" seconds before resuming operation");
//  for (int i = REFILL_DELAY; i > 0; i--) {
//    Serial.println(i - 1); //to pass a satisfying 0 seconds in the end
//    delay(1000);
//  }
//}

float grSecToMlHr(float grSec) {
  float ans = grSecToMlMin(grSec) * 60;
  return ans;
}

float grSecToMlMin(float grSec) {
  float ans = grSec / RHO * 60;
  return ans;
}


Scales *scales ; //DO NOT MOVE. Or it will not init
Chronometer *chr;
LinReg *lr;
int prevMass = 0;
long prevRecalcTime;
long refillEnd;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setupSerial();
  scales = new Scales();
  scales->setupSWSerial();
  setupRTC();
  chr = new Chronometer();
  lr = new LinReg();
  prevMass = scales->waitGetReading();
  prevRecalcTime = millis();
  refillEnd = millis();
  Serial.println("Setup is over");
  Serial.println();
}

void loop() { // run over and over

  for (int i = 0; i < AVER_PTS; i++) {
    int curMass = scales->waitGetReading();
    if (curMass != -1) {
      if (curMass > prevMass) {
        //        refillDelay();
        Serial.print(prevMass);
        chr->reset();
        lr->reset();
        refillEnd = millis() + REFILL_DELAY_SEC * 1000;
      }
      uint32_t curTime = chr->curTime();
      Serial.print(curTime);
      Serial.print(':');
      Serial.println (curMass);

      //      Serial.println("I AM PUTTING TIME AND MASS:");
      //      Serial.println(curTime);
      //      Serial.println(curMass);
      if (millis() > refillEnd) {
        lr->add(curTime, curMass);
      }
      prevMass = curMass;
    }
    delay(10);  //DO NOT DELETE. WIthout this delay reading scales does not work. If I don't find a reason it will be reasonable to move it to the scales class. TODO, watch it
  }

  if (Serial.available()) {
    String inputS = Serial.readString();
    Serial.println(inputS);
  }

  int points = MAXLEN / 2;  //to average. Number of points. Not time. Ideally it should be measure in minimal acceptable error. But not yet. TODO maybe
  if (millis() - prevRecalcTime > 4000 && lr->canBeTrusted(points)) { //removed commented-out section with averaging with diffeent points value. FOr certain reason adding those method calls resulted in complete mess of linear regressor work. Check git if you want to play with it
    float slow = lr->getCoeff(points );
    Serial.print("Coeff is [ml/hour] _fast_,mid,slow:");
    Serial.print(grSecToMlHr(slow), 1);
    Serial.println();
    prevRecalcTime = millis();
  }
}

