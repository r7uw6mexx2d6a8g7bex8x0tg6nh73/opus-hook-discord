#pragma once
#include <cstring> // For memset

// FreeverbReverb class - based on Freeverb algorithm
class FreeverbReverb {
private:
    // Constants for the Freeverb algorithm
    static constexpr int NUM_COMBS = 8;
    static constexpr int NUM_ALLPASSES = 4;
    static constexpr float FIXED_GAIN = 0.015f;
    static constexpr float SCALE_WET = 3.0f;
    static constexpr float SCALE_DRY = 2.0f;
    static constexpr float SCALE_DAMP = 0.4f;
    static constexpr float SCALE_ROOM = 0.28f;
    static constexpr float OFFSET_ROOM = 0.7f;
    static constexpr float INITIAL_ROOM = 0.5f;
    static constexpr float INITIAL_DAMP = 0.5f;
    static constexpr float INITIAL_WET = 1.0f / SCALE_WET;
    static constexpr float INITIAL_DRY = 0.0f;
    static constexpr float INITIAL_WIDTH = 1.0f;
    static constexpr float INITIAL_MODE = 0.0f;
    static constexpr float FREEZE_MODE = 0.5f;
    
    // Internal buffer sizes (adjust these if needed for performance)
    static constexpr int STEREO_SPREAD = 23;
    
    // Comb filter tunings for 44.1kHz (will be adjusted for actual sample rate)
    static constexpr int COMB_TUNING_L[NUM_COMBS] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr int ALLPASS_TUNING_L[NUM_ALLPASSES] = {556, 441, 341, 225};
    
    // Comb filter implementation
    struct Comb {
        float* buffer = nullptr;
        int bufsize = 0;
        int bufidx = 0;
        float feedback = 0.0f;
        float filterstore = 0.0f;
        float damp1 = 0.0f;
        float damp2 = 0.0f;
        
        ~Comb() {
            if (buffer) delete[] buffer;
            buffer = nullptr;
        }
        
        void init(int size) {
            bufsize = size;
            bufidx = 0;
            filterstore = 0.0f;
            
            if (buffer) delete[] buffer;
            buffer = new float[size]();  // Zero-initialize
        }
        
        inline float process(float input) {
            float output = buffer[bufidx];
            filterstore = (output * damp2) + (filterstore * damp1);
            
            buffer[bufidx] = input + (filterstore * feedback);
            if (++bufidx >= bufsize) bufidx = 0;
            
            return output;
        }
        
        void mute() {
            filterstore = 0.0f;
            if (buffer) {
                memset(buffer, 0, bufsize * sizeof(float));
            }
        }
        
        void setdamp(float val) {
            damp1 = val;
            damp2 = 1.0f - val;
        }
        
        void setfeedback(float val) {
            feedback = val;
        }
    };
    
    // Allpass filter implementation
    struct Allpass {
        float* buffer = nullptr;
        int bufsize = 0;
        int bufidx = 0;
        float feedback = 0.5f;
        
        ~Allpass() {
            if (buffer) delete[] buffer;
            buffer = nullptr;
        }
        
        void init(int size) {
            bufsize = size;
            bufidx = 0;
            
            if (buffer) delete[] buffer;
            buffer = new float[size]();  // Zero-initialize
        }
        
        inline float process(float input) {
            float output = buffer[bufidx];
            buffer[bufidx] = input + (output * feedback);
            if (++bufidx >= bufsize) bufidx = 0;
            
            return output - input;
        }
        
        void mute() {
            if (buffer) {
                memset(buffer, 0, bufsize * sizeof(float));
            }
        }
        
        void setfeedback(float val) {
            feedback = val;
        }
    };
    
    // Filter arrays
    Comb combL[NUM_COMBS];
    Comb combR[NUM_COMBS];
    Allpass allpassL[NUM_ALLPASSES];
    Allpass allpassR[NUM_ALLPASSES];
    
    // Control parameters
    float gain = FIXED_GAIN;
    float roomsize = INITIAL_ROOM * SCALE_ROOM + OFFSET_ROOM;
    float damp = INITIAL_DAMP * SCALE_DAMP;
    float wet = INITIAL_WET * SCALE_WET;
    float wet1 = INITIAL_WIDTH/2.0f * wet;
    float wet2 = (1.0f-INITIAL_WIDTH)/2.0f * wet;
    float dry = INITIAL_DRY * SCALE_DRY;
    float width = INITIAL_WIDTH;
    float mode = INITIAL_MODE;
    
    // Sample rate and other state
    int sampleRate = 48000;
    int channels = 2;
    bool initialized = false;
    float sampleRateRatio = 1.0f;  // Used to adjust buffer sizes for different sample rates
    
    // Private method to update wet values
    void updateWetValues();
    
public:
    FreeverbReverb() = default;
    ~FreeverbReverb();
    
    void init(int rate, int numChannels);
    void updateParams(float size, float dampening, float reverbWidth, float mix);
    void process(float* inBuffer, int numSamples);
    void setRoomSize(float value);
    void setDamp(float value);
    void setWet(float value);
    void setDry(float value);
    void setWidth(float value);
    void setFreeze(bool freezeMode);
    void mute();
};

// Global reverb processor declaration
extern FreeverbReverb reverbProcessor; 