/*************************************************** 
  Breathing Monitor
    - Measures air temperature using MLX90614 sensor
    - User sets patient detect temperature (minimum temp)
      using "C" button.
    - User sets exhale detection temperature (threshold temp)
      using "B" button.
    - Temperatures in Degrees "F"
    

   Hardware:
      (1) Adafruit Feather M0 Adalogger: https://www.adafruit.com/product/2796
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

#define VBATPIN A7                      //Analog IN # for Measuring 5V Battery
#define AO_TEMP A0                      //Analog OUT for exporting temperature to scope/external
//#define MEASURE_IN_C                    //COMMENT THIS LINE TO MEASURE IN FAHRENHEIT OTHERWISE IN CELCIUS
#define TEMP_THRESHOLD 90.0             //Default exhale temperature threshold (ENTER IN PROPER UNITS F/C)
#define MIN_TEMP 80.0                   //Default patient detector temperature (ENTER IN PROPER UNITS F/C)
#define AO_MIN 70.0                     //Minimum Temperature for analog output (ENTER IN PROPER UNITS F/C)
#define AO_MAX 110.0                    //Maximum Temperature for analog output (ENTER IN PROPER UNITS F/C)
#define PATIENT_HYST 2.0                //min +deg for patient absent once detected
#define EXHALE_HYST 1.0                 //min +deg required state change from inhale to exhale
#define EXHALE true                     //Exhale = true state
#define INHALE false                    //Inhale = false state
#define BUTTON_B 6                      //"B" button pin #
#define BUTTON_C 5                      //"C" button pin #
#define EXHALE_PIN 11                   //Pin # used to indicate exhale detected
#define INHALE_PIN 12                   //Pin # used to indicae inhale detected
#define PATIENT_PIN 10                  //Pin # used to indicate patient detected
#define ALARM_PIN 13                    //Pin # used to indicate alarm condition
#define MEASURE_DELAY_MILLIS 150        //milliseconds between measurement cycles
#define OLED_SET_CHG_MILLIS 1000        //milliseconds to hold the display for changes threshold temps
#define ALARM_DELAY_SEC 7               //seconds to set alarm if state change does not occur
#define MEASURE_AVG 2                   //number of measurements to use to determine temperature
#define MEASURE_AVG_MILLIS 50           //milliseconds between averaging measurements
#define SERIAL_BAUD 19200               //Serial port baud rate
#define ENABLE_SERIAL                   //COMMENT THIS LINE OUT TO DISABLE SERIAL
#define SERIAL_DATA                     //COMMENT THIS LINE OUT TO DISABLE data feed to serial port (requires ENABLE_SERIAL)
#define DISPLAY                         //COMMENT THIS LINE OUT TO DISABLE LOCAL DISPLAY
//#define DEBUG                           //UNCOMMENT TO ENABLE DEBUG VIA SERIAL (requires ENABLE_SERIAL)
#define ENABLE_AO                       //COMMENT THIS LINE TO DISABLE WRITE TO ANALOG OUTPUT

/***************************
 *  GLOBAL VARIABLES
 */
Adafruit_MLX90614 mlx = Adafruit_MLX90614();              //MLX sensor
#ifdef DISPLAY
  Adafruit_FeatherOLED oled = Adafruit_FeatherOLED();       //OLED display
#endif

float objT= 0.0f;                                         //Object Temperature
float ambT= 0.0f;                                         //Case temperature at sensor
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
bool sensorFail = false;                                  //FLAG:  flag for faulty or open IR Sensor
long stateTime = 0;                                       //Unix time stamp for current state
long statePrevTime = 0;                                   //Unix stamp from previous state change
long bpmTime = 0;                                         //Elapse time between respitory cycle
long bpmPrevTime = 0;                                     //Start of respitory cycle
float bpm = 0.0f;                                         //calculated breaths per minute (BPM)
char exhaleCtr = 0;                                       //exhale counter for BPM calculator
int tempDigital = 0;                                      //Analog output value

/**************************
 *  FUNCTION PROTOTYPES
 */
void setThreshold();                                      //INTERRUPT routine for setting exhale temp threshold
void setMinimum();                                        //INTERRUPT routine for setting patient detecor temp
void updateDisplay();                                     //Updates the OLED display
float getBatteryVoltage();                                //Measures the battery voltage and returns value in Volts DC
void updateSerial();                                      //Send bitstatus via serial port
void updateAO();                                          //Drive analog output with current temperature

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
  #ifdef MEAURE_IN_C
    ambT = mlx.readAmbientTempC();
  #else
    ambT = mlx.readAmbientTempF();
  #endif
  
  for(char ctr=0;ctr<MEASURE_AVG;ctr++){
    #ifdef MEASURE_IN_C
      objT += mlx.readObjectTempC();      //Take measurement in Degrees C from MLX sensor
    #else
      objT += mlx.readObjectTempF();
    #endif
    
    delay(MEASURE_AVG_MILLIS);
  }
  objT = objT/MEASURE_AVG;                //find average measured temperature

  if(objT>500.0)                         //Detect open/faulty IR sensor
    sensorFail = true;
  else 
    sensorFail = false;

  //DEBUG PATIENT DETECTOR
  /*
  #ifdef DEBUG                           
    Serial.print("OBJ TEMP = ");
    Serial.println(objT);
    Serial.print("Patient Threshold = ");
    Serial.println(minTemp);
    Serial.print("NO PATIENT = ");
    Serial.println((objT+(float)PATIENT_HYST)<minTemp);
  #endif
  */
  if((objT<(minTemp+PATIENT_HYST))&&!patient){    //looking for new patient
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
  }else if(objT<(minTemp-PATIENT_HYST)&&patient){       //Patient air drops below threshold
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
            bpm = (1/((float)bpmTime-(float)bpmPrevTime))*120.0;       //time between exhales in seconds 
          }else{
            bpm = 0.0f;
          }
          
          #ifdef DEBUG                                 //Debug breaths per minute
            Serial.print("BPM TIME = ");
            Serial.print(bpmTime);
            Serial.print(", BPM PREV TIME = ");
            Serial.print(bpmPrevTime);
            Serial.print(", CALC BPM = ");
            Serial.println(bpm);
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

          /*
          #ifdef DEBUG                                    //DEBUG ALARM
            Serial.print("ALARM=TRUE,");
            Serial.print(" STATE PREV TIME = ");
            Serial.print(statePrevTime);
            Serial.print(", CURRENT TIME = ");
            Serial.println(millis()/1000);
          #endif
          */
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
    delay(50);                        //for debounce
    thresholdSet = false;
  }

  if(minimumSet){                     //set new patient detector temp if button pressed
    #ifdef DISPLAY
      minimumSetDisp = true;
    #endif
    minTemp = (int)objT;
    delay(50);                        //for debounce
    minimumSet = false;
  }

  #ifdef DISPLAY
    updateDisplay();                   //UPDATE OLED DISPLAY
  #endif

  #ifdef ENABLE_AO
    updateAO();                          //UPDATE ANALOG OUTPUT
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
      #ifdef MEASURE_IN_C
        oled.print(" C");
      #else
        oled.print(" F");
      #endif
      
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
      #ifdef MEASURE_IN_C
        oled.print(" C");
      #else
        oled.print(" F");
      #endif
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
    #ifdef MEASURE_IN_C
      oled.print(" C");
    #else
      oled.print(" F");
    #endif
    
    // get the current voltage of the battery from
    float battery = getBatteryVoltage();
  
    // update the battery icon
    oled.setBattery(battery);
    oled.renderBattery();
  
    //Display the current breathing mode, "No Patient", or sensor fail
    oled.setCursor(0,9);
    oled.setTextSize(2);
    if(sensorFail)
    {
      oled.println("CHK SENSOR");
    }else if(alarm)
    {
      oled.println("! ALARM !");
    }else if(patient){
      if(state)
        oled.println("EXHALE");
      else
        oled.println("INHALE");
    }else
    {
      oled.println("NO PATIENT");
    }
  
    //Display the measured temperature & breaths per minute
    if(!sensorFail){
      oled.setTextSize(1);
      oled.print("T: ");
      oled.print(objT);
      #ifdef MEASURE_IN_C
        oled.print(" C");
      #else
        oled.print(" F");
      #endif
      oled.setCursor(75,25);
      oled.print("BPM: ");
      oled.print((int)bpm);
    }
  
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
 *    FORMAT STRING:  Patient Present, Inhale/Exhale (false/true), Alarm, Breaths/minute, Measured temp, Ambient Temp, Temperature units (C/F)
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
    Serial.print("OTEMP=");
    Serial.print(objT);
    Serial.print(",");

    //Ambient/sensor case temp
    Serial.print("ATEMP=");
    Serial.print(ambT);
    Serial.print(",");

    //Note temperature Units
    Serial.print("T_UNIT=");
    #ifdef MEASURE_IN_C
      Serial.print("CELCIUS");
    #else
      Serial.print("FAHRENHEIT");
    #endif
    
    Serial.println();
  #endif
 }

 /**********************
  * ANALOG OUT FOR TRENDNG
  */
void updateAO(){
  if(!sensorFail)
    tempDigital = map(objT,AO_MIN,AO_MAX,0,255);
  else
    tempDigital = 0;
    
  analogWrite(AO_TEMP,tempDigital);

  /*
  #ifdef DEBUG                                        //Debug Analog Output
    Serial.print("AO: ObjTemp = ");
    Serial.print(objT);
    Serial.print(" DigitalTemp = ");
    Serial.println(tempDigital);
  #endif
  */
}

