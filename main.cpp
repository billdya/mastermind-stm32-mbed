#include "mbed.h"
#include "TextLCD.h"

// --------------------------------------------------------
// 1. HARDWARE CONFIGURATION
// --------------------------------------------------------
DigitalIn btnPrev(PC_13, PullUp); // Blue User Button
DigitalIn btnNext(A0, PullUp);    
DigitalIn btnOk(A1, PullUp);      

DigitalOut ledGreen(LED1);        // Onboard Green LED
AnalogIn randomSeedPin(A2);       

TextLCD *lcd; 

// --- 7-Segment Display Pins (Individual DigitalOuts for ISR Safety) ---
DigitalOut segA(D8);
DigitalOut segB(D9);
DigitalOut segC(D10);
DigitalOut segD(D11);
DigitalOut segE(D12);
DigitalOut segF(D14);
DigitalOut segG(D15);

DigitalOut digitTens(A3);         // Common Cathode for Digit 1
DigitalOut digitOnes(A4);         // Common Cathode for Digit 2

// --------------------------------------------------------
// 2. GAME VARIABLES & STATE MACHINE
// --------------------------------------------------------
typedef enum {
    STATE_INIT,
    STATE_INPUT_GUESS,
    STATE_EVALUATE,
    STATE_WIN,
    STATE_LOSE
} GameState;

GameState currentState = STATE_INIT;

char availableColors[6] = {'B', 'G', 'R', 'Y', 'P', 'O'};
char secretCode[4];
char playerGuess[4];
volatile int attemptsRemaining = 10; 
int currentGuessIndex = 0;  
int selectedColorIndex = 0; 

// --- 7-Segment Hex Map (Common Cathode) ---
const uint8_t numMap[11] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x00  // 10 (Blank/Off)
};

// --------------------------------------------------------
// 3. HELPER FUNCTIONS & INTERRUPTS
// --------------------------------------------------------

Ticker displayTicker;
int activeDigit = 0;

void writeSegments(uint8_t pattern) {
    segA = (pattern & 0x01) ? 1 : 0;
    segB = (pattern & 0x02) ? 1 : 0;
    segC = (pattern & 0x04) ? 1 : 0;
    segD = (pattern & 0x08) ? 1 : 0;
    segE = (pattern & 0x10) ? 1 : 0;
    segF = (pattern & 0x20) ? 1 : 0;
    segG = (pattern & 0x40) ? 1 : 0;
}

void multiplexDisplays() {
    digitTens = 1; 
    digitOnes = 1; 
    
    if (activeDigit == 0) {
        int tensVal = attemptsRemaining / 10;
        writeSegments((tensVal == 0) ? numMap[10] : numMap[tensVal]);
        digitTens = 0; 
        activeDigit = 1; 
    } else {
        int onesVal = attemptsRemaining % 10;
        writeSegments(numMap[onesVal]);
        digitOnes = 0; 
        activeDigit = 0; 
    }
}

// Updated LCD Display function to handle the 5th "Submit" OK press
void updateLCDGuessDisplay() {
    lcd->cls();
    lcd->locate(0, 0);
    lcd->printf("Guess: %c %c %c %c", playerGuess[0], playerGuess[1], playerGuess[2], playerGuess[3]);
    lcd->locate(0, 1);
    
    if (currentGuessIndex < 4) {
        // Still selecting colors
        lcd->printf("Next: %c", availableColors[selectedColorIndex]);
    } else {
        // 4 colors picked, waiting for final OK to submit
        lcd->printf("Press OK to Eval");
    }
}

// --------------------------------------------------------
// 4. THE MAIN SUPERLOOP
// --------------------------------------------------------
int main() {
    
    ThisThread::sleep_for(100ms);
    lcd = new TextLCD(D2, D3, D4, D5, D6, D7, TextLCD::LCD16x2); 
    
    displayTicker.attach(&multiplexDisplays, 5ms);
    
    while (1) {
        switch (currentState) {
            
            case STATE_INIT: {
                lcd->cls();
                lcd->printf("Starting Game...");
                
                srand(randomSeedPin.read_u16()); 
                for(int i = 0; i < 4; i++) {
                    int randomIndex = rand() % 6; 
                    secretCode[i] = availableColors[randomIndex];
                }

                // To know the secret code beforehand
                printf("The secret code is: %c %c %c %c\n", secretCode[0], secretCode[1], secretCode[2], secretCode[3]);
                
                attemptsRemaining = 10;
                currentGuessIndex = 0;
                selectedColorIndex = 0;
                ledGreen = 0;
                
                for(int i = 0; i < 4; i++) {
                    playerGuess[i] = '_';
                }
                
                ThisThread::sleep_for(1000ms);
                updateLCDGuessDisplay();
                currentState = STATE_INPUT_GUESS;
                break;
            }
            
            case STATE_INPUT_GUESS: {
                // --- NEXT BUTTON ---
                if (btnNext == 0) {
                    if (currentGuessIndex < 4) {
                        selectedColorIndex++;
                        if (selectedColorIndex > 5) selectedColorIndex = 0; 
                        updateLCDGuessDisplay();
                    }
                    
                    // Wait for the player to release the button (prevents auto-cycling)
                    while(btnNext == 0) { ThisThread::sleep_for(10ms); } 
                    ThisThread::sleep_for(50ms); // Software debounce
                }
                
                // --- PREV BUTTON ---
                if (btnPrev == 0) {
                    if (currentGuessIndex < 4) {
                        selectedColorIndex--;
                        if (selectedColorIndex < 0) selectedColorIndex = 5; 
                        updateLCDGuessDisplay();
                    }
                    
                    // Wait for the player to release the button
                    while(btnPrev == 0) { ThisThread::sleep_for(10ms); }
                    ThisThread::sleep_for(50ms); // Software debounce
                }
                
                // --- OK BUTTON ---
                if (btnOk == 0) {
                    if (currentGuessIndex < 4) {
                        // Lock in the chosen color
                        playerGuess[currentGuessIndex] = availableColors[selectedColorIndex];
                        currentGuessIndex++; // Advance cursor
                        updateLCDGuessDisplay();
                    } else {
                        // All 4 spots are filled, and OK was pressed AGAIN. Submit guess!
                        currentState = STATE_EVALUATE;
                    }
                    
                    // Wait for the player to release the button
                    while(btnOk == 0) { ThisThread::sleep_for(10ms); }
                    ThisThread::sleep_for(50ms); // Software debounce
                }
                break;
            }
                
            case STATE_EVALUATE: {
                int exactMatches = 0;
                int misplacedMatches = 0;
                
                bool secretChecked[4] = {false, false, false, false};
                bool guessChecked[4] = {false, false, false, false};

                for (int i = 0; i < 4; i++) {
                    if (playerGuess[i] == secretCode[i]) {
                        exactMatches++;
                        secretChecked[i] = true;
                        guessChecked[i] = true;
                    }
                }

                for (int i = 0; i < 4; i++) {
                    if (!guessChecked[i]) {
                        for (int j = 0; j < 4; j++) {
                            if (!secretChecked[j] && (playerGuess[i] == secretCode[j])) {
                                misplacedMatches++;
                                secretChecked[j] = true; 
                                break; 
                            }
                        }
                    }
                }
                
                lcd->cls();
                lcd->locate(0, 0);
                lcd->printf("%d Correct", exactMatches);
                lcd->locate(0, 1);
                lcd->printf("%d Misplaced", misplacedMatches);
                
                attemptsRemaining--;
                
                ThisThread::sleep_for(2500ms); 
                
                if (exactMatches == 4) {
                    currentState = STATE_WIN;
                } 
                else if (attemptsRemaining <= 0) {
                    currentState = STATE_LOSE;
                } 
                else {
                    currentGuessIndex = 0;
                    selectedColorIndex = 0;
                    for(int i = 0; i < 4; i++) playerGuess[i] = '_';
                    updateLCDGuessDisplay();
                    currentState = STATE_INPUT_GUESS;
                }
                break;
            }
                
            case STATE_WIN: {
                lcd->cls();
                lcd->locate(0, 0);
                lcd->printf("Congratulations!");
                
                while(1) {
                    ledGreen = !ledGreen; 
                    ThisThread::sleep_for(300ms);
                }
                break;
            }
                
            case STATE_LOSE: {
                lcd->cls();
                lcd->locate(0, 0);
                lcd->printf("Game Over");
                lcd->locate(0, 1);
                lcd->printf("Code: %c%c%c%c", secretCode[0], secretCode[1], secretCode[2], secretCode[3]);
                
                while(1) {
                    ThisThread::sleep_for(100ms);
                }
                break;
            }
        } 
    } 
}