
#include "daisysp.h"
#include "daisy_patch_sm.h"
#include <string>
#include "../../../MyHelpers/I2CHandler.cpp" 
#include "../../../MyHelpers/MCP4728Daisy.h" 
///I2CHandler.cpp"
using namespace daisy;
using namespace daisysp;
using namespace patch_sm;
DaisyPatchSM patch;
using namespace auxilary;
I2CHandler i2c;
MCP4728 mcp;

#define number_of_bytes 32
#define DAC_ADDRESS 0x60

uint8_t data[number_of_bytes] = {0};
int main(void)
{
    // patch..Configure();
    patch.Init();

    patch.StartLog(false);
    I2CHandle::Config i2c_conf;
    i2c_conf.periph = I2CHandle::Config::Peripheral::I2C_1;
    i2c_conf.speed  = I2CHandle::Config::Speed::I2C_400KHZ;
    i2c_conf.mode   = I2CHandle::Config::Mode::I2C_MASTER;
    i2c_conf.pin_config.scl  = DaisyPatchSM::B7;
    i2c_conf.pin_config.sda  = DaisyPatchSM::B8;
    mcp.Init(i2c_conf);
    System::Delay(5000);
    i2c.Init();
    int i = 0;
    while(1) {


        uint16_t data = i;
        i+=1000;
        i%=4000;
        auto i2cResult =  mcp.analogWrite(MCP4728::DAC_CH::A ,data , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        if (i2cResult == I2CHandle::Result::OK) 
        {
            // patch.PrintLine("Successfully got requested number of bytes: %d." , number_of_bytes);
            patch.PrintLine("Successfully got requested number of bytes: %d.",data);
                
            
        }else
        {
            patch.Print("i2c error");
        }
        patch.Delay(2000);
        //sleep(1); 
        
    } 
}
