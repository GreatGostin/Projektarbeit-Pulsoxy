#include <msp430.h>

unsigned int ADC_Value;

//After these Book Examples
//https://books.google.de/books?id=R2jsDwAAQBAJ&pg=PA464&lpg=PA464&dq=msp430fr2355+ADCBUSY&source=bl&ots=aGy7aMoXAE&sig=ACfU3U2cfVyJO0GjSj2s7cRDmaeuu89TSw&hl=de&sa=X&ved=2ahUKEwjI66fr-Kz0AhXO2qQKHehCByQQ6AF6BAglEAM#v=onepage&q=msp430fr2355%20ADCBUSY&f=false

int main(void)
{

    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog timer

    P1DIR |= BIT0;
    P6DIR |= BIT6;

    P1SEL1 |= BIT2;
    P1SEL0 |= BIT2;

    PM5CTL0 &= ~LOCKLPM5; //Without this the pins won't be configured in Hardware.

    ADCCTL0 &= ~ADCSHT;
    ADCCTL0 |= ADCSHT_2;
    ADCCTL0 |= ADCON;
    ADCCTL1 |= ADCSSEL_2;
    ADCCTL1 |= ADCSHP;
    ADCCTL2 &= ~ADCRES;
    ADCCTL2 |= ADCRES_2;
    ADCMCTL0 |= ADCINCH_2;

    ADCIE |= ADCIE0;
    __enable_interrupt();

    while(1)
    {
    ADCCTL0 |= ADCENC | ADCSC;
    }
    return 0;
}

#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void)
{
    ADC_Value = ADCMEM0;
    if(ADC_Value > 3613)
    {
        P1OUT |= BIT0;
        P6OUT &= ~BIT6;
    }
    else
    {
        P1OUT &= ~BIT0;
        P6OUT |= BIT6;
    }

}



