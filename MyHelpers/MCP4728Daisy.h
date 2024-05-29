#pragma once
#ifndef MCP4728_H
#define MCP4728_H
#include "daisysp.h"
#include "daisy_patch_sm.h"

using namespace daisy;
using namespace daisysp;
using namespace patch_sm;
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))
class MCP4728
{
public:

    enum class CMD
    {
        FAST_WRITE = 0x00,
        MULTI_WRITE = 0x40,
        SINGLE_WRITE = 0x58,
        SEQ_WRITE = 0x50,
        SELECT_VREF = 0x80,
        SELECT_GAIN = 0xC0,
        SELECT_PWRDOWN = 0xA0
    };
    enum class DAC_CH { A, B, C, D };
    enum class VREF { VDD, INTERNAL_2_8V };
    enum class PWR_DOWN { NORMAL, GND_1KOHM, GND_100KOHM, GND_500KOHM };
    enum class GAIN { X1, X2 };

    I2CHandle::Result Init(I2CHandle::Config const & config)
    {
        i2c_conf = config;
        return i2c.Init(i2c_conf);
    }

    I2CHandle::Result analogWrite(DAC_CH ch, uint16_t data, VREF vref, PWR_DOWN pd, GAIN gain)
    {

        uint8_t transmit[3]; 
        transmit[0] = (uint8_t)CMD::SINGLE_WRITE | ((uint8_t)ch << 1);
        // i2c.TransmitBlocking(addr_,data,sizeof(uint8_t),100);
        //wire_->write((uint8_t)CMD::SINGLE_WRITE | (ch << 1));

        transmit[1] = ((uint8_t)vref << 7) | ((uint8_t)pd << 5) | ((uint8_t)gain << 4) | highByte(data);
        // i2c.TransmitBlocking(addr_,&data,sizeof(uint8_t),100);
        //wire_->write(((uint8_t)eep_[ch].vref << 7) | ((uint8_t)eep_[ch].pd << 5) | ((uint8_t)eep_[ch].gain << 4) | highByte(eep_[ch].data));
        
        transmit[2] = lowByte(data);
        auto result = i2c.TransmitBlocking(addr_,transmit,3*sizeof(uint8_t),100);
        //wire_->write(lowByte(eep_[ch].data));
        return result;
        // else
        // {
        //     reg_[ch].data = data;
        //     return fastWrite();
        // }
    }

    I2CHandle::Result fastWrite(uint16_t data)
    {
        // i2c->beginTransmission(addr_);
        // for (uint8_t i = 0; i < 4; ++i)
        // {
        //     wire_->write((uint8_t)CMD::FAST_WRITE | highByte(reg_[i].data));
        //     wire_->write(lowByte(reg_[i].data));
        // }
        // return wire_->endTransmission();
        uint8_t transmit[2]; 
        transmit[0] = (uint8_t)CMD::FAST_WRITE | highByte(data);

        transmit[1] = lowByte(data);

        auto result = i2c.TransmitBlocking(addr_,transmit,3*sizeof(uint8_t),100);
        return result;
    }

    // void attach(TwoWire& w, uint8_t pin)
    // {
    //     wire_ = &w;
    //     pin_ldac_ = pin;
    //     pinMode(pin_ldac_, OUTPUT);
    //     enable(false);
    //     readRegisters();
    // }

    // void enable(bool b)
    // {
    //     digitalWrite(pin_ldac_, !b);
    // }

    // uint8_t analogWrite(DAC_CH ch, uint16_t data, bool b_eep = false)
    // {
    //     return analogWrite((uint8_t)ch, data, b_eep);
    // }

    // uint8_t analogWrite(uint8_t ch, uint16_t data, bool b_eep = false)
    // {
    //     if (b_eep)
    //     {
    //         eep_[ch].data = data;
    //         return singleWrite(ch);
    //     }
    //     else
    //     {
    //         reg_[ch].data = data;
    //         return fastWrite();
    //     }
    // }

    // uint8_t analogWrite(uint16_t a, uint16_t b, uint16_t c, uint16_t d, bool b_eep = false)
    // {
    //     if (b_eep)
    //     {
    //         reg_[0].data = eep_[0].data = a;
    //         reg_[1].data = eep_[1].data = b;
    //         reg_[2].data = eep_[2].data = c;
    //         reg_[3].data = eep_[3].data = d;
    //         return seqWrite();
    //     }
    //     else
    //     {
    //         reg_[0].data = a;
    //         reg_[1].data = b;
    //         reg_[2].data = c;
    //         reg_[3].data = d;
    //         return fastWrite();
    //     }
    // }

    // uint8_t selectVref(VREF a, VREF b, VREF c, VREF d)
    // {
    //     reg_[0].vref = a;
    //     reg_[1].vref = b;
    //     reg_[2].vref = c;
    //     reg_[3].vref = d;

    //     uint8_t data = (uint8_t)CMD::SELECT_VREF;
    //     for (uint8_t i = 0; i < 4; ++i) bitWrite(data, 3 - i, (uint8_t)reg_[i].vref);

    //     wire_->beginTransmission(addr_);
    //     wire_->write(data);
    //     return wire_->endTransmission();
    // }

    // uint8_t selectPowerDown(PWR_DOWN a, PWR_DOWN b, PWR_DOWN c, PWR_DOWN d)
    // {
    //     reg_[0].pd = a;
    //     reg_[1].pd = b;
    //     reg_[2].pd = c;
    //     reg_[3].pd = d;

    //     uint8_t h = ((uint8_t)CMD::SELECT_PWRDOWN) | ((uint8_t)a << 2) | (uint8_t)b;
    //     uint8_t l = 0 | ((uint8_t)c << 6) | ((uint8_t)d << 4);

    //     wire_->beginTransmission(addr_);
    //     wire_->write(h);
    //     wire_->write(l);
    //     return wire_->endTransmission();
    // }

    // uint8_t selectGain(GAIN a, GAIN b, GAIN c, GAIN d)
    // {
    //     reg_[0].gain = a;
    //     reg_[1].gain = b;
    //     reg_[2].gain = c;
    //     reg_[3].gain = d;

    //     uint8_t data = (uint8_t)CMD::SELECT_GAIN;
    //     for (uint8_t i = 0; i < 4; ++i) bitWrite(data, 3 - i, (uint8_t)reg_[i].gain);

    //     wire_->beginTransmission(addr_);
    //     wire_->write(data);
    //     return wire_->endTransmission();
    // }

    //void setID(uint8_t id) { addr_ = I2C_ADDR + id; }


    // void readRegisters()
    // {
    //     wire_->requestFrom((int)addr_, 24);
    //     if (wire_->available() == 24)
    //     {
    //         for (uint8_t i = 0; i < 8; ++i)
    //         {
    //             uint8_t data[3];
    //             bool isEeprom = i % 2;
    //             for (uint8_t i = 0; i < 3; ++i) data[i] = wire_->read();

    //             uint8_t ch = (data[0] & 0x30) >> 4;
    //             if (isEeprom)
    //             {
    //                 read_eep_[ch].vref = (VREF)    ((data[1] & 0b10000000) >> 7);
    //                 read_eep_[ch].pd   = (PWR_DOWN)((data[1] & 0b01100000) >> 5);
    //                 read_eep_[ch].gain = (GAIN)    ((data[1] & 0b00010000) >> 4);
    //                 read_eep_[ch].data = (uint16_t)((data[1] & 0b00001111) << 8 | data[2]);
    //             }
    //             else
    //             {
    //                 read_reg_[ch].vref = (VREF)    ((data[1] & 0b10000000) >> 7);
    //                 read_reg_[ch].pd   = (PWR_DOWN)((data[1] & 0b01100000) >> 5);
    //                 read_reg_[ch].gain = (GAIN)    ((data[1] & 0b00010000) >> 4);
    //                 read_reg_[ch].data = (uint16_t)((data[1] & 0b00001111) << 8 | data[2]);
    //             }
    //         }
    //     }
    //}

    // uint8_t getVref(uint8_t ch, bool b_eep = false) { return b_eep ? (uint8_t)read_eep_[ch].vref : (uint8_t)read_reg_[ch].vref; }
    // uint8_t getGain(uint8_t ch, bool b_eep = false) { return b_eep ? (uint8_t)read_eep_[ch].gain: (uint8_t)read_reg_[ch].gain; }
    // uint8_t getPowerDown(uint8_t ch, bool b_eep = false) { return b_eep ? (uint8_t)read_eep_[ch].pd : (uint8_t)read_reg_[ch].pd; }
    // uint16_t getDACData(uint8_t ch, bool b_eep = false) { return b_eep ? (uint16_t)read_eep_[ch].data : (uint16_t)read_reg_[ch].data; }

private:

    // uint8_t fastWrite()
    // {
    //     i2c->beginTransmission(addr_);
    //     for (uint8_t i = 0; i < 4; ++i)
    //     {
    //         wire_->write((uint8_t)CMD::FAST_WRITE | highByte(reg_[i].data));
    //         wire_->write(lowByte(reg_[i].data));
    //     }
    //     return wire_->endTransmission();
    // }

    // uint8_t multiWrite()
    // {
    //     wire_->beginTransmission(addr_);
    //     for (uint8_t i = 0; i < 4; ++i)
    //     {
    //         wire_->write((uint8_t)CMD::MULTI_WRITE | (i << 1));
    //         wire_->write(((uint8_t)reg_[i].vref << 7) | ((uint8_t)reg_[i].pd << 5) | ((uint8_t)reg_[i].gain << 4) | highByte(reg_[i].data));
    //         wire_->write(lowByte(reg_[i].data));
    //     }
    //     return wire_->endTransmission();
    // }

    // uint8_t seqWrite()
    // {
    //     wire_->beginTransmission(addr_);
    //     wire_->write((uint8_t)CMD::SEQ_WRITE);
    //     for (uint8_t i = 0; i < 4; ++i)
    //     {
    //         wire_->write(((uint8_t)eep_[i].vref << 7) | ((uint8_t)eep_[i].pd << 5) | ((uint8_t)eep_[i].gain << 4) | highByte(eep_[i].data));
    //         wire_->write(lowByte(eep_[i].data));
    //     }
    //     return wire_->endTransmission();
    // }

    I2CHandle::Result singleWrite(uint8_t ch)
    {
        uint8_t data[3]; 
        data[0] = (uint8_t)CMD::SINGLE_WRITE | (ch << 1);
        // i2c.TransmitBlocking(addr_,data,sizeof(uint8_t),100);
        //wire_->write((uint8_t)CMD::SINGLE_WRITE | (ch << 1));

        data[1] = ((uint8_t)eep_[ch].vref << 7) | ((uint8_t)eep_[ch].pd << 5) | ((uint8_t)eep_[ch].gain << 4) | highByte(eep_[ch].data);
        // i2c.TransmitBlocking(addr_,&data,sizeof(uint8_t),100);
        //wire_->write(((uint8_t)eep_[ch].vref << 7) | ((uint8_t)eep_[ch].pd << 5) | ((uint8_t)eep_[ch].gain << 4) | highByte(eep_[ch].data));
        
        data[2] = lowByte(eep_[ch].data);
        auto result = i2c.TransmitBlocking(addr_,data,3*sizeof(uint8_t),100);
        //wire_->write(lowByte(eep_[ch].data));
        return result;
    }

private:

    struct DACInputData
    {
        VREF vref;
        PWR_DOWN pd;
        GAIN gain;
        uint16_t data;
    };

    const uint8_t I2C_ADDR {0x60};

    uint8_t addr_ {I2C_ADDR};
    uint8_t pin_ldac_;

    DACInputData reg_[4];
    DACInputData eep_[4];
    DACInputData read_reg_[4];
    DACInputData read_eep_[4];

    //TwoWire* wire_;
    // setup the configuration
    I2CHandle::Config i2c_conf;

    I2CHandle i2c;
    //    i2c.Init(i2c_conf);
    //    // now i2c points to the corresponding peripheral and can be used.
    //    i2c.TransmitBlocking( ... );
};

#endif // MCP4728_H
