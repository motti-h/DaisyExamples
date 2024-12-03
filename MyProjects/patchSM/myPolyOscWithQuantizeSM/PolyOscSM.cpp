#include "daisysp.h"
//#include <string>
#include "daisy_patch_sm.h"
#include "../../../MyHelpers/I2CHandler.cpp" 
#include "../../../MyHelpers/MCP4728Daisy.h" 
#include "dev/oled_ssd130x.h"
#include <random>

using namespace daisy;
using namespace daisysp;
using namespace patch_sm;

using MyOledDisplay = OledDisplay<SSD130xI2c128x64Driver>;

#define DAC_MAX 4095.f
#define SCALE_CV 819.2 

DaisyPatchSM patch;

MCP4728 mcp;

MyOledDisplay display;
uint32_t lastUpdated = 0;
// Create a random device and seed it
std::random_device rd;
std::mt19937 gen(rd()); // Mersenne Twister generator

Oscillator osc[3];
std::string waveNames[5];
int waveform;
int final_wave;
float testval;

//i2c
#define BUFF_SIZE 8
static uint8_t DMA_BUFFER_MEM_SECTION output_buffer[BUFF_SIZE];
float tone = (1.f/12.f), equalDev = (1.f/7.f);
GPIO LDac;

std::vector<float> notesVector;

std::vector<float> sika_voltages_one_octave = {0.000f, 0.087f, 0.252f, 0.418f, 0.586f, 0.671f, 0.839f, 1.000f, 1.087f, 1.252f, 1.418f, 1.586f, 1.671f, 1.839f, 2.000f};
std::vector<float> huseyni_voltages_one_octave = { 0.0, 0.125, 0.25, 0.41666, 0.583, 0.7083, 0.833, 1.0, 1.125, 1.25, 1.41666, 1.583, 1.7083, 1.833, 2.0 };
std::vector<float> huzam_voltages_one_octave = { 0.0, 3/24.f, 7/24.f, 9/24.f, 15/24.f, 17/24.f, 21/24.f, 1.0, 27/24.f, 31/24.f, 33/24.f, 38/24.f, 41/24.f, 45/24.f, 2.0 };
std::vector<float> rast_voltages_2_octave = { 0.0, 2.f*tone, 3.5f*tone, 5.f*tone, 7.f*tone, 9.f*tone, 10.5f*tone, 1,1 + 2.f*tone,1 + 3.5f*tone,1 + 5.f*tone,1 + 7.f*tone,1 + 9.f*tone,1 + 10.5f*tone, 2};
std::vector<float> hijaz_voltages_2_octave = { 0.0, 1.f*tone, 3.5f*tone, 5.f*tone, 7.f*tone, 8.f*tone, 11.f*tone, 1,1 + 1.f*tone,1 + 3.5f*tone,1 + 5.f*tone,1 + 7.f*tone,1 + 8.f*tone,1 + 11.f*tone, 2};
std::vector<float> equal_Temprament = { 0.0, 1.f*equalDev, 2.f*equalDev, 3.f*equalDev, 4.f*equalDev, 5.f*equalDev, 6.f*equalDev, 1,1 + 1.f*equalDev,1 + 2.f*equalDev,1 + 3.f*equalDev,1 + 4.f*equalDev,1 + 5.f*equalDev,1 + 6.f*equalDev, 2};
std::vector<std::vector<float>> scales_one_octave = { equal_Temprament/*, huzam_voltages_one_octave, rast_voltages_one_octave */};
//knobs
float cvInArr[2];
float closestNotes[4] = { 0.0 };
float frequencies[4] = {0};

int getRandomInt(int min, int max);
void UpdateControls();
void CalculateClosestNote(float* cvIn, int* indexes);
uint16_t ConvertNoteToDacValue(float note);
//testing i2c
void testWriteMcp(I2CHandle::Config& config);
I2CHandle::Result WriteMCP_Voltage(I2CHandle& i2c ,uint16_t chA,uint16_t chB);
void writeVref(I2CHandle& i2c , I2CHandle::Config& config);
void writeGain(I2CHandle& i2c , I2CHandle::Config& config);

uint16_t errorCount = 0;
bool high=false;
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    UpdateControls();
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i]; /**< Copy the left input to the left output */
        out[1][i] = in[1][i]; /**< Copy the right input to the right output */
    }
    // for(size_t i = 0; i < size; i++)
    // {
    //     float sig = osc[0].Process();
    //     OUT_L[i]  = sig;
    //     OUT_R[i]  = sig;
    // }
    // for(size_t i = 0; i < size; i++)
    // {

         
    //      float sig = osc[0].Process();
    //      out[0][i] = sig ;
    //     // float mix = 0;
    //     // //Process and output the three oscillators
    //     // for(size_t chn = 0; chn < 3; chn++)
    //     // {
    //     //     float sig = osc[chn].Process();
    //     //     mix += sig * .25f;
    //     //     out[chn][i] = sig;
    //     // }

    //     // //output the mixed oscillators
    //     // out[3][i] = mix;
    // }
}

void SetupOsc(float samplerate)
{
    for(int i = 0; i < 3; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(.5);
    }
}

void SetupWaveNames()
{
    waveNames[0] = "sine";
    waveNames[1] = "triangle";
    waveNames[2] = "saw";
    waveNames[3] = "ramp";
    waveNames[4] = "square";
}

void UpdateOled();

int main(void)
{
    notesVector = scales_one_octave.at(0);

    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    //patch.StartLog(true);
    samplerate = patch.AudioSampleRate();

    waveform   = 0;
    final_wave = Oscillator::WAVE_POLYBLEP_TRI;

    SetupOsc(samplerate);
    SetupWaveNames();

    //i2c configuration
     I2CHandle::Config _i2c_config;
    _i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    _i2c_config.speed  = I2CHandle::Config::Speed::I2C_100KHZ;
    _i2c_config.mode   = I2CHandle::Config::Mode::I2C_MASTER;
    _i2c_config.address = 0x60;
    _i2c_config.pin_config.scl  = DaisyPatchSM::B7;
    _i2c_config.pin_config.sda  =DaisyPatchSM::B8;
    // initialise the peripheral
    I2CHandle _i2c;
    _i2c.Init(_i2c_config);
    patch.Delay(50);
    writeVref(_i2c,_i2c_config);
    patch.Delay(50);
    writeGain(_i2c,_i2c_config);
    patch.Delay(50);

    patch.PrintLine("DAC init OK");
    
    //oled display configuration

    // MyOledDisplay::Config disp_cfg;
    // disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl    = DaisyPatchSM::B7;
    // disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda    = DaisyPatchSM::B8;
    // display.Init(disp_cfg);

    //start
    patch.StartAdc();
    patch.StartDac();
    patch.StartAudio(AudioCallback);
    int lastSendChannelA = 0, lastSendChannelB = 0;
    while(1)
    {
        cvInArr[0] = (patch.GetAdcValue(CV_1)+1.0)*2;
        cvInArr[1] = (patch.GetAdcValue(CV_2)+1.0)*2; 
        //patch.PrintLine("Print a float value: %d",  (int)(cvInArr[0]*100));
        // Quantize to semitones
        int indexes[2] = {0,0};
        CalculateClosestNote(cvInArr,indexes);    
        //frequencies[0] = powf(2.f, closestNotes[0]) * 55; //Hz
        uint16_t cvOut1 = ConvertNoteToDacValue(hijaz_voltages_2_octave[indexes[0]]);
        uint16_t cvOut2 = ConvertNoteToDacValue(rast_voltages_2_octave[indexes[1]]);
        //patch.PrintLine("cv2 out value: %d",  cvOut2);
        if(cvOut1 != lastSendChannelA)
        {
            lastSendChannelA = cvOut1;
            auto i2cResult = WriteMCP_Voltage(_i2c,cvOut1,lastSendChannelB);
            if (i2cResult != I2CHandle::Result::OK) 
            {
            _i2c.Init(_i2c_config);
                //patch.Print("i2c error");
            }
        }

        if(cvOut2 != lastSendChannelB)
        {
            lastSendChannelB = cvOut2;
            auto i2cResult = WriteMCP_Voltage(_i2c,lastSendChannelA,cvOut2);
            if (i2cResult != I2CHandle::Result::OK) 
            {
            _i2c.Init(_i2c_config);
                //patch.Print("i2c error");
            }
        }
  

        patch.Delay(2);
        // auto i2cResult1 =  mcp.analogWrite(MCP4728::DAC_CH::A ,cvOut1 , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        // patch.Delay(20);
        // auto i2cResult2 =  mcp.analogWrite(MCP4728::DAC_CH::B ,cvOut2 , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        // patch.Delay(20);
        // if (i2cResult1 != I2CHandle::Result::OK || i2cResult2 != I2CHandle::Result::OK) 
        // {
        //     mcp.Init(i2c_conf);
        //     patch.Print("i2c error count");
        // }

        //////////////////////////////////////////////
        // patch.Delay(50);
        // auto i2cResult = WriteMCP_Voltage(_i2c,4095,4095);
        // if (i2cResult != I2CHandle::Result::OK) 
        // {
        //    _i2c.Init(_i2c_config);
        //     patch.Print("i2c error");
        // }
        // patch.Delay(50);
        // i2cResult = WriteMCP_Voltage(_i2c,0,0);
        // if (i2cResult != I2CHandle::Result::OK) 
        // {
        //     _i2c.Init(_i2c_config);
        //     patch.Print("i2c error");
        // }
        //UpdateOled();
        
    }
}

void UpdateOled()
{
    u_int32_t now = System::GetNow();
    if(now - lastUpdated > 100)
    {
        lastUpdated = now;
        char strbuff[24];
        sprintf(strbuff, "welcome 3 daisy!");
        display.Fill(false);
        display.SetCursor(0, 0);
        display.WriteString(strbuff, Font_7x10, true);
        display.SetCursor(0, 11);

        // std::string str = std::to_string(cvInArr[0]);
        // const int length = str.length(); 
        // char* char_array = new char[length + 1]; 
        // strcpy(char_array, str.c_str()); 
        // display.WriteString(char_array, Font_7x10, true);
       
        int randomX = getRandomInt(0,128);

        int randomY = getRandomInt(15,64);
        display.DrawCircle(randomX, randomY,5,true);
        display.Update();
    }
    
}

void UpdateControls()
{
    patch.ProcessAllControls();

    osc[0].SetFreq(frequencies[0]);
    osc[0].SetWaveform(Oscillator::WAVE_SAW);

}

void CalculateClosestNote(float* cvIn, int* indexes){
    float minDiff1 = cvIn[0], minDiff2 = cvIn[1];
    closestNotes[0] = 0;
    closestNotes[1] = 0;
    //int index1 =0,index2 = 0;
    for (auto index = 0; index < notesVector.size(); ++index) 
    {
        float diff1 = std::abs(notesVector[index] - cvIn[0]), diff2 = std::abs(notesVector[index] - cvIn[1]);
        if (diff1 < minDiff1) 
            {
                minDiff1 = diff1;
                indexes[0] = index;
                //closestNotes[0] = note;  
            }

        if (diff2 < minDiff2) 
            {
                minDiff2 = diff2;
                indexes[1] = index;
                //closestNotes[1] = note;  
            }
    }
}

uint16_t ConvertNoteToDacValue(float note){
    return (note == 2.000f) ? (uint16_t)(note*1948+80) : (uint16_t)(note*2000);
}


int getRandomInt(int min, int max) {

    std::uniform_int_distribution<> distr(min, max); // Define range

    return distr(gen);
}

void testWriteMcp(I2CHandle::Config& config)
{
if(high)
        {
        auto i2cResult1 =  mcp.analogWrite(MCP4728::DAC_CH::A ,2000 , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        patch.Delay(1);
        auto i2cResult2 =  mcp.analogWrite(MCP4728::DAC_CH::B ,3000 , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        // patch.Delay(20);
        if (i2cResult1 != I2CHandle::Result::OK || i2cResult2 != I2CHandle::Result::OK) 
        {
            mcp.Init(config);
            patch.Print("i2c error high");
        }
        }else{
        auto i2cResult1 =  mcp.analogWrite(MCP4728::DAC_CH::A ,0 , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        patch.Delay(1);
        auto i2cResult2 =  mcp.analogWrite(MCP4728::DAC_CH::B ,0 , MCP4728::VREF::INTERNAL_2_8V, MCP4728::PWR_DOWN::NORMAL, MCP4728::GAIN::X1);
        // patch.Delay(20);
        if (i2cResult1 != I2CHandle::Result::OK || i2cResult2 != I2CHandle::Result::OK) 
        {
            mcp.Init(config);
            patch.Print("i2c error low");
        }
        }
        high=!high;
}

I2CHandle::Result WriteMCP_Voltage(I2CHandle& i2c ,uint16_t chA,uint16_t chB)
{
        output_buffer[0] = static_cast<uint8_t>(chA >> 8);
        output_buffer[1] = static_cast<uint8_t>(chA & 0xFF);
        output_buffer[2] = static_cast<uint8_t>(chB >> 8);
        output_buffer[3] = static_cast<uint8_t>(chB & 0xFF);
        return i2c.TransmitBlocking(0x60, output_buffer,4,10);
}

void writeVref(I2CHandle& i2c , I2CHandle::Config& config){
        output_buffer[0] = 0x60<<1;
        output_buffer[0] = static_cast<uint8_t> (0b10001111);
         I2CHandle::Result i2cResult_2= i2c.TransmitBlocking(0x60, output_buffer, 1, 10);

        if(i2cResult_2 == I2CHandle::Result::OK) {
            patch.PrintLine("OK TRANSMISSION vref");
        }else{
            patch.PrintLine("error TRANSMISSION vref");
        i2c.Init(config);
        }
}

void writeGain(I2CHandle& i2c , I2CHandle::Config& config){

        output_buffer[0] = 0x60<<1;
        output_buffer[0] = static_cast<uint8_t> (0b11000000);
         I2CHandle::Result i2cResult_2= i2c.TransmitBlocking(0x60, output_buffer, 1, 1000);

        if(i2cResult_2 == I2CHandle::Result::OK) {
            patch.PrintLine("OK TRANSMISSION gain");
        }else{
            patch.PrintLine("error TRANSMISSION gain");
        i2c.Init(config);
        }

}
        
