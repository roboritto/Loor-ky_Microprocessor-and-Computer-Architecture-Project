#include "mbed.h"
#include <string>
#include "SSD1306.h"

// --- Hardware Definitions ---
DigitalOut greenLed(D8);
DigitalOut redLed(D12);  
DigitalOut buzzer(D9);

// Initialize the I2C bus and the OLED
I2C i2c(D14, D15);
SSD1306 oled(&i2c, 0x78);

// Keypad Pins
DigitalOut row1(D2), row2(D3), row3(D4), row4(D5);
DigitalIn col1(D6, PullUp), col2(D7, PullUp), col3(D10, PullUp), col4(D11, PullUp);

// --- System Variables ---
string currentPassword = "AD180804"; 
string inputBuffer = "";
int wrongAttempts = 0;

enum SystemState {
    LOCKED,
    UNLOCKED,
    LOCKED_OUT,
    CHANGE_PW_NEW
};
SystemState currentState = LOCKED;

// --- Helper Functions ---

// Hardware fix for 0.91" 128x32 OLED screens
void fixOLED_Resolution() {
    char cmds[5];
    cmds[0] = 0x00; 
    cmds[1] = 0xA8; 
    cmds[2] = 0x1F; 
    cmds[3] = 0xDA; 
    cmds[4] = 0x02; 
    i2c.write(0x78, cmds, 5);
}

void beep(int duration_ms) {
    buzzer = 1;
    wait_ms(duration_ms);
    buzzer = 0;
}

void updateOLED(string message) {
    oled.clearDisplay(); 
    wait_ms(10); 
    oled.printf("%s\n", message.c_str());
}

// --- REAL Keypad Scanner ---
char readKeypad() {
    DigitalOut* rows[] = {&row1, &row2, &row3, &row4};
    DigitalIn* cols[] = {&col1, &col2, &col3, &col4};
    char keys[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    for (int r = 0; r < 4; r++) {
        row1 = 1; row2 = 1; row3 = 1; row4 = 1; 
        *rows[r] = 0; 
        wait_ms(2); 

        for (int c = 0; c < 4; c++) {
            if (*cols[c] == 0) { 
                char pressedKey = keys[r][c];
                
                beep(50); 
                
                while (*cols[c] == 0) { 
                    wait_ms(10);
                }
                return pressedKey;
            }
        }
    }
    return '\0'; 
}

// --- Main Program ---
int main() {
    // 1. Hardware Initialization
    i2c.frequency(400000); 
    wait_ms(100); 
    fixOLED_Resolution();
    
    greenLed = 0;
    redLed = 1; 
    buzzer = 0;
    
    updateOLED("System Ready");
    wait_ms(1000);
    updateOLED("Enter Password:");

    // 2. Main Super Loop
    while (true) {
        char keyPress = readKeypad(); 

        if (keyPress != '\0') {
            
            switch (currentState) {
                case LOCKED:
                    if (keyPress == '#') {
                        if (inputBuffer == currentPassword) {
                            // CORRECT PASSWORD -> UNLOCK
                            currentState = UNLOCKED;
                            wrongAttempts = 0;
                            updateOLED("Door is open");
                            
                            redLed = 0;   
                            greenLed = 1; 
                            beep(100); wait_ms(100); beep(100); 
                            
                            wait_ms(5000); 
                            
                            greenLed = 0; 
                            redLed = 1;   
                            currentState = LOCKED;
                            inputBuffer = "";
                            updateOLED("Enter Password:");
                        } else {
                            // WRONG PASSWORD
                            wrongAttempts++;
                            inputBuffer = "";
                            if (wrongAttempts >= 3) {
                                currentState = LOCKED_OUT;
                                updateOLED("Try again in 30s");
                                buzzer = 1; 
                                wait_ms(1000); 
                                buzzer = 0;
                                wait_ms(29000); 
                                wrongAttempts = 0;
                                currentState = LOCKED;
                                updateOLED("Enter Password:");
                            } else {
                                updateOLED("Wrong PW!");
                                wait_ms(1000);
                                updateOLED("Enter Password:");
                            }
                        }
                    } 
                    else if (keyPress == '*') {
                        if (inputBuffer == currentPassword) {
                            // ACTIVATE PASSWORD CHANGE
                            currentState = CHANGE_PW_NEW;
                            inputBuffer = "";
                            updateOLED("Enter 8-char PW");
                        } else {
                            // *** NEW LOGIC: ACT AS A CLEAR BUTTON ***
                            inputBuffer = "";
                            beep(200); // Slightly longer beep to confirm clear
                            updateOLED("Cleared");
                            wait_ms(800); // Brief pause so the user can read it
                            updateOLED("Enter Password:");
                        }
                    } 
                    else {
                        // TYPING PASSWORD (0-9, A-D)
                        if (inputBuffer.length() < 8) {
                            inputBuffer += keyPress;
                            // Show asterisks on OLED
                            string hiddenStars(inputBuffer.length(), '*');
                            updateOLED(hiddenStars);
                        }
                    }
                    break;

                case CHANGE_PW_NEW:
                    if (keyPress == '#') {
                        if (inputBuffer.length() == 8) {
                            // SAVE NEW PASSWORD
                            currentPassword = inputBuffer; 
                            currentState = LOCKED;
                            inputBuffer = "";
                            updateOLED("Password Saved!");
                            beep(100); wait_ms(100); beep(100); 
                            wait_ms(1500);
                            updateOLED("Enter Password:");
                        } else {
                            // REJECT: Not enough characters
                            updateOLED("Must be 8 chars!");
                            beep(500); 
                            wait_ms(1500);
                            inputBuffer = ""; 
                            updateOLED("Enter 8-char PW");
                        }
                    } 
                    else if (keyPress == '*') {
                        // CANCEL PASSWORD CHANGE (Acts as an exit button here)
                        currentState = LOCKED;
                        inputBuffer = "";
                        updateOLED("Cancelled");
                        wait_ms(1000);
                        updateOLED("Enter Password:");
                    } 
                    else {
                        // TYPING NEW PASSWORD
                        if (inputBuffer.length() < 8) {
                            inputBuffer += keyPress;
                            // Show actual characters so they know what they are setting
                            updateOLED(inputBuffer); 
                        }
                    }
                    break;
                    
                case UNLOCKED:
                case LOCKED_OUT:
                    break;
            }
        }
        wait_ms(50); 
    }
}
