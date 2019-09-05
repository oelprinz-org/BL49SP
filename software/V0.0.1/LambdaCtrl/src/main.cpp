/* LambdaCtrl
 *
 *	This programm is designed to build a standalone wideband lambda controller.
 *	It's based on BOSCH CJ125 which is the interface between the lambda probe and the �C.
 *	
 *	It work's with the NanoLambda Board from Ruzki.
 *
 *	Hardware (c) Ruzki --> Georg Geraskin
 *	Software (c) Alfagta --> Mario Schoebinger
 *
 *	LambdaCtrl is licensed under a
 *	Creative Commons Attribution-NonCommercial 4.0 International License.
 *
 *	You should have received a copy of the license along with this
 *	work. If not, see <http://creativecommons.org/licenses/by-nc/4.0/>.
 *
 **/

#include <LambdaCtrl.h>

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
