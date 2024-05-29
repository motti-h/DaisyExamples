#include "daisysp.h"
#include "daisy_patch_sm.h"
using namespace daisy;
using namespace daisysp;
using namespace patch_sm;
namespace auxilary
{
    /** Private implementation for I2CHandle */
    unsigned char address = 8;
    long counter = 1L;
    I2CHandle _i2c;
    I2CHandle::Config i2c_conf;
    class I2CHandler
    {
        public:
        daisy::I2CHandle::Result Init()
        {
                
                i2c_conf.periph = I2CHandle::Config::Peripheral::I2C_1;
                i2c_conf.speed  = I2CHandle::Config::Speed::I2C_100KHZ;
                i2c_conf.mode   = I2CHandle::Config::Mode::I2C_MASTER;
                i2c_conf.pin_config.scl  = DaisyPatchSM::B7;
                i2c_conf.pin_config.sda  = DaisyPatchSM::B8;
                
                return _i2c.Init(i2c_conf);
        }

        daisy::I2CHandle::Result Init(I2CHandle::Config config,daisy::Pin scl,daisy::Pin sda)
        {
                
                i2c_conf.periph = config.periph;
                i2c_conf.speed  = config.speed;
                i2c_conf.mode   = config.mode;
                i2c_conf.pin_config.scl  = scl;
                i2c_conf.pin_config.sda  = sda;
                
                return _i2c.Init(i2c_conf);
        }

        daisy::I2CHandle::Result RequestData(int address, uint8_t *data,int numberOfBytes) 
        {
            I2CHandle::Result i2cResult = _i2c.ReceiveBlocking(address, data, numberOfBytes, 500);
            if (i2cResult == I2CHandle::Result::OK) {
                return daisy::I2CHandle::Result::OK;
            } else {
                _i2c.Init(i2c_conf);
                return daisy::I2CHandle::Result::ERR;
            }
        }

        daisy::I2CHandle::Result TransmitData(int address, uint8_t *data,int numberOf)
        {
            I2CHandle::Result i2cResult = _i2c.TransmitBlocking(address, data, numberOf, 500);
            if (i2cResult == I2CHandle::Result::OK) {
                return daisy::I2CHandle::Result::OK;
            } else {
                _i2c.Init(i2c_conf);
                return daisy::I2CHandle::Result::ERR;
            }
        }

    };
}