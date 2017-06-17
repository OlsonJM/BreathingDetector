/*************************************************** 
  Breathing Monitor
    - Measures air temperature using MLX90614 sensor
    - User sets patient detect temperature (minimum temp)
      using "C" button.
    - User sets exhale detection temperature (threshold temp)
      using "B" button.
    - Temperatures in Degrees "F"
    

   Hardware:
      (1) Adafruit Feather M0 Addalogger: https://www.adafruit.com/product/2796
      (1) Adafruit Feather OLED display: https://www.adafruit.com/product/2900
      (1) Adafruit Feather Backplane: https://www.adafruit.com/product/2890
      (1) Melexis MLX90614 IR Sensor:https://www.adafruit.com/product/1747
          Sensor Part#: MLX90614ESF-DAA (3Vdc Medical Accuracy version, using I2C bus)
          Spec Sheet: https://www.melexis.com/-/media/files/documents/datasheets/mlx90614-datasheet-melexis.pdf
      (1) 5mm RED LED       (ALARM)
      (1) 5mm GREEN LED     (EXHALE)
      (1) 5mm WHITE LED     (INHALE)
      (1) 5mm BLUE LED      (PATIENT PRESENT)
      (4) 1k, 1/4W resistor (for LED's)

  Author: James M. Olson
  
         
 ****************************************************/


#include <Wire.h>                       //Feather Wiring Library
#include <Adafruit_MLX90614.h>          //Adafruit MLX IR sensor library      
#include <Adafruit_FeatherOLED.h>       //Feather OLED Display library

#define VBATPIN A7                      //Feather Analog In for Measuring 5V Battery
#define TEMP_THRESHOLD 87.0             //Default exhale temperature threshold
#define MIN_TEMP 80.0                   //Default patient detector temperature
#define PATIENT_HYST 2                  //min +deg for patient present/absent. deltaT require to detect patient
#define EXHALE_HYST 1                   //min +deg required state change from inhale to exhale
#define EXHALE true                     //Exhale = true state
#define INHALE false                    //Inhale = false state
#define BUTTON_B 6                      //"B" button pin #
#define BUTTON_C 5                      //"C" button pin #
#define EXHALE_PIN 11                   //Pin # used to indicate exhale detected
#define INHALE_PIN 12                   //Pin # used to indicae inhale detected
#define PATIENT_PIN 10                  //Pin # used to indicate patient detected
#define ALARM_PIN 13                    //Pin # used to indicate alarm condition
#define MEASURE_DELAY_MILLIS 100         //milliseconds between measurement cycles
#define OLED_SET_CHG_MILLIS 1000        //milliseconds to hold the display for changes threshold temps
#define ALARM_DELAY_SEC 7               //seconds to set alarm if state change does not occur
#define MEASURE_AVG 10                  //number of measurements to use to determine temperature
#define MEASURE_AVG_MILLIS 5            //milliseconds between averaging measurements
#define SERIAL_BAUD 19200               //Serial port baud rate
#define ENABLE_SERIAL                   //COMMENT THIS LINE OUT TO DISABLE SERIAL
//#define SERIAL_DATA                     //COMMENT THIS LINE OUT TO DISABLE data feed to serial port (requires ENABLE_SERIAL)
#define DISPLAY                         //COMMENT THIS LINE OUT TO DISABLE LOCAL DISPLAY
#define DEBUG                           //UNCOMMENT TO ENABLE DEBUG VIA SERIAL (requires ENABLE_SERIAL)


/***************************
 *  GLOBAL VARIABLES
 */
Adafruit_MLX90614 mlx = Adafruit_MLX90614();              //MLX sensor
#ifdef DISPLAY
  Adafruit_FeatherOLED oled = Adafruit_FeatherOLED();       //OLED display
#endif

float objT= 0.0f;                                         //Object Temperature
float tempThreshold = TEMP_THRESHOLD;                     //Exhale threshold temperature
float minTemp = MIN_TEMP;                                 //Patient detetor temperature
bool state = INHALE;                                      //Inhale/exhale state (Default=inhale)
bool prevState = INHALE;                                  //Previous state (for detectnig change)
bool patient = false;                                     //FLAG:  Patient present state
bool alarm = false;                                       //FLAG:  Alarm Status
bool thresholdSet = false;                                //FLAG:  Interrupt flag for setting Exhale threshold
bool minimumSet = false;                                  //FLAG:  Interrupt flag for setting Patient detector temp
bool thresholdSetDisp = false;                            //FLAG:  flag for exhale threshold change.
bool minimumSetDisp = false;                              //FLAG:  flag for patient detector temp change.
long stateTime = 0;                                       //Unix time stamp for current state
long statePrevTime = 0;                                   //Unix stamp from previous state change
long bpmTime = 0;                                         //Elapse time between respitory cycle
long bpmPrevTime = 0;                                     //Start of respitory cycle
float bpm = 0.0f;                                         //calculated breaths per minute (BPM)
char exhaleCtr = 0;                                       //exhale counter for BPM calculator

/**************************
 *  FUNCTION PROTOTYPES
 */
void setThreshold();                                      //INTERRUPT routine for setting exhale temp threshold
void setMinimum();                                        //INTERRUPT routine for setting patient detecor temp
void updateDisplay();                                     //Updates the OLED display
float getBatteryVoltage();                                //Measures the battery voltage and returns value in Volts DC
void updateSerial();                                      //Send bitstatus via serial port

/***************
 *  Device Initialization
 */
void setup() {
  #ifdef ENABLE_SERIAL
    Serial.begin(SERIAL_BAUD);                            //Open serial port for passing data to Serial interface
  #endif
  pinMode(EXHALE_PIN,OUTPUT);                             //Output for Exhale detected
  pinMode(INHALE_PIN,OUTPUT);                             //Output for Inhale condition 
  pinMode(ALARM_PIN,OUTPUT);                              //Output to indicate alarm condition
  pinMode(PATIENT_PIN,OUTPUT);                            //Output to indicate patient is detected
  pinMode(BUTTON_B, INPUT_PULLUP);                        //Input to set exhale temp threshold
  pinMode(BUTTON_C, INPUT_PULLUP);                        //Input to set patient detector temp

  #ifdef DISPLAY
    oled.init();                                          //Initialize OLED display
    oled.setBatteryVisible(true);                         //Add battery indicator to display
  #endif
  
  mlx.begin();                                            //Initialize the IR sensor

  attachInterrupt(BUTTON_B, setThreshold, FALLING);       //Assign interrupt to OLED temp threshold button
  attachInterrupt(BUTTON_C, setMinimum, FALLING);         //Assing interrupt to OLED patient detector button
  stateTime = millis()/1000;   
  statePrevTime = stateTime;
}


/**********
 *  INTERRUPT ROUTINE -  set the exhale detection temperature. Set during Inhale respitory cycle.
 */
void setThreshold(){
  if(!thresholdSet)
    thresholdSet = true;                        //Set FLAG to set the exhale threshold temp
}

/**********
 *   INTERRUPT ROUTINE -  set the patient detector temperature
 */
void setMinimum(){
  if(!minimumSet)
    minimumSet = true;                          //Set FLAG to set the patient detector temp
}

/**********
 *    MAIN PROCESS LOOP
 */
void loop() {
  objT = 0.0f;

  for(char ctr=0;ctr<MEASURE_AVG;ctr++){
    objT += mlx.readObjectTempF();        //Take measurement in Degrees F from MLX sensor
    delay(MEASURE_AVG_MILLIS);
  }
  objT = objT/MEASURE_AVG;               //find average measured temperature

  if((objT-PATIENT_HYST)<(minTemp)){       // Determine if NEW patient is present
    patient = false;
    alarm = false;
    bpm = 0;
    digitalWrite(EXHALE_PIN,LOW);
    digitalWrite(INHALE_PIN,LOW);
    digitalWrite(PATIENT_PIN,LOW);
    digitalWrite(ALARM_PIN,LOW);
    #ifdef SERIAL_DATA
      updateSerial();
    #endif       
  }else{
    if(!patient){                             //New patient found - set BPM & ALARM state times
      patient = true;
      statePrevTime = millis()/1000;          //Reset all counters & timers for new patient
      stateTime = statePrevTime;
      bpmTime = statePrevTime;
      bpmPrevTime = bpmTime;
      exhaleCtr = 0;
      digitalWrite(PATIENT_PIN,HIGH);
      #ifdef SERIAL_DATA
        updateSerial();
      #endif
    }
    
    if(((objT-EXHALE_HYST)>tempThreshold)&&(state==INHALE))    //Patient detected - determine if exhaling or inhaling
    {
       digitalWrite(EXHALE_PIN,HIGH);
       digitalWrite(INHALE_PIN,LOW);
       state  = EXHALE;
       exhaleCtr++;
    }else if((objT<tempThreshold)&&(state==EXHALE))
    {
       digitalWrite(EXHALE_PIN,LOW);
       digitalWrite(INHALE_PIN,HIGH);
       state = INHALE;
    }


    if(state!=prevState){                               //check to see if there was a state change
        digitalWrite(ALARM_PIN, LOW);
        alarm = false;
        if(exhaleCtr >= 2){                             //one complete breathing cycle. Start Exhale to next start Exhale
          bpmTime = millis()/1000;
          if((bpmTime - bpmPrevTime) != 0)              //avoid div by zero
          {
            bpm = 1/(((float)bpmTime-(float)bpmPrevTime)/30.0);       //time between exhales in seconds divided by 60 sec/min
          }else{
            bpm = 0.0f;
          }
          #ifdef DEBUG
            Serial.print("BPM TIME = ");
            Serial.println(bpmTime);
            Serial.print("BPM PREV TIME = ");
            Serial.println(bpmPrevTime);
          #endif

          exhaleCtr = 0;
          bpmPrevTime = bpmTime;
        }
        statePrevTime = stateTime;
        stateTime = millis()/1000;
        prevState = state;
        
    }else
    {
        if((millis()/1000-statePrevTime)>ALARM_DELAY_SEC){
          digitalWrite(ALARM_PIN,HIGH);
          alarm = true;
          #ifdef DEBUG
            Serial.println("ALARM = TRUE");
            Serial.print("STATE PREV TIME = ");
            Serial.println(statePrevTime);
            Serial.print("CURRENT TIME = ");
            Serial.println(millis()/1000);
          #endif
        }
    }

    #ifdef SERIAL_DATA
      updateSerial();
    #endif
  }

  if(thresholdSet){                   //set new exhale threshold if button pressed
    #ifdef DISPLAY
      thresholdSetDisp = true;
    #endif
    tempThreshold = (int)objT;
    delay(10);                        //for debounce
    thresholdSet = false;
  }

  if(minimumSet){                     //set new patient detector temp if button pressed
    #ifdef DISPLAY
      minimumSetDisp = true;
    #endif
    minTemp = (int)objT;
    delay(10);                        //for debounce
    minimumSet = false;
  }

  #ifdef DISPLAY
    updateDisplay();                   //UPDATE OLED DISPLAY
  #endif
  
  delay(MEASURE_DELAY_MILLIS);
}


/**************
 *   UPDATE THE OLED DISPLAY
 */
void updateDisplay(){
  #ifdef DISPLAY  
    if(thresholdSetDisp){                     //Display Exhale threshold changed screen
      thresholdSetDisp = false;
      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.println("EXHALE THRESHOLD SET");
      oled.setTextSize(2);
      oled.print("  ");
      oled.print(tempThreshold);
      oled.print(" F");
      oled.display();
      delay(OLED_SET_CHG_MILLIS);
    }
    
    if(minimumSetDisp)                       //Display Patient detector changed screen
    {
      minimumSetDisp = false;
      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.println("PATIENT THRESHOLD SET");
      oled.setTextSize(2);
      oled.print("  ");
      oled.print(minTemp);
      oled.print(" F");
      oled.display();
      delay(OLED_SET_CHG_MILLIS);
    }
  
    //Dispaly the exhale target threshold temperature
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0,0);
    oled.print("S:");
    oled.print((int)minTemp);
    oled.print("/");
    oled.print((int)tempThreshold);
    oled.print(" F");
    
    // get the current voltage of the battery from
    float battery = getBatteryVoltage();
  
    // update the battery icon
    oled.setBattery(battery);
    oled.renderBattery();
  
    //Display the current breathing mode or "No Patient"
    oled.setCursor(0,9);
    oled.setTextSize(2);
    if(patient){
      if(state)
        oled.println("EXHALE");
      else
        oled.println("INHALE");
    }else
    {
      oled.println("NO PATIENT");
    }
  
    //Display the measured temperature & breaths per minute
    oled.setTextSize(1);
    oled.print("T: ");
    oled.print(objT);
    oled.print(" F");
    oled.setCursor(75,25);
    oled.print("BPM: ");
    oled.print((int)bpm);
  
    // update the display with the new screen data
    oled.display();
  #endif
 
}

/***********************
 * Measure the battery voltage on the analog pin
 * and convert to volts DC
 */

float getBatteryVoltage() {

  float measuredvbat = analogRead(VBATPIN);

  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage

  return measuredvbat;

}

/*************************
 * Formatted serial string to send out device status for
 * external device.
 *    FORMAT STRING:  Patient Present, Inhale/Exhale (false/true), Alarm, Breaths/minute, Measured temp
 */
 void updateSerial(){
  #ifdef ENABLE_SERIAL
    //Patient status
    Serial.print("PATIENT=");
    if(patient)
      Serial.print("TRUE,");
    else
      Serial.print("FALSE,");

    //respitory status
    Serial.print("STATE=");
    if(state)
      Serial.print("EXHALE,");
    else
      Serial.print("INHALE,");

    //Alarm status
    Serial.print("ALARM=");
    if(alarm)
      Serial.print("TRUE,");
    else
      Serial.print("FALSE,");

    //current calculed breaths/minute
    Serial.print("BPM=");
    Serial.print(bpm);
    Serial.print(",");

    //measured temperature
    Serial.print("TEMP=");
    Serial.print(objT);

    Serial.println();
  #endif
 }

