
#include "FPS_GT511C3.h"
#include "SoftwareSerial.h"

FPS_GT511C3 fps(0, 2);  //ESP1 or ESP12

void setup()
{
  Serial.begin(9600);
  delay(100);
  fps.Open();
}

void loop()
{

  // Identify fingerprint test
  if (fps.IsPressFinger())
  {
    fps.CaptureFinger(false);
    int id = fps.Identify1_N();
    if (id <200)
    {
      Serial.print("Deleting ID:");
      Serial.println(id);

      if(fps.DeleteID(id)){
        Serial.println("ID deleted graciously.");
      }else{
        Serial.println("Could not delete ID.");
      }
    }
    else
    {
      Serial.println("Finger not found");
    }
  }
  else
  {
    fps.SetLED(false);
    delay(500);
    fps.SetLED(true);
    delay(500);
  }
}
