//#include <SD.h> //search #SDCARD for blocks related
#include <RTClib.h>
#include <math.h>
#include <Wire.h>
#include <SoftwareSerial.h>


//Since this code is to be used for several tasks and I cannot compile bloat and push it to ardy MC completely AND I am noob enough to implement it via automated code generation
//neither I am familiar with git submodules nor other stuff that could POTENTIALLY help me, some of the parts are to be commented out or in.
// Use hashtags for the purposes:
// #FLOWCALC, #SDCARD, #CONTROL, #PUMP

//======================= The circuit =======================
// == SoftwareSerial:
// >>>>>>> ACHTUNG: Pinout depends on cable. This pinout is for CONNFLY cable which has rx-tx crossed. Normal cables would not do it <<<<<<
// * RX is digital pin 10(CHANGED TO 8) (connect to TX of other device)
// * TX is digital pin 11(CHANGED TO 9) (connect to RX of other device)
// By the time this comment is written the setup works with this:
// pin8 - orange
// pin9 - red
// == DB9 plug <-> ardy board:
// 5 <-> GND
// 2 <-> D9
// 3 <-> D8
// == RTC <-> ardy board:
// GND <-> GND
// VCC <-> Vin
// SDA <-> A4
// SCL <-> A5
// == SD cardreader <-> ardy board:
// GND  <-> GND
// VCC  <-> 5V
// MISO <-> D12
// MOSI <-> D11
// SCK  <-> D13
// CS   <-> D4
// == LongerPump <-> ardy ( http://www.longerpump.com/index.php/OtherTechnicalArticles/show/152.html )
// 1 <-> +5V* (speed control; 3.3V for slow debug)  [yellow]
// 2 <-> D7 (?on-off switch. Does not work when to +5V?)  [orange]
// 4 <-> GND  [green to 5]
// 5 <-> GND  [black]
//======================= Endof Circuit =======================


SoftwareSerial mySerial(8, 9, true); // RX, TX  //Do not refactor this line. DOn't know what this does and don't know how to hid it in the code
RTC_DS1307 RTC;


#define SCALES_POLL_PERIOD  10 //sec
//also check //const float INTEGRAT_TIME = 100; //sec for configuring averager
#define AVER_PTS  3 //used for tests  //probably obsolete. Watch it, refactor candidate
#define MAXLEN 128  //full buffer length. This is double the number of stored points for averageing //probably obsolete. Watch it, refactor candidate
#define RHO 1.  //Liquid density. With good precision
#define PIN_PUMP 7  //0v for ON state, +5v for OFF state
const int REFILL_DELAY_SEC = 30;  //30 seconds default //obsolete?
const int MIN_TRUSTED_AVERAGE_TIME_SEC = 30;

//2300-800+600
const int MASS_PUMP_ON = 1500; //gr   #PUMP
const int MASS_PUMP_OFF = 2900; //gr  #PUMP
//const int SD_TIME_DELAY_SEC = 30;//30 seconds ACHTUNG WARNING CHANGE TO SOMETHING MORE SANE FOR PROD #SDCARD


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
    int sentRequest = 0;
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
        sentRequest = 1;
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
      } else if (sentRequest > 0) {
        if (sentRequest > 1) {  //perhaps looping is too fast for serial to react. Yet it always outputs one error message before receiving the data
          Serial.println("ERROR: NO SCALES RESPONCE");
        } else {
          sentRequest++;
        }
      }
      sentRequest = false;
      //      }
      return (result);
    }
};

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
};

float grSecToMlHr(float grSec) {
  float ans = grSecToMlMin(grSec) * 60;
  return ans;
}

float grSecToMlMin(float grSec) {
  float ans = grSec / RHO * 60;
  return ans;
}

//#FLOWCALC
class ExpoAverage {
  private:
    const float INTEGRAT_TIME = 150; //sec
    float MULT = .9;
    float bank = 0.;  //or buffer or average or numerator or whatever you call it. I call it bank today
    float dummy = 0;
    float denom = 1;

  public:
    bool firstPut = true;
    void init() {
      firstPut = true;
      MULT = exp(- SCALES_POLL_PERIOD * 1. / INTEGRAT_TIME);  //I am averaging speeds with equal weights. Exp argument is 1/N where N is the number of readings\calls\points
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
      if (dt <= 0 || dt > 100) {  //magic constant 100. Used to deal with millis overflow. May fail though
        //        Serial.println("Insertion rejected");
        return;
      } else {
        Serial.print(dt);
        Serial.print(" dt accepted dm ");
        Serial.println(dm);
        float dmbydt = 1.* dm / dt;
        if (firstPut) { //for faster converge
          Serial.println("Averager starting now");
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
//ENDOF #FLOWCALC

/*
  //#SDCARD
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
  //#SDCARD endof class
*/

//#PUMP  TODO: add timer condition to poweroff the pump. Perhaps Class wrap would do good here too.
void pumpInit() {
  pinMode(PIN_PUMP, OUTPUT);
  pumpOff();
}
void pumpOn() {
  Serial.println("Pump on");
  digitalWrite(PIN_PUMP, LOW);
}
void pumpOff() {
  Serial.println("Pump off");
  digitalWrite(PIN_PUMP, HIGH);
}

void pumpControl(int currentMass) {
  if (currentMass < 0) {
    return ;
  }
  if (currentMass < MASS_PUMP_ON) {
    pumpOn();
  }
  if (currentMass > MASS_PUMP_OFF) {  //TODO add panic button or something like that
    pumpOff();
  }
}
bool pumpIsOn() {
  return !digitalRead(PIN_PUMP);  //not tested. No reason to worry
}
//ENDOF #PUMP

Scales *scales ; //DO NOT MOVE. Or it will not init
Chronometer *chr;
//LinReg *lr;
ExpoAverage *ea;
//SDCard *sd; //#SDCARD
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
  pumpInit();//#PUMP
  scales = new Scales();
  scales -> setupSWSerial();
  chr = new Chronometer();
  chr -> setupRTC();
  ea = new ExpoAverage();
  ea -> init(); //todo make expoaverage calculate multiplier dependent on needed time constant and whatever else if it has not been done already
  //  sd = new SDCard();  //#SDCARD
  //  nextSDTime = millis() + SD_TIME_DELAY_SEC * 1000; //#SDCARD

  prevRecalcTime = millis();
  refillEnd = millis();
  prevMassTime = chr->curTime();

  Serial.println("Setup is over");
  Serial.println();
}

void loop() { // run over and over
  if (0xFFFFFFFF - millis() < 5000) { //Happy Millenium!
    delay(6000);  //Wait for it
    setup();  //Start a new life
  }


  int curMass = scales->waitGetReading();
  if (curMass != -1) {
    if (curMass > prevMass) {
      Serial.println("RESET");
      chr->reset();
      ea->reset();  //there are two points of reset in the code
      refillEnd = millis() + REFILL_DELAY_SEC * 1000; //WATCH IT TODO REFACTOR TO INCLUDE PUMPONFLAG #PUMP
    }

    uint32_t curTime = chr->curTime();
    if (curTime > 100000) { //perhaps due to disconnect bad big time reading happens. No idea why
      Serial.println("WARNING: BAD TIME RECEIVED. Dropping input");
      //      continue;
    } else {
      Serial.print(curTime);
      Serial.print(":%");
      Serial.print(curMass);
      Serial.println("^");

      if (millis() > refillEnd && !pumpIsOn()) { //WATCH IT TODO REFACTOR TO INCLUDE PUMPONFLAG #PUMP
        ea->add(curTime - prevMassTime, prevMass - curMass);
      } else {
        ea->reset();  //there are two points of reset in the code
      }
      prevMass = curMass;
      prevMassTime = curTime;
      /*
        //      // #SDCARD
        //      if (millis() > nextSDTime && millis() > refillEnd ) { //todo: refactor this to be hidden inside sd class
        //        Serial.println("printing to sd");
        //        nextSDTime = millis() +  SD_TIME_DELAY_SEC * 1000;
        //        //        sd->checkOFAndWrite(String (chr->getSeconds()), chr->curDate(), curMass);
        //        uint32_t secs = chr->getSeconds();
        //        String secsS = String (secs);
        //        sd->checkOFAndWrite(secsS, curMass);
        //
        //      } else {
        //        //        Serial.println("waiting for sd");
        //      }
        //ENDOF #SDCARD block
      */
    }
  }
  delay(100);  //DO NOT DELETE. WIthout this delay reading scales does not work. If I don't find a reason it will be reasonable to move it to the scales class. TODO, watch it

  if (Serial.available()) {
    String inputS = Serial.readString();
    Serial.println(inputS);
    requiredFlow = inputS.toInt();  //looks pretty ugly, I agree
    Serial.print("Got task flow: ");
    Serial.println(requiredFlow);
  }
  /**
    this block is needed for realtime flow control. Commented out for soujas sd card logger to save space because MC is glitching heavily
  */
  //#FLOWCALC
  int points = 30;
  //  if (millis() - prevRecalcTime > 4000 && lr->enoughPoints(points)) { //removed commented-out section with averaging with diffeent points value. FOr certain reason adding those method calls resulted in complete mess of linear regressor work. Check git if you want to play with it
  if (millis() - prevRecalcTime > 10000 ) { //removed commented-out section with averaging with diffeent points value. FOr certain reason adding those method calls resulted in complete mess of linear regressor work. Check git if you want to play with it

    Serial.print("Coeff is [ml/hour] :$");
    prevRecalcTime = millis();
    float mlPerHr = grSecToMlHr(ea->getAver());
    Serial.print(mlPerHr);
    Serial.println("^");
    Serial.print("Trust rate: @");
    float tr = ea->getTrustRate();
    Serial.print(tr);
    Serial.println("^");
    //UNCOMMENT THIS WHEN #CONTROL IS NEEDED:
    if (tr > .96) { //todo unmagic const
      Serial.print("~");
      Serial.print( requiredFlow / mlPerHr);
      Serial.println("^");
      ea->reset();//WATCH IT TEST IT
    }
  }
  pumpControl(curMass); //#PUMP
};
