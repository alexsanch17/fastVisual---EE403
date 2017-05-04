#include "fsl_device_registers.h"
#include "fsl_i2c.h"
#include "fsl_i2s.h"
#include "fsl_wm8904.h"
#include "fsl_debug_console.h"

#define I2C2_MASTER_CLOCK_FREQUENCY (12000000)

#define SAMPLE_RATE			 (32000U)									//This is what we want the Macro to be
#define SAMPLE_RATE_REG  kWM8904_SampleRate32kHz  //This enum gets sent to the codec
#define OVERSAMPLE_RATE  kWM8904_FsRatio768X    //Our PLL Rate is 24576000.   So, the oversample rate is 24576000/32000 which is 768
#define I2S_CLOCK_DIVIDER (CLOCK_GetAudioPllOutFreq() / SAMPLE_RATE / 16U / 2U)  //THis computes the clock rate for the I2S bit clock.

/*

	These  data structures are used to initialize the I2S units.
	We are going to use the SDK drivers to set up the I2S peripherals.  The LPC54608
	has a bunch of "Flexcomm" units that can be configured to be several different peripheral
	types.   In the case of I2S,   Flexcomm channels 6 and 7 can be configured for I2S
	
	UM10912 (LPC5460x User Manual) Chapter 21 describes the Flexomm

*/

static i2s_config_t s_TxConfig;
static i2s_config_t s_RxConfig;

/*This function hides the complicated initialization of the onboard CODEC*/
void InitAudio_CODEC(void)
{

   /*
			Before we set the Flexcom channels for I2S,  we have to set up the
		external WM8904 CODEC.  It has many configuration settings.   Get the datasheet for the WM8904 and you can see 
	  all of the different options.  Page 6 of the WM8904 has a block diagram of the internal functions of the CODEC.
	
		To configure the WM8904,  we have to use an I2C interface.  I2C is a two-wire serial bus
	  that is  common used for inter chip communication.  The WM8904 has an I2C interface which is used
	  to configure the CODEC.   There are registers exposed over this interface.    This init fuction will
	  take care of this intialization. 
	
	  To talk I2C,  we are going to use the NXP SDK drivers and an NXP driver for the WM8904.  The structures below
	   are used by the drivers to configure the I2C interface and the WM8904
	 */
	
   	i2c_master_config_t i2cConfig;

    wm8904_config_t codecConfig;
  
    wm8904_handle_t codecHandle;
	
	 /*
				The external CODEC requires a master clock.  This is used by the sigma-delta converters in the CODEC.
				The LPC54608 has a phased locked loop (PLL) clock synthesizer dedicated for generating audio
				clocks.   The structures below are used by the NXP drivers for the Audio PLL
	 */
	 
	  pll_config_t audio_pll_config = 
		{
        .desiredRate = 24576000U, .inputRate = 12000000U,
    };
		
		pll_setup_t audio_pll_setup;
		
		/*
				The I2C signal that are connected to the WM8902 is from FlexComm 2. (See the schematic for the LPCXpresso54608 to see this connection.
			  Enable the clock for the I2C
	  */
    
		CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);
		
    /* reset FLEXCOMM for I2C */
    RESET_PeripheralReset(kFC2_RST_SHIFT_RSTn);
		
    /* Initialize AUDIO PLL clock */
    CLOCK_SetupAudioPLLData(&audio_pll_config, &audio_pll_setup);
    audio_pll_setup.flags = PLL_SETUPFLAG_POWERUP | PLL_SETUPFLAG_WAITLOCK;
    CLOCK_SetupAudioPLLPrec(&audio_pll_setup, audio_pll_setup.flags);

    /*
			Flexcomm 6 and 7 are used the I2S channels (one for transmit and one for recieve)
   		I2S clocks will be derived from the Audio PLL.
		*/
    CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM6);
    CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM7);
		
		/* Attach PLL clock to MCLK for I2S, no divider */
    CLOCK_AttachClk(kAUDIO_PLL_to_MCLK);
    SYSCON->MCLKDIV = SYSCON_MCLKDIV_DIV(0U);
		
    /*
		 *  Initialize the I2C to talk to the WM8904
		 *
     * enableMaster = true;
     * baudRate_Bps = 100000U;
     * enableTimeout = false;
     */
    I2C_MasterGetDefaultConfig(&i2cConfig);
    i2cConfig.baudRate_Bps = WM8904_I2C_BITRATE;
    I2C_MasterInit(I2C2, &i2cConfig, I2C2_MASTER_CLOCK_FREQUENCY);


    /*
			Now that the I2C is setup, we can talk to the WM8904
			We are using the NXP provided provided drivers
     */
    WM8904_GetDefaultConfig(&codecConfig);
		codecConfig.format.fsRatio = OVERSAMPLE_RATE;     //These Macros are set at the top of the file.
		codecConfig.format.sampleRate = SAMPLE_RATE_REG;
		
		/*
		Tell the driver what I2C Channel we are use and then initialize the CODEC
		*/
    
		codecHandle.i2c = I2C2;
    if (WM8904_Init(&codecHandle, &codecConfig) != kStatus_Success)
    {
        PRINTF("WM8904_Init failed!\r\n");
    }
		
    /* Adjust output volume it to your needs, 0x0006 for -51 dB, 0x0039 for 0 dB etc. */
    /* Page 154 of the WM8904 data sheet has the volume settings*/
		WM8904_SetVolume(&codecHandle, 0x0039, 0x0039);
  
    /*
			The WM8904 is now configured.   We have to set the I2S channels
		  to transmit and recieve audio data
		
		*/
		
    /* Reset FLEXCOMM for I2S.  Flexcomm 6 is the transmit interface and FlexComm7 is the receive*/
    RESET_PeripheralReset(kFC6_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kFC7_RST_SHIFT_RSTn);
		
		
		/*
			We are using the NXP Driver to setup the I2S.   We must populate
		  the configuration structures and their drivers will hit the registers for us.
		
		  The Transmitter is setup to by an I2S master and the receiver as an I2S slave
			
		*/
		/*
     * masterSlave = kI2S_MasterSlaveNormalMaster;
     * mode = kI2S_ModeI2sClassic;
     * rightLow = false;
     * leftJust = false;
     * pdmData = false;
     * sckPol = false;
     * wsPol = false;
     * divider = 1;
     * oneChannel = false;
     * dataLength = 16;
     * frameLength = 32;
     * position = 0;
     * watermark = 4;
     * txEmptyZero = true;
     * pack48 = false;
     */
    I2S_TxGetDefaultConfig(&s_TxConfig);
    s_TxConfig.divider = I2S_CLOCK_DIVIDER;  //This macro is set at the top of the file.  It is a divder to generate the bit clock

    /*
     * masterSlave = kI2S_MasterSlaveNormalSlave;
     * mode = kI2S_ModeI2sClassic;
     * rightLow = false;
     * leftJust = false;
     * pdmData = false;
     * sckPol = false;
     * wsPol = false;
     * divider = 1;
     * oneChannel = false;
     * dataLength = 16;
     * frameLength = 32;
     * position = 0;
     * watermark = 4;
     * txEmptyZero = false;
     * pack48 = false;
     */

    I2S_RxGetDefaultConfig(&s_RxConfig);
		
		
		/*
			The only setting we will change is the FIFO Watermark.
			The incoming and outgoing I2S data go through a set of FIFO buffers.
			The watermark level is the point at which the I2S will generate an interrupt.
			
			We are are going to interrupt when there are at least 2 samples available or less than 2 to transmit.
		  This will give us some to to prep a new sample while another is being transmitted or recieved.
		*/
		
		s_RxConfig.watermark = 2;
		s_TxConfig.watermark = 2;
		
		/*
			Pass our configuration structures to the driver
		*/
    I2S_TxInit(I2S0, &s_TxConfig);
    I2S_RxInit(I2S1, &s_RxConfig);
		
		/*
			Enable Interrupts to the Flexcomms and set to the highest priority
			We are going to trigger interrupts on the FIFO level (watermark) flags 
		*/
		NVIC_SetPriority(FLEXCOMM6_IRQn,0);
		NVIC_SetPriority(FLEXCOMM7_IRQn,0);
				
		NVIC_EnableIRQ(FLEXCOMM6_IRQn);
		NVIC_EnableIRQ(FLEXCOMM7_IRQn);
		
		/*
			  We are going to put in some data to transmit FIFO to get it Started!
				THis will ensure there is always some data to be sent out.   Out Rx IRQ will be putting a sample into the output
				queue	everytime it gets a sample in from the input queue
		*/
		I2S0->FIFOWR = 0xFFFFFFFF;
		I2S0->FIFOWR = 0xFFFFFFFF;
	
    I2S_EnableInterrupts(I2S0, kI2S_TxLevelFlag);
    I2S_EnableInterrupts(I2S1, kI2S_RxLevelFlag);
    
    I2S_Enable(I2S0);
		I2S_Enable(I2S1);


	  /* At this point, the interrupts will start firing*/

}

/***
 *      ____  _       _ _        _                            
 *     |  _ \(_) __ _(_) |_ __ _| |                           
 *     | | | | |/ _` | | __/ _` | |                           
 *     | |_| | | (_| | | || (_| | |                           
 *     |____/|_|\__, |_|\__\__,_|_|   _                       
 *     |  \/  (_)___/ _ __ ___  _ __ | |__   ___  _ __   ___  
 *     | |\/| | |/ __| '__/ _ \| '_ \| '_ \ / _ \| '_ \ / _ \ 
 *     | |  | | | (__| | | (_) | |_) | | | | (_) | | | |  __/ 
 *     |_|  |_|_|\___|_|  \___/| .__/|_| |_|\___/|_| |_|\___| 
 *      ___       _            |_|                            
 *     |_ _|_ __ | |_ ___ _ __ / _| __ _  ___ ___             
 *      | || '_ \| __/ _ \ '__| |_ / _` |/ __/ _ \            
 *      | || | | | ||  __/ |  |  _| (_| | (_|  __/            
 *     |___|_| |_|\__\___|_|  |_|  \__,_|\___\___|            
 *     / ___|  ___| |_ _   _ _ __                             
 *     \___ \ / _ \ __| | | | '_ \                            
 *      ___) |  __/ |_| |_| | |_) |                           
 *     |____/ \___|\__|\__,_| .__/                            
 *                          |_|                               
 */

void Init_DMIC(void)
{
		
	/*
	Setting up a peripheral is generally always the same.
	a.)  Set up and enable a clocks to the peripheral.    By default,  peripherals may not be clocked at boot to save power
	b.)  If the peripheral is wired to the outside world, you must set up the pin multiplexers.   Because there
		    are many peripherals they must share IO pins.  There is a multiplexor at each IO pin to select the function.
	       See Chapter 7 (IOCON) of the the LPC54608 User manual (page 137).   Each pin has peripherals that can be routed to it.
	       Table 173 shows you how to select what is routed to each pin.
	c.)  Setup the core peripheral registers to configure its operation
	d.)  Enable interrupts (if used) in the NVIC.  There also may be interrupt masks in the pheriphal registers that need set
	e.)  Go!
	*/
	
	
	
	
	/*
		   Ok Let's set up the clock for the DMIC
			
				We start in the "SYSCON" registers. (chapter 4 in the user manual).
	
				The System Configuration peripheral is where start when configuring the clocks.
	*/
		
  /*		There is a a register for the DMIC clock. (page 61 in chapter 3).   
				Here we can select what sources our DMIC.    
				
				In the line below,  notice that the register is mapped to a struct.   If you look in "fsl_device_registers.h"
				you can see another #include file "LPC54608.h".  This file has register definitions.    Instead of having
				a macro (#define) for every register,   there is a data structure for each periphal that has the same layout
				as the pheripheral block in memory.   Pointers are created to these structure types that point the BASE ADDRESS of the peripheral.
				This makes accessing the registers a bit easier as we can use structure notation.
				
				We use the -> notation when getting to a register as the #define for the base address are pointers to a struct.
		
			  1st we will select the clock source for the DMIC.    We will select the 12MHZ FRO clock.   There is a 12MHz internal oscillator in the chip that
				can be used clocking peripherals.  Clocking can be complicated but it is all described inthe SYSCON chapter.   
				Section 4.4.1 describes all the different clocking options.
	*/
	
	  SYSCON->DMICCLKSEL = 0; //Select 12MHZ FRO for clock source
	
	/* Each peripheral clock can also be  divided down.    Section 4.5.58 of the user manual describes the DMIC clock divider.  
	We sometimes want to divide clocks to get other application timings just right. (In our case the same rate of the microphone).*/
	
	SYSCON->DMICCLKDIV = 2; //Divide by 3.   Note that divide by one is a zero,   divide by 2 is a 1, etc.
	
	//The clock source is selected.   Now we have to enable the "gate"

	/*Page 54 of the user manual shows the bits in AHB clock control register 1.  This is where the bit for gating the DMIC
	peripheral is at.  Also notice that I am not using a logical OR operationto set the bit.   There are registers that
	allow you to set bits just by writing a 1 to a bit in the register. (see 4.5.22 on page 55)
	*/

  SYSCON->AHBCLKCTRLSET[1] = SYSCON_AHBCLKCTRL_DMIC_MASK; 

	
	/*For good measure we can reset a periperiphal through peripheral set and reset registers.   See Section 4.5.10 for DMIC reset
	Notice the usage of "set" and "clear" registers!   These are special registers that allow you to directly set or clear bits by writing ones to the registers.
	*/
	
	 SYSCON->PRESETCTRLSET[1] = SYSCON_AHBCLKCTRL_DMIC_MASK; //assert the reset
	 SYSCON->PRESETCTRLCLR[1] = SYSCON_AHBCLKCTRL_DMIC_MASK; //deassert the reset

  /*We need the DMIC function routed to the physical IO pins.   
	  Open the schematic for the LPCXPresso54608 board.   You will see digital mic on page 9.  It has 2 IO signals CLK and Data
		that are connected to microcontroller IO pins P1_2 and P1_3
		
		Because there  are many peripherals they must share IO pins.  There is a MUX at each IO pin to select the function.
	   See Change 7 (IOCON) of the the LPC54608 User manual (page 137).   each pin has certain functions that can be enabled.
	   Table 173 shows you how to select the different peripherals for each IO pin.
		 
		 Notice the array notation below.   The struct that represents the IOCON was organized such that the arrays map
		 to the IO ports and pins.  pretty cool huh?   Besides selecting the pin function  with the multiplexer,  there are some other bits that
		 configure an IO pin properties.  See Section 7.4.2
		 
	*/
	IOCON->PIO[1][2] = IOCON_PIO_DIGIMODE(1)  | IOCON_PIO_FILTEROFF(1) | IOCON_PIO_FUNC(5);
	IOCON->PIO[1][3] = IOCON_PIO_DIGIMODE(1)  | IOCON_PIO_FILTEROFF(1) | IOCON_PIO_FUNC(5);

   
	 /*At this point the clock is set up and the peripheral is routed to the IO pins,  we now setup the core peripheral registers.
	 
	 At this point,  there are details about the DMIC we will cover later (like how it actually works) but you can see below that we
	 just need to set and clear bits in the control registers*/

	DMIC0->CHANEN  = 0x02;  //Enable Channel 1
	DMIC0->IOCFG = 0;			  //Each Channel is independent

	DMIC0->CHANNEL[1].PHY_CTRL = 1 ; // Sample on the rising Edge and use standard sampling
	
	//Set up the DC blocking filter,  limit to 16-bits and apply some gain.
	DMIC0->CHANNEL[1].DC_CTRL = DMIC_CHANNEL_DC_CTRL_DCPOLE(3)            | 
															DMIC_CHANNEL_DC_CTRL_SATURATEAT16BIT_MASK | 
															DMIC_CHANNEL_DC_CTRL_DCGAIN(2);
	
								
	DMIC0->CHANNEL[1].PREAC4FSCOEF = 0 ;// No Compenstation
	DMIC0->CHANNEL[1].GAINSHIFT = 0; //No Shift in gain (twos complement 16 bit)
		
		
	/*See 26.7.2.2. (Page 461) of the user manual.   The sample rate
	is controlled by the 2FS register, the OSR register and the DMIC clock rate.
	Knowing how we set up the clock to the DMIC at the beginning of this function, 
	can you calculate the sample rate we are now using?
	*/
	
	DMIC0->USE2FS = 1; 		         	// use 1 FS for PCM data
	DMIC0->CHANNEL[1].OSR = 125;    //Oversample by 125
	DMIC0->CHANNEL[1].DIVHFCLK = 0; // Divide DMIC input clock by 1 ( no divide)
	
	/*In the register below, we will enable the DMIC AND also enable interrupts *inside* the module.  
	this means that an interrupt will be generated when the FIFO gets to a certain level.   This means that the peripheral will
	generate an interrupt signal the ARM Cortex NVIC*/
	
	DMIC0->CHANNEL[1].FIFO_CTRL = DMIC_CHANNEL_FIFO_CTRL_ENABLE_MASK | 
																DMIC_CHANNEL_FIFO_CTRL_INTEN_MASK  | 
																DMIC_CHANNEL_FIFO_CTRL_TRIGLVL(2)  |
																DMIC_CHANNEL_FIFO_CTRL_RESETN_MASK;

  /*The line above enables an interrupt to be generated from within the DMIC peripheral. 
  	We also have to enable the interrupt in the ARM Cortex NVIC (the nested vectored interrupt controller)
		
		There are 256 slots in the table. Each peripheral has one or more "slots" in the table.   
		Chapter 3 (page 21) of the user manual document these slots.  The DMIC is slot 25.
		*/

	NVIC_EnableIRQ(DMIC0_IRQn);

   /* This sets the priority to the highest level. 
			the NVIC is documented in the ARM technical reference manual for the M4,  not in the user manual for the LPC54608
   */
  NVIC_SetPriority(DMIC0_IRQn,0);

  /*
	
	All done!!
	
	Once this function is called the DMIC peripheral will start getting samples and will generate interrupts!
	
	*/
}

