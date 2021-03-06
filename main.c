#include <msp430.h>
#include <msp430g2553.h>

/* Constant Parameters */
#define SHORTFLASH 30
#define LONGFLASH 90
#define PRESSTICKS 60
#define DEBOUNCE_TIME 40

// bitmasks for output pins
#define LEFT_LIGHT BIT0
#define RIGHT_LIGHT BIT1

// and for input pins
#define SWITCH_LEFT BIT5
#define SWITCH_RIGHT BIT6
#define SWITCH_MIDDLE ~(BIT5 + BIT6)

/* Global Variables */

unsigned int turn_state; 			// current button input state
unsigned int timeout_counter;   // down counter for button timeout
unsigned char led_flash;		// flashing button bitmask
unsigned int flash_counter; 	// down counter to next flash
unsigned int current_flash_interval; // flash interval
unsigned int left_pressed; // for testing

/* =============== procedures and interrupt handlers =============== */

/*
 * Initializers
 */

void init_buttons() {
	 P1DIR &= ~(SWITCH_LEFT + SWITCH_RIGHT); // Set button pin as an input pin
	 P1OUT |= (SWITCH_LEFT + SWITCH_RIGHT); // Set pull up resistor on for button
	 P1REN |= (SWITCH_LEFT + SWITCH_RIGHT); // Enable pull up resistor for button to keep pin high until pressed
	 P1IES |= (SWITCH_LEFT + SWITCH_RIGHT); // Enable Interrupt to trigger on the falling edge (high (unpressed) to low (pressed) transition)
	 //P1IES &= ~(SWITCH_LEFT + SWITCH_RIGHT); // look for rising edge
	 P1IFG &= ~(SWITCH_LEFT + SWITCH_RIGHT); // Clear the interrupt flag for the button
	 P1IE |= (SWITCH_LEFT + SWITCH_RIGHT); // Enable interrupts on port 1 for the button
}

void init_lights() {
	P1DIR |= (LEFT_LIGHT+ RIGHT_LIGHT);  // set output pins
	P1OUT |= (LEFT_LIGHT + RIGHT_LIGHT); // both on
}

void init_gpio(){
	init_lights();
	init_buttons();
}

void init_wdt(){ // setup WDT
	  WDTCTL =(WDTPW + // (bits 15-8) password
	                   // bit 7=0 => watchdog timer on
	                   // bit 6=0 => NMI on rising edge (not used here)
	                   // bit 5=0 => RST/NMI pin does a reset (not used here)
	           WDTTMSEL + // (bit 4) select interval timer mode
	           WDTCNTCL +  // (bit 3) clear watchdog timer counter
	  		          0 // bit 2=0 => SMCLK is the source
	  		          +1 // bits 1-0 = 01 => source/8K
	  		   );
	  IE1 |= WDTIE;		// enable the WDT interrupt (in the system interrupt register IE1)
}

void init_timer() { // use timer to debounce
	TACTL = TASSEL_1 | ID_2 | MC_1; // ACLK divided by 4, count to CCR0
	TACCR0 = DEBOUNCE_TIME;
	TACCTL0 &= ~CCIE; // disable timer A interrupt for now
}

//* Basic operations of LED's (as procedures for clarity)

void both_on(){
	P1OUT |= (LEFT_LIGHT + RIGHT_LIGHT); // both on
	led_flash= 0;
	turn_state = 0;
}

void left_signal_on() {
	P1OUT |= (LEFT_LIGHT + RIGHT_LIGHT); // both on
	led_flash = LEFT_LIGHT;
	current_flash_interval=SHORTFLASH;
	flash_counter=current_flash_interval;  // start counting down from here
	turn_state = 1;
}

void right_signal_on() {
	P1OUT |= (LEFT_LIGHT + RIGHT_LIGHT); // both on
	led_flash = RIGHT_LIGHT;
	current_flash_interval=SHORTFLASH;
	flash_counter=current_flash_interval;
	turn_state = 2;
}

void main(void) {
	BCSCTL1 = CALBC1_1MHZ;	// set 1Mhz calibration for clock
  	DCOCTL  = CALDCO_1MHZ;

  	init_wdt();
  	init_gpio();
  	init_timer();
  	both_on();
  	BCSCTL3 |= LFXT1S_2;
	 _bis_SR_register(GIE+LPM0_bits);  // enable interrupts and also turn the CPU off!
}

void run_state_machine() {
	if (P1IN & SWITCH_LEFT) { // if the left button is currently down
		left_signal_on();
		P1IES |= (SWITCH_LEFT + SWITCH_RIGHT); // now look for falling edge, which will occur when the switch is in the middle
	} else if (P1IN & SWITCH_RIGHT) {
		right_signal_on();
		P1IES |= (SWITCH_LEFT + SWITCH_RIGHT); // now look for falling edge, which will occur when the switch is in the middle
	} else if (SWITCH_MIDDLE) {
		both_on();
		P1IES &= ~(SWITCH_LEFT + SWITCH_RIGHT); // now look for rising edge, which will occur when the switch is left or right
	}
}

// ==== GPIO Input Interrupt Handler =======

interrupt void button_handler(){
	// now debounce the button
	TACCTL0 = CCIE; // enable timer interrupts
	TACCR0 = 0; // stop the timer
	TACCR0 = DEBOUNCE_TIME; // start the timer
	P1IE &= ~(SWITCH_LEFT + SWITCH_RIGHT); // disable button interrupts while debouncing
	P1IFG &= ~(SWITCH_LEFT + SWITCH_RIGHT); // clear button flags
}
// DECLARE function button_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(button_handler, ".int02")

interrupt void timer_debounce() {
	TACCTL0 &= ~TAIFG; // clear timer interrupt flag
	run_state_machine();
	TACCTL0 &= ~CCIE; // disable timer interrupts
	P1IE |= (SWITCH_LEFT + SWITCH_RIGHT); // debouncing done, re-enable button interrupts
}
ISR_VECTOR(timer_debounce, ".int09")


// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of about 8K/1MHz 8.192ms.
interrupt void WDT_interval_handler(){
	/* flash action */
	if (--flash_counter == 0){	// is it time to flash?
		P1OUT ^= led_flash; 	// toggle one or more led's
		flash_counter=current_flash_interval;
	}
}
// DECLARE function WDT_interval_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(WDT_interval_handler, ".int10")
