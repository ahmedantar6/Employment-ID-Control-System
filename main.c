#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

// ───── Pin & Port Setup ──────────────────────────────────────────────────────
#define LCD_D_P    PORTA
#define LCD_D_DDR  DDRA
#define LCD_C_P    PORTB
#define LCD_C_DDR  DDRB

#define EN   PB0
#define RW   PB1
#define RS   PB2

#define KP_PORT PORTC
#define KP_PIN  PINC
#define KP_DDR  DDRC

// ───── Keypad Layout ─────────────────────────────────────────────────────────
const char keymap[4][4] = {
    { '7','8','9','/' },
    { '4','5','6','*' },
    { '1','2','3','-' },
    { 'A','0','=','+' }
};

// ───── Low‐level LCD Helpers ──────────────────────────────────────────────────

// Toggle the EN line to latch data/command
void enable(void) {
    _delay_us(1);
    LCD_C_P |=  (1 << EN);
    _delay_us(1);
    LCD_C_P &= ~(1 << EN);
    _delay_ms(1);
}

// Send a full byte as command (t=0) or data (t=1)
void SEND_D_C(unsigned char a, unsigned char t) {
    if (t)      LCD_C_P |=  (1 << RS);
    else        LCD_C_P &= ~(1 << RS);
    LCD_C_P &= ~(1 << RW);

    // high nibble
    LCD_D_P = (LCD_D_P & 0xF0) | ((a >> 4) & 0x0F);
    enable();
    // low nibble
    LCD_D_P = (LCD_D_P & 0xF0) | (a & 0x0F);
    enable();
}

// Move cursor to (x,y)
void LCD_X_Y(unsigned char x, unsigned char y) {
    if (y == 0) SEND_D_C(0x80 + x, 0);
    else        SEND_D_C(0xC0 + x, 0);
}

// Send a zero-terminated string
void LCD_D_C_ST(const char *s) {
    while (*s) {
        SEND_D_C(*s++, 1);
    }
}

// Initialize LCD in 4-bit, 2-line mode
void LCD_INIT1(void) {
    const unsigned char com[] = { 0x33, 0x32, 0x28, 0x0C, 0x06, 0x01 };
    _delay_ms(20);
    for (unsigned char i = 0; i < sizeof(com); i++) {
        SEND_D_C(com[i], 0);
    }
    _delay_ms(2);
}

// ───── Keypad Scanning ────────────────────────────────────────────────────────
// Rows PC4–PC7, Cols PC0–PC3
char getKey(void) {
    KP_DDR  = 0xF0;   // PC4–PC7 outputs
    KP_PORT = 0x0F;   // PC0–PC3 pull-ups

    while (1) {
        for (uint8_t row = 0; row < 4; row++) {
            KP_PORT = 0x0F | (~(1 << (row + 4)) & 0xF0);
            _delay_us(5);
            for (uint8_t col = 0; col < 4; col++) {
                if (!(KP_PIN & (1 << col))) {
                    _delay_ms(20);
                    while (!(KP_PIN & (1 << col)));
                    return keymap[row][col];
                }
            }
        }
    }
}

// Read exactly len digits into buf, mask with '*' if mask=1
void readCode(char *buf, uint8_t len, uint8_t mask) {
    for (uint8_t i = 0; i < len; i++) {
        char k;
        do { k = getKey(); } while (k < '0' || k > '9');
        buf[i] = k;
        SEND_D_C(mask ? '*' : k, 1);
    }
    buf[len] = '\0';
}

// Convert 4-char buffer "abcd" to integer abcd
int toInt(const char *buf) {
    return (buf[0]-'0')*1000
         + (buf[1]-'0')*100
         + (buf[2]-'0')*10
         + (buf[3]-'0');
}

// ───── Main Program ──────────────────────────────────────────────────────────
int main(void) {
    // Configure LCD and keypad pins
    LCD_D_DDR  |= 0x0F;                              // PA0–PA3 = D4–D7
    LCD_C_DDR  |= (1<<EN)|(1<<RW)|(1<<RS);          // PB0–PB2 = control

    LCD_INIT1();

    char idBuf[5], pwdBuf[5];
    int  idVal, pwdVal;

    while (1) {
        // Prompt for ID
        SEND_D_C(0x01, 0);
        LCD_X_Y(0, 0);
        LCD_D_C_ST("Enter ID:");
        LCD_X_Y(0, 1);
        readCode(idBuf, 4, 0);

        // Prompt for Password (masked)
        SEND_D_C(0x01, 0); //Clear LCD
        LCD_X_Y(0, 0);
        LCD_D_C_ST("Enter Pass:");
        LCD_X_Y(0, 1);
        readCode(pwdBuf, 4, 1);

        // Convert and check
        idVal  = toInt(idBuf);
        pwdVal = toInt(pwdBuf);

        SEND_D_C(0x01, 0);
        LCD_X_Y(0, 0);
        if ((idVal >= 2330 && idVal <= 2340) && (idVal == pwdVal)) {
            LCD_D_C_ST("Access Granted");
        } else {
            LCD_D_C_ST("Access Denied");
        }
        _delay_ms(2000);
    }

    return 0;
}
