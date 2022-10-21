/*
 * M=main.c
 *
 *  Created on: Sep 24, 2022
 *      Author: Bassem El-Behaidy
 */

#include "../SHARED/ATMEGA32_Registers.h"
#include "../SHARED/stdTypes.h"
#include "../SHARED/errorState.h"
#include "../SHARED/BIT_MATH.h"

#include "../TMU/TMU_int.h"

#include "../MCAL/DIO/DIO_int.h"
#include "../MCAL/EEPROM/EEPROM_int.h"
#include "../MCAL/TIMER/TIMER_int.h"
#include "../MCAL/PWM/PWM_int.h"

#include "../HAL/COOLENT/COOLENT_int.h"
#include "../HAL/HEATER/HEATER_int.h"
#include "../HAL/LM35/LM35_int.h"
#include "../HAL/LD/LD_int.h"
#include "../HAL/Switch/Switch_int.h"
#include "../HAL/SevSeg/SevSeg_int.h"


#include "WtrHtr_config.h"
#include "WtrHtr_priv.h"

#define DEBUG 0

#if DEBUG
OnOff_t PowerStatus = OFF;
u8 Global_u8TempSetValue = 30 ;			// Set Temperature
#else
OnOff_t PowerStatus = OFF;
u8 Global_u8TempSetValue ;				// Set Temperature
#endif


OnOff_t HeaterStatus = OFF;
OnOff_t CoolentStatus = OFF;
u8 LedStatus = LD_OFF ;

u8 Global_u8DisplayMode = NORMAL ;
u8 Global_u8SetupSw = NONE;
u16 EEPROM_SetTempAddress = (u16)SET_ADDRESS ;
u8 SetTempSaveID = 0x55 ;
bool SetTempUpdate = TRUE ;

u8 Global_u8TempValue = 30;					// Actual Temperature
s8 Global_s8TempError = 0 ;					// Difference between Set point and Actual Temp

extern bool Global_blConverted;

int
main(void)
{

#if DEBUG
	u8 SSeg_u8ActiveModule = TEMP_UNITS ;
	LD_enuInit();
	Switch_enuInit();
	SevSeg_enuInit();
	LM35_enuInit();
	Heater_enuInit();
	Coolent_enuInit();
//	TMU_vidInit();

	_SEI_;


	while(1)
	{
		if( SetTempUpdate == TRUE )
			EEPROM_Access( &EEPROM_SetTempAddress );
		CheckPowerSwitch( NULL );
		if( PowerStatus == ON )
		{
			CheckTemperatureStatus( NULL );
			DisplayTemperature( &SSeg_u8ActiveModule );
			CheckIncrementSwitch( NULL );
			CheckDecrementSwitch( NULL );
			AdjustRedLampStatus( &LedStatus );
			if( HeaterStatus == ON	&&	CoolentStatus == OFF )
				AdjustHeaterStatus( NULL );
			else if( HeaterStatus == OFF	&&	CoolentStatus == ON )
				AdjustCoolentStatus( NULL );
		}

	}

#else

	u8 SSeg_u8ActiveModule = TEMP_UNITS ;

	LD_enuInit();
	Switch_enuInit();
	SevSeg_enuInit();
	LM35_enuInit();
	Heater_enuInit();
	Coolent_enuInit();
	TMU_vidInit();

	EEPROM_Access( &EEPROM_SetTempAddress );

	TMU_vidCreateTask(CheckTemperatureStatus, NULL					, 8 , PAUSED , 5 , 0 );
	TMU_vidCreateTask(DisplayTemperature	, &SSeg_u8ActiveModule	, 7 , PAUSED , 2 , 0 );
	TMU_vidCreateTask(AdjustHeaterStatus	, NULL					, 6 , PAUSED , 50, 11);
	TMU_vidCreateTask(AdjustCoolentStatus	, NULL					, 5 , PAUSED , 50, 11);
	TMU_vidCreateTask(AdjustRedLampStatus	, NULL					, 4 , PAUSED , 50, 17);
	TMU_vidCreateTask(CheckIncrementSwitch	, NULL					, 3 , PAUSED , 3 , 1 );
	TMU_vidCreateTask(CheckDecrementSwitch	, NULL					, 2 , PAUSED , 3 , 1 );
	TMU_vidCreateTask(CheckPowerSwitch		, NULL					, 1 , READY  , 3 , 0 );
	TMU_vidCreateTask(EEPROM_Access			, &EEPROM_SetTempAddress, 0 , READY  , 3 , 2 );

	TMU_vidStartScheduler();
#endif
}

void DisplayTemperature(void *Copy_pu8ActiveModule)
{
	u8 *Local_u8ActiveModule = (u8*)Copy_pu8ActiveModule ;
	u8 Local_u8DisplayValue;
	static u8 BlinkDelay = BLINK_COUNTS ;
	static bool BlinkStatus = FALSE ;

	if( PowerStatus == ON)
	{
		if( Global_u8DisplayMode == NORMAL )								//	NORMAL Operation Mode
		{																	//	*********************
			Local_u8DisplayValue = Global_u8TempValue ;						//	Set Display Value to Actual Temperature Value
			if(BlinkStatus == TRUE ) BlinkStatus = FALSE ;					//	Confirm Blinking is Disabled
		}
		else																// SETUP Mode
		{																	//	*********
			Local_u8DisplayValue = Global_u8TempSetValue ;					//	Set Display Value to Set Temperature Value
			BlinkDelay--;													//	Blink Delay counter decrement
			if(!BlinkDelay)													//	Check if Blink delay is completed
			{
				BlinkDelay = BLINK_COUNTS;									//	Reset Blink Delay Counter
				BlinkStatus = ( ( BlinkStatus == TRUE )? FALSE : TRUE ) ;	//	Flip BlinkStatus Value
			}
		}

		if( *Local_u8ActiveModule == TEMP_UNITS )
		{
			SevSeg_enuModuleDisable( TEMP_TENS );							// TURN OFF Seven Segment Tens Module
			SevSeg_enuSetDigitValue( Local_u8DisplayValue % 10 );			// Set Value for Units Digit
			if( BlinkStatus == FALSE )SevSeg_enuModuleEnable( TEMP_UNITS );	// TURN ON Seven Segment Units Module
			*Local_u8ActiveModule = TEMP_TENS ;								// Switch Active module to be Displayed to Tens
		}
		else
		{
			SevSeg_enuModuleDisable( TEMP_UNITS );							// TURN OFF Seven Segment Units Module
			SevSeg_enuSetDigitValue( Local_u8DisplayValue / 10 );			// Set Value for Tens Digit
			if( BlinkStatus == FALSE )SevSeg_enuModuleEnable( TEMP_TENS );	// TURN ON Seven Segment Tens Module
			*Local_u8ActiveModule = TEMP_UNITS ;							// Switch Active module to be Displayed to Units
		}
	}
	else
	{
		SevSeg_enuModuleDisable( TEMP_UNITS );								//	TURN OFF Seven Segment Units Module
		SevSeg_enuModuleDisable( TEMP_TENS );								//	TURN OFF Seven Segment Tens Module
		*Local_u8ActiveModule = TEMP_UNITS;									//	Set Active module to be Displayed to Units
		TMU_vidPauseTask( 7 );												//	Pause Display Temperature Task
	}
}

void CheckTemperatureStatus(void *pNULL)
{
	u16 Local_u16TempValue ;

#if DEBUG != 1
	static u16 Local_u16TempAccValue = 0 ;
	static u8 Local_u8TempReadCounter = TEMP_AVG_READINGS ;
#endif

	if( Global_u8DisplayMode == NORMAL && PowerStatus == ON )
	{
#if DEBUG == 1
		if( Global_blConverted == TRUE )
		{
			if( ES_OK == LM35_enuReadTemp( &Local_u16TempValue ) )
			{
				Global_u8TempValue = Local_u16TempValue ;
				Global_s8TempError = Global_u8TempValue - Global_u8TempSetValue ;
				ADC_enuStartConversion();
			}

		}
#else
		if( Global_blConverted == TRUE )
		{
			if( ES_OK == LM35_enuReadTemp( &Local_u16TempValue ) )
			{
				Local_u16TempAccValue += Local_u16TempValue ;
				ADC_enuStartConversion();
				Local_u8TempReadCounter--;
			}
		}
		if( !Local_u8TempReadCounter )
		{
			Local_u8TempReadCounter = TEMP_AVG_READINGS;
			Global_u8TempValue = Local_u16TempAccValue / TEMP_AVG_READINGS ;
			Global_s8TempError = Global_u8TempValue - Global_u8TempSetValue ;
#endif
			if( Global_s8TempError >= HTR_TEMP_TOLERANCE )
			{
				if( HeaterStatus == OFF )
				{
					CoolentStatus = ON ;
					LedStatus = LD_ON ;
#if DEBUG !=1
					TMU_vidResumeTask( 5 );
#endif
				}
				else
				{
					HeaterStatus = OFF ;
				}
			}
			else if( Global_s8TempError <= -COOLENT_TEMP_TOLERANCE )
			{
				if( CoolentStatus == OFF )
				{
					HeaterStatus = ON ;
					LedStatus = LD_BLINK ;
#if DEBUG !=1
					TMU_vidResumeTask( 6 );
#endif
				}
				else
				{
					CoolentStatus = OFF ;
				}
#if DEBUG !=1
			}
#endif
		}
	}
	else
	{
		LedStatus = LD_OFF ;
		HeaterStatus = OFF ;
		CoolentStatus = OFF ;
#if DEBUG !=1
		TMU_vidPauseTask( 8 );
#endif
	}
}

void CheckIncrementSwitch(void *pNULL)
{
	u8 Local_u8SwitchValue;


		if( ES_OK == Switch_enuGetPressed( INC_SW , &Local_u8SwitchValue ) )
		{
			static u8 press = 0, BounceDelay = BOUNCE_COUNTS ;
			static u16 SetupDelay = SETUP_COUNTS ;
			if ( (Local_u8SwitchValue == DIO_u8HIGH ) && press == 0 )  // First press
			{
				if( Global_u8DisplayMode == NORMAL )
				{
					Global_u8DisplayMode = SETUP ;
					Global_u8SetupSw = INC_SW ;
				}
				else
				{
					if( Global_u8SetupSw != INC_SW )
					{
						Global_u8SetupSw = INC_SW ;
					}
					if( Global_u8TempSetValue < TEMP_MAX_LIMIT )
					{
						SetupDelay = SETUP_COUNTS;
						press = 1 ;
#if DEBUG == 1
						LedStatus = LD_ON ;
#else
						Global_u8TempSetValue++;
#endif
					}
				}
			}
			else if ( (Local_u8SwitchValue == DIO_u8LOW ) && press == 1 ) // Removed first press
			{
				BounceDelay--;
				if( !BounceDelay )
				{
					press = 0 ;
					BounceDelay = BOUNCE_COUNTS ;
				}
			}
			else if (	( Global_u8SetupSw == INC_SW ) &&	press == 0  &&
						(Local_u8SwitchValue == DIO_u8LOW ) ) // Unpress delay in Setup mode
			{
				SetupDelay--;
				if( !SetupDelay )
				{
					TMU_vidResumeTask( 0 );
					Global_u8DisplayMode = NORMAL ;
					Global_u8SetupSw = NONE ;
					SetTempUpdate = TRUE ;
				}
			}

		}
}
void CheckDecrementSwitch(void *pNULL )
{
	u8 Local_u8SwitchValue;

	if( ES_OK == Switch_enuGetPressed( DEC_SW , &Local_u8SwitchValue ) )
	{
		static u8 press = 0, BounceDelay = BOUNCE_COUNTS ;
		static u16 SetupDelay = SETUP_COUNTS ;

		if ( (Local_u8SwitchValue == DIO_u8HIGH ) && press == 0 )  //First press
		{
			if( Global_u8DisplayMode == NORMAL )
			{
				Global_u8DisplayMode = SETUP ;
				Global_u8SetupSw = DEC_SW ;
			}
			else
			{
				if( Global_u8SetupSw != DEC_SW )
				{
					Global_u8SetupSw = DEC_SW ;
				}
				if( Global_u8TempSetValue > TEMP_MIN_LIMIT )
				{
					SetupDelay = SETUP_COUNTS;
					press = 1 ;
#if DEBUG == 1
					LedStatus = LD_OFF ;
#else
					Global_u8TempSetValue--;
#endif
				}
			}
		}
		else if ( (Local_u8SwitchValue== DIO_u8LOW ) && press == 1 ) // removed first press
		{
			BounceDelay--;
			if( !BounceDelay )
			{
				press = 0 ;
				BounceDelay = BOUNCE_COUNTS ;
			}
		}
		else if (	( Global_u8SetupSw == DEC_SW )	&&	press == 0	&&
					(Local_u8SwitchValue == DIO_u8LOW ) )						 // Unpress delay in Setup mode
		{
			SetupDelay--;
			if( !SetupDelay )
			{
				TMU_vidResumeTask( 0 );
				Global_u8SetupSw = NONE ;
				Global_u8DisplayMode = NORMAL ;
				SetTempUpdate = TRUE ;
			}
		}

	}
}

void CheckPowerSwitch(void *pNULL )
{
	u8 Local_u8SwitchValue;

	if( ES_OK == Switch_enuGetPressed( PWR_SW , &Local_u8SwitchValue ) )
	{
		static u8 press = 0, BounceDelay = BOUNCE_COUNTS ;

		if ( (Local_u8SwitchValue == DIO_u8HIGH ) && press == 0 )  //First press
		{
			press = 1 ;
			if( PowerStatus == OFF )
			{
				PowerStatus = ON ;
#if DEBUG == 1
				LedStatus = LD_OFF ;
#else
				TMU_vidResumeTask( 8 );
				TMU_vidResumeTask( 7 );
				TMU_vidResumeTask( 6 );
				TMU_vidResumeTask( 5 );
				TMU_vidResumeTask( 4 );
				TMU_vidResumeTask( 3 );
				TMU_vidResumeTask( 2 );
#endif
			}
			else if( PowerStatus == ON )
			{
				PowerStatus = OFF ;
#if DEBUG == 1
				LedStatus = LD_BLINK ;
#else
				TMU_vidPauseTask( 3 );
				TMU_vidPauseTask( 2 );
#endif
			}
		}
		else if ( (Local_u8SwitchValue== DIO_u8LOW ) && press == 1 ) // removed first press
		{
			BounceDelay--;
			if( !BounceDelay )
			{
				press = 0 ;
				BounceDelay = BOUNCE_COUNTS ;
			}
		}
	}
}

void AdjustRedLampStatus(void *Copy_pu8RedLampStatus)		//	TASK PRIORITY = 4
{
	static u8 Local_u8PrevStatus = LD_OFF ;
	u8 *Local_u8RedLampStatus = (u8*)Copy_pu8RedLampStatus;

	if( *Local_u8RedLampStatus == LD_BLINK )
	{
		Local_u8PrevStatus = ( Local_u8PrevStatus == LD_ON )? LD_OFF : LD_ON ;
		LD_enuSetState( RED_LD , Local_u8PrevStatus );
	}
	else if( *Local_u8RedLampStatus != Local_u8PrevStatus)
	{
		Local_u8PrevStatus = *Local_u8RedLampStatus ;
		LD_enuSetState( RED_LD , Local_u8PrevStatus );
	}
	if( PowerStatus == OFF )
	{
		TMU_vidPauseTask( 4 );							//	TASK Self Pause in case of POWER OFF Status
	}
}

void AdjustHeaterStatus( void *pNULL)
{
	static OnOff_t PrevHeaterStatus = OFF;

	if( PrevHeaterStatus != HeaterStatus)
	{
		if( HeaterStatus == OFF )
		{
	//		Heater_enuSetState( (f32)( HTR_TEMP_TOLERANCE + 1.0 ));
			Heater_enuDisable();
			TMU_vidPauseTask( 6 );
		}
		else
		{
	//		Heater_enuSetState( Global_s8TempError );
			Heater_enuEnable();
		}
		PrevHeaterStatus = HeaterStatus ;
	}

}

void AdjustCoolentStatus( void *pNULL)
{
	static OnOff_t PrevCoolentStatus = OFF;

	if( PrevCoolentStatus != CoolentStatus)
	{
		if( CoolentStatus == OFF )
		{
	//		Coolent_enuSetState( (f32)( ( -COOLENT_TEMP_TOLERANCE ) - 1.0 ) );
			Coolent_enuDisable();
			TMU_vidPauseTask( 5 );
		}
		else
		{
	//		Coolent_enuSetState( Global_s8TempError );
			Coolent_enuEnable();
		}
		PrevCoolentStatus = CoolentStatus ;
	}
}

void EEPROM_Access( void *Copy_u16SetTempAddress )
{
	u16 *Local_u16TempAddress = (u16*)Copy_u16SetTempAddress ;
	static u8 Local_u8StoredSetTemp = INIT_SET_TEMP ;
	u8 Local_u8SetTempSaveID ;

	if( Local_u8StoredSetTemp != Global_u8TempSetValue)
	{
		if( ES_OK == EEPROM_enuReadByte( *Local_u16TempAddress , &Local_u8SetTempSaveID ) )
		{
			if( Local_u8SetTempSaveID != SetTempSaveID )
			{
				if( ES_OK == EEPROM_enuWriteByte( *Local_u16TempAddress , SetTempSaveID ) )
					Global_u8TempSetValue = INIT_SET_TEMP ;
			}
			if( ES_OK == EEPROM_enuWriteByte( *( Local_u16TempAddress + 1 ) , Global_u8TempSetValue ) )
				TMU_vidPauseTask( 0 );
		}
		Local_u8StoredSetTemp = Global_u8TempSetValue ;
	}
	SetTempUpdate = FALSE ;
}
