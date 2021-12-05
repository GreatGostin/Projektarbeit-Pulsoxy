
#include <msp430.h>
#include "PCD8544.h"

//LDC PIN DEFINES
#define LCD5110_SCLK_PIN            BIT5
#define LCD5110_DN_PIN              BIT7
#define LCD5110_SCE_PIN             BIT0
#define LCD5110_DC_PIN              BIT1
#define LCD5110_SELECT              P6OUT &= ~LCD5110_SCE_PIN
#define LCD5110_DESELECT            P6OUT |= LCD5110_SCE_PIN
#define LCD5110_SET_COMMAND         P1OUT &= ~LCD5110_DC_PIN
#define LCD5110_SET_DATA            P1OUT |= LCD5110_DC_PIN
#define LCD5110_COMMAND             0
#define LCD5110_DATA                1

//Defines for normal sequence
#define UPDATELCD 5;
#define FULLBATT 4
#define HALFBATT 3
#define MINBATT  2
#define CRITBATT 1

static int SP02[151] = [101 101 101 100 100 100 100 100 99  99  99  99  98  98  98  98  98  97  97  97  97  96  96  96  95  95  95  95  94  94  94  94  93  93  93  92  92  92  91  91  91  90  90  90  89  89  89  88  88  88  87  87  87  86  86  86  85  85  84  84  84  83  83  83  82  82  81  81  81  80  80  79  79  79  78  78  77  77  76  76  75  75  75  74  74  73  73  72  72  71  71  70  70  69  69  68  68  67  67  66  66  65  65  64  64  63  63  62  62  61  61  60  60  59  59  58  57  57  56  56  55  55  54  53  53  52  52  51  51  50  49  49  48  48  47  46  46  45  44  44  43  43  42  41  41  40  39  39  38  37  37];


typedef enum{ DCRED = 0, ACRED, DCINFRA, ACINFRA, DCOFF, ACOFF, TEMP, LIPO}ADCTYPE;
unsigned int ADCValue[8];//

unsigned int StartStepA   = 0;
unsigned int StartStepB   = 0;

unsigned int HTresholdRed = 0xFFFF; //Initial Value 0xFFFF
unsigned int LTresholdRed = 0;
unsigned int HTresholdInfra = 0xFFFF;
unsigned int LTresholdInfra = 0;

//Variables for Processing ADC Values;
unsigned long rms_AC_Red   = 0;
unsigned long mean_DC_Red   = 0;
unsigned long rms_AC_Infra = 0;
unsigned long mean_DC_Infra = 0;
unsigned int MeanTemp = 0;
unsigned int VoltageLevel = 0;

unsigned int maxIntensity = 10;

unsigned int MaxACRed   = 0;
unsigned int MinACRed   = 0;

unsigned int LCDBattFlag = 0;

unsigned int SP02Level = 0;
unsigned int MeanSP02 = 0;


//Set All Pins to Configure the LDC
void InitLCDPins(void);

//First Takes the AC Part, afterwards the DC Value.
//i is used for the three Cases, Red, InfraRed, Off
void GetDiodeADC(unsigned int chann);

//Take the ADC Value of the NTC and converts it to the equivalent Temperature.
//Used for slight Correction of LEDS;
void GetTemp(void);

//Take the ADV Value of the LiPo Battery.
//Checks for it to be between 3.7 to 3.0 ??
//Sets Flags to Change the Battery Sign on the LCD
void GetBatteryVoltage(void);


//Checks the Threshold for either red or infrared.
//Tries to keep it in a range by changing the PWM that controls the Intensity
void CheckLEDIntensity(void);

int CalculateSP02(void);
int CaluclateRPM(int samples);



void main(void) {

    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog timer

    /*  Default settings after reset:
    *
    *   Source of the Main system clock (MCLK) and sub-main system clock (SMCLK) is
    *   Internal Digitally Controlled Oscillator Clock (DCO).
    *   MCLK is about 1 MHz.
    *   VLO is about 32 kHz.
    *
    *
    */
    //Variables used for the normal sequence
    unsigned int i = 0;
    unsigned int samplesHBeat = 0;
    unsigned int takeBattery  = 0;
    unsigned int updLCD    = 0;
    unsigned int bpm =0;




    //Configuration for ADC Pin start on DC for RED (A3, P1.3)
    //Later each ADC Pin (A1, A3, A10, A11) is configured and triggered manually

    P1SEL0 |= BIT1 | BIT3;
    P1SEL1 |= BIT1 | BIT3;
    P5SEL0 |= BIT2 | BIT3;
    P5SEL1 |= BIT2 | BIT3;

    ADCCTL0  &= ~ADCENC;
    ADCCTL0  |= ADCON;    //ADC-Core on
    ADCCTL1  |= ADCSHP;   //This just makes it so when you start the conversion it stops eventually.
    ADCCTL2  &= ~ADCRES;  //Reset Bit Conversion
    ADCCTL2  |= ADCRES_2; //12 Bit Conversion
    ADCMCTL0 |= ADCINCH_3; // Channel A3
    //ADCIE |= ADCIE0;

    InitLCDPins();

    __delay_cycles(300000); // we have to wait a bit for the LCD to boot up.
    initLCD();

    clearLCD();

    __delay_cycles(500000); //So that we come from a fres boot the Screen will be empty and doesn't accidentally contain old Information


    //Use PIn 5.0 and 5.1 for switching the LEDs

    P5DIR |= BIT0 | BIT1;
    P5SEL0 |= BIT0 | BIT1;

    TB2CCR0  = 1000-1;          //Every 1 ms  the LED is already on for 110 us and will continue for another 110 us
    TB2CCTL1 |= OUTMOD_2;    //TB2CCR1 toggle/set
    TB2CCR1  = 110-1;         //0..110us , 1.89ms ... 2ms
    TB2CCTL2 |= OUTMOD_6;    //TBCCR2 reset/set
    TB2CCR2  = 890-1;         //0.890 ms - 1.11 ms
    TB2CTL   |= TBSSEL_2 | MC_3 |TBCLR; // SMCLK, Up-Down-Mode, Interrupt Enable on Max Value and Zero
    TB2CTL   |= TBIE;
    TB2CCTL0 |= CCIE; //Enable Interrupt when CCR0 is reached.

    //Two PWM for Intensity
    //P6.3 and P6.4

    P6DIR  |= BIT3 | BIT4;
    P6SEL0 |= BIT3 | BIT4;


    TB3CCR0 = maxIntensity;
    TB3CCTL4 |= OUTMOD_7;
    TB3CCTL5 |= OUTMOD_7;
    TB3CCR4   = 4;
    TB3CCR5   = 4;
    TB3CTL   |= TBSSEL__SMCLK  | MC__UP | TBCLR; //Up-Mode 1 MHz


    PM5CTL0 &= ~LOCKLPM5; //Without this the pins won't be configured in Hardware.
    TB2CTL &= ~TBIFG;
    TB2CCTL0 &= ~CCIFG;

    setAddr(8,0);
    writeStringToLCD("SPO2");

    setAddr(60,0);
    writeStringToLCD("bpm");

    setAddr(10,3);
    writeStringToLCD("94%");
    setAddr(62,3);
    writeStringToLCD("62");

    clearBank(5);
    setAddr(38,5);
    writeBattery(BATTERY_FULL);

    __bis_SR_register(GIE);

    while(1)
    {
        if(StartStepA)
        {
            GetDiodeADC(DCRED);

            //Take RMS of AC and Mean of
            rms_AC_Red = (ADCValue[ACRED]*ADCValue[ACRED]) >> 9; //Div by 9 makes it easier
            mean_DC_Red = ADCValue[DCRED] >> 9;

            //Wait a bit more (like 20us) to start the ADC for both Diodes off
            __delay_cycles(20);

            GetDiodeADC(DCOFF);



        }
        else if(StartStepB)
        {
            GetDiodeADC(DCINFRA);
            rms_AC_Infra += (ADCValue[ACINFRA]*ADCValue[ACINFRA]) >> 9; //Div by 9 makes it easier
            mean_DC_Infra += ADCValue[DCINFRA] >> 9;

            i++;

            if(i = 1000)
            {

                //Send the Data via SPI.
            }

        }

    }


} // eof main


#pragma vector = TIMER2_B0_VECTOR
__interrupt void TIMER2_B0_ISR(void)
{
    TB2CCTL0 &= ~CCIFG;
    StartStepA = 1;
}


#pragma vector = TIMER2_B1_VECTOR
__interrupt void TIMER2_B1_ISR(void)
{
    TB2CTL &= ~TBIFG;
    StartStepB = 1;
}



//Takes around 80 (80 us) for the entire function
//Using Active Polling because the combination of Low Power Mode and Interrupt  isn't working properly
void GetDiodeADC(unsigned int chann)
{
    ADCCTL0  &= ~ADCENC;
    ADCMCTL0 |= ADCINCH_3;
    ADCCTL0  |= ADCENC | ADCSC;

    while(ADCCTL1 & ADCBUSY);
    ADCValue[chann] = ADCMEM0;

    ADCCTL0  &= ~ADCENC;
    ADCMCTL0 |= ADCINCH_10;
    ADCCTL0  |= ADCENC | ADCSC;

    chann= chann+1;

    while(ADCCTL1 & ADCBUSY);
    ADCValue[chann] = ADCMEM0;

}

void GetTemp(void)
{
    ADCCTL0  &= ~ADCENC;
    ADCMCTL0 |= ADCINCH_11;
    ADCCTL0  |= ADCENC | ADCSC;

    while(ADCCTL1 & ADCBUSY);
    ADCValue[TEMP] = ADCMEM0;

    MeanTemp =+ (ADCValue[TEMP] >> 9);
}
void GetBatteryVoltage(void)
{
    ADCCTL0  &= ~ADCENC;
    ADCMCTL0 |= ADCINCH_1;
    ADCCTL0  |= ADCENC | ADCSC;

    while(ADCCTL1 & ADCBUSY);

    VoltageLevel = ADCMEM0;

    //Based on the battery level, Change a Flag for The LCD

    if(VoltageLevel >= 0xF000)
    {
        LCDBattFlag = 4; // Full
    }
    else if(VoltageLevel >= 0x8000 && VoltageLevel < 0xF000)
    {
        LCDBattFlag = 3; // One line missing (~ 75%)
    }
    else if(VoltageLevel >= 0x4000 && VoltageLevel < 0x8000)
    {
        LCDBattFlag = 2; // Two lines missing (~ 50%)
    }
    else if(Voltagelevel >= 0x2000 && VoltageLevel < 0x4000)
    {
        LCDBattFlag = 1; // Three lines missing (~ 25$)
    }
    else
    {
        LCDBattFlag ^= BIT0; // Blinking
    }

}


//We will maybe every 100 ms Check for the Intensity
//Maybe even more. Its need to be evaluated during testing.
void CheckLEDIntensity(void)
{
    //Disable both PWM
    TB3CTL &= ~MC_0;

    if(ADCValue[ACRED] < LTresholdRed)
    {
        TB3CCR4++;
        if(TB3CCR4 >= maxIntensity)
        {
            TB3CCR4 = maxIntensity;
        }
    }
    else if(ADCValue[ACRED] < HTresholdRed)
    {
        TB3CCR4--;
        if(TB3CCR4 <= 1)
        {
            TB3CCR4 = 1;
        }
    }

    if(ADCValue[ACRed] < LTresholdInfra)
    {
        TB3CCR5++;
        if(TB3CCR5 >= maxIntensity)
        {
            TB3CCR5 = maxIntensity;
        }
    }
    else if(ADCValue[ACRED] < HTresholdInfra)
    {
        TB3CCR5--;
        if(TB3CCR5 <= 1)
        {
            TB3CCR5 = 1;
        }
    }


    //Based on the current Level the Threshold gets Adjusted
    //These will be set during
    LTresholdRed = 0;
    HTresholdRed = 0xFFFF;
    LTresholdInfra = 0;
    HTresholdInfra = 0xFFFF;

    //Dont Forget to start the timer again.
    TB3CTL |= MC__UP;
}

int CalculateSP02(void)
{
    unsigned int R;

    R = ((rms_AC_Red / mean_DC_Red)/(rms_AC_Infra/mean_DC_Infra) -0.5) *100; //We have to subtract 0.5 so that we can Access the Array

    SP02Level = SP02[R];

    MeanSP02 += SP02Level >> 2; //We take an Average of 4 Values to Calculate the Sp02 Level

    return MeanSP02;
}

int CaluclateRPM(int samples)
{

    BPM = (500* 60)/(Samples/4);

}


void InitLCDPins(void)
{

    P1SEL0 |= LCD5110_DN_PIN | LCD5110_SCLK_PIN;
    P1OUT &= ~BIT0;

    P6OUT |= LCD5110_SCE_PIN;
    P6DIR |= LCD5110_SCE_PIN;

    P1OUT |= LCD5110_DC_PIN;  // Disable LCD, set Data mode
    P1DIR |= LCD5110_DC_PIN;  // Set pins to output direction

    UCA0CTLW0 |= UCSWRST;                     // **Put state machine in reset**
    UCA0CTLW0 |= UCMST|UCSYNC|UCCKPH|UCMSB;   // 3-pin, 8-bit SPI master
    UCA0CTLW0 |= UCSSEL_3; // SMCLK
    UCA0BRW   |= 0; //1:1


    UCA0CTLW0 &= ~UCSWRST;

}

void setAddr(unsigned char xAddr, unsigned char yAddr) {
    writeToLCD(LCD5110_COMMAND, PCD8544_SETXADDR | xAddr);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETYADDR | yAddr);
}

void writeToLCD(unsigned char dataCommand, unsigned char data) {
    LCD5110_SELECT;

    if(dataCommand) {
        LCD5110_SET_DATA;
    } else {
        LCD5110_SET_COMMAND;
    }

    UCA0TXBUF  = data;
    while(!(UCTXIFG & UCA0IFG ))

    LCD5110_DESELECT;
}

void initLCD() {
    writeToLCD(LCD5110_COMMAND, PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETVOP | 0x3F);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETTEMP | 0x02);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETBIAS | 0x03);
    writeToLCD(LCD5110_COMMAND, PCD8544_FUNCTIONSET);
    writeToLCD(LCD5110_COMMAND, PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL);
}

void writeCharToLCD(char c) {
    unsigned char i;
    for(i = 0; i < 5; i++) {
        writeToLCD(LCD5110_DATA, font[c - 0x20][i]);
    }
    writeToLCD(LCD5110_DATA, 0);
}

void writeStringToLCD(const char *string) {
    while(*string) {
        writeCharToLCD(*string++);
    }
}

void writeBattery(const int i)
{
    unsigned char j;
    for(j = 0 ; j < 7; j++)
    {
        writeToLCD(LCD5110_DATA, Battery[i][j]);
    }
    writeToLCD(LCD5110_DATA, 0);

}

void clearLCD() {
    setAddr(0, 0);
    int i = 0;
    while(i < PCD8544_MAXBYTES) {
        writeToLCD(LCD5110_DATA, 0);
        i++;
    }
    setAddr(0, 0);
}

void clearBank(unsigned char bank) {
    setAddr(0, bank);
    int i = 0;
    while(i < PCD8544_HPIXELS) {
        writeToLCD(LCD5110_DATA, 0);
        i++;
    }
    setAddr(0, bank);
}


