/* LambdaCtrl
 *
 *	This programm is designed to build a standalone wideband lambda controller.
 *	It's based on BOSCH CJ125 which is the interface between the lambda probe and the uC.
 *	
 *	Hardware open source BL49sp
 *	Software (c) Alfagta --> Mario Schoebinger
 *
 *	LambdaCtrl is licensed under a
 *	Creative Commons Attribution-NonCommercial 4.0 International License.
 *
 *	You should have received a copy of the license along with this
 *	work. If not, see <http://creativecommons.org/licenses/by-nc/4.0/>.
 *
 **/

#include <stdint.h>
#define CJ_LSU_ARRAY_SIZE  24  // Number of values within the array

/* Constant tables to interpolate lambda from given pump current 
 * 
 *  Ip is datasheet value x 1000
 *  Lambda is datasheet value x 100
 *  
 *  http://www.bosch-motorsport.de/content/downloads/Products/resources/2779229707/en/pdf/Lambda_Sensor_LSU_4.9_Datasheet_51_en_2779147659.pdf
 *  
 **/
static const int16_t cjLsu49Ip[CJ_LSU_ARRAY_SIZE] = { -2000, -1602, -1243, -927, -800, -652, -405, -183, -106, -40, 0, 15, 97, 193, 250, 329, 671, 938, 1150, 1385, 1700, 2000, 2150, 2250 };
static const int16_t cjLsu49Lambda[CJ_LSU_ARRAY_SIZE] = { 65, 70, 75, 80, 82, 85, 90, 95, 97, 99, 100, 101, 105, 110, 113, 117, 142, 170, 199, 243, 341, 539, 750, 1011 };

/* Constant table to simulate a narrow band output */
static const int16_t NbLambda[CJ_LSU_ARRAY_SIZE] = {};


//#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <Wire.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <FastPID.h>

/* Define IO */
#define CJ125_NSS_PIN                         10    /* Pin used for chip select in SPI communication. */  
#define LED_STATUS_POWER                      7   /* Pin used for power the status LED, indicating we have power. */
#define LED_STATUS_HEATER                     5   /* Pin used for the heater status LED, indicating heater activity. */
#define HEATER_OUTPUT_PIN                     6   /* Pin used for the PWM output to the heater circuit. */
#define USUP_ANALOG_INPUT_PIN                 A0  /* Analog input for power supply.*/
#define UVREF_ANALOG_INPUT_PIN                A1  /* Analog input for reference voltage supply.*/
#define UR_ANALOG_INPUT_PIN                   A6  /* Analog input for temperature.*/
#define UA_ANALOG_INPUT_PIN                   A7  /* Analog input for lambda.*/
#define LAMBDA_PWM_OUTPUT_PIN                 3   /* Analog output linear Lambda voltage */
#define EN_INPUT_PIN                          16  /* Enable pin, low when engine running */
#define SP1_INPUT_PIN                         2   /* Spare input 1 */
#define SP2_INPUT_PIN                         4   /* Spare input 2 */

/* Define parameters */
#define START_CONF_CNT                        10    /* Startup confident count, Min < UBatterie < Max, CJ Status ok */
#define CJ_OK_CONF_CNT                        10    /* Cj read with status ready confident count */ 

#define CJ_CALIBRATION_SAMPLES                10    /* Read values this time and build average (10) */
#define CJ_CALIBRATION_PERIOD                 150   /* Read a value every xx milliseconds  (150) */

#define CJ_PUMP_FACTOR                        1000  /* 1000 according to datasheet */
#define CJ_PUMP_RES_SHUNT                     61.9  /* 61,9 Ohm according to datasheet */

#define CJ_ERR_CNT                            2   /* xx Reads with error triggers a error for cj */
#define LAMBDA_PERIOD                         8   /* Every xx milliseconds calculate lambda */    

#define UA_MIN_VALUE                          150   /* Min allowed calibration value */               
#define UA_MAX_VALUE                          400   /* Max allowerd calibration value */

#define UR_MIN_VALUE                          150   /* Min allowed calibration value */
#define UR_MAX_VALUE                          300   /* Max allowerd calibration value */

#define USUP_MIN_ERR                          10500 /* Min allowed supply voltage value */
#define USUP_MAX_ERR                          17000 /* Max allowed supply voltage value */

#define USUP_MIN_OK                           11000 /* Min allowed supply voltage value */
#define USUP_MAX_OK                           16500 /* Max allowed supply voltage value */

#define USUP_ERR_CNT                          2   /*  Allowed error count, switch to preset if supply out of range for this count */

#define PROBE_CONDENSATE_PERIOD               6000  /* xx milliseconds */ 
#define PROBE_CONDENSATE_VOLT                 1500  /* xx millivolt during condensate heat */
#define PROBE_CONDENSATE_LIMIT                34    /* 34 is the value which is used for 11V supply voltage */

#define PROBE_PREHEAT_PERIOD                  1000  /* xx milliseconds between each step (1000ms) */
#define PROBE_PREHEAT_STEP                    400   /* xx millivolt per step (400mV) */
#define PROBE_PREHEAT_MAX                     13000 /* xx millivolt for end preheat, after this we go to pid */
#define PROBE_PREHEAT_TIMOUT                  15000 /* xx milliseconds preheat timeout (15000ms) */

#define PROBE_PID_PERIOD                      10    /* xx milliseconds */

#define DEBUG                                 1   /* Define debug mode 0 = off, 1 = Minimum, 2= all */

#define CFG_EEPROM_ADDR                       0x0000  /* Address for configuration within EEPROM */

#define MODE_TIMEOUT                          5000  /* Max allowed time within a mode between START -> CALIBRATE */

/* Define CJ125 registers */
#define CJ125_IDENT_REG_REQUEST               0x4800  /* Identify request, gives revision of the chip. */
#define CJ125_DIAG_REG_REQUEST                0x7800  /* Dignostic request, gives the current status. */
#define CJ125_INIT_REG1_REQUEST               0x6C00  /* Requests the first init register. */
#define CJ125_INIT_REG2_REQUEST               0x7E00  /* Requests the second init register. */
#define CJ125_INIT_REG1_MODE_CALIBRATE        0x569D  /* Sets the first init register in calibration mode. */
#define CJ125_INIT_REG1_MODE_NORMAL_V8        0x5688  /* Sets the first init register in operation mode. V=8 amplification. */
#define CJ125_INIT_REG1_MODE_NORMAL_V17       0x5689  /* Sets the first init register in operation mode. V=17 amplification. */
#define CJ125_DIAG_REG_STATUS_OK              0x28FF  /* The response of the diagnostic register when everything is ok. */
#define CJ125_DIAG_REG_STATUS_NOPOWER         0x2855  /* The response of the diagnostic register when power is low. */
#define CJ125_DIAG_REG_STATUS_NOSENSOR        0x287F  /* The response of the diagnostic register when no sensor is connected. */
#define CJ125_INIT_REG1_STATUS_0              0x2888  /* The response of the init register when V=8 amplification is in use. */
#define CJ125_INIT_REG1_STATUS_1              0x2889  /* The response of the init register when V=17 amplification is in use. */

/* Define DAC MCP4725 address MCP4725A2T-E/CH */
#define DAC1_ADDR                             0x64  /* Address for DAC 1 chip A0 tied to GND */
//#define DAC2_ADDR                           0x65  /* Address for DAC 2 chip A0 tied to +5V */

/* Define DAC MCP4725 registers */
#define MCP4725_CMD_WRITEDAC                  0x40  /* Writes data to the DAC */
#define MCP4725_CMD_WRITEDACEEPROM            0x60    /* Writes data to the DAC and the EEPROM (persisting the assigned value after reset) */

/* Lambda Outputs are defined as follow:
 *  
 *  0.5V = 0,68 Lambda = 10 AFR (Gasoline)
 *  4,5V = 1,36 Lambda = 20 AFR (Gasoline)
 *
 *  Internal Lambda is used in a range from 68..136
 *  
 **/
#define LambdaToVoltage(a) ((a * 5882UL / 100UL) - 3500UL)
#define VoltageTo8BitDac(a) (a * 255UL / 5000UL)      
#define VoltageTo12BitDac(a) (a * 4095UL / 5000UL)

/* Calculate millivolt from adc */
#define AdcToVoltage(a) (a * 5000UL / 1023UL)

typedef enum{
  INVALID = 0,
  PRESET,
  START,
  CALIBRATION,
  IDLE,
  CONDENSATE,
  PREHEAT,
  PID,
  RUNNING,
  ERROR
} state;

typedef enum
{
  cjINVALID,
  cjCALIBRATION,
  cjNORMALV8,
  cjNORMALV17,
  cjERROR,
} cjmode;

typedef struct
{
  uint8_t StartConfCnt;
  uint8_t CjConfCnt;
  uint8_t CjCalSamples;
  uint8_t CjCalPeriod;
  uint8_t CjErrCnt;
  uint8_t LambdaPeriod;
  uint8_t SupplErrCnt;
  uint16_t tCondensate;
  uint16_t tPreheat;
}tCfg;

typedef struct
{
  uint8_t Mode;
  uint16_t Flags;
  uint32_t Tick;
  uint32_t LastMainTick;
  uint32_t LastHeatTick;
  uint32_t LastCjTick;
  uint32_t StartHeatTick;
  uint32_t LastSerialTick;
  uint32_t LastErrorTick;
  uint32_t LastTimeoutTick;
  int16_t VoltageOffset;
  int16_t SupplyVoltage;
  uint8_t SupplyErrCnt;
  int16_t RefVoltage;
  uint16_t CjAnsw;
  uint16_t CjState; 
  uint8_t CjMode;
  uint8_t CjErrCnt;
  int16_t UAOpt;
  int16_t UROpt;
  uint8_t HeatState;
  int16_t Lambda;
} tAbl;

typedef struct
{
  int16_t UA;
  int16_t UR;
  int16_t USup;
  int16_t URef;
  uint8_t EN;
  uint8_t S1;
  uint8_t S2;   
} tInputs;

typedef struct
{
  uint8_t Heater;
  uint8_t Wbl;
  uint16_t Dac1;
  uint16_t Dac2;
  uint8_t Led1;
  uint8_t Led2;
} tOutputs;

typedef struct
{
  int16_t IP;
  int16_t UR;
  int16_t UB;
  
} tCj125;


const static char ModeName[][15] =
{
  { "INVALID" },
  { "PRESET" },
  { "START" },
  { "CALIBRATION" },
  { "IDLE" },
  { "CONDENSATE" },
  { "PREHEAT" },
  { "PID" },
  { "RUNNING" },
  { "ERROR" },    
};

extern tCfg Cfg;
extern tAbl Abl;
extern tInputs In;
extern tOutputs Out;
extern tCj125 Cj;

extern FastPID HeaterPid;

extern void Preset(void);
extern void Start(void);
extern void Calibrate(void);
extern void Idle(void);
extern void Condensate(void);
extern void Preheat(void);
extern void Running(void);
extern void Error(void);
extern uint8_t CheckUBatt(void);

extern void Inputs(tInputs* In);
extern void Outputs(tOutputs* Out);
extern uint16_t ComCj(uint16_t data);
extern void ComDac(uint8_t addr, uint16_t data);

extern int16_t CalcLambda(void);
extern int16_t Interpolate(int16_t Ip);


extern void CheckCfg(tCfg* cfg);
extern void ParseSerial(uint8_t ch);
extern void SendTbl(uint8_t* src, size_t size);
extern void SendCfg(void);

/* Global variables */
tCfg Cfg;
tAbl Abl;
tInputs In;
tOutputs Out;
tCj125 Cj;

/* Objects */
FastPID HeaterPid;

void setup()
{
	/* Reset structs */
	memset((void*)&Abl, 0, sizeof(Abl));
	memset((void*)&In, 0, sizeof(In));
	memset((void*)&Out, 0, sizeof(Out));
	memset((void*)&Cj, 0, sizeof(Cj));
	
	/* Read config from eeprom */
	eeprom_read_block((void*)&Cfg, CFG_EEPROM_ADDR, sizeof(Cfg));
	CheckCfg(&Cfg);

	/* Setup io's */
	pinMode(CJ125_NSS_PIN, OUTPUT);
	pinMode(LED_STATUS_POWER, OUTPUT);
	pinMode(LED_STATUS_HEATER, OUTPUT);
	pinMode(HEATER_OUTPUT_PIN, OUTPUT);
	pinMode(LAMBDA_PWM_OUTPUT_PIN, OUTPUT);
	pinMode(EN_INPUT_PIN, INPUT);
	pinMode(SP1_INPUT_PIN, INPUT_PULLUP);
	pinMode(SP2_INPUT_PIN, INPUT_PULLUP);
	
	/* Change PWM frequency for heater pin to 122Hz */
	TCCR1B = 1 << CS12;

	/* Set I2C frequency to 400kHz */
	TWBR = ((F_CPU / 400000L) - 16) / 2;
		
	/* Preset io's */
	digitalWrite(CJ125_NSS_PIN, HIGH);
	digitalWrite(LED_STATUS_POWER, LOW);
	digitalWrite(LED_STATUS_HEATER, LOW);
	digitalWrite(HEATER_OUTPUT_PIN, LOW);
	digitalWrite(LAMBDA_PWM_OUTPUT_PIN, LOW);
	
	/* Init objects */
	HeaterPid.configure(17.0, 3.0, 0.0, 100, 8, false);
	
	SPI.begin();  
	SPI.setClockDivider(SPI_CLOCK_DIV8);
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE1);
	
	Wire.begin();
	
	Serial.begin(115200);
	
	/* Wait a little bit, CJ125 needs also time to start */
	delay(250);

	wdt_disable();
	wdt_reset();
	wdt_enable(WDTO_250MS);
}

void loop()
{
	/* Reset the watchdog */
	wdt_reset();

	/* Read tick */
	Abl.Tick = millis();
	if ((Abl.Tick - Abl.LastMainTick) >= 2)
	{
		/* Read Inputs */	
		Inputs(&In);
		
		/* Reference voltage */
		Abl.RefVoltage = (int16_t)((uint32_t)In.URef * 5000UL / 1023UL);	//This should be exactly 1.225V so we can calculate the adc offset from this
		/* Calculate the voltage offset */
		Abl.VoltageOffset = 1225 - Abl.RefVoltage;
		/* Calculate battery voltage */
		Abl.SupplyVoltage = Abl.VoltageOffset + (int16_t)((uint32_t)In.USup * 24500UL / 1023UL);
		/* Sanity check for battery voltage */
		if (CheckUBatt() == false)
		{		
			if (Abl.Mode != PRESET) Abl.Mode = PRESET;		
		}	
		
		if (Abl.Mode == START || Abl.Mode == CALIBRATION)
		{
			if ((Abl.Tick - Abl.LastTimeoutTick) >= MODE_TIMEOUT)
			{
				Abl.Mode = PRESET;
			}
		}

		/* Jump to the current mode function */
		switch (Abl.Mode)
		{
		case INVALID:
			Abl.Mode = PRESET;
			break;
			
		case PRESET:
			Preset();
			break;
			
		case START:
			Start();
			break;
			
		case CALIBRATION:
			Calibrate();
			break;
			
		case IDLE:
			Idle();
			break;
			
		case CONDENSATE:
			Condensate();
			break;
			
		case PREHEAT:
			Preheat();
			break;
			
		case RUNNING:
			Running();
			break;	
		
		case ERROR:
			Error();
			break;
			
		default:
			Abl.Mode = PRESET;
			break;			
		}	
		
		/* Write Outputs */
		Outputs(&Out);

		/* Update Serial port in debug mode */
		if ((Abl.Tick - Abl.LastSerialTick) >= 150)
		{
			if (Abl.Mode == PRESET)
			{
				Out.Led1 ^= 1;
			}
			
	#if (DEBUG > 1)
			Serial.print(F("UR:"));
			Serial.println(In.UR);		
			Serial.print(F("UA:"));
			Serial.println(In.UA);
			Serial.print(F("IP:"));
			Serial.println(Cj.IP);	
	#endif
	#if (DEBUG > 0)
			Serial.print(F("Mode:"));
			Serial.println(ModeName[Abl.Mode]);	
			Serial.print(F("Supply:"));
			Serial.println(Abl.SupplyVoltage / 1000.0);	

			if (Abl.CjState == cjNORMALV8 || Abl.CjState == cjNORMALV17)
			{
				Serial.print(F("Lambda:"));
				Serial.println(Abl.Lambda / 100.0);					
			}
			else
			{
				Serial.print(F("Cj:"));
				Serial.println(Abl.CjAnsw, HEX);
			}
	#endif	
			Abl.LastSerialTick = Abl.Tick;
		}
		Abl.LastMainTick = Abl.Tick;
	}
}


/* Read inputs */
void Inputs(tInputs* In)
{
  /* Analog */
  In->USup = analogRead(USUP_ANALOG_INPUT_PIN);
  In->URef = analogRead(UVREF_ANALOG_INPUT_PIN);
  In->UA = analogRead(UA_ANALOG_INPUT_PIN);
  In->UR = analogRead(UR_ANALOG_INPUT_PIN);
  
  /* Digital */
  In->EN = digitalRead(EN_INPUT_PIN);
  In->S1 = digitalRead(SP1_INPUT_PIN);
  In->S2 = digitalRead(SP2_INPUT_PIN);    
}

/* Set outputs */
void Outputs(tOutputs* Out)
{
  /* Analog */
  analogWrite(HEATER_OUTPUT_PIN, Out->Heater);
  analogWrite(LAMBDA_PWM_OUTPUT_PIN, Out->Wbl);
  
  /* Digital */
  digitalWrite(LED_STATUS_POWER, Out->Led1);
  digitalWrite(LED_STATUS_HEATER, Out->Led2);

  /* Dac */
  ComDac(DAC1_ADDR, Out->Dac1);
//  ComDac(DAC2_ADDR, Out->Dac2);
  
}

/* Send and receive to/from cj125 */
uint16_t ComCj(uint16_t data)
{
  uint16_t ret;
  uint8_t msb, lsb;
  ret = 0;
  
  digitalWrite(CJ125_NSS_PIN, LOW);
  msb = SPI.transfer((data >> 8));
  lsb = SPI.transfer((data & 0xFF));
  digitalWrite(CJ125_NSS_PIN, HIGH);
  ret = makeWord(msb, lsb);
  return ret;
}

/* Send DAC values to MCP4726 */
void ComDac(uint8_t addr, uint16_t data)
{
  Wire.beginTransmission(addr);     // Start transmission
  Wire.write(MCP4725_CMD_WRITEDAC); // Command write dac without eeprom
  Wire.write(data / 16);            // Upper data bits          (D11.D10.D9.D8.D7.D6.D5.D4)
  Wire.write((data % 16) << 4);     // Lower data bits          (D3.D2.D1.D0.x.x.x.x) 
  Wire.endTransmission();           // End transmission
} 

/* Check if supply voltage is in allowed range */
uint8_t CheckUBatt(void)
{
  uint8_t ret;
  
  if (Abl.SupplyVoltage < USUP_MIN_ERR || Abl.SupplyVoltage > USUP_MAX_ERR)
  {
    Abl.SupplyErrCnt++;
    if (Abl.SupplyErrCnt > USUP_ERR_CNT)
    {
      ret = false;
      Abl.SupplyErrCnt = 0;
    }
    else
    {
      ret = true;
    }
  }
  else
  {
    Abl.SupplyErrCnt = 0;
    ret = true;
  }
  
  return ret;
}

/* Calculate Ip and lambda*/
int16_t CalcLambda(void)
{
  int16_t ret;
  
  /* Check if UA opt == UA because then we have Lambda 1.0 */
  if (In.UA == Abl.UAOpt)
  {
    ret = 100;
  }
  else
  {
    float UAActVolt, UAOptVolt, UADelta;
    float Amplifier;
    
    UAActVolt = (float)In.UA * 5.0 / 1023.0; 
    UAOptVolt = (float)Abl.UAOpt * 5.0 / 1023.0;  
    UADelta = (UAActVolt - UAOptVolt);  
    
    Amplifier = 17.0;
    if (Abl.CjMode == cjNORMALV8)
    {
      Amplifier = 8.0;  
    }
    
    Cj.IP = (int16_t)((UADelta * CJ_PUMP_FACTOR) / (CJ_PUMP_RES_SHUNT * Amplifier) * 1000); 
    ret = Interpolate(Cj.IP);
  }
  
  return ret;
}

/* Calculate value from a given Ip, Lambda table */
int16_t Interpolate(int16_t Ip)
{
  uint8_t i;
  float y, x0, x1, y0, y1;
  float m, b;
  
  i = 0;
  y = 0;
  
  /* Check if IP is less then min known IP */
  if (Ip <= cjLsu49Ip[0])
  {
    y = (float)cjLsu49Lambda[0];  
  } 
  /* Check if IP is greater then max known IP */
  else if(Ip >= cjLsu49Ip[CJ_LSU_ARRAY_SIZE - 1])
  {
    y = (float)cjLsu49Lambda[CJ_LSU_ARRAY_SIZE - 1];  
  }
  
  while ((i < CJ_LSU_ARRAY_SIZE - 1) && (y == 0.0))
  {
    /* Check if IP matches the table value, so we know lambda exactly without calculation */
    if (cjLsu49Ip[i] == Ip)
    {
      y = (float)cjLsu49Lambda[i];  
    }
    /* Check if we are between two values */
    else if ((cjLsu49Ip[i] <= Ip) && (Ip <= cjLsu49Ip[i + 1]))
    {
      /* Copy the values for the interpolation */
      x0 = (float)cjLsu49Ip[i];
      x1 = (float)cjLsu49Ip[i + 1];
      y0 = (float)cjLsu49Lambda[i];
      y1 = (float)cjLsu49Lambda[i + 1];   
      
      /* Calculate the gain */
      m = (y1 - y0) / (x1 - x0);  
      
      /* Calculate the offset */
      b = (y1 - (x1 * m));
      
      /* Calculate lambda value */
      y = ((float)Ip * m) + b;
    }
    i++;
  }
  
  return (int16_t)y;
}

/* --- Operationg Mode --- */

/* This function set's the variables 
 * to start values 
 */
void Preset(void)
{
  static uint8_t n;
  Abl.Mode = PRESET;
  
  Abl.Lambda = 100;
  Out.Dac1 = VoltageTo12BitDac(LambdaToVoltage(100));
  Out.Dac2 = Out.Dac1;
  Out.Wbl = VoltageTo8BitDac(LambdaToVoltage(100));
  Out.Heater = 0;
  Out.Led2 = LOW;
  
  Abl.Flags = 0;
  Abl.UAOpt = 0;
  Abl.UROpt = 0;
  Abl.CjState = cjINVALID;
  Abl.CjMode = cjINVALID;
  Abl.CjErrCnt = 0;
  
  Abl.HeatState = 0;  

  if((USUP_MIN_OK < Abl.SupplyVoltage) && (Abl.SupplyVoltage < USUP_MAX_OK))
  { 
    n++;
    if (n >= START_CONF_CNT)
    {
      Abl.Mode = START;
      n = 0;
    }
  }
  else
  {
    n = 0;
  }
  Abl.LastTimeoutTick = Abl.Tick;
}

/* This function read the cj125 after startup.
 * CJ125 answear must be okay several times to go futher
 */
void Start(void)
{ 
  static uint8_t n;
  Abl.Mode = START;
  Abl.CjState = ComCj(CJ125_DIAG_REG_REQUEST);
    
  if ((Abl.CjState & CJ125_DIAG_REG_STATUS_OK) == CJ125_DIAG_REG_STATUS_OK)
  {
    n++;
    if (n >= CJ_OK_CONF_CNT)
    {
      Abl.Mode = CALIBRATION;
      Abl.LastTimeoutTick = Abl.Tick;
      n = 0;
    }
  }
  else
  {
    n = 0;
  } 
} 

/* This function turns the CJ125 into calibration mode.
 * Then it sampels the UA, UR values several times and build
 * the average
 */
void Calibrate(void)
{
  static uint8_t n;
  uint16_t ret;
  Abl.Mode = CALIBRATION;
  
  /* Turn cj125 into calibration mode */
  if (Abl.CjMode != cjCALIBRATION)
  {
    ret = ComCj(CJ125_INIT_REG1_MODE_CALIBRATE);
    Abl.CjMode = cjCALIBRATION;
    n = 0;
    Abl.LastCjTick = Abl.Tick;
  }
  
  /* Read UA, UR */
  if ((Abl.Tick - Abl.LastCjTick) >= CJ_CALIBRATION_PERIOD)
  {
    /* Only if UA , UR within allowed range */
    if (((UA_MIN_VALUE < In.UA) && (In.UA < UA_MAX_VALUE)) && ((UR_MIN_VALUE < In.UR) && (In.UR < UR_MAX_VALUE)))
    {
      Abl.UAOpt += In.UA;
      Abl.UROpt += In.UR;   
      Out.Led1 ^= 1;
      n++;
    }
    /* Samples done, build average */
    if (n >= CJ_CALIBRATION_SAMPLES)
    {
      Abl.UAOpt /= CJ_CALIBRATION_SAMPLES;
      Abl.UROpt /= CJ_CALIBRATION_SAMPLES;
      Out.Led1 = HIGH;
      Abl.Mode = IDLE;
      Serial.print("UA:");
      Serial.println(Abl.UAOpt);
      Serial.print("UR:");
      Serial.println(Abl.UROpt);
      Abl.LastTimeoutTick = Abl.Tick;
      n = 0;
    }
    /* Save tick count */
    Abl.LastCjTick = Abl.Tick;
  }
  
}

/* This function is called
 * after calibration as long
 * as enable input is not on ground.
 */
void Idle(void)
{
  uint16_t ret;
  static uint32_t ledtick;
  
  Abl.Mode = IDLE;
  
  /* Turn cj125 into normal mode */
  if (Abl.CjMode != cjNORMALV8)
  {
    ret = ComCj(CJ125_INIT_REG1_MODE_NORMAL_V8);
    Abl.CjMode = cjNORMALV8;
    ledtick = Abl.Tick;
  }
  
  /* Toggle heater led  to show we idle */
  if ((Abl.Tick - ledtick) >= 250)
  {
    Out.Led2 ^= 1;
    ledtick = Abl.Tick;
  }
  
  /* Check if we can go to heater condensate (Enable pin low) */
  if (In.EN == LOW)
  {
    Abl.Mode = CONDENSATE;
    Out.Led2 = LOW;
  }
  
} 

/* This functions limits the heat intensity 
 * according to the datasheet to 1.5V
 * for arround 5 seconds
 */
void Condensate(void)
{
  static uint32_t ledtick;
  
  Abl.Mode = CONDENSATE;
  
  /* Set heater state to condensate */
  if (Abl.HeatState != CONDENSATE)
  {
    Abl.HeatState = CONDENSATE;
    Abl.LastHeatTick = ledtick = Abl.Tick;
  }
  
  /* Calculate the required pwm for heater condensate */
  Out.Heater = (uint8_t)(PROBE_CONDENSATE_VOLT * 255UL / (uint32_t)Abl.SupplyVoltage);
  
  /* Sanity check */
  if (Out.Heater > PROBE_CONDENSATE_LIMIT)
  {
    Out.Heater = PROBE_CONDENSATE_LIMIT;
  }
  
  /* Check if condensate period expired, if so go to preheat */
  if ((Abl.Tick - Abl.LastHeatTick) >= PROBE_CONDENSATE_PERIOD)
  {
    Abl.Mode = PREHEAT;
    Abl.LastTimeoutTick = Abl.Tick;
  }
  
  /* Toggle heater led fast to show we start condensate */
  if ((Abl.Tick - ledtick) >= 100)
  {
    Out.Led2 ^= 1;
    ledtick = Abl.Tick;
  }
  
}

/* This functions ramps up the voltage 
 * from 8 Volts to 13 volts in steps of 0.4V
 * per second
 */
void Preheat(void)
{
  static uint8_t start, step, end;
  Abl.Mode = PREHEAT;
  /* Set heater state to preheat */
  if (Abl.HeatState != PREHEAT)
  {
    Abl.HeatState = PREHEAT;
    Abl.LastHeatTick = Abl.Tick;
    Abl.StartHeatTick = Abl.Tick;
    
    /* Calculate the starting pulse with value 
     *  255 is max pwm value, so 255 == Supply voltage
     *  We start preheat at 8 Volt.
     *  start = 8 * 255 / Supply
     */
    start = (uint8_t)(8000UL * 255UL / (uint32_t)Abl.SupplyVoltage);
    /* Calculate the step pulse width */
    step = (uint8_t)(PROBE_PREHEAT_STEP * 255UL / (uint32_t)Abl.SupplyVoltage);
    /* Calculate end pulse with */
    if (Abl.SupplyVoltage < PROBE_PREHEAT_MAX)
    {
      end = 255;
    }
    else
    {
      end = (uint8_t)(PROBE_PREHEAT_MAX * 255UL / (uint32_t)Abl.SupplyVoltage);
    }
    /* Start pwm */
    Out.Heater = start;
  } 
  
  /* Every second one step */
  if ((Abl.Tick - Abl.LastHeatTick) >= PROBE_PREHEAT_PERIOD)
  {
    /* Toggle heater LED */
    Out.Led2 ^= 1;
    /* Check if we have reached end phase */
    if ((Out.Heater >= end) || (In.UR <= Abl.UROpt))
    {
      Abl.Mode = RUNNING;
      Abl.LastTimeoutTick = Abl.Tick;
    }
    else
    {
      Out.Heater += step;
      /* Limit heater voltage to end pulse width */
      if (Out.Heater > end) Out.Heater = end;
    }   
    /* Save tick count */
    Abl.LastHeatTick = Abl.Tick;
  } 
  
  /* Check if we have a timeout */
  if ((Abl.Tick - Abl.StartHeatTick) >= PROBE_PREHEAT_TIMOUT)
  {
    Abl.Mode = PRESET;
  }
  
}

/* This function is called 
 * when everything was ok and
 * we are in running mode.
 * Running mode means we read lambda
 * values and send them to the dac
 */
void Running(void)
{ 
  static uint8_t EnCnt;
  
  Abl.Mode = RUNNING;

  /* Set heater state to pid */
  if (Abl.HeatState != PID)
  {
    Abl.HeatState = PID;
  }
  
  /* Check if cj is in normal mode */
  if (Abl.CjMode != cjNORMALV8)
  {
    ComCj(CJ125_INIT_REG1_MODE_NORMAL_V8);
    Abl.CjMode = cjNORMALV8;
  }

  /* Recalc heater pid */
  if ((Abl.Tick - Abl.LastHeatTick) >= PROBE_PID_PERIOD)
  {
    
    if (Abl.HeatState == PID)
    {
      Out.Heater = (uint8_t)HeaterPid.step(In.UR, Abl.UROpt); 
      Out.Led2 = HIGH;
    }
    else
    {
      Out.Heater = 0;
      Out.Led2 = LOW;
    }
    /* Save tick count */
    Abl.LastHeatTick = Abl.Tick;
  }
  
  /* Recalc lambda value and check if cj is okay */
  if ((Abl.Tick - Abl.LastCjTick) >= LAMBDA_PERIOD)
  {
    Abl.CjAnsw = ComCj(CJ125_DIAG_REG_REQUEST); // Read cj diagnostic information
    
    if ((Abl.CjAnsw & CJ125_DIAG_REG_STATUS_OK) != CJ125_DIAG_REG_STATUS_OK)  // If not okay count up
    {
      Abl.CjErrCnt++;
      if (Abl.CjErrCnt > CJ_ERR_CNT)  // Error cnt to high?
      {
        Abl.CjState = cjERROR;  // Change cjState to error
        Abl.Mode = ERROR;
        Abl.CjErrCnt = 0; // Reset error counter
      }
    }
    else
    {
      Abl.CjState = cjNORMALV8; // Set normal mode 
      Abl.CjErrCnt = 0; // Reset error counter
    }   
    
    if (Abl.Mode != ERROR)
    {
      /* Calculate actual lambda value */
      Abl.Lambda = CalcLambda();      
      /* Check range */
      int16_t LambdaLimit = constrain(Abl.Lambda, 68, 136);     
      /* Calc output for DAC */
      Out.Wbl = (uint8_t)VoltageTo8BitDac(LambdaToVoltage(LambdaLimit));
      Out.Dac1 = VoltageTo12BitDac(LambdaToVoltage(LambdaLimit));
    }   
    /* Check if we must go to start (Enable pin HIGH) */
    if (In.EN == HIGH)
    {
      EnCnt++;
      if (EnCnt > 2) Abl.Mode = PRESET; 
    }
    else
    {
      EnCnt = 0;
    }   
    /* Save tick count */
    Abl.LastCjTick = Abl.Tick;
  }
}

/* This function is called when an error occure.
 * at the moment we come only from running mode
 * to error mode.
 */
void Error(void)
{ 
  /* Reset values */
  Abl.Lambda = 100;
  Out.Dac1 = VoltageTo12BitDac(LambdaToVoltage(100));
  Out.Dac2 = Out.Dac1;
  Out.Wbl = VoltageTo8BitDac(LambdaToVoltage(100));
  Out.Heater = 0;
  
  if ((Abl.Tick - Abl.LastErrorTick) >= 500)
  {
    /* Toogle led to show error is active */
    if (Abl.CjState == cjERROR)
    {
      Out.Led1 ^= 1;
      Out.Led2 ^= 1;
    } 
    
    /* Save tick count */
    Abl.LastErrorTick = Abl.Tick;
  }
  
  /* Check if we can go to start (Enable pin HIGH) */
  if (In.EN == HIGH)
  {
    Abl.Mode = PRESET;
  }
}


/* This function checks if the config is valid.
*  if not we build a dummy config and save
*  the config to the eeporm.
*/
void CheckCfg(tCfg* cfg)
{
  if (cfg->SupplErrCnt == 255)  
  {
    cfg->SupplErrCnt  = 2;
    cfg->StartConfCnt = 10;
    cfg->CjConfCnt = 10;
    cfg->CjErrCnt = 2;
    cfg->CjCalPeriod = 150;
    cfg->CjCalSamples = 10;
    cfg->LambdaPeriod = 17;
    cfg->tCondensate = 6000;
    cfg->tPreheat = 1000;
  }
  eeprom_update_block((const void*)cfg, CFG_EEPROM_ADDR, sizeof(tCfg));
}

void SendTbl(uint8_t* src, size_t size)
{
  for(uint16_t i = 0; i < size; i++)
  {
    Serial.write(*src++);
  }
}

void SendCfg(void)
{
  uint8_t *src = (uint8_t *)&Cfg;

  for (uint16_t i = 0;  i  < sizeof(Cfg); i++)
  {
    Serial.write(*src++);
  }
}
