#include "LPC17xx.h"
#include "pwm.h"
#include "LPC17xx.h"

/*! 
 *  @brief    Inicjalizuje moduł PWM do sterowania diodą RGB.
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects:
 *            Konfiguruje piny P2.0, P2.1, P2.2 jako wyjścia PWM1.1, PWM1.2, PWM1.3 // na schemacie są to piny PIO1_9, PIO1_10, PIO1_11
 *            Ustawia częstotliwość PWM z okresem 255 cykli
 *            Włącza licznik PWM i tryb PWM
 */
void PWM_Init(void) {
    // wlacz zasilanie dla PWM1
    LPC_SC->PCONP |= (1 << 6);  // PCONP[6] = PWM1 // table 46.
    
    LPC_PINCON->PINSEL4 &= ~(0b11 | (0b11 << 2) | (0b11 << 4));  // wyczysc bity 0–5
    LPC_PINCON->PINSEL4 |=  (0b01 | (0b01 << 2) | (0b01 << 4));  // ustaw PWM1.1–1.3


    // ustaw tryb single edge
    LPC_PWM1->PCR = (1 << 9) | (1 << 10) | (1 << 11);  //  PWM1.1, 1.2, 1.3 output

    // ustaw czestotliwosc (przy zalozeniu PCLK_PWM1 = 25 MHz)
    LPC_PWM1->PR = 0;         // prescaler = 1, gdy 
    LPC_PWM1->MR0 = 255;      // okres PWM = 255 cykli //"sufit ustawiony na 255"
    LPC_PWM1->MCR = (1 << 1); // reset on MR0 (PWM cycle)

    LPC_PWM1->MR1 = 255;  
	LPC_PWM1->MR2 = 255;  
	LPC_PWM1->MR3 = 255; 

    LPC_PWM1->LER = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 0);  // load MR0–MR3

    LPC_PWM1->TCR = (1 << 0) | (1 << 3);  // enable counter and PWM mode
}

/*! 
 *  @brief    Ustawia kolor diody RGB poprzez zmianę wypełnienia sygnałów PWM.
 *  @param red 
 *            Wartość składowej czerwonej (0-255)
 *  @param green 
 *            Wartość składowej zielonej (0-255)
 *  @param blue 
 *            Wartość składowej niebieskiej (0-255)
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects:
 *            Aktualizuje rejestry MR1, MR2, MR3 modułu PWM1
 *            Zmienia wypełnienie sygnałów PWM na pinach wyjściowych
 */
void PWM_SetColor(uint8_t red, uint8_t green, uint8_t blue) {
    if( red < 0 || red > 255 ) return;
    if( green < 0 || green > 255 ) return;
    if( blue < 0 || red > 255 ) return;

    LPC_PWM1->MR1 = red;
    LPC_PWM1->MR2 = green;
    LPC_PWM1->MR3 = blue;

    LPC_PWM1->LER = (1 << 1) | (1 << 2) | (1 << 3);  // zaktualizuj MR1–MR3
}
