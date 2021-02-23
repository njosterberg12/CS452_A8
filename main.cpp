#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include "SevSegNum.h"
#include "semphr.h"
#include "queue.h"
#include "Wire.h"
#include <ClosedCube_HDC1080.h>
#include <Stepper.h>

#define SevenSegCC1 44
#define SevenSegCC2 46

#define SevenSegA 4
#define SevenSegB 5
#define SevenSegC 6
#define SevenSegD 7
#define SevenSegE 8
#define SevenSegF 9
#define SevenSegG 10
#define SevenSegDP 11

#define DIP1 53 // DIP starts at 1 instead of 0 so it matches the dip switch board for clarity
#define DIP2 51
#define DIP3 49
#define DIP4 47
#define DIP5 45
#define DIP6 43
#define DIP7 41
#define DIP8 39

#define FRAME_RATE 2/3
//#define ONE_SEC 1000 / portTICK_PERIOD_MS
// #define HALF_SEC 1000 / portTICK_PERIOD_MS / 2



// task prototypes
void vSevSegDisplay(void *pvParameters);
void vDipSwitch(void *pvParameters);
void vMoveStepper(void *pvParameters);


// function prototypes
void sevSegNumbers(int);
void printSevSeg(int, int, int);
void segManager(int, int);
void checkQueueIsFull(int);


QueueHandle_t leftDigQueue = 0;
QueueHandle_t rightDigQueue = 0;
QueueHandle_t tempOrHumQueue = 0;
QueueHandle_t stepperQueue = 0;

SemaphoreHandle_t xBinarySemaphore;

TaskHandle_t LeftTask_Handle;
TaskHandle_t RightTask_Handle;

ClosedCube_HDC1080 hdc1080;

Stepper step_motor(2048, 24, 28, 26, 30);


void setup() {

  // configures pins to be used as outputs for 7 seg display
  pinMode(SevenSegA, OUTPUT);
  pinMode(SevenSegB, OUTPUT);
  pinMode(SevenSegC, OUTPUT);
  pinMode(SevenSegD, OUTPUT);
  pinMode(SevenSegE, OUTPUT);
  pinMode(SevenSegF, OUTPUT);
  pinMode(SevenSegG, OUTPUT);
  pinMode(SevenSegDP, OUTPUT);

  // configures pins to be used as outputs to turn on each digit of 7 seg display
  pinMode(SevenSegCC1, OUTPUT); // right digit
  pinMode(SevenSegCC2, OUTPUT); // left digit

  // configures dip switch pins
  pinMode(DIP1, INPUT);
  pinMode(DIP2, INPUT);
  pinMode(DIP3, INPUT);
  pinMode(DIP4, INPUT);
  pinMode(DIP5, INPUT);
  pinMode(DIP6, INPUT);
  pinMode(DIP7, INPUT);
  pinMode(DIP8, INPUT);

  Serial.begin(9600);

  // initialize HDC1080
  hdc1080.begin(0x40);

   // put your setup code here, to run once:
  while(!Serial)
  {
    ;
  }

  // step motor set to fast speed.
  step_motor.setSpeed(1000);

  leftDigQueue = xQueueCreate(5, sizeof (int)); // queues initialized to hold 1 value
  rightDigQueue = xQueueCreate(5, sizeof (int));
  stepperQueue = xQueueCreate(2, sizeof (int));

  xBinarySemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xBinarySemaphore);

  xTaskCreate(vDipSwitch, "Dip", 512, NULL, 2, NULL); 
  xTaskCreate(vSevSegDisplay, "Display", 128, NULL, 1, &LeftTask_Handle);
  xTaskCreate(vMoveStepper, "Stepper", 2048, NULL, 3, NULL);

  vTaskStartScheduler();
}

void loop() {
  //not needed in RTOS
}

/*********************************************************
 * void vDipSwitch(void *pvParameters)
 * 
 * This function runs the dip switch 
 * *******************************************************/
void vDipSwitch(void *pvParameters)
{
  (void) pvParameters;

  int stepCount = 0;
  int x, y;
  int i;

  int test = 1;
  // display hex numbers on sevSegment Display
  for(i = 0; i < 256; i++)
  {
    x = i / 16;
    y = i % 16;
    segManager(x,y);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  i = 0;
  // check dip switches 1,2,3,4
  for(;;)
  {
    if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == LOW))
    {
      segManager(0,19);
      checkQueueIsFull(test);
      Serial.print("T=");
      Serial.print(hdc1080.readTemperature());
      Serial.println("C");
      // (0,0,0,0)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5, 19);

      // DO NOTHING 
      // (0,0,0,1)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == LOW))
    {
      //Serial.println("move CCW");
      checkQueueIsFull(test);
      segManager(3, 16);
      while(i < 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = -1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i=0;
      // (0,0,1,0)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5, 19);

      // DO NOTHING 
      // (0,0,1,1)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == LOW))
    {
      //Serial.println("move CW");
      checkQueueIsFull(test);
      segManager(2,17);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = 1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      // (0,1,0,0)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5,19);

      // DO NOTHING 
      // (0,1,0,1)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == LOW))
    {
      //Serial.println("Move CW then CCW"); 
      checkQueueIsFull(test);
      segManager(4, 17);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = 1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      segManager(4, 16);
      //while(i <= 2048)
      checkQueueIsFull(test);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = -1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      // (0,1,1,0)
    }
    else if((digitalRead(DIP1) == LOW) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      segManager(5,19);
      Serial.println("Stop");
      // DO NOTHING 
      // (0,1,1,1)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == LOW))
    {
      checkQueueIsFull(test);
      i = 1;
      Serial.print("RH=");
      Serial.print(hdc1080.readHumidity());
      Serial.println("%");
      segManager(1, 17);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = 1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      // (1,0,0,0)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5,19);
      // DO NOTHING 
      // (1,0,0,1)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == LOW))
    { 
      //Serial.println("move CCW");
      checkQueueIsFull(test);
      segManager(3, 16);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = -1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      // (1,0,1,0)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == LOW) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5, 19);
      // DO NOTHING 
      // (1,0,1,1)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == LOW))
    {
      checkQueueIsFull(test);
      segManager(2, 17);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = 1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      // (1,1,0,0)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == LOW) && (digitalRead(DIP4) == HIGH))
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5,19);
      // DO NOTHING 
      // (1,1,0,1)
    }
    else if((digitalRead(DIP1) == HIGH) && (digitalRead(DIP2) == HIGH) && (digitalRead(DIP3) == HIGH) && (digitalRead(DIP4) == LOW))
    { 
      checkQueueIsFull(test);
      segManager(4, 17);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = 1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      checkQueueIsFull(test);
      segManager(4, 16);
      while(i <= 2048)
      {
        //Serial.println("check if it gets to the while loop");
        stepCount = -1;
        xQueueSend(stepperQueue, &stepCount, portMAX_DELAY);
        taskYIELD();

        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(1);
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        i++;
      }
      i = 0;
      // (1,1,1,0)
    }
    else
    {
      checkQueueIsFull(test);
      Serial.println("Stop");
      segManager(5,19);
      // DO NOTHING 
      // (1,1,1,1)
    }
  }
}

/*******************************************************
 * void vSevSegDisplay(void *pvParameters)
 * 
 *  Task to display digits on seven seg display 
 * ****************************************************/
void vSevSegDisplay(void *pvParameters)
{
  (void) pvParameters;
  int num = 0, num2 = 0;
  for(;;)
  {
    xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
    xQueueReceive(leftDigQueue, &num, 0); // receives left digit from queue
    printSevSeg(SevenSegCC2, SevenSegCC1, num);
    xQueueReceive(rightDigQueue, &num2, 0); // receives right digit from queue
    printSevSeg(SevenSegCC1, SevenSegCC2, num2);
    xSemaphoreGive(xBinarySemaphore);
  }
}

/****************************************************
 * void vMoveStepper(void *pvParameters)
 * 
 *  Task to move stepper motor
 * *************************************************/
void vMoveStepper(void *pvParameters)
{
  (void) pvParameters;
  int num_steps = 0;
  for(;;)
  {
    if(!xQueueReceive(stepperQueue, &num_steps, portMAX_DELAY))
    {
      Serial.println("Task Not received.");
    }
    step_motor.step(num_steps); //move stepper number of steps
    taskYIELD();
  }
}

// function to switch between digits
void sevSegNumbers(int num)
{
  switch(num) // switches received queue data to display whatever left dig needed
  {
    case 0:
      printNum0on();
      printNum0off();
      break;
    case 1:
      printNum1on();
      printNum1off();
      break;
    case 2:
      printNum2on();
      printNum2off();
      break;
    case 3:
      printNum3on();
      printNum3off();
      break;
    case 4:
      printNum4on();
      printNum4off();
      break;
    case 5:
      printNum5on();
      printNum5off();
      break;
    case 6:
      printNum6on();
      printNum6off();
      break;
    case 7:
      printNum7on();
      printNum7off();
      break;
    case 8:
      printNum8on();
      printNum8off();
      break;
    case 9:
      printNum9on();
      printNum9off();
      break;
    case 10:          // A
      printNumAon();
      printNumAoff();
      break;
    case 11:          // B
      printNumBon();
      printNumBoff();
      break;
    case 12:          // C
      printNumCon();
      printNumCoff();
      break;
    case 13:          // D
      printNumDon();
      printNumDoff();
      break;
    case 14:          // E
      printNumEon();
      printNumEoff();
      break;
    case 15:          // F
      printNumFon();
      printNumFoff();
      break;
    case 16:          // L
      printNumLon();
      printNumLoff();
      break;
    case 17:          // R
      printNumRon();
      printNumRoff();
      break;
    case 18:          // dp
      printDPon();
      printDPoff();
      break;
    case 19:
      printDashon();
      printDashoff();
      break;
  }
}

// function to print digits to seven seg display
void printSevSeg(int side, int off, int num)
{
  digitalWrite(off, LOW);
  digitalWrite(side, HIGH);
  sevSegNumbers(num);
  vTaskDelay(FRAME_RATE);
  sevSegNumbers(num);
}

// function manages 7 seg Display
void segManager(int lNum, int rNum)
{
  xQueueSend(leftDigQueue, &lNum, portMAX_DELAY);
  xQueueSend(rightDigQueue, &rNum, portMAX_DELAY);
}

// function to check if queue is full. if its full from ISR, reset the queue
void checkQueueIsFull(int test)
{
  if(test == 0)
  {
    for(int i = 0; i < 16; i++)
    {
      for(int j = 0; j < 16; j++)
      {
        segManager(i,j);
        vTaskDelay((1000 / portTICK_PERIOD_MS / 2) /4);
      }
    }
    for(int i = 0; i < 10; i++) // if full, display OF
    {
      segManager(0, 15);
    }
  }
  if(xQueueIsQueueFullFromISR(leftDigQueue) == pdTRUE) // if function is full from ISR, reset queues
  {
    xQueueReset(leftDigQueue);
    xQueueReset(rightDigQueue);
    xSemaphoreGive(xBinarySemaphore);
    segManager(0,15);
    vTaskDelay((1000 / portTICK_PERIOD_MS) * 5);
    xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
  }
  else
  {
    xSemaphoreGive(xBinarySemaphore);
    vTaskDelay((1000 / portTICK_PERIOD_MS) * 5);
    xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
  }
}
