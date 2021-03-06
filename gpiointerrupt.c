#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>

/* Driver configuration */
#include "ti_drivers_config.h"
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/Timer.h>

// Define fucntion
#define DISPLAY(x) UART_write(uart, &output, x);

// Driver Handles - Global variables
Timer_Handle timer0;
volatile unsigned char TimerFlag = 0;

// UART Global Variables
char output[64];
int bytesToSend;

// Driver Handles - Global variables
UART_Handle uart;

// I2C Global Variables
static const struct {
    uint8_t address;
    uint8_t resultReg;
    char *id;
} sensors[3] = {
    { 0x48, 0x0000, "11X" },
    { 0x49, 0x0000, "116" },
    { 0x41, 0x0001, "006" }
};

uint8_t txBuffer[1];
uint8_t rxBuffer[2];
I2C_Transaction i2cTransaction;

// Driver Handles - Global variables
I2C_Handle i2c;




typedef struct task {
    int state;
    unsigned int period;
    unsigned int elapsedTime;
    int (*Tick)(int);
} task;

const unsigned int numTasks = 3;
const unsigned long tasksPeriodGCD = 100;
enum {UART, Button, TEMP};

enum Button_State {Button_Noupdate, Button_Increase, Button_Decrease};
int Tick_ButtonUpdate(int state);

enum TEMP_State {Heat_Off, Heat_On};
int Tick_TempUpdate(int state);


int Tick_UART(int state);


task taskList[numTasks] = {
    { 0, 1000, 0, Tick_UART },
    { Button_Noupdate, 200, 0, Tick_ButtonUpdate},
    { Heat_Off, 500, 0, Tick_TempUpdate }
};

// Global variables
short temp = 20;    // current set temperature
unsigned long TimerCount = 0;   // counter for how long the program has been running

// prototypes
void timerCallback(Timer_Handle myHandle, int_fast16_t status);
void initTimer(void);
void initUART(void);
void initI2C(void);
int16_t readTemp(void);
void gpioButtonFxn0(uint_least8_t index);
void gpioButtonFxn1(uint_least8_t index);
int Tick_UART(int state);
int Tick_TempUpdate(int state);
int Tick_ButtonUpdate(int state);


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0) {
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Turn off user LED */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);


    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);
    GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_1);

    initUART();
    initI2C();
    initTimer();

    while (1) {
        while(!TimerFlag) {}
        int i;
        for (i = 0; i < numTasks; i++)
        {
            if (taskList[i].elapsedTime >= taskList[i].period)
            {
                taskList[i].state = taskList[i].Tick(taskList[i].state);
                taskList[i].elapsedTime = 0;
            }
            taskList[i].elapsedTime += tasksPeriodGCD;
        }
        TimerCount += tasksPeriodGCD;
        TimerFlag = 0;
    }
    return (NULL);
}


void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    TimerFlag = 1;
}

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn0(uint_least8_t index)
{
    taskList[Button].state = Button_Increase;
}

/*
 *  ======== gpioButtonFxn1 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_1.
 *  This may not be used for all boards.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn1(uint_least8_t index)
{
    taskList[Button].state = Button_Decrease;
}


int Tick_UART(int state){
    //DISPLAY(snprintf(output, 64, "<%02d,%02d,%d,%04d>\n\r", readTemp(), temp, taskList[TEMP].state, TimerCount / 1000))
      DISPLAY(snprintf(output, 64, "<Sens temp: %02d, BTN Temp: %02d, TaskList: %d, Running for: %04d>\n\r", readTemp(), temp, taskList[TEMP].state, TimerCount / 1000))
}


int Tick_TempUpdate(int state){
    if (readTemp() >= temp) {
        GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
        return Heat_Off;
    } else {
        GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
        return Heat_On;
    }
}

int Tick_ButtonUpdate(int state){
    switch(state){
    case Button_Increase:
        temp++;
        break;
    case Button_Decrease:
        temp--;
        break;
    }
    return Button_Noupdate;
}

void initTimer(void)
{
     Timer_Params params;

     // Init the driver
     Timer_init();

  
     Timer_Params_init(&params);
     params.period = 100000;
     params.periodUnits = Timer_PERIOD_US;
     params.timerMode = Timer_CONTINUOUS_CALLBACK;
     params.timerCallback = timerCallback;

     
     timer0 = Timer_open(CONFIG_TIMER_0, &params);
     if (timer0 == NULL) {
         /* Failed to initialized timer */
         while (1) {}
     }

     if (Timer_start(timer0) == Timer_STATUS_ERROR) {
         /* Failed to start timer */
         while (1) {}
     }
}

void initUART(void)
{
    UART_Params uartParams;
    // Init the driver
    UART_init();

    // Configure the driver
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.baudRate = 115200;

    // Open the driver
    uart = UART_open(CONFIG_UART_0, &uartParams);

    if (uart == NULL) {
        /* UART_open() failed */
        while (1);
    }
}

// Make sure you call initUART() before calling this function.
void initI2C(void)
{
    int8_t i, found;
    I2C_Params i2cParams;
    DISPLAY(snprintf(output, 64, "Initializing I2C Driver - "))

    // Init the driver
    I2C_init();

    // Configure the driver
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // Open the driver
    i2c = I2C_open(CONFIG_I2C_0, &i2cParams);
    if (i2c == NULL)
    {
        DISPLAY(snprintf(output, 64, "Failed\n\r"))
        while (1);
    }

    DISPLAY(snprintf(output, 32, "Passed\n\r"))

    // Boards were shipped with different sensors.
    // Welcome to the world of embedded systems.
    // Try to determine which sensor we have.
    // Scan through the possible sensor addresses
    /* Common I2C transaction setup */
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;
    found = false;
    for (i=0; i<3; ++i)
    {
        i2cTransaction.slaveAddress = sensors[i].address;
        txBuffer[0] = sensors[i].resultReg;
        DISPLAY(snprintf(output, 64, "Is this %s? ", sensors[i].id))

        if (I2C_transfer(i2c, &i2cTransaction))
        {
            DISPLAY(snprintf(output, 64, "Found\n\r"))
            found = true;
            break;
        }
        DISPLAY(snprintf(output, 64, "No\n\r"))
    }
    if(found)
    {
        DISPLAY(snprintf(output, 64, "Detected TMP%s I2C address: %x\n\r", sensors[i].id, i2cTransaction.slaveAddress))
    }
    else
    {
        DISPLAY(snprintf(output, 64, "Temperature sensor not found, contact professor\n\r"))
    }
}

int16_t readTemp(void)
{
    int j;
    int16_t temperature = 0;
    i2cTransaction.readCount = 2;

    if (I2C_transfer(i2c, &i2cTransaction))
    {
        /*
        * Extract degrees C from the received data;
        * see TMP sensor datasheet
        */
        temperature = (rxBuffer[0] << 8) | (rxBuffer[1]);
        temperature *= 0.0078125;
        /*
        * If the MSB is set '1', then we have a 2's complement
        * negative value which needs to be sign extended
        */
        if (rxBuffer[0] & 0x80)
        {
            temperature |= 0xF000;
        }
    }
    else
    {
        DISPLAY(snprintf(output, 64, "Error reading temperature sensor (%d)\n\r",i2cTransaction.status))
        DISPLAY(snprintf(output, 64, "Please power cycle your board by unplugging USB and plugging back in.\n\r"))
    }

    return temperature;
}


