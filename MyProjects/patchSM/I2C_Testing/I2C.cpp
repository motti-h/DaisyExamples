#include "daisysp.h"
#include "daisy_patch_sm.h"
#include <string>
#include "I2CHandler.cpp"

using namespace daisy;
using namespace daisysp;
using namespace patch_sm;
DaisyPatchSM patch;
using namespace auxilary;
I2CHandler i2c;

#define number_of_bytes 32
#define arduino_address 8

uint8_t data[number_of_bytes] = {0};
int main(void)
{
    


    // patch..Configure();
    patch.Init();

    patch.StartLog(false);
    System::Delay(5000);
    i2c.Init();

    while(1) {

        I2CHandle::Result i2cResult = i2c.RequestData(arduino_address,data,number_of_bytes);
        if (i2cResult == I2CHandle::Result::OK) 
        {
            // patch.PrintLine("Successfully got requested number of bytes: %d." , number_of_bytes);
            patch.PrintLine((char*)data);
                
            
        }else
        {
            patch.Print("i2c error");
        }
               
        
    } 
}