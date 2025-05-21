#include "lpc17xx_pinsel.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"

#include "light.h"
#include "oled.h"
#include "temp.h"
#include "acc.h"

#include "pwm.h"

static uint32_t msTicks = 0;
static uint8_t buf[20];



#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);



static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

static const char *songs[] = {
		"E2, F4, E2, E4, E2, E4, E2, E4, E2, E4, E2, E4, E2, E4, D2, D2, D2", // kompozytor: Malwina Wodnicka (nie trzeba placic za X bo nie wie co to jest)
		"G2, G2, G2, F1, F1, F1, F1, D8, D4, F2, F2, F2, F2, F2, E2, E2"
};

/*! 
 *  @brief    Odtwarza pojedynczy dźwięk o określonej częstotliwości i czasie trwania.
 *  @param note 
 *             Wartość określająca częstotliwość dźwięku (okres w mikrosekundach)
 *  @param durationMs 
 *             Czas trwania dźwięku w milisekundach
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Generuje sygnał prostokątny na pinie P0.26
 */
static void playNote(uint32_t note, uint32_t durationMs) {
    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
    	Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

/*! 
 *  @brief    Konwertuje znak reprezentujący nutę na odpowiednią wartość częstotliwości.
 *  @param ch 
 *             Znak reprezentujący nutę (A-G, a-g)
 *  @returns  Wartość okresu w mikrosekundach dla danej nuty, 0 jeśli znak nie reprezentuje nuty
 *  @side effects: 
 *            Brak
 */
static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

/*! 
 *  @brief    Konwertuje znak reprezentujący czas trwania nuty na wartość w milisekundach.
 *  @param ch 
 *             Znak reprezentujący czas trwania (0-9)
 *  @returns  Czas trwania w milisekundach
 *  @side effects: 
 *            Brak
 */
static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

/*! 
 *  @brief    Konwertuje znak reprezentujący pauzę między nutami na wartość w milisekundach.
 *  @param ch 
 *             Znak reprezentujący pauzę (+, ,, ., _)
 *  @returns  Czas pauzy w milisekundach
 *  @side effects: 
 *            Brak
 */
static uint32_t getPause(uint8_t ch)
{
    switch (ch) {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}

/*! 
 *  @brief    Odtwarza melodię zapisaną jako ciąg znaków.
 *  @param song 
 *             Wskaźnik na ciąg znaków reprezentujący melodię
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Generuje sekwencję dźwięków na pinie P0.26
 */
static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);

    }
}

/*! 
 *  @brief    Konwertuje wartość liczbową na ciąg znaków.
 *  @param value 
 *             Wartość liczbowa do konwersji
 *  @param pBuf 
 *             Wskaźnik na bufor, w którym zostanie zapisany wynik
 *  @param len 
 *             Długość bufora
 *  @param base 
 *             Podstawa systemu liczbowego (2-36)
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Modyfikuje zawartość bufora wskazywanego przez pBuf
 */
static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base)
{
    static const char* pAscii = "0123456789abcdefghijklmnopqrstuvwxyz";
    int pos = 0;
    int tmpValue = value;

    // the buffer must not be null and at least have a length of 2 to handle one
    // digit and null-terminator
    if (pBuf == NULL || len < 2)
    {
        return;
    }

    // a valid base cannot be less than 2 or larger than 36
    // a base value of 2 means binary representation. A value of 1 would mean only zeros
    // a base larger than 36 can only be used if a larger alphabet were used.
    if (base < 2 || base > 36)
    {
        return;
    }

    // negative value
    if (value < 0)
    {
        tmpValue = -tmpValue;
        value    = -value;
        pBuf[pos++] = '-';
    }

    // calculate the required length of the buffer
    do {
        pos++;
        tmpValue /= base;
    } while(tmpValue > 0);


    if (pos > len)
    {
        // the len parameter is invalid.
        return;
    }

    pBuf[pos] = '\0';

    do {
        pBuf[--pos] = pAscii[value % base];
        value /= base;
    } while(value > 0);

    return;

}

/*! 
 *  @brief    Obsługa przerwania SysTick - inkrementuje licznik milisekund.
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Zwiększa wartość zmiennej msTicks
 */
void SysTick_Handler(void) {
    msTicks++;
}

/*! 
 *  @brief    Zwraca aktualną wartość licznika milisekund.
 *  @returns  Aktualna wartość licznika msTicks
 *  @side effects: 
 *            Brak
 */
static uint32_t getTicks(void)
{
    return msTicks;
}

/*! 
 *  @brief    Inicjalizuje interfejs SSP (Serial Peripheral Interface).
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Konfiguruje piny P0.7, P0.8, P0.9 i P2.2
 *            Inicjalizuje i włącza peryferia SSP1
 */
static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

/*! 
 *  @brief    Inicjalizuje interfejs I2C.
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Konfiguruje piny P0.10 i P0.11
 *            Inicjalizuje i włącza peryferia I2C2
 */
static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

/*! 
 *  @brief    Inicjalizuje przetwornik analogowo-cyfrowy (ADC).
 *  @returns  Brak wartości zwracanej (void)
 *  @side effects: 
 *            Konfiguruje pin P0.23 jako wejście ADC
 *            Inicjalizuje ADC z częstotliwością 200kHz
 *            Włącza kanał 0 ADC
 */
static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.0 on P0.23
	 */
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 23;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 0.2Mhz
	 *  ADC channel 0, no Interrupt
	 */
	ADC_Init(LPC_ADC, 200000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_0,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_0,ENABLE);

}

/*! 
 *  @brief    Główna funkcja programu.
 *  @returns  Nigdy nie zwraca wartości (nieskończona pętla)
 *  @side effects: 
 *            Inicjalizuje wszystkie peryferia
 *            Monitoruje wilgotność gleby, temperaturę i poziom oświetlenia
 *            Steruje diodą RGB i wyświetlaczem OLED
 *            Odtwarza melodie w zależności od stanu wilgotności gleby
 */
int main (void)
{
    int i = 0;
    int playedSad = 0;
    int playedHappy = 0;


    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    // konfiguracja ukladu LM4811
    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn



    int32_t t = 0;
    uint32_t lux = 0;
//    uint32_t trim = 0;
    uint32_t soilHumidity = 0;

//    PWM_Init();
    init_i2c();
    init_ssp();
    init_adc();

    oled_init();
    light_init();
//    acc_init();
    PWM_Init();

    temp_init (&getTicks);


	if (SysTick_Config(SystemCoreClock / 1000)) {
		    while (1);  // Capture error
	}


    light_enable();
    light_setRange(LIGHT_RANGE_4000);

    oled_clearScreen(OLED_COLOR_WHITE);

    // poziom wilgotnosci
	sprintf(buf, "Wilg.  : ");
	oled_putString(1, 9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    // jedne znak zajmuje 6 pix szerokowsci i 8 wysokosci
    // najpierw wyswietlamy napis "Swiatlo: " zajmujacy 9 znakow
    oled_putString(1,1,  (uint8_t*)"Temp   : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    // poziom naswietlenia
	sprintf(buf, "Swiatlo: ");
	oled_putString(1, 18, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    PWM_SetColor(255,255,255); // dioda swieci na bialo

    while(1) {

        // odczyt temperatury
        t = temp_read();

        // odczyt swiatla
        lux = light_read();

        // natychmiastowy start przetwarzania
        ADC_StartCmd(LPC_ADC,ADC_START_NOW);

        // czekaj az flaga statusu przetworzenia bedzie != 0
        while (!(ADC_ChannelGetStatus(LPC_ADC,ADC_CHANNEL_0,ADC_DATA_DONE)));
        // przypisanie wartosci z rejestru ADCx->ADDR0 wartosci znaczajacej odczyt z przetwornika AD
		soilHumidity = ADC_ChannelGetData(LPC_ADC,ADC_CHANNEL_0);

		intToString(soilHumidity, buf, 10, 10);
		oled_fillRect((1 + 9 * 6), 49, 80, 56, OLED_COLOR_WHITE);

		// przyjmujemy przedzialy odczytu jako:
		// Gleba sucha: 2801 - 4095
		// Gleba wilgotna: 1601 - 2800
		// Gleba mokra: 0 - 1600

		// sucha gleba
		if (soilHumidity > 2801)
		{
			// wyswietlenie informacji na ekranie OLED
			sprintf(buf, "sucho");

			PWM_SetColor(200, 255, 30); // dioda swieci na #c8ff1e

            // smutna piosenka
            if(playedSad == 0){
            	// smutna piosenka
				playSong((uint8_t*)songs[0]);
				playedHappy = 0;
				playedSad = 1;
            }

		}
		// wilgotna gleba
		else if (soilHumidity > 1601)
		{
			sprintf(buf, "w normie");

			PWM_SetColor(30, 255, 0); // dioda swieci na #1eff00

			// wesola piosenka
			if(playedHappy == 0){
				// smutna piosenka
				playSong((uint8_t*)songs[1]);
				playedHappy = 1;
				playedSad = 0;
			}
		}
		// mokra gleba
		else
		{
			sprintf(buf, "zbyt mokro");

			PWM_SetColor(30, 255, 255); // dioda swieci na #1effff
		}

		oled_fillRect(55, 9, 94, 18, OLED_COLOR_WHITE);
		oled_putString(55, 9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

		if (lux > 2000)
		{
			sprintf(buf, "jasno");
		}
		else if (lux > 350)
		{
			sprintf(buf, "OK");
		}
		else
		{
			sprintf(buf, "ciemno");
		}
		// ustaw na bialo miejsce
		oled_fillRect(55, 18, 94, 26, OLED_COLOR_WHITE);

		//wyswietlamy napis ze wzgledu na wartosc lux
		oled_putString(55, 18, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

        intToString(t, buf, 10, 10);
        oled_fillRect((1+9*6),1, 80, 8, OLED_COLOR_WHITE);
        oled_putString((1+9*6),1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

        Timer0_Wait(200);
    }

}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
