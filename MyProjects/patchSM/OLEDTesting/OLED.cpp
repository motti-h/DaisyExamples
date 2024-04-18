#include <stdio.h>
#include <string>
#include "daisy_patch_sm.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;
using namespace patch_sm;

#define I2C_ADDRESS 22

/** Typedef the OledDisplay to make syntax cleaner below 
 *  This is a 4Wire SPI Transport controlling an 128x64 sized SSDD1306
 * 
 *  There are several other premade test 
*/
using MyOledDisplay = OledDisplay<SSD130xI2c128x64Driver>;

DaisyPatchSM hw;
MyOledDisplay display;

//variables
uint8_t  value   = 3;

uint8_t random(uint8_t dontCare) {
    static uint8_t number = 10;
    return number++;
}

void request_N_Bytes_and_Crc(uint8_t testData[32], uint8_t requestedNumber) {
  uint8_t crc = 0;
  uint8_t random_number = random(255);
  for (uint8_t i = 0; i < requestedNumber; i++) {
    testData[i] = random_number;
    crc = crc + random_number;
    random_number++;
  }
  // The CRC is send as the final uint8_t.
  testData[requestedNumber] = crc;
}

int main(void)
{
    uint8_t index = 0;
    unsigned char address = 0x40;
    uint8_t testData[33];
    long counter = 1L;

    hw.Init();
    // setup the configuration
    I2CHandle::Config i2c_conf;
    i2c_conf.periph = I2CHandle::Config::Peripheral::I2C_1;
    i2c_conf.speed  = I2CHandle::Config::Speed::I2C_100KHZ;
    i2c_conf.mode   = I2CHandle::Config::Mode::I2C_SLAVE;
    i2c_conf.pin_config.scl  = hw.GetPin(daisy::patch_sm::DaisyPatchSM::PinBank::B,7);
    i2c_conf.pin_config.sda  = hw.GetPin(daisy::patch_sm::DaisyPatchSM::PinBank::B,8);
    // initialise the peripheral
    I2CHandle i2c;
    I2CHandle::Result i2cInitResult = i2c.Init(i2c_conf);
    // now i2c points to the corresponding peripheral and can be used.
    

    uint8_t message_idx;
    /** Configure the Display */
    MyOledDisplay::Config disp_cfg;
    // disp_cfg.driver_config.transport_config.i2c_address = 
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl    = hw.GetPin(daisy::patch_sm::DaisyPatchSM::PinBank::B,7);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda    = hw.GetPin(daisy::patch_sm::DaisyPatchSM::PinBank::B,8);
    
    /** And Initialize */
    display.Init(disp_cfg);

    message_idx = 0;
    char strbuff[128];

    while(1)
    {
        System::Delay(500);
        switch(message_idx)
        {
            case 0:
            if(i2cInitResult==I2CHandle::Result::ERR)
            {
                sprintf(strbuff, "init error ");
            }else
            {
                 sprintf(strbuff, "init ok");
            } 
            break;
            case 1: sprintf(strbuff, "play. . ."); break;
            case 2: sprintf(strbuff, "with. . ."); break;
            case 3: sprintf(strbuff, "me. . ."); break;
            case 4: {
                std::string str  = "value " + std::to_string(static_cast<uint32_t>(value));
                char *      cstr = &str[0];
                sprintf(strbuff,cstr);
            } break;
            default: break;
        }
        message_idx = (message_idx + 1) % 5;
        display.Fill(true);
        display.SetCursor(0, 0);
        display.WriteString(strbuff, Font_11x18, false);
        display.Update();

        
        uint16_t size = 1;
        uint32_t timeout = 1;
       
        //i2c.ReceiveBlocking(I2C_ADDRESS, &value, size, timeout);

        // Wait for the requested number of bytes.
        uint8_t number = 0;
        //seed.PrintLine("%05ld Waiting for requested number of bytes on address %0x.", counter, address);
        I2CHandle::Result i2cResult = i2c.ReceiveBlocking(address, &number, 1, 500);
        if (i2cResult == I2CHandle::Result::OK) {
            //seed.PrintLine("%05ld Received requested number of bytes: %0x.", counter, number);

            // Generate the requested number of bytes.
            request_N_Bytes_and_Crc(testData, number);

            // Transmit the requested number of bytes.
            //seed.PrintLine("%05ld Sending %0x bytes from address %0x.", counter, number + 1, address);
            i2cResult = i2c.TransmitBlocking(address, testData, number + 1, 500);
            if(i2cResult == I2CHandle::Result::OK) {
                //seed.PrintLine("%05ld %0x bytes were sent to address 0x%x.", counter, address);
            }
        } else {
            //seed.PrintLine("%05ld No request for data received.", counter);
        }
        System::Delay(1000);
        counter++;
        
    }
}
