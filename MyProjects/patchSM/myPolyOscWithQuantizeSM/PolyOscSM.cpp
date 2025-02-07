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
bool V_REF = (bool)MCP4728::VREF::INTERNAL_2_8V;
bool DAC_GAIN = (bool)MCP4728::GAIN::X2;
DaisyPatchSM patch;
MCP4728 mcp;

// Create a random device and seed it
std::random_device rd;
std::mt19937 gen(rd()); // Mersenne Twister generator
#define NUM_OSCILLATORS 1
Oscillator osc[NUM_OSCILLATORS];
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
float coma = (1/9)*tone;
std::vector<float> rastComa = { 0.0, 9*coma,16*coma, 22*coma, 31*coma, 40*coma, 47*coma, 1.0,1 + 9*coma,1 + 16*coma,1 + 22*coma,1 +31*coma,1 +40*coma,1 + 47*coma, 2.0 };

//IOs
Switch       button;
//notes vectors of maqams
std::vector<float> sika_voltages_one_octave = {0.000f, 0.087f, 0.252f, 0.418f, 0.586f, 0.671f, 0.839f, 1.000f, 1.087f, 1.252f, 1.418f, 1.586f, 1.671f, 1.839f, 2.000f};

std::vector<float> huzam_voltages_one_octave = { 0.0, 3/24.f, 7/24.f, 9/24.f, 15/24.f, 17/24.f, 21/24.f, 1.0, 27/24.f, 31/24.f, 33/24.f, 38/24.f, 41/24.f, 45/24.f, 2.0 };

std::vector<float> iraq = { 
    0.0, 0.75f*tone, 1.75f*tone, 2.5f*tone, 3.25f*tone, 4.25f*tone, 5.25f*tone, 
    1.0, 1 + 0.75f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.5f*tone,1 + 5.25f*tone, 
    2.0, 2 + 0.75f*tone,1 + 1.75f*tone,2 + 2.5f*tone,2 + 3.25f*tone,2 + 4.25f*tone,2 + 5.25f*tone, 
    3.0, 3 + 0.75f*tone,3 + 1.75f*tone,3 + 2.5f*tone,3 + 3.5f*tone,3 + 4.5f*tone,3 + 5.25f*tone, 4.0   
    };
std::vector<float> huseyni = { 
    0.0, 0.75f*tone, 1.5f*tone, 2.5f*tone, 3.5f*tone, 4.25f*tone, 5.f*tone, 
    1.0, 1 + 0.75f*tone,1 + 1.5f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.25f*tone,1 + 5.f*tone, 
    2.0, 2 + 0.75f*tone,2 + 1.5f*tone,2 + 2.5f*tone,2 + 3.5f*tone,2 + 4.25f*tone,2 + 5.f*tone, 
    3.0, 3 + 0.75f*tone,3 + 1.5f*tone,3 + 2.5f*tone,3 + 3.5f*tone,3 + 4.25f*tone,3 + 5.f*tone, 4.0
    };
std::vector<float> rast = { 
    0.0, 1.f*tone, 1.75f*tone, 2.5f*tone, 3.5f*tone, 4.5f*tone, 5.25f*tone, 
    1, 1 + 1.f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.5f*tone,1 + 5.25f*tone, 
    2, 2 + 1.f*tone,2 + 1.75f*tone,2 + 2.5f*tone,2 + 3.5f*tone,2 + 4.5f*tone,2 + 5.25f*tone, 
    3, 3 + 1.f*tone,3 + 1.75f*tone,3 + 2.5f*tone,3 + 3.5f*tone,3 + 4.5f*tone,3 + 5.25f*tone, 4
    };
std::vector<float> hijaz = { 
    0.0, 0.5f*tone, 2.f*tone, 2.5f*tone, 3.5f*tone, 4.25f*tone, 5.f*tone, 
    1, 1 + 0.5f*tone, 1 + 2.f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.25f*tone,1 + 5.f*tone, 
    2, 2 + 0.5f*tone, 2 + 2.f*tone,2 + 2.5f*tone,2 + 3.5f*tone,2 + 4.25f*tone,2 + 5.f*tone,
    3, 1 + 0.5f*tone, 3 + 2.f*tone,3 + 2.5f*tone,3 + 3.5f*tone,3 + 4.25f*tone,3 + 5.f*tone, 4
    };
std::vector<float> suznak = { 0.0, 1.f*tone, 1.75f*tone, 2.5f*tone, 3.5f*tone, 4.0f*tone, 5.5f*tone, 1.0,1 + 1.f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.0f*tone,1 + 5.5f*tone, 2.0 };
std::vector<float> huzam = { 0.0, 0.75f*tone, 1.75f*tone, 2.25f*tone, 3.75f*tone, 4.25f*tone, 5.25f*tone, 1.0,1 + 0.75f*tone,1 + 1.75f*tone,1 + 2.25f*tone,1 + 3.75f*tone,1 + 4.25f*tone,1 + 5.25f*tone, 2.0 };
std::vector<float> equal_Temprament = { 
    0.0, 1.f*equalDevision, 2.f*equalDevision, 3.f*equalDevision, 4.f*equalDevision, 5.f*equalDevision, 6.f*equalDevision, 
    1, 1 + 1.f*equalDevision,1 + 2.f*equalDevision,1 + 3.f*equalDevision,1 + 4.f*equalDevision,1 + 5.f*equalDevision,1 + 6.f*equalDevision, 
    2, 2 + 1.f*equalDevision,2 + 2.f*equalDevision,2 + 3.f*equalDevision,2 + 4.f*equalDevision,2 + 5.f*equalDevision,2 + 6.f*equalDevision, 
    3, 3 + 1.f*equalDevision,3 + 2.f*equalDevision,3 + 3.f*equalDevision,3 + 4.f*equalDevision,3 + 5.f*equalDevision,3 + 6.f*equalDevision, 4
    };
std::vector<std::vector<float>> scales{huseyni,hijaz,rast,iraq,/*,suznak,huzam*/};
uint16_t scaleIndex = 0;
//knobs
float cvInArr[2];

//for oscillator use
float frequencies[4] = {0};
#define DAC_SCALE 0.98782

//LFO
Oscillator lfoOscillator;
Parameter  lfoFreqCtrl;
Parameter  lfoAmpCtrl;
uint32_t lastTrigger = 0;
// float bpm = 0;



int getRandomInt(int min, int max);
void UpdateControls();
void CalculateClosestNote(float* cvIn, int* indexes);
uint16_t ConvertNoteToDacValue(float note);
//I2C DAC helpers
void testWriteMcp(I2CHandle::Config& config);
I2CHandle::Result WriteMCP_Voltage(I2CHandle& i2c ,uint16_t chA,uint16_t chB);
void writeVref(I2CHandle& i2c , I2CHandle::Config& config);
void writeGain(I2CHandle& i2c , I2CHandle::Config& config);

void SetupOsc(float samplerate);
void SetupWaveNames();

//audio callback
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    UpdateControls();
    // for(size_t i = 0; i < size; i++)
    // {
    //     out[0][i] = in[0][i]; /**< Copy the left input to the left output */
    //     out[1][i] = in[1][i]; /**< Copy the right input to the right output */
    // }

    //LFO
    if(patch.gate_in_1.Trig()) {
        auto now = patch.system.GetNow();
        float t = (float)(now - lastTrigger)/1000.f;
        float f = 1.f/(float)t;
        lfoOscillator.SetFreq(f);
        lastTrigger = now;
    }
    
    if(patch.gate_in_2.Trig()) lfoOscillator.Reset();
    for(size_t i = 0; i < size; i++)
    {
        float sig = osc[0].Process();
        out[0][i]  = sig;
        out[1][i]  = sig;

        patch.WriteCvOut(CV_OUT_1, lfoOscillator.Process()+1);
        patch.WriteCvOut(CV_OUT_2, lfoOscillator.Process()+1);
    }

    
 

   
}
int main(void)
{
    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    //patch.StartLog(true);
    samplerate = patch.AudioSampleRate();
    button.Init(patch.D6);
    //oscillator
    waveform   = 0;
    SetupOsc(samplerate);
    SetupWaveNames();

    //LFO
    lfoOscillator.Init(samplerate);
    lfoOscillator.SetAmp(1);
    lfoOscillator.SetWaveform(lfoOscillator.WAVE_SIN);
    lfoOscillator.SetFreq(1);//hz
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

    //start
    patch.StartAdc();

    //lfo
    lastTrigger = patch.system.GetNow();
    // DacHandle::Config dac_config;
    // dac_config.mode     = DacHandle::Mode::DMA;
    // dac_config.bitdepth = DacHandle::BitDepth::BITS_12; /**< Sets the output value to 0-4095 */
    // dac_config.chn               = DacHandle::Channel::BOTH;
    // dac_config.buff_state        = DacHandle::BufferState::DISABLED;
    // dac_config.target_samplerate = 48000;
    // patch.dac.Init(dac_config);
    patch.StartDac();
    patch.StartAudio(AudioCallback);
    int lastSendChannelA = 0, lastSendChannelB = 0;
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
           
        
        uint16_t cvOut1 = ConvertNoteToDacValue(scales[scaleIndex][indexes[0]]);
        uint16_t cvOut2 = ConvertNoteToDacValue(scales[scaleIndex][indexes[1]]);
        
        //patch.PrintLine("cv2 out value: %d",  cvOut2);
        if(cvOut1 != lastSendChannelA)
        {
            lastSendChannelA = cvOut1;
            //prepare internal oscillator frequency
            frequencies[0] = powf(2.f, scales[scaleIndex][indexes[0]]+0.25) * 55; //Hz

            // float dacOut =(scales[scaleIndex][indexes[0]]-0.01)/1.006;
            // patch.WriteCvOut(CV_OUT_1, dacOut);

            auto i2cResult = WriteMCP_Voltage(i2c_handle,cvOut1,lastSendChannelB);
            if (i2cResult != I2CHandle::Result::OK) i2c_handle.Init(i2c_config);
        }

        if(cvOut2 != lastSendChannelB)
        {
            lastSendChannelB = cvOut2;

            // float dacOut =(scales[scaleIndex][indexes[1]]-0.01)/1.006;
            // patch.WriteCvOut(CV_OUT_2, dacOut);

            auto i2cResult = WriteMCP_Voltage(i2c_handle,lastSendChannelA,cvOut2);
            if (i2cResult != I2CHandle::Result::OK) i2c_handle.Init(i2c_config);
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
    //return (note == 2.000f) ? (uint16_t)(note*1948+80) : (uint16_t)(note*2000);
    // return  (uint16_t)(note*2000);
    if(DAC_GAIN == (bool)MCP4728::GAIN::X2){
       return (uint16_t)(note*1000);
    }
    else{
        return (uint16_t)(note*2000);
    }
}

void UpdateControls()
{
    patch.ProcessAllControls();
    osc[0].SetFreq(frequencies[0]);
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
        auto writeVREG_CMD = 0b10000000;
        
        output_buffer[0] = static_cast<uint8_t> (writeVREG_CMD | V_REF << 3 | V_REF << 2 | V_REF << 1 | V_REF ); //command --> xxxx channels --> yyyy
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
        auto writeGAIN_CMD = 0b11000000;
        output_buffer[0] = static_cast<uint8_t> (writeGAIN_CMD | DAC_GAIN << 3 | DAC_GAIN << 2 | DAC_GAIN << 1 | DAC_GAIN ); //xxxx1111 1--> make channel gain x2 0 -->make channel gain x1 
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
    for(int i = 0; i < NUM_OSCILLATORS; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(.5);
        osc[0].SetWaveform(Oscillator::WAVE_SIN);
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