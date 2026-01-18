/*
 * Project: Smart Workstation System
 * Author: Rami ELAMRI
 * Platform: PIC16F887 / MikroC
 * Description: Automated workstation with safety priority and access control.
 */

#include "soft_i2c_lcd.h"

/* === PINS CONFIGURATION === */
sbit PROXIMITY_SENSOR at RB0_bit;   // Formerly CAPTEUR_PROXIMITE
sbit EMERGENCY_STOP   at RB2_bit;   // Formerly ARRET_URGENCE

sbit LED_GREEN        at RC0_bit;   // Formerly LED_VERTE
sbit LED_RED          at RC1_bit;   // Formerly LED_ROUGE
sbit BUZZER           at RC2_bit;

// Soft I2C Connections
sbit Soft_I2C_Scl           at RC3_bit;
sbit Soft_I2C_Sda           at RC4_bit;
sbit Soft_I2C_Scl_Direction at TRISC3_bit;
sbit Soft_I2C_Sda_Direction at TRISC4_bit;

/* === VARIABLES === */
unsigned int pieces_total = 0;
unsigned int pieces_ko = 0;
unsigned int pieces_ok = 0;
unsigned int mesure_valeur = 0;
unsigned int quality_rate = 0;     // Formerly taux_qualite

unsigned long work_seconds = 0;    // Formerly secondes_travail
unsigned short ticks = 0;

char operator_name[6] = "----";    // Formerly nom_operateur
char badge_id[5];                  // Formerly id_badge
char txt[12];

const unsigned int THRESHOLD_MIN = 400; // Formerly SEUIL_MIN
const unsigned int THRESHOLD_MAX = 600; // Formerly SEUIL_MAX

// Delay functions required for the Soft I2C Library
void Delay_1us()  { Delay_us(1); }
void Delay_5us()  { Delay_us(5); }
void Delay_8us()  { Delay_us(8); }
void Delay_10us() { Delay_us(10); }
void Delay_22us() { Delay_us(22); }
void Delay_50us() { Delay_us(50); }
void Delay_80us() { Delay_us(80); }

/* === FUNCTIONS === */

// Function to compare two strings
short Is_Equal(char *a, char *b) {
   while(*a && *b) {
      if(*a != *b) return 0;
      a++; b++;
   }
   return (*a == *b);
}

// Function to make a short beep sound
void Short_Beep() {
   BUZZER = 1;
   Delay_ms(100);
   BUZZER = 0;
}

// Function to update the LCD display
void Display_Info() {
   I2C_LCD_Out(1,1, operator_name);
   I2C_LCD_Out(1,7," T:"); // T = Time
   
   // Formatting time as MM:SS
   I2C_LCD_Chr(1,11,'0'+((work_seconds/60)/10));
   I2C_LCD_Chr(1,12,'0'+((work_seconds/60)%10));
   I2C_LCD_Chr(1,13,':');
   I2C_LCD_Chr(1,14,'0'+((work_seconds%60)/10));
   I2C_LCD_Chr(1,15,'0'+((work_seconds%60)%10));
}

// Function to handle Operator Login
void Identification() {
   char c;
   char i = 0;

   I2C_LCD_Cmd(_LCD_CLEAR);
   I2C_LCD_Out(1,1,"LOGIN...");
   UART1_Write_Text("\r\nLogin with ID: ");

   while(1) {
      if(UART1_Data_Ready()) {
         c = UART1_Read();

         if(c == 13) { // Enter key pressed
            badge_id[i] = 0;

            if(Is_Equal(badge_id,"3103")) { strcpy(operator_name,"RAMI"); break; }
            

            UART1_Write_Text("\r\nERROR\r\nCODE: ");
            i = 0;
         }
         else if(i < 4 && c >= '0' && c <= '9') {
            badge_id[i++] = c;
            UART1_Write('*'); // Mask the password characters
         }
      }
   }

   UART1_Write_Text("\r\nLOGIN OK: ");
   UART1_Write_Text(operator_name);
   UART1_Write_Text("\r\n");

   Short_Beep();
   I2C_LCD_Cmd(_LCD_CLEAR);
   Display_Info();
}

/* === MAIN PROGRAM === */
void main() {

   ANSEL = 0x01; ANSELH = 0; // Configure Analog Pins

   // Configure Input/Output Directions (1=Input, 0=Output)
   TRISA0_bit = 1; // Potentiometer
   TRISB0_bit = 1; // Proximity Sensor
   TRISB2_bit = 1; // Emergency Stop

   TRISC0_bit = 0; // LED Green
   TRISC1_bit = 0; // LED Red
   TRISC2_bit = 0; // Buzzer
   PORTC = 0;

   // Initialize Libraries
   UART1_Init(9600);
   ADC_Init();
   Soft_I2C_Init();
   I2C_LCD_Init();
   I2C_LCD_Cmd(_LCD_CURSOR_OFF);

   // Run Login Sequence
   Identification();

   while(1) {

      Delay_ms(10);
      ticks++;

      // Timer logic (approx every 1 second)
      if(ticks >= 100) {
         work_seconds++;
         ticks = 0;
         Display_Info();
      }

      /* === EMERGENCY STOP ROUTINE === */
      if(EMERGENCY_STOP == 1) {
         I2C_LCD_Cmd(_LCD_CLEAR);
         I2C_LCD_Out(1,1,"EMERGENCY STOP");
         BUZZER = 1;

         // Lock the system here while Emergency button is pressed
         while(EMERGENCY_STOP == 1) {
            LED_RED = ~LED_RED; // Blink Red LED
            Delay_ms(200);
         }

         BUZZER = 0;
         LED_RED = 0;
         I2C_LCD_Cmd(_LCD_CLEAR);
         Display_Info();
      }

      /* === PART DETECTION ROUTINE === */
      if(PROXIMITY_SENSOR == 1) {

         Delay_ms(100); // Stabilization delay
         mesure_valeur = ADC_Read(0); // Read Potentiometer
         pieces_total++;

         // Check if part is within tolerance
         if(mesure_valeur >= THRESHOLD_MIN && mesure_valeur <= THRESHOLD_MAX) {
            LED_GREEN = 1;
         } else {
            pieces_ko++;
            LED_RED = 1;
            Short_Beep();
         }

         // Calculate stats
         pieces_ok = pieces_total - pieces_ko;
         quality_rate = (pieces_ok * 100UL) / pieces_total;

         // Send data to Serial Terminal
         UART1_Write_Text("OP:");
         UART1_Write_Text(operator_name);
         UART1_Write_Text(" OK:");
         WordToStr(pieces_ok,txt);
         UART1_Write_Text(txt);
         UART1_Write_Text(" KO:");
         WordToStr(pieces_ko,txt);
         UART1_Write_Text(txt);
         UART1_Write_Text("\r\n");

         // Update LCD with stats
         I2C_LCD_Out(2,1,"OK:");
         WordToStr(pieces_ok,txt);
         I2C_LCD_Chr(2,4,txt[3]);
         I2C_LCD_Chr(2,5,txt[4]);

         I2C_LCD_Out(2,8,"QR:"); // QR = Quality Rate (formerly TC)
         ByteToStr(quality_rate,txt);
         I2C_LCD_Out(2,11,txt);
         I2C_LCD_Out(2,14,"%");

         // Reset LEDs
         LED_GREEN = 0;
         LED_RED = 0;

         Delay_ms(300);   // Anti-bounce / Prevent double counting
      }
   }
}
//3103 