#include "FPS_GT511C3.h"
#include "SoftwareSerial.h"

//FPS connected to pin 4 and 5 - see previous schemas
//FPS_GT511C3 fps(0, 2);  // ESP1
FPS_GT511C3 fps(4, 5);  //ESP12

void setup()
{
  Serial.begin(9600);
  delay(100);
  fps.Open();

  fps.DeleteAll();
}

void loop()
{
  fps.SetLED(true); // turn on the LED inside the fps
  delay(1000);
  fps.SetLED(false);// turn off the LED inside the fps
  delay(1000);
  }
