/*******************************************|
|           IMP Project - Terminal          |
|          Author: Veronika Laukova         |
|           Login: xlauko00                 |
|                                           |
********************************************/
/*******************************************|
|    KEYPAD : R1 - GPIO12    C1 - GPIO14    |
|             R2 - GPIO25    C2 - GPIO13    |
|             R3 - GPIO17    C3 - GPIO16    |
|             R4 - GPIO27                   |
|                                           |
|  GREEN LED : GPIO18     RED LED : GPIO26  |
|                                           |
********************************************/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TIMEOUT 300

void PW_correct();
void PW_wrong();
void PW_changed();
void PW_interrupted();
void PW_timeout();
void PW_mode_changed();
void PW_flash();
void SetupPins();
char getKey();
void syncNVS();

char keys[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// Define GPIO pin numbers for Keypad and LED diodes
const int pin_rows[4] = {12, 25, 17, 27};
const int pin_cols[3] = {14, 13, 16};
const int pin_g_led = 18;
const int pin_r_led = 26;

// Array for the right PW
char setPW[20] = {'\0'};

// Array for the entered PW
char loadPW[4] = {'\0'};

// Bool for checking PW match
bool matchPW = true;

// Bool for Timeout feature
bool timeoutOn = false;
int passedTime = 0;

// Bools for PW redefinition
bool settingNewPW = false;
bool secondPhase = false;

// NVS initialization
esp_err_t nvsRet;
nvs_handle_t my_handle;
size_t size;

void app_main() {
    strcpy(setPW, "1234\0");
    size = sizeof(setPW);
    // Synchronize with flash memory
    syncNVS();

    // Print monitor PW message
    printf("\n  +------------------+\n");
    printf("  |     PW: ");
    printf("%c", setPW[0]);
    printf("%c", setPW[1]);
    printf("%c", setPW[2]);
    printf("%c", setPW[3]);
    printf("     |\n");
    printf("  +------------------+\n");

    int index = 0;
    char c;
    SetupPins();

    // Main program loop
    while(1){
        c = getKey();
        if(settingNewPW) {
            if(!secondPhase) {
                if(c == '#'){
                    // Change mode back
                    settingNewPW = false;
                    timeoutOn = false;
                    index = 0;
                }
                else if(c == '*') {
                    // Password setting process restarted
                    index = 0;
                    timeoutOn = false;
                    PW_flash();
                }
                else if(c == 'X') {
                    // Timed out
                    settingNewPW = false;
                    timeoutOn = false;
                    index = 0;
                    PW_timeout();
                }
                else {
                    loadPW[index] = c;
                    if(index == 3) {
                        for(int k = 0; k < 4; k++) {
                            if(setPW[k] != loadPW[k]) {
                                matchPW = false;
                            }
                        }
                        if(matchPW) {
                            PW_correct();
                            secondPhase = true;
                        }
                        else {
                            PW_wrong();
                        }
                        matchPW = true;
                        index = 0;
                        timeoutOn = false;
                        continue;
                    }
                    index++;
                    timeoutOn = true;
                }
            }
            else {
                // Second phase of setting new PW

                if(c == '#' || c == '*') {
                    // Reset MODE back to basic PW entering
                    settingNewPW = false;
                    secondPhase = false;
                    index = 0;
                    timeoutOn = false;
                }
                else if(c == 'X') {
                    // Timed out
                    settingNewPW = false;
                    secondPhase = false;
                    timeoutOn = false;
                    index = 0;
                    PW_timeout();
                }
                else {
                    loadPW[index] = c;
                    if(index == 3) {
                        for(int k = 0; k < 4; k++) {
                            setPW[k] = loadPW[k];
                        }

                        // Open NVS
                        nvsRet = nvs_open("storage", NVS_READWRITE, &my_handle);

                        // Update NVS sotred PW
                        nvsRet = nvs_set_str(my_handle, "setPW", setPW);
                        if(nvsRet != ESP_OK){
                            printf("NVS write failed!\n");
                        }
                        else {
                            // Print monitor message
                            printf("\n  +------------------+\n");
                            printf("  |   New PW: ");
                            printf("%c", setPW[0]);
                            printf("%c", setPW[1]);
                            printf("%c", setPW[2]);
                            printf("%c", setPW[3]);
                            printf("   |\n");
                            printf("  +------------------+\n");
                        }

                        // Commit changes to the NVS
                        nvsRet = nvs_commit(my_handle);
                        if(nvsRet != ESP_OK) {
                            printf("NVS commit failed!\n");
                        }

                        // Close NVS
                        nvs_close(my_handle);

                        // Reset PW input settings
                        settingNewPW = false;
                        secondPhase = false;
                        
                        index = 0;
                        timeoutOn = false;
                        PW_changed();
                        continue;
                    }
                    timeoutOn = true;
                    index++;
                }
            }
        }
        else {
            if(c == '#'){
                // Reset PW entering process
                PW_interrupted();
                index = 0;
                timeoutOn = false;
            }
            else if(c == '*') {
                // Switch to PW change mode
                PW_mode_changed();
                settingNewPW = true;
                secondPhase = false;
                timeoutOn = false;
                index = 0;
            }
            else if(c == 'X') {
                // Timed out
                timeoutOn = false;
                index = 0;
                PW_timeout();
            }
            else {
                loadPW[index] = c;
                if(index == 3) {
                    // Checking if the PWs match
                    for(int k = 0; k < 4; k++) {
                        if(setPW[k] != loadPW[k]) {
                            matchPW = false;
                        }
                    }
                    if(matchPW) {
                        PW_correct();
                    }
                    else {
                        PW_wrong();
                    }
                    matchPW = true;
                    index = 0;
                    timeoutOn = false;
                    continue;
                }
                timeoutOn = true;
                index++;
            }
        }
    }
}

// Entered the correct PW
void PW_correct() {
    gpio_set_level(pin_r_led, 0);
    for(int i = 0; i < 10; i++){
        gpio_set_level(pin_g_led, 1);
        vTaskDelay(10);
        gpio_set_level(pin_g_led, 0);
        vTaskDelay(10);
    }
    gpio_set_level(pin_r_led, 1);
}

// Entered a wrong PW
void PW_wrong() {
    for(int i = 0; i < 10; i++){
        gpio_set_level(pin_r_led, 0);
        vTaskDelay(10);
        gpio_set_level(pin_r_led, 1);
        vTaskDelay(10);
    }
}

// PW successfully set
void PW_changed() {
    for(int i = 0; i < 5; i++) {
        gpio_set_level(pin_r_led, 1);
        gpio_set_level(pin_g_led, 1);
        vTaskDelay(20);
        gpio_set_level(pin_r_led, 0);
        gpio_set_level(pin_g_led, 0);
        vTaskDelay(20);
    }
}

// Pressed '#' - reset PW attempt
void PW_interrupted() {
    gpio_set_level(pin_g_led, 1);
    vTaskDelay(100);
    gpio_set_level(pin_g_led, 0);
}

// The PW has not been completed in time
// Timeout after 3 seconds of inactivity
void PW_timeout() {
    for(int i = 0; i < 3; i++){
        gpio_set_level(pin_r_led, 0);
        vTaskDelay(30);
        gpio_set_level(pin_r_led, 1);
        vTaskDelay(20);
    }
}

// Changed mode to setting new PW
void PW_mode_changed() {
    gpio_set_level(pin_g_led, 1);
    vTaskDelay(100);
    gpio_set_level(pin_g_led, 0);
    gpio_set_level(pin_r_led, 0);
}

// Flash LEDs when reseting the "Set new PW" process
void PW_flash() {
    gpio_set_level(pin_g_led, 1);
    gpio_set_level(pin_r_led, 1);
    vTaskDelay(40);
    gpio_set_level(pin_g_led, 0);
    gpio_set_level(pin_r_led, 0);
}

// Reads input key from 4x3 Keypad. 
// Gradually sets level of ROW pins (OUTPUT) to 0. 
// Checks for level 0 on COL pins (INPUT).
char getKey() {
    while(1) {
        if(timeoutOn) {
            if(passedTime >= TIMEOUT) {
                return 'X';
            }
        }
        for(int i = 0; i < 4; i++){
            gpio_set_level(pin_rows[i], 0);
            for(int j = 0; j < 3; j++) {
                if(gpio_get_level(pin_cols[j]) == 0) { 
                    while(gpio_get_level(pin_cols[j]) == 0) { // Wait until the button is released
                        vTaskDelay(5);                        // to prevent detecting the same 
                    }                                         // button press multiple times
                    return keys[i][j];
                }
            }
            // Set level of ROW[i] pin back to 1.
            gpio_set_level(pin_rows[i], 1);
        }
        vTaskDelay(5);
        if(timeoutOn) {
            passedTime += 5;
        }
        else {
            passedTime = 0;
        }
        
    }
}

// Setting INPUT/OUTPUT on PINS
void SetupPins() {
    // Green LED settings
    gpio_set_direction(pin_g_led, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_g_led, 0);

    // Red LED settings
    gpio_set_direction(pin_r_led, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_r_led, 1);

    // Keypad ROW settings
    for(int i = 0; i < 4; i++) {
        gpio_set_direction(pin_rows[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pin_rows[i], 1);
    }

    // Keypad COLUMN settings
    for(int j = 0; j < 3; j++) {
        gpio_set_direction(pin_cols[j], GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin_cols[j], GPIO_PULLUP_ONLY);
    }
}

// Synchronize PW with NVS
void syncNVS() {
    // Initializing storage
    nvsRet = nvs_flash_init();
    if (nvsRet == ESP_ERR_NVS_NO_FREE_PAGES || nvsRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsRet = nvs_flash_init();
    }

    // Opening storage
    nvsRet = nvs_open("storage", NVS_READWRITE, &my_handle);
    if(nvsRet == ESP_ERR_NVS_NOT_INITIALIZED) {
        // Initialization missing -> retry initialization
        nvsRet = nvs_flash_init();
        if(nvsRet == ESP_ERR_NVS_NO_FREE_PAGES || nvsRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvsRet = nvs_flash_init();
        }
        nvsRet = nvs_open("storage", NVS_READWRITE, &my_handle);
        ESP_ERROR_CHECK(nvsRet);
    }

    // Get PW from NVS
    nvsRet = nvs_get_str(my_handle, "setPW", setPW, &size);
    if(nvsRet == ESP_ERR_NVS_NOT_FOUND) {
        // PW is not written in NVS yet -> write PW into NVS
        nvsRet = nvs_set_str(my_handle, "setPW", setPW);
        if(nvsRet != ESP_OK){
            printf("NVS write failed!\n");
        }
        // Commit changes to the NVS
        nvsRet = nvs_commit(my_handle);
    }
    // Close NVS
    nvs_close(my_handle);
}