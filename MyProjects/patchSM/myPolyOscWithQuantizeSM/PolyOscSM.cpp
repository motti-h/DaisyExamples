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

#define DAC_MAX 4095.f
#define SCALE_CV 819.2 

DaisyPatchSM patch;
MCP4728 mcp;

// Create a random device and seed it
std::random_device rd;
std::mt19937 gen(rd()); // Mersenne Twister generator

Oscillator osc[3];
std::string waveNames[5];
int waveform;
int final_wave;
float testval;
uint16_t errorCount = 0;
bool high=false;
//i2c
#define BUFF_SIZE 8
static uint8_t DMA_BUFFER_MEM_SECTION output_buffer[BUFF_SIZE];
float tone = (1.f/6.f), equalDevision = (1.f/7.f);
//IOs
Switch       button;
//notes vectors of maqams
std::vector<float> sika_voltages_one_octave = {0.000f, 0.087f, 0.252f, 0.418f, 0.586f, 0.671f, 0.839f, 1.000f, 1.087f, 1.252f, 1.418f, 1.586f, 1.671f, 1.839f, 2.000f};

std::vector<float> huzam_voltages_one_octave = { 0.0, 3/24.f, 7/24.f, 9/24.f, 15/24.f, 17/24.f, 21/24.f, 1.0, 27/24.f, 31/24.f, 33/24.f, 38/24.f, 41/24.f, 45/24.f, 2.0 };


std::vector<float> huseyni = { 0.0, 0.75f*tone, 1.5f*tone, 2.5f*tone, 3.5f*tone, 4.25f*tone, 5.f*tone, 1.0,1 + 0.75f*tone,1 + 1.5f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.25f*tone,1 + 5.f*tone, 2.0 };
std::vector<float> rast = { 0.0, 1.f*tone, 1.75f*tone, 2.5f*tone, 3.5f*tone, 4.5f*tone, 5.25f*tone, 1,1 + 1.f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.5f*tone,1 + 5.25f*tone, 2};
std::vector<float> hijaz = { 0.0, 0.5f*tone, 2.f*tone, 2.5f*tone, 3.5f*tone, 4.25f*tone, 5.f*tone, 1,1 + 0.5f*tone,1 + 2.f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.25f*tone,1 + 5.f*tone, 2};
std::vector<float> suznak = { 0.0, 1.f*tone, 1.75f*tone, 2.5f*tone, 3.5f*tone, 4.0f*tone, 5.5f*tone, 1.0,1 + 1.f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.0f*tone,1 + 5.5f*tone, 2.0 };
std::vector<float> huzam = { 0.0, 0.75f*tone, 1.75f*tone, 2.25f*tone, 3.75f*tone, 4.25f*tone, 5.25f*tone, 1.0,1 + 0.75f*tone,1 + 1.75f*tone,1 + 2.25f*tone,1 + 3.75f*tone,1 + 4.25f*tone,1 + 5.25f*tone, 2.0 };
std::vector<float> equal_Temprament = { 0.0, 1.f*equalDevision, 2.f*equalDevision, 3.f*equalDevision, 4.f*equalDevision, 5.f*equalDevision, 6.f*equalDevision, 1,1 + 1.f*equalDevision,1 + 2.f*equalDevision,1 + 3.f*equalDevision,1 + 4.f*equalDevision,1 + 5.f*equalDevision,1 + 6.f*equalDevision, 2};
std::vector<std::vector<float>> scales{huseyni,rast,hijaz,suznak,huzam};
uint16_t scaleIndex = 0;
//knobs
float cvInArr[2];

//for oscillator use
float frequencies[4] = {0};

int getRandomInt(int min, int max);
void UpdateControls();
void CalculateClosestNote(float* cvIn, int* indexes);
uint16_t ConvertNoteToDacValue(float note);
//I2C DAC helpers
void testWriteMcp(I2CHandle::Config& config);
I2CHandle::Result WriteMCP_Voltage(I2CHandle& i2c ,uint16_t chA,uint16_t chB);
void writeVref(I2CHandle& i2c , I2CHandle::Config& config);
void writeGain(I2CHandle& i2c , I2CHandle::Config& config);

//audio callback
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
   
}
int main(void)
{
    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    //patch.StartLog(true);
    samplerate = patch.AudioSampleRate();
    button.Init(patch.D6);
    //oscillator
    // waveform   = 0;
    // final_wave = Oscillator::WAVE_POLYBLEP_TRI;
    // SetupOsc(samplerate);
    // SetupWaveNames();

    //i2c configuration
    I2CHandle::Config i2c_config;
    i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    i2c_config.speed  = I2CHandle::Config::Speed::I2C_100KHZ;
    i2c_config.mode   = I2CHandle::Config::Mode::I2C_MASTER;
    i2c_config.address = 0x60;
    i2c_config.pin_config.scl  = DaisyPatchSM::B7;
    i2c_config.pin_config.sda  = DaisyPatchSM::B8;
    // initialise DAC peripheral 
    I2CHandle i2c_handle;
    i2c_handle.Init(i2c_config);
    patch.Delay(50);
    writeVref(i2c_handle,i2c_config);
    patch.Delay(50);
    writeGain(i2c_handle,i2c_config);
    patch.Delay(50);

    patch.PrintLine("DAC init OK");
//

    //start
    patch.StartAdc();
    patch.StartDac();
    patch.StartAudio(AudioCallback);
    int lastSendChannelA = 0, lastSendChannelB = 0;
    int max=0;
    while(1)
    {
        button.Debounce();
        if(button.RisingEdge()){
            scaleIndex++;
            if(scaleIndex >= scales.size()){
                scaleIndex = 0;
            }
        }
        cvInArr[0] = (patch.GetAdcValue(CV_1)+1.0)*2;
        cvInArr[1] = (patch.GetAdcValue(CV_2)+1.0)*2; 
        //patch.PrintLine("Print a float value: %d",  (int)(cvInArr[0]*100));
        // Quantize to semitones
        int indexes[2] = {0,0};
        CalculateClosestNote(cvInArr,indexes);    
        //frequencies[0] = powf(2.f, closestNotes[0]) * 55; //Hz
        uint16_t cvOut1 = ConvertNoteToDacValue(scales[scaleIndex][indexes[0]]);
        uint16_t cvOut2 = ConvertNoteToDacValue(scales[scaleIndex][indexes[1]]);
        //patch.PrintLine("cv2 out value: %d",  cvOut2);
        if(cvOut1 != lastSendChannelA)
        {
            lastSendChannelA = cvOut1;
            auto i2cResult = WriteMCP_Voltage(i2c_handle,cvOut1,lastSendChannelB);
            if (i2cResult != I2CHandle::Result::OK) 
            {
            i2c_handle.Init(i2c_config);
                //patch.Print("i2c error");
            }
        }

        if(cvOut2 != lastSendChannelB)
        {
            lastSendChannelB = cvOut2;
            auto i2cResult = WriteMCP_Voltage(i2c_handle,lastSendChannelA,cvOut2);
            if (i2cResult != I2CHandle::Result::OK) 
            {
            i2c_handle.Init(i2c_config);
                //patch.Print("i2c error");
            }
        }
        patch.Delay(2);
        // max++;
        // max = max%4;  
        // patch.WriteCvOut(CV_OUT_BOTH, (float)max);
    }
}



void CalculateClosestNote(float* cvIn, int* indexes){
    float minDiff1 = cvIn[0], minDiff2 = cvIn[1];

    for (auto index = 0; index < equal_Temprament.size(); ++index) 
    {
        float diff1 = std::abs(equal_Temprament[index] - cvIn[0]), diff2 = std::abs(equal_Temprament[index] - cvIn[1]);
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

void UpdateControls()
{
    patch.ProcessAllControls();
    // osc[0].SetFreq(frequencies[0]);
    // osc[0].SetWaveform(Oscillator::WAVE_SAW);
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