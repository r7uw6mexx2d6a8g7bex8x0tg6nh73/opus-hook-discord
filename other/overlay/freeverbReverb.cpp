#include "freeverbReverb.hpp"
#include <cstring> // For memset
#include <cmath>
#include <algorithm>

// Global instance declaration
FreeverbReverb reverbProcessor;

// Comb filter implementation
FreeverbReverb::Comb::~Comb() {
    delete[] buffer;
}

void FreeverbReverb::Comb::init(int size) {
    bufsize = size;
    buffer = new float[size];
    bufidx = 0;
    memset(buffer, 0, size * sizeof(float));
}

inline float FreeverbReverb::Comb::process(float input) {
    float output = buffer[bufidx];
    filterstore = (output * damp2) + (filterstore * damp1);
    buffer[bufidx] = input + (filterstore * feedback);
    
    if (++bufidx >= bufsize) {
        bufidx = 0;
    }
    
    return output;
}

void FreeverbReverb::Comb::mute() {
    for (int i = 0; i < bufsize; i++) {
        buffer[i] = 0.0f;
    }
    filterstore = 0.0f;
}

void FreeverbReverb::Comb::setdamp(float val) {
    damp1 = val;
    damp2 = 1.0f - val;
}

void FreeverbReverb::Comb::setfeedback(float val) {
    feedback = val;
}

// Allpass filter implementation
FreeverbReverb::Allpass::~Allpass() {
    delete[] buffer;
}

void FreeverbReverb::Allpass::init(int size) {
    bufsize = size;
    buffer = new float[size];
    bufidx = 0;
    memset(buffer, 0, size * sizeof(float));
}

inline float FreeverbReverb::Allpass::process(float input) {
    float output;
    float bufout;
    
    bufout = buffer[bufidx];
    output = -input + bufout;
    buffer[bufidx] = input + (bufout * feedback);
    
    if (++bufidx >= bufsize) {
        bufidx = 0;
    }
    
    return output;
}

void FreeverbReverb::Allpass::mute() {
    for (int i = 0; i < bufsize; i++) {
        buffer[i] = 0.0f;
    }
}

void FreeverbReverb::Allpass::setfeedback(float val) {
    feedback = val;
}

// FreeverbReverb implementation
FreeverbReverb::~FreeverbReverb() {
    // Destructor will call destructors for Comb and Allpass arrays
}

void FreeverbReverb::init(int rate, int numChannels) {
    // Set state
    sampleRate = rate;
    channels = numChannels;
    initialized = false;
    
    // Calculate sample rate ratio compared to 44.1kHz
    sampleRateRatio = (float)sampleRate / 44100.0f;
    
    try {
        // Initialize comb filters with adjusted sizes for sample rate
        for (int i = 0; i < NUM_COMBS; i++) {
            int adjustedSize = (int)(COMB_TUNING_L[i] * sampleRateRatio);
            if (adjustedSize < 10) adjustedSize = 10;  // Safety
            
            combL[i].init(adjustedSize);
            combR[i].init(adjustedSize + STEREO_SPREAD);
        }
        
        // Initialize allpass filters with adjusted sizes for sample rate
        for (int i = 0; i < NUM_ALLPASSES; i++) {
            int adjustedSize = (int)(ALLPASS_TUNING_L[i] * sampleRateRatio);
            if (adjustedSize < 10) adjustedSize = 10;  // Safety
            
            allpassL[i].init(adjustedSize);
            allpassR[i].init(adjustedSize + STEREO_SPREAD);
            
            allpassL[i].setfeedback(0.5f);
            allpassR[i].setfeedback(0.5f);
        }
        
        // Set default parameters
        updateParams(0.8f, 0.2f, 1.0f, 0.5f);
        mute();
        
        initialized = true;
    }
    catch (...) {
        initialized = false;
    }
}

// Update all reverb parameters at once
void FreeverbReverb::updateParams(float size, float dampening, float reverbWidth, float mix) {
    if (!initialized) return;
    
    setRoomSize(size);
    setDamp(dampening);
    setWidth(reverbWidth);
    setWet(mix);
    setDry(1.0f - mix);
}

// Process a block of audio
void FreeverbReverb::process(float* inBuffer, int numSamples) {
    if (!initialized || !inBuffer || numSamples <= 0) return;
    
    // If mono
    if (channels == 1) {
        for (int i = 0; i < numSamples; i++) {
            float input = inBuffer[i] * gain;
            float outL = 0.0f;
            float outR = 0.0f;
            
            // Process comb filters in parallel
            for (int j = 0; j < NUM_COMBS; j++) {
                outL += combL[j].process(input);
                outR += combR[j].process(input);
            }
            
            // Process allpass filters in series
            for (int j = 0; j < NUM_ALLPASSES; j++) {
                outL = allpassL[j].process(outL);
                outR = allpassR[j].process(outR);
            }
            
            // Calculate stereo output
            inBuffer[i] = outL * wet1 + outR * wet2 + inBuffer[i] * dry;
        }
    }
    // If stereo
    else if (channels >= 2) {
        for (int i = 0; i < numSamples; i++) {
            float inputL = inBuffer[i*2] * gain;
            float inputR = inBuffer[i*2+1] * gain;
            float outL = 0.0f;
            float outR = 0.0f;
            
            // Process comb filters in parallel
            for (int j = 0; j < NUM_COMBS; j++) {
                outL += combL[j].process(inputL);
                outR += combR[j].process(inputR);
            }
            
            // Process allpass filters in series
            for (int j = 0; j < NUM_ALLPASSES; j++) {
                outL = allpassL[j].process(outL);
                outR = allpassR[j].process(outR);
            }
            
            // Calculate stereo output with cross-feed
            float outL2 = outL * wet1 + outR * wet2;
            float outR2 = outR * wet1 + outL * wet2;
            
            inBuffer[i*2] = outL2 + inputL * dry;
            inBuffer[i*2+1] = outR2 + inputR * dry;
        }
    }
}

// Set room size (affects feedback of comb filters)
void FreeverbReverb::setRoomSize(float value) {
    if (!initialized) return;
    
    roomsize = value * SCALE_ROOM + OFFSET_ROOM;
    
    for (int i = 0; i < NUM_COMBS; i++) {
        combL[i].setfeedback(roomsize);
        combR[i].setfeedback(roomsize);
    }
}

// Set damping factor
void FreeverbReverb::setDamp(float value) {
    if (!initialized) return;
    
    damp = value * SCALE_DAMP;
    
    for (int i = 0; i < NUM_COMBS; i++) {
        combL[i].setdamp(damp);
        combR[i].setdamp(damp);
    }
}

// Set wet level (reverb amount)
void FreeverbReverb::setWet(float value) {
    if (!initialized) return;
    
    wet = value * SCALE_WET;
    updateWetValues();
}

// Set dry level (original signal)
void FreeverbReverb::setDry(float value) {
    if (!initialized) return;
    
    dry = value * SCALE_DRY;
}

// Set stereo width
void FreeverbReverb::setWidth(float value) {
    if (!initialized) return;
    
    width = value;
    updateWetValues();
}

// Set freezing mode (infinite sustain)
void FreeverbReverb::setFreeze(bool freezeMode) {
    if (!initialized) return;
    
    if (freezeMode) {
        roomsize = 1.0f;
        damp = 0.0f;
        
        for (int i = 0; i < NUM_COMBS; i++) {
            combL[i].setfeedback(1.0f);
            combR[i].setfeedback(1.0f);
            combL[i].setdamp(0.0f);
            combR[i].setdamp(0.0f);
        }
    }
    else {
        // Restore previous values
        for (int i = 0; i < NUM_COMBS; i++) {
            combL[i].setfeedback(roomsize);
            combR[i].setfeedback(roomsize);
            combL[i].setdamp(damp);
            combR[i].setdamp(damp);
        }
    }
}

// Mute/reset all internal buffers
void FreeverbReverb::mute() {
    if (!initialized) return;
    
    for (int i = 0; i < NUM_COMBS; i++) {
        combL[i].mute();
        combR[i].mute();
    }
    
    for (int i = 0; i < NUM_ALLPASSES; i++) {
        allpassL[i].mute();
        allpassR[i].mute();
    }
}

// Update wet1 and wet2 values based on width
void FreeverbReverb::updateWetValues() {
    wet1 = wet * (width/2.0f + 0.5f);
    wet2 = wet * ((1.0f-width)/2.0f);
} 