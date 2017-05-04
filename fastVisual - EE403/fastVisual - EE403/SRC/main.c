
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_emc.h"
#include "pin_mux.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "eGFX.h"
#include "eGFX_Driver.h"
#include "FONT_5_7_1BPP.h"
#include "pin_mux.h"
#include "fsl_device_registers.h"
#include "fsl_i2c.h"
#include "fsl_i2s.h"
#include "fsl_wm8904.h"
#include "Audio.h"
#include "fsl_iocon.h"
#include "fsl_common.h"
#include "clock_config.h"
#include "fsl_power.h"

#include "fsl_i2c.h"
#include "fsl_ft5406.h"

#include "arm_math.h"


#include "Sprites_16BPP_565.h"

#include "OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP.h"
#include "Consolas__26px__Regular__AntiAliasGridFit_1BPP.h"
#include "Magneto__26px__Regular__AntiAliasGridFit_16BPP_565.h" 
 
#define SLIDER_REGION_START_X	 85
#define SLIDER_REGION_STOP_X	 SLIDER_REGION_START_X+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeX
#define SLIDER_REGION_START_Y  50
#define SLIDER_REGION_STOP_Y	 (eGFX_PHYSICAL_SCREEN_SIZE_Y - 75)

#define SLIDER2_REGION_START_X	 160
#define SLIDER2_REGION_STOP_X	 SLIDER2_REGION_START_X+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeX
#define SLIDER2_REGION_START_Y  50
#define SLIDER2_REGION_STOP_Y	 (eGFX_PHYSICAL_SCREEN_SIZE_Y - 75)

#define SLIDER3_REGION_START_X	 235
#define SLIDER3_REGION_STOP_X	 SLIDER3_REGION_START_X+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeX
#define SLIDER3_REGION_START_Y  50
#define SLIDER3_REGION_STOP_Y	 (eGFX_PHYSICAL_SCREEN_SIZE_Y - 75)

#define SLIDER4_REGION_START_X	 335
#define SLIDER4_REGION_STOP_X	 SLIDER4_REGION_START_X+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeX
#define SLIDER4_REGION_START_Y  50
#define SLIDER4_REGION_STOP_Y	 (eGFX_PHYSICAL_SCREEN_SIZE_Y - 75)

#define BUTTON_REGION_START_X 	0
#define BUTTON_REGION_STOP_X		75
#define BUTTON_REGION_START_Y		0
#define BUTTON_REGION_STOP_Y		75

#define BUTTON2_REGION_START_X 	(eGFX_PHYSICAL_SCREEN_SIZE_X - 100)
#define BUTTON2_REGION_STOP_X		eGFX_PHYSICAL_SCREEN_SIZE_X
#define BUTTON2_REGION_START_Y	0
#define BUTTON2_REGION_STOP_Y		100

#define BUTTON3_REGION_START_X 	(eGFX_PHYSICAL_SCREEN_SIZE_X - 75)
#define BUTTON3_REGION_STOP_X		eGFX_PHYSICAL_SCREEN_SIZE_X
#define BUTTON3_REGION_START_Y	(eGFX_PHYSICAL_SCREEN_SIZE_Y - 75)
#define BUTTON3_REGION_STOP_Y		eGFX_PHYSICAL_SCREEN_SIZE_Y

#define BUTTON4_REGION_START_X 	0
#define BUTTON4_REGION_STOP_X		75
#define BUTTON4_REGION_START_Y	(eGFX_PHYSICAL_SCREEN_SIZE_Y - 75)
#define BUTTON4_REGION_STOP_Y		eGFX_PHYSICAL_SCREEN_SIZE_Y

#define  BUFFER_SIZE  									 1024

volatile float * MicBuffer;					
volatile float * BackgroundMicBuffer; 

uint32_t BackgroundBufferIdx = 0;     

volatile float MicBuffer1[BUFFER_SIZE];  
volatile float MicBuffer2[BUFFER_SIZE];
volatile uint32_t NextBufferReady = 0;

uint32_t BuffersCaptured = 0;

int32_t x=10,y=10;
int32_t x2=10,y2=10;
int32_t x3=10,y3=10;
int32_t x4=10,y4=10;

void DMIC0_IRQHandler()
{
	int16_t NextSampleIn;
	
	if(DMIC0->CHANNEL[1].FIFO_STATUS & DMIC_CHANNEL_FIFO_STATUS_INT_MASK)
	{
		DMIC0->CHANNEL[1].FIFO_STATUS |= DMIC_CHANNEL_FIFO_STATUS_INT_MASK;

		
		NextSampleIn = (int16_t)(DMIC0->CHANNEL[1].FIFO_DATA);
	
    BackgroundMicBuffer[BackgroundBufferIdx] =  (float)((NextSampleIn))  / 
((float)(1<<15));
		
		BackgroundBufferIdx++;

		if(BackgroundBufferIdx == BUFFER_SIZE)
		{
			  

/* Swap Buffers */
			if(BackgroundMicBuffer == MicBuffer1)
				{
					BackgroundMicBuffer = MicBuffer2;
					MicBuffer = MicBuffer1;
				}
				else
				{
					BackgroundMicBuffer = MicBuffer1;
					MicBuffer = MicBuffer2;
				}

				if(NextBufferReady == 0)
					NextBufferReady = 1;
				
	  		BackgroundBufferIdx=0;
	  }
  }
}

void InitMicBuffers()
{
	for (int i=0;i<BUFFER_SIZE;i++)
	{
		MicBuffer1[i] = 0;
		MicBuffer2[i] = 0;
	}

	MicBuffer = MicBuffer1;

	BackgroundMicBuffer = MicBuffer2;

	NextBufferReady = 0;
}

arm_rfft_fast_instance_f32 MyFFT;

float	 FFT_RawDataOut[BUFFER_SIZE];

float  FFT_PowerSpectrum[BUFFER_SIZE];

#define TEST1_PORT 4
#define TEST1_PIN  6

#define TEST2_PORT 3
#define TEST2_PIN  21

ft5406_handle_t touch_handle;
   
touch_event_t touch_event;

enum states {
	Home,
	Circles,
	Shattered,
	Lines,
	Equalizer
} state;
typedef enum states states;
states state;

int32_t BassSliderPosition = (SLIDER_REGION_STOP_Y + SLIDER_REGION_START_Y)/2;
int32_t MidSliderPosition = (SLIDER2_REGION_STOP_Y + SLIDER2_REGION_START_Y)/2;
int32_t TrebleSliderPosition = (SLIDER3_REGION_STOP_Y + SLIDER3_REGION_START_Y)/2;
int32_t VolumeSliderPosition = (SLIDER4_REGION_STOP_Y + SLIDER4_REGION_START_Y)/2;

float BassSliderValue;
float MidSliderValue;
float TrebleSliderValue;
float	VolumeSliderValue;


void SysTick_Handler()
{
	GPIO->NOT[TEST1_PORT] = 1<<TEST1_PIN;
}
void HomeScreen()
{
		eGFX_ImagePlane_Clear(&eGFX_BackBuffer);
	
		eGFX_printf_HorizontalCentered_Colored(&eGFX_BackBuffer,
											eGFX_PHYSICAL_SCREEN_SIZE_Y/2 - 15, 
											&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,   
											eGFX_RGB888_TO_RGB565(255,255,0),
											"fast      ");
	
		eGFX_printf_HorizontalCentered_Colored(&eGFX_BackBuffer,
											eGFX_PHYSICAL_SCREEN_SIZE_Y/2 - 15, 
											&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,   
											eGFX_RGB888_TO_RGB565(0,128,0),
											"      Visual");

		eGFX_printf_HorizontalCentered_Colored(&eGFX_BackBuffer,
											eGFX_PHYSICAL_SCREEN_SIZE_Y/2 + 10, 
											&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,   
											eGFX_RGB888_TO_RGB565(255,255,255),
											"An Audio Visualizer By The Globetrotters");
	
		eGFX_printf_HorizontalCentered_Colored(&eGFX_BackBuffer,
											10, 
											&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,   
											eGFX_RGB888_TO_RGB565(255,255,255),
											"Welcome to Home");
	
		eGFX_printf_HorizontalCentered_Colored(&eGFX_BackBuffer,
											(eGFX_PHYSICAL_SCREEN_SIZE_Y-45), 
											&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,   
											eGFX_RGB888_TO_RGB565(255,0,0),
											"<-- Click Here For Visualizers -->");
		// Home Button
		eGFX_Blit(&eGFX_BackBuffer,
								eGFX_PHYSICAL_SCREEN_SIZE_X - 60, //x coordinate
								0,			//y coordinate 
								&Sprite_16BPP_565_home_icon);
								
		// Equalizer Button
		eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								0,			//y coordinate 
								&Sprite_16BPP_565_equalizer);
								
		// Shattered Button						
		eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								eGFX_PHYSICAL_SCREEN_SIZE_Y - 60,			//y coordinate 
								&Sprite_16BPP_565_piramid_silver);
								
		// Circles Button
		eGFX_Blit(&eGFX_BackBuffer,
								eGFX_PHYSICAL_SCREEN_SIZE_X - 60, //x coordinate
								eGFX_PHYSICAL_SCREEN_SIZE_Y - 60,			//y coordinate 
								&Sprite_16BPP_565_whirl_silver);
								
							
	
		eGFX_Dump(&eGFX_BackBuffer);
}

void CirclesVisual()
{			
	if(NextBufferReady == 1)
			{
				
				GPIO->SET[TEST2_PORT]=1<<TEST2_PIN;
	
				arm_rfft_fast_f32(&MyFFT,
													  (float *)MicBuffer,
													  FFT_RawDataOut,0);
				
				GPIO->CLR[TEST2_PORT]=1<<TEST2_PIN;
														
				 arm_cmplx_mag_squared_f32(FFT_RawDataOut,
																	 FFT_PowerSpectrum,
																	 BUFFER_SIZE/2
																	);
										
				eGFX_ImagePlane_Clear(&eGFX_BackBuffer);
														
/*				// Shattered Button						
				eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								eGFX_PHYSICAL_SCREEN_SIZE_Y - 60,			//y coordinate 
								&Sprite_16BPP_565_piramid_silver);
*/
				// Equalizer Button
				eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								0,			//y coordinate 
								&Sprite_16BPP_565_equalizer);
				
				for(int i=0;i<eGFX_PHYSICAL_SCREEN_SIZE_X;i++)
				{
								float Point1 = (VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/2));

								float Point2 = (VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/2));
								
								float Point3 = (VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/1.5));

								float Point4 = -(VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5));
								
								float Point5 = -(VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/3));

								float Point6 = (VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/3));
					
					if (i < (200/BassSliderValue) && i < (300*BassSliderValue))
								{eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);	
								eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.3,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/4.3 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);			
								eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.2,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.2 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);			
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point3+eGFX_PHYSICAL_SCREEN_SIZE_X/1.5,
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point3+eGFX_PHYSICAL_SCREEN_SIZE_X/3,
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/3 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/3,
												Point6+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/1.5,
												Point6+eGFX_PHYSICAL_SCREEN_SIZE_Y/3 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
							eGFX_DrawCircle(&eGFX_BackBuffer,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.3,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.2 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.2,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/4.3 - FFT_PowerSpectrum[i]/12*BassSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);}
				else if (i >= (200/MidSliderValue) && i < (400/MidSliderValue))
								{eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);	
								eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.3,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/4.3 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);			
								eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.2,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.2 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);			
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point3+eGFX_PHYSICAL_SCREEN_SIZE_X/1.5,
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point3+eGFX_PHYSICAL_SCREEN_SIZE_X/3,
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/3 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/3,
												Point6+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/1.5,
												Point6+eGFX_PHYSICAL_SCREEN_SIZE_Y/3 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
							eGFX_DrawCircle(&eGFX_BackBuffer,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.3,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.2 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.2,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/4.3 - FFT_PowerSpectrum[i]/12*MidSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);}
							else if(i >= (400/TrebleSliderValue))
								{eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);	
								eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.3,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/4.3 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);			
								eGFX_DrawCircle(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.2,      
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.2 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);			
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point3+eGFX_PHYSICAL_SCREEN_SIZE_X/1.5,
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point3+eGFX_PHYSICAL_SCREEN_SIZE_X/3,
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/3 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/3,
												Point6+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.5 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/1.5,
												Point6+eGFX_PHYSICAL_SCREEN_SIZE_Y/3 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
							eGFX_DrawCircle(&eGFX_BackBuffer,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.3,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/1.2 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
								eGFX_DrawCircle(&eGFX_BackBuffer,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.2,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/4.3 - FFT_PowerSpectrum[i]/12*TrebleSliderValue,
												3,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);}
				}	
				
				
														
				eGFX_Dump(&eGFX_BackBuffer);
					
				NextBufferReady = 0;
		}
	}

void ShatteredVisual()
{

	if(NextBufferReady == 1)
			{
				GPIO->SET[TEST2_PORT]=1<<TEST2_PIN;
				
				arm_rfft_fast_f32(&MyFFT,
													  (float *)MicBuffer,
													  FFT_RawDataOut,0);
				
				GPIO->CLR[TEST2_PORT]=1<<TEST2_PIN;
														
				 arm_cmplx_mag_squared_f32(FFT_RawDataOut,
																	 FFT_PowerSpectrum,
																	 BUFFER_SIZE/2
																	);
										
				 eGFX_ImagePlane_Clear(&eGFX_BackBuffer);
				
	/*			// Circles Button
				eGFX_Blit(&eGFX_BackBuffer,
								eGFX_PHYSICAL_SCREEN_SIZE_X - 60, //x coordinate
								eGFX_PHYSICAL_SCREEN_SIZE_Y - 60,			//y coordinate 
								&Sprite_16BPP_565_whirl_silver);
											*/			
				// Equalizer Button
				eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								0,			//y coordinate 
								&Sprite_16BPP_565_equalizer);
														
				for(int i=0;i<eGFX_PHYSICAL_SCREEN_SIZE_X;i++)
				{
								float Point1 = (VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/2));

								float Point2 = (VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/2));
								
								float Point3 = (VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/1.5));

								float Point4 = -(VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/2));
								
								float Point5 = -(VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/2));

								float Point6 = (VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/3));
					
					if (i < (200/BassSliderValue) && i < (300*BassSliderValue))
								{
									eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*BassSliderValue,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*BassSliderValue,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*BassSliderValue,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*BassSliderValue,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*BassSliderValue,																					
												eGFX_RGB888_TO_RGB565(0,128,0)  												
												);	
							}		
					else if (i >= (200/MidSliderValue) && i < (400/MidSliderValue))
								{
									
									eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,									
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,									
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,									
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/12*MidSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,255,0)  												
												);	
											}
							else if( i >= (400/TrebleSliderValue))
								{
									eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/15*TrebleSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/2.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/15*TrebleSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/15*TrebleSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/1.33,									
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/15*TrebleSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);				
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point1+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,									
												Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/15*TrebleSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);	
								eGFX_DrawHline(&eGFX_BackBuffer,                       
												Point4+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_X/4.0,									
												Point5+eGFX_PHYSICAL_SCREEN_SIZE_Y/2.0 - FFT_PowerSpectrum[i]/15*TrebleSliderValue,																					
												eGFX_RGB888_TO_RGB565(255,0,0)  												
												);	
											}
				}	
				
				
														
				eGFX_Dump(&eGFX_BackBuffer);
					
				NextBufferReady = 0;
		}
	}

void LinesVisual()
{
	if(NextBufferReady == 1)
			{
				GPIO->SET[TEST2_PORT]=1<<TEST2_PIN;
				
				arm_rfft_fast_f32(&MyFFT,
													  (float *)MicBuffer,
													  FFT_RawDataOut,0);
				
				GPIO->CLR[TEST2_PORT]=1<<TEST2_PIN;
														
				 arm_cmplx_mag_squared_f32(FFT_RawDataOut,
																	 FFT_PowerSpectrum,
																	 BUFFER_SIZE/2
																	);
										
				 eGFX_ImagePlane_Clear(&eGFX_BackBuffer);

				// Equalizer Button
				eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								0,			//y coordinate 
								&Sprite_16BPP_565_equalizer);
														
				for(int i=0;i<eGFX_PHYSICAL_SCREEN_SIZE_X;i++)
				{
								float Point1 = (VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/2));

								float Point2 = (VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/2));
								
								float Point3 = (VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/1.5));

								float Point4 = -(VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/2));
								
								float Point5 = -(VolumeSliderValue*MicBuffer[i-1]*(eGFX_PHYSICAL_SCREEN_SIZE_X/2));

								float Point6 = (VolumeSliderValue*MicBuffer[i]*(eGFX_PHYSICAL_SCREEN_SIZE_Y/3));
					
					if (i < (200/BassSliderValue) && i < (300*BassSliderValue))   			

								{eGFX_DrawLine(&eGFX_BackBuffer, 
												i-1, Point1+eGFX_PHYSICAL_SCREEN_SIZE_Y/2, 
												i,   Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
									eGFX_DrawLine(&eGFX_BackBuffer, 
												i-1, Point3+eGFX_PHYSICAL_SCREEN_SIZE_Y/2, 
												i,   Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/2,
												eGFX_RGB888_TO_RGB565(0,128,0)
												);
								}
				else if (i >= (200/MidSliderValue) && i < (400/MidSliderValue)) 

								{eGFX_DrawLine(&eGFX_BackBuffer, 
												i-1, Point1+eGFX_PHYSICAL_SCREEN_SIZE_Y/2, 
												i,   Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
									eGFX_DrawLine(&eGFX_BackBuffer, 
												i-1, Point3+eGFX_PHYSICAL_SCREEN_SIZE_Y/2, 
												i,   Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/2,
												eGFX_RGB888_TO_RGB565(255,255,0)
												);
								}
							else if (i >= (400/TrebleSliderValue))
								{eGFX_DrawLine(&eGFX_BackBuffer, 
												i-1, Point1+eGFX_PHYSICAL_SCREEN_SIZE_Y/2, 
												i,   Point2+eGFX_PHYSICAL_SCREEN_SIZE_Y/2,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
									eGFX_DrawLine(&eGFX_BackBuffer, 
												i-1, Point3+eGFX_PHYSICAL_SCREEN_SIZE_Y/2, 
												i,   Point4+eGFX_PHYSICAL_SCREEN_SIZE_Y/2,
												eGFX_RGB888_TO_RGB565(255,0,0)
												);
								
								}

											}
				}	
				
				
														
				eGFX_Dump(&eGFX_BackBuffer);
					
				NextBufferReady = 0;
		}


void EqualizerScreen()
{

	  eGFX_ImagePlane_Clear(&eGFX_BackBuffer);
				
     if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y, &x))
        {
         if (touch_event == kTouch_Contact)
            {
							  if(	 x>=SLIDER_REGION_START_X && x<=SLIDER_REGION_STOP_X 
									&& y>SLIDER_REGION_START_Y && y<= SLIDER_REGION_STOP_Y)
								{
										BassSliderPosition = y;
								}
            }
        }
				
			if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y2, &x2))
        {
            if (touch_event == kTouch_Contact)
            {
							if(	 x2>=SLIDER2_REGION_START_X && x2<=SLIDER2_REGION_STOP_X 
									&& y2>SLIDER2_REGION_START_Y && y2<= SLIDER2_REGION_STOP_Y)
								{
										MidSliderPosition = y2;
								}
            }
        }
				
			if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y3, &x3))
        {
            if (touch_event == kTouch_Contact)
            {
							  if(	 x3>=SLIDER3_REGION_START_X && x3<=SLIDER3_REGION_STOP_X 
									&& y3>SLIDER3_REGION_START_Y && y3<= SLIDER3_REGION_STOP_Y)
								{
										TrebleSliderPosition = y3;
								}
            }
        }
				
			if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y4, &x4))
        {
            if (touch_event == kTouch_Contact)
            {
							  if(	 x4>=SLIDER4_REGION_START_X && x4<=SLIDER4_REGION_STOP_X 
									&& y4>SLIDER4_REGION_START_Y && y4<= SLIDER4_REGION_STOP_Y)
								{
										VolumeSliderPosition = y4;
								}
            }
        }
				
				eGFX_printf_HorizontalCentered_Colored(&eGFX_BackBuffer,
											eGFX_PHYSICAL_SCREEN_SIZE_Y-20, 
											&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,   
											eGFX_RGB888_TO_RGB565(255,255,255),
											"Equalizer");
				
				// Home Button
				eGFX_Blit(&eGFX_BackBuffer,
								eGFX_PHYSICAL_SCREEN_SIZE_X-60, // x coordinate
								0,			//y coordinate 
								&Sprite_16BPP_565_home_icon);
				
				// Shattered Button
				eGFX_Blit(&eGFX_BackBuffer,
								10, //x coordinate
								eGFX_PHYSICAL_SCREEN_SIZE_Y - 60,			//y coordinate 
								&Sprite_16BPP_565_piramid_silver);
				
				// Circles Button
				eGFX_Blit(&eGFX_BackBuffer,
								eGFX_PHYSICAL_SCREEN_SIZE_X - 60, //x coordinate
								eGFX_PHYSICAL_SCREEN_SIZE_Y - 60,			//y coordinate 
								&Sprite_16BPP_565_whirl_silver);

				eGFX_printf(&eGFX_BackBuffer,(SLIDER_REGION_STOP_X/2 - 12),(SLIDER_REGION_START_Y - 40), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"%.2f", BassSliderValue,eGFX_RGB888_TO_RGB565(0,128,0)); 
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER2_REGION_STOP_X/2 - 12),(SLIDER2_REGION_START_Y - 40), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"%.2f", MidSliderValue,eGFX_RGB888_TO_RGB565(255,255,0));
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER3_REGION_STOP_X/2 - 12),(SLIDER3_REGION_START_Y - 40), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"%.2f", TrebleSliderValue,eGFX_RGB888_TO_RGB565(255,0,0));
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER4_REGION_STOP_X/2 - 12),(SLIDER4_REGION_START_Y - 40), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"%.2f", VolumeSliderValue,eGFX_RGB888_TO_RGB565(0,0,255));
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER_REGION_STOP_X/2 - 12),(SLIDER_REGION_STOP_Y + 30), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"Low", BassSliderValue); 
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER2_REGION_STOP_X/2 - 12),(SLIDER2_REGION_STOP_Y + 30), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"Mid", MidSliderValue);
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER3_REGION_STOP_X/2 - 12),(SLIDER3_REGION_STOP_Y + 30), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"High", TrebleSliderValue);
				
				eGFX_printf(&eGFX_BackBuffer,(SLIDER4_REGION_STOP_X/2 - 12),(SLIDER4_REGION_STOP_Y + 30), 
				&OCR_A_Extended__20px__Bold__SingleBitPerPixelGridFit_1BPP,"Vol");
				
				BassSliderValue =  (float)(SLIDER_REGION_STOP_Y - BassSliderPosition)/(float)(SLIDER_REGION_STOP_Y-SLIDER_REGION_START_Y);
				MidSliderValue =  (float)(SLIDER2_REGION_STOP_Y - MidSliderPosition)/(float)(SLIDER2_REGION_STOP_Y-SLIDER2_REGION_START_Y);
				TrebleSliderValue =  (float)(SLIDER3_REGION_STOP_Y - TrebleSliderPosition)/(float)(SLIDER3_REGION_STOP_Y-SLIDER3_REGION_START_Y);
				VolumeSliderValue =  (float)(SLIDER4_REGION_STOP_Y - VolumeSliderPosition)/(float)(SLIDER4_REGION_STOP_Y-SLIDER4_REGION_START_Y);
	
				eGFX_Box SliderFillBox;

				SliderFillBox.P1.Y = BassSliderPosition;  
				SliderFillBox.P1.X = (SLIDER_REGION_STOP_X+SLIDER_REGION_START_X)/2 - 5;
				SliderFillBox.P2.Y = SLIDER_REGION_STOP_Y + Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2;
				SliderFillBox.P2.X = (SLIDER_REGION_STOP_X+SLIDER_REGION_START_X)/2 + 5;
				
				eGFX_DrawFilledBox(&eGFX_BackBuffer,&SliderFillBox,eGFX_RGB888_TO_RGB565(0,128,0));
								
				eGFX_Blit(&eGFX_BackBuffer,
								SLIDER_REGION_START_X,
								BassSliderPosition - Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2 + 1,																			 //y coordinate 
								&Sprite_16BPP_565_Mushroom_Super_icon_red);																								
			
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER_REGION_START_Y,SLIDER_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2+1,SLIDER_REGION_START_Y,SLIDER_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER_REGION_START_Y,SLIDER_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2-1,SLIDER_REGION_START_Y,SLIDER_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				
				// Band 2 - Yellow
				
				eGFX_Box SliderFillBox2;

				SliderFillBox2.P1.Y = MidSliderPosition;   
				SliderFillBox2.P1.X = (SLIDER2_REGION_STOP_X+SLIDER2_REGION_START_X)/2 - 5;
				SliderFillBox2.P2.Y = SLIDER2_REGION_STOP_Y + Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2;
				SliderFillBox2.P2.X = (SLIDER2_REGION_STOP_X+SLIDER2_REGION_START_X)/2 + 5;
				
				eGFX_DrawFilledBox(&eGFX_BackBuffer,&SliderFillBox2,eGFX_RGB888_TO_RGB565(255,255,0));
								
				eGFX_Blit(&eGFX_BackBuffer,
								SLIDER2_REGION_START_X,
								MidSliderPosition - Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2 + 1,																			 //y coordinate of where to put the monkey head
								&Sprite_16BPP_565_Mushroom_Super_icon_red);																								
			
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER2_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER2_REGION_START_Y,SLIDER2_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER2_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2+1,SLIDER2_REGION_START_Y,SLIDER2_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER2_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER2_REGION_START_Y,SLIDER2_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER2_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2-1,SLIDER2_REGION_START_Y,SLIDER2_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				
				// Band 3 - Red				
				eGFX_Box SliderFillBox3;

				SliderFillBox3.P1.Y = TrebleSliderPosition;   
				SliderFillBox3.P1.X = (SLIDER3_REGION_STOP_X+SLIDER3_REGION_START_X)/2 - 5;
				SliderFillBox3.P2.Y = SLIDER3_REGION_STOP_Y + Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2;
				SliderFillBox3.P2.X = (SLIDER3_REGION_STOP_X+SLIDER3_REGION_START_X)/2 + 5;
				
				eGFX_DrawFilledBox(&eGFX_BackBuffer,&SliderFillBox3,eGFX_RGB888_TO_RGB565(255,0,0));
								
				eGFX_Blit(&eGFX_BackBuffer,
								SLIDER3_REGION_START_X,
								TrebleSliderPosition - Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2 + 1,																			 //y coordinate of where to put the monkey head
								&Sprite_16BPP_565_Mushroom_Super_icon_red);																								
			
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER3_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER3_REGION_START_Y,SLIDER3_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER3_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2+1,SLIDER3_REGION_START_Y,SLIDER3_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER3_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER3_REGION_START_Y,SLIDER3_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER3_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2-1,SLIDER3_REGION_START_Y,SLIDER3_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
			  
				// Band 4 - Volume - Blue
				
				eGFX_Box SliderFillBox4;

				SliderFillBox4.P1.Y = VolumeSliderPosition;   
				SliderFillBox4.P1.X = (SLIDER4_REGION_STOP_X+SLIDER4_REGION_START_X)/2 - 5;
				SliderFillBox4.P2.Y = SLIDER4_REGION_STOP_Y + Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2;
				SliderFillBox4.P2.X = (SLIDER4_REGION_STOP_X+SLIDER4_REGION_START_X)/2 + 5;
				
				eGFX_DrawFilledBox(&eGFX_BackBuffer,&SliderFillBox4,eGFX_RGB888_TO_RGB565(0,0,255));
								
				eGFX_Blit(&eGFX_BackBuffer,
								SLIDER4_REGION_START_X,
								VolumeSliderPosition - Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2 + 1,																			 //y coordinate of where to put the monkey head
								&Sprite_16BPP_565_Mushroom_Super_icon_red);																								
			
			
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER4_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER4_REGION_START_Y,SLIDER4_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER4_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2+1,SLIDER4_REGION_START_Y,SLIDER4_REGION_START_X-5,eGFX_RGB888_TO_RGB565(255,255,255));
				
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER4_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2,SLIDER4_REGION_START_Y,SLIDER4_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_DrawVline(&eGFX_BackBuffer,SLIDER4_REGION_STOP_Y+Sprite_16BPP_565_Mushroom_Super_icon_red.SizeY/2-1,SLIDER4_REGION_START_Y,SLIDER4_REGION_STOP_X+5,eGFX_RGB888_TO_RGB565(255,255,255));
				eGFX_Dump(&eGFX_BackBuffer);
	}
int main(void)
{
		CLOCK_EnableClock(kCLOCK_InputMux);
		
    CLOCK_EnableClock(kCLOCK_Iocon);
	
    CLOCK_EnableClock(kCLOCK_Gpio0);
  
    CLOCK_EnableClock(kCLOCK_Gpio1);
		
    CLOCK_EnableClock(kCLOCK_Gpio2);
  
    CLOCK_EnableClock(kCLOCK_Gpio3);
			
    CLOCK_EnableClock(kCLOCK_Gpio4);

    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    BOARD_InitPins();
	
    BOARD_BootClock_PLL_RUN();
  
    BOARD_InitDebugConsole();
		
		BOARD_InitSPIFI_ExternalFlash();

		BOARD_InitSDRAM();

		eGFX_InitDriver();

    InitMicBuffers();

   	Init_DMIC();
	
	  IOCON_PinMuxSet(IOCON, TEST1_PORT, TEST1_PIN, IOCON_FUNC0 |
		IOCON_MODE_INACT| IOCON_DIGITAL_EN);

    IOCON_PinMuxSet(IOCON, TEST2_PORT, TEST2_PIN, IOCON_FUNC0 |
		IOCON_MODE_INACT| IOCON_DIGITAL_EN);
		
    GPIO->DIR[TEST1_PORT] |= 1<<TEST1_PIN;
	 
	  GPIO->DIR[TEST2_PORT] |= 1<<TEST2_PIN;
	 
	  SysTick_Config(SystemCoreClock/1000);

	  arm_rfft_fast_init_f32(&MyFFT,1024);
		
		FT5406_Init(&touch_handle,I2C2);
		
		int32_t x5=10,y5=10;
		int32_t x6=10,y6=10;
		int32_t x7=10,y7=10;
		int32_t x8=10,y8=10;

	while(1)
	{
		
		// Top Left Button
		if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y5, &x5))
						{
							if (touch_event == kTouch_Down || touch_event == kTouch_Up)
											{
										if(	 x5>=BUTTON_REGION_START_X && x5<=BUTTON_REGION_STOP_X
														&& y5>BUTTON_REGION_START_Y && y5<=BUTTON_REGION_STOP_Y) // Top Left Corner
														{
																state = Equalizer;
														
														}
											}
							}
						
	// Top Right Button
		if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y6, &x6))
						{
							if (touch_event == kTouch_Down || touch_event == kTouch_Up)
											{
										if(	 x6>=BUTTON2_REGION_START_X && x6<=BUTTON2_REGION_STOP_X
														&& y6>BUTTON2_REGION_START_Y && y6<=BUTTON2_REGION_STOP_Y)
														{
																state = Home;
														}
											}
							}
						
		// Bottom Right Button		
		if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y7, &x7))
						{
							if (touch_event == kTouch_Down || touch_event == kTouch_Up)
											{
										if(	 x7>=BUTTON3_REGION_START_X && x7<=BUTTON3_REGION_STOP_X
														&& y7>BUTTON3_REGION_START_Y && y7<=BUTTON3_REGION_STOP_Y)
														{
																state = Circles;
														}
											}
							}
						
		// Bottom Left Button
		if (kStatus_Success == FT5406_GetSingleTouch(&touch_handle, &touch_event, &y8, &x8))
						{
							if (touch_event == kTouch_Down || touch_event == kTouch_Up)
											{
										if(	 x8>=BUTTON4_REGION_START_X && x8<=BUTTON4_REGION_STOP_X
														&& y8>BUTTON4_REGION_START_Y && y8<=BUTTON4_REGION_STOP_Y)
														{
																state = Shattered;
														}
											}
							}
						
		if(state == Home){
			HomeScreen();
		}
		if(state == Circles){
			CirclesVisual();
		}
		if(state == Shattered){
			ShatteredVisual();
		}
		if(state == Lines){
			LinesVisual();
		}
		if(state == Equalizer){
			EqualizerScreen();
		} 
		
		}

}

