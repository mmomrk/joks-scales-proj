#include <SD.h>

#include <RTClib.h>
#include <math.h>
#include <Wire.h>
#include <SoftwareSerial.h>

//The circuit:
// * RX is digital pin 10 (connect to TX of other device) //CHANGED TO 8
// * TX is digital pin 11 (connect to RX of other device) //CHANGED TO 9
//uncomment this if the new line does not work
//SoftwareSerial mySerial(10, 11, true); // RX, TX  //Do not refactor this line. DOn't know what this does and don't know how to hid it in the code
SoftwareSerial mySerial(8, 9, true); // RX, TX  //Do not refactor this line. DOn't know what this does and don't know how to hid it in the code
RTC_DS1307 RTC;


#define SCALES_POLL_PERIOD  4
#define AVER_PTS  3 //used for tests
#define MAXLEN 128  //full buffer length. This is double the number of stored points for averageing
#define RHO 1.  //Liquid density. With good precision
const int REFILL_DELAY_SEC = 30;  //30 seconds default
const int MIN_TRUSTED_AVERAGE_TIME_SEC = 30;
const int MASS_PUMP_ON = 1500;
const int MASS_PUMP_OFF = 3200;
const int SD_TIME_DELAY_SEC = 10;//30 seconds ACHTUNG WARNING CHANGE TO SOMETHING MORE SANE FOR PROD


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
        //        Serial.print(mesLeng);
        //        Serial.print(" * ");
        //        Serial.println(input);  //used for debug only
        if (mesLeng == 2 || mesLeng == 7) {
          resultI = input;
          //          Serial.print("lower mass: ");
          //          Serial.println(resultI);
        } else if (mesLeng == 3 || mesLeng == 8) {
          int intput = input;

          result = 256 * intput + resultI;
          if (mesLeng == 8) {
            result = -1; //because the message is repeated two times. I could have inserted check here but not now. TODO later
          }

          //          Serial.print ("Got upper mass: ");
          //          Serial.println(intput);
          //          Serial.print("Got mass: ");
          //          Serial.println(result);
        }
      }
      //      }

      return (result);
    }
};

//String getDateTimeS(void) {
//  DateTime now = RTC.now();
//  String dateTimeString = \
//                          String(now.year(), DEC) + \
//                          String('/') + \
//                          String(now.month(), DEC) + \
//                          String('/') + \
//                          String(now.day(), DEC) + \
//                          String(' ') + \
//                          String(now.hour(), DEC) + \
//                          String(':') + \
//                          String(now.minute(), DEC) + \
//                          String(':') + \
//                          String(now.second(), DEC) + \
//                          String('\n');
//  return dateTimeString;
//}

/*
   A class to get time readings from RTC that basically does the standsrd millis() but with seconds and RTC-based
*/
class Chronometer { //DO NOT MOVE
  private:
    uint32_t startTime = 0;
  public:
    Chronometer() {
      setupRTC();
      reset();
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
    void reset(void) {
      Serial.println("CHR RESET");
      startTime = getSeconds();
    }
    uint32_t curTime() {  //returns seconds
      uint32_t ct ;
      ct = getSeconds() - startTime;
      return (ct);
    }
    uint32_t getSeconds(void) { //unixtime seconds
      DateTime now = RTC.now();
      uint32_t rv = now.unixtime();
      //      Serial.println(rv);
      return rv;
    }
    //    String curDate() {
    //      String rv; //retval
    //      DateTime now = RTC.now();
    //      //      Serial.print(now.year(), DEC);
    //      //      Serial.print('/');
    //      //      Serial.print(now.month(), DEC);
    //      //      Serial.print('/');
    //      //      Serial.print(now.day(), DEC);
    //      //      Serial.print(' ');
    //      //      Serial.print(now.hour(), DEC);
    //      //      Serial.print(':');
    //      //      Serial.print(now.minute(), DEC);
    //      //      Serial.print(':');
    //      //      Serial.print(now.second(), DEC);
    //      //      Serial.println();
    //
    //      String syear = String(now.year(), DEC);
    //      String smonth = String(now.month(), DEC);
    //      String sday = String(now.day(), DEC);
    //      String shour = String(now.hour(), DEC);
    //      String sminute = String(now.minute(), DEC);
    //      String ssecond = String(now.second(), DEC);
    //      rv = String(syear + "-" + smonth + "-" + sday + "-" + shour + "-" + sminute + "-" + ssecond);
    //      Serial.println(rv);
    //      return rv;
    //    }
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
      //      reset();
      //      Serial.println("LR RESET DONE");
      //      dump();
    }

    void dump(int from, int to) {
      Serial.print("dumping from ");
      Serial.print(from);
      Serial.print(" to ");
      Serial.println(to);
      for (int i = from; i < to; i++) {
        Serial.print(x[i], 0.);
        Serial.print(" - ");
        Serial.print(y[i], 0.);
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

    bool enoughPoints(int points) { //this method checks if there is enough data for average value to be correct  //warning: with certain points variable this may always return false because of small time for integration. See TRUSTED_AVERAGE_TIME
      return enoughPoints(getImin(points), getImax(points));
    }

    bool enoughPoints(int imin, int imax) {
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

    float getCoeff(int points) {  //don't forget to call enoughPoints method before using this one. It may return nan and not bother about it
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

class ExpoAverage {
  private:
    const float INTEGRAT_TIME = 100; //sec
    float MULT = .9;
    float bank = 0.;  //or buffer or average or numerator or whatever you call it. I call it bank today
    float dummy = 0;
    float denom = 1;

  public:
    bool firstPut = true;
    void init() {
      MULT = exp(-4. / INTEGRAT_TIME);
      //      Serial.print("MULT IS ");
      //      Serial.println(MULT);
      float sum = 0;
      float prevSum = -1;
      while (sum != prevSum) {
        prevSum = sum;
        sum = sum * MULT + 1;
      }
      denom = sum;
      Serial.print ("Denominator converged to:");
      Serial.println(denom);
    }
    void add(int dt, int dm) {
      if (dt == 0) {
        //        Serial.println("Insertion rejected");
        return;
      } else {
        Serial.print(dt);
        Serial.print(" dt accepted dm ");
        Serial.println(dm);
        float dmbydt = 1.* dm / dt;
        if (firstPut) { //for faster converge

          bank = dmbydt * denom;
          firstPut = false;
        } else {  //for the rest of our lives
          dummy = dummy * MULT + 1;
          bank = bank * MULT + dmbydt;
          //          Serial.print (bank);
          //          Serial.print(" BANK and DENOM ");
          //          Serial.println(denom);
        }
      }
    }
    float getTrustRate() {
      return (dummy / denom);
    }
    float getAver() {
      return bank / denom;
    }
    void reset() {
      dummy = 0;  //to avoid bad transitional effects
      firstPut = true;
    }
};


class SDCard {
  private :
    String filenam;
    const int chipSelect = 4;
    int lineCount = -1;
    const int MAXLINES = 3000;

  public:
    SDCard () {
      init();
      filenam = "err.txt";
    }
    void init() {
      Serial.print("Ini SD");

      // see if the card is present and can be initialized:
      if (!SD.begin(chipSelect)) {
        Serial.println("Card failed");
        // don't do anything more:
        return;
      }
      Serial.println("card initialized.");
    }
    void checkOFAndWrite(String seconds, int mass) {
      Serial.println("Check and write " );
      Serial.println(seconds);
      Serial.println(mass);

      String toWrite = seconds + " " + String(mass);
      Serial.println(toWrite);
      //      Serial.println(seconds);
      Serial.println(filenam );

      if ((lineCount == -1 || lineCount == MAXLINES || filenam == "" || filenam == ".txt") && seconds != "") {
        filenam = seconds.substring(2) + ".txt";
        Serial.println(filenam);
      }
      if (lineCount == -1) {
        lineCount = 0;
      }
      //      checkFileName(seconds);
      write(toWrite);
      lineCount ++;
    }
    //    void checkFileName(String seconds) {
    //      Serial.println(seconds);
    //      Serial.println(filenam == "");
    //      if (lineCount == -1 || lineCount == MAXLINES || filenam == "") {
    //        filenam = seconds + ".txt";
    //        Serial.println(filenam);
    //      }
    //      if (lineCount == -1) {
    //        lineCount = 0;
    //      }
    //    }
    void write(String toWrite) {
      File dataFile = SD.open(filenam , FILE_WRITE);
      // if the file is available, write to it:
      if (dataFile) {
        dataFile.println(toWrite);
        dataFile.close();
      }
      // if the file isn't open, pop up an error:
      else {
        Serial.println("ERROR opening " + filenam);
        //        digitalWrite(13, HIGH);
      }
    }
};

Scales *scales ; //DO NOT MOVE. Or it will not init
Chronometer *chr;
//LinReg *lr;
ExpoAverage *ea;
SDCard *sd;
int prevMass = 0;
long prevRecalcTime;  //msec
long refillEnd;
long prevMassTime;//sec
int requiredFlow  = 0;
long nextSDTime;


void setup() {
  //  pinMode(LED_BUILTIN, OUTPUT);

  setupSerial();
  if (MASS_PUMP_ON > MASS_PUMP_OFF) { //todo: pump handling
    while (true) {
      Serial.print("ASSERT FAILED. BAD MASS PUMP SETTINGS");
    }
  }
  scales = new Scales();
  scales->setupSWSerial();
  chr = new Chronometer();
  chr->  setupRTC();
  ea = new ExpoAverage();
  ea->init();
  //  lr = new LinReg();
  //  lr->reset();
  //  lr->dump();
  sd = new SDCard();
  prevRecalcTime = millis();
  refillEnd = millis();
  prevMassTime = chr->curTime();
  nextSDTime = millis() + SD_TIME_DELAY_SEC * 1000;
  Serial.println("Setup is over");
  Serial.println();
}

void loop() { // run over and over

  int curMass = scales->waitGetReading();
  if (curMass != -1) {
    if (curMass > prevMass) {
      //        refillDelay();
      Serial.println("RESET");
      chr->reset();
      ea->reset();
      //      lr->reset();
      refillEnd = millis() + REFILL_DELAY_SEC * 1000;
    }

    uint32_t curTime = chr->curTime();
    if (curTime > 100000) { //perhaps due to disconnect bad big time reading happens. No idea why
      Serial.println("WARNING: BAD TIME RECEIVED. Dropping input");
      //      continue;
    } else {
      Serial.print(curTime);
      Serial.print(':');
      Serial.println (curMass);

      if (millis() > refillEnd) {
        ea->add(curTime - prevMassTime, prevMass - curMass);
        //      lr->add(curTime, curMass);
      }
      prevMass = curMass;
      prevMassTime = curTime;
      if (millis() > nextSDTime && millis() > refillEnd ) { //todo: refactor this to be hidden inside sd class
        Serial.println("printing to sd");
        nextSDTime = millis() +  SD_TIME_DELAY_SEC * 1000;
        //        sd->checkOFAndWrite(String (chr->getSeconds()), chr->curDate(), curMass);
        uint32_t secs = chr->getSeconds();
        String secsS = String (secs);
        sd->checkOFAndWrite(secsS, curMass);

      } else {
        //        Serial.println("waiting for sd");
      }
    }
  }
  delay(10);  //DO NOT DELETE. WIthout this delay reading scales does not work. If I don't find a reason it will be reasonable to move it to the scales class. TODO, watch it

  if (Serial.available()) {
    String inputS = Serial.readString();
    Serial.println(inputS);
    requiredFlow = inputS.toInt();  //looks pretty ugly, I agree
    Serial.print("Got task flow: ");
    Serial.println(requiredFlow);
  }
  /**
      this block is needed for realtime flow control. Commented out for soujas sd card logger to save space because micro is glitching heavily
  */
  //  int points = 30;
  //  if (millis() - prevRecalcTime > 4000 && lr->enoughPoints(points)) { //removed commented-out section with averaging with diffeent points value. FOr certain reason adding those method calls resulted in complete mess of linear regressor work. Check git if you want to play with it
  //  if (millis() - prevRecalcTime > 10000 ) { //removed commented-out section with averaging with diffeent points value. FOr certain reason adding those method calls resulted in complete mess of linear regressor work. Check git if you want to play with it
  //
  //    Serial.print("Coeff is [ml/hour] :");
  //    prevRecalcTime = millis();
  //    float mlPerHr = grSecToMlHr(ea->getAver());
  //    Serial.println(mlPerHr);
  //    Serial.print("Trust rate: ");
  //    float tr = ea->getTrustRate();
  //    Serial.println(tr);
  //    //UNCOMMENT THIS WHEN CONTROL IS NEEDED:
  //    //    if (tr > .9) {
  //    //      Serial.print("#");
  //    //      Serial.println( requiredFlow / mlPerHr);
  //    //      ea->reset();//WATCH IT TEST IT
  //    //    }
  //  }

}

