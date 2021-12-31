#include "GrooveBox.h"
#include <string.h>
#define SAMPLES_PER_BUFFER 128

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}
GrooveBox::GrooveBox(uint32_t *_color)
{
    for(int i=0;i<8;i++)
    {
        instruments[i].Init();
    }
    instruments[0].SetOscillator(MACRO_OSC_SHAPE_KICK);
    instruments[0].SetAHD(10, 20000, 0);
    instruments[1].SetOscillator(MACRO_OSC_SHAPE_SNARE);
    instruments[1].SetAHD(10, 20000, 0);
    instruments[2].SetOscillator(MACRO_OSC_SHAPE_CYMBAL);
    instruments[2].SetAHD(10, 100, 1000);
    instruments[3].SetOscillator(MACRO_OSC_SHAPE_CSAW);
    instruments[3].SetAHD(10, 1000, 8000);
    instruments[4].SetOscillator(MACRO_OSC_SHAPE_WAVE_PARAPHONIC);
    instruments[4].SetAHD(4000, 1000, 20000);
    memset(trigger, 0, 16*16);
    memset(notes, 0, 16*16);
    color = _color;
}

int16_t workBuffer[SAMPLES_PER_BUFFER];
int16_t workBuffer2[SAMPLES_PER_BUFFER];
static uint8_t sync_buffer[SAMPLES_PER_BUFFER];
int samplesPerStep = 44100/8;
int nextTrigger = 0;

void GrooveBox::Render(int16_t* buffer, size_t size)
{
    memset(workBuffer2, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);

    for(int v=0;v<6;v++)
    {
        int16_t pa = GetInstrumentParamA(v);
        int16_t pb = GetInstrumentParamB(v);
        memset(sync_buffer, 0, SAMPLES_PER_BUFFER);
        memset(workBuffer, 0, SAMPLES_PER_BUFFER);
        instruments[v].SetParameter(INSTRUMENT_PARAM_MACRO_MODULATION, pa);
        instruments[v].SetParameter(INSTRUMENT_PARAM_MACRO_TIMBRE, pb);
        instruments[v].Render(sync_buffer, workBuffer, SAMPLES_PER_BUFFER);
        // mix in the instrument
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            q15_t instrument = mult_q15(workBuffer[i], 0x5fff);
            workBuffer2[i] = add_q15(workBuffer2[i], instrument);
        }
    }

    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {
        int requestedNote = GetNote();
        if(requestedNote >= 0)
        {
            instruments[currentVoice].Strike();
            instruments[currentVoice].SetPitch((requestedNote << 7));
        }
        if(IsPlaying())
        {
            if(--nextTrigger < 0)
            {
                for(int v=0;v<6;v++)
                {
                    int requestedNote = GetTrigger(v, CurrentStep);
                    if(requestedNote >= 0)
                    {
                        instruments[v].SetPitch((requestedNote << 7));
                        instruments[v].Strike();
                    }
                }
                UpdateAfterTrigger(CurrentStep);
                CurrentStep = (++CurrentStep)%16;
                nextTrigger = samplesPerStep;
            }
        }
        int16_t* chan = (buffer+i*2);
        chan[0] = workBuffer2[i];
        chan[1] = workBuffer2[i];
    }
}
int GrooveBox::GetTrigger(uint voice, uint step)
{
    return trigger[voice][step]?notes[voice][step]+48:-1;
}
void GrooveBox::UpdateAfterTrigger(uint step)
{
    for(int i=0;i<16;i++)
    {
        int x = i%4;
        int y = i/4;
        int key = x+(y+1)*5;
        if(step == i)
        {
            color[key] = urgb_u32(250, 30, 80);
        }
        else
            color[key] = trigger[currentVoice][i]?urgb_u32(100, 60, 200):urgb_u32(0,0,0);
    }
}

bool GrooveBox::IsPlaying()
{
    return playing;
}
int GrooveBox::GetNote()
{
    int res = needsNoteTrigger;
    if(res >= 0)
        res += 48;
    needsNoteTrigger = -1;
    return res;
}
void GrooveBox::UpdateDisplay(ssd1306_t *p)
{
    ssd1306_clear(p);
    char str[32];
    if(soundSelectMode)
    {
        sprintf(str, "SOUND SELECT: %i", currentVoice);
        ssd1306_draw_string(p, 8, 8, 1, str);
    } else {
        int16_t a = instrumentParamA[currentVoice];
        sprintf(str, "A%i",a);
        ssd1306_draw_string(p, 16, 24, 1, str);
        int16_t b = instrumentParamB[currentVoice];
        sprintf(str, "B%i",b);
        ssd1306_draw_string(p, 48, 24, 1, str);
    }
}
void GrooveBox::OnAdcUpdate(uint8_t a, uint8_t b)
{
    instrumentParamA[currentVoice] = a;
    instrumentParamB[currentVoice] = b;
}
uint8_t GrooveBox::GetInstrumentParamA(int voice)
{
    return instrumentParamA[voice];
}
uint8_t GrooveBox::GetInstrumentParamB(int voice)
{
    return instrumentParamB[voice];
}

void GrooveBox::OnKeyUpdate(uint key, bool pressed)
{
    int x=key/5;
    int y=key%5;
    if(x<4 && y>0 && pressed)
    {
        
        int sequenceStep = x+(y-1)*4;
        if(soundSelectMode)
        {
            currentVoice = sequenceStep;
        }
        else if(!writing)
        {
            lastNotePlayed = needsNoteTrigger = sequenceStep;
        }
        else if(liveWrite)
        {
            bool setTrig = trigger[currentVoice][CurrentStep] = true;
            notes[currentVoice][CurrentStep] = sequenceStep;
            color[x+y*5] = urgb_u32(5, 3, 20);
        }
        else
        {
            bool setTrig = trigger[currentVoice][sequenceStep] = !trigger[currentVoice][sequenceStep];
            if(setTrig)
            {
                notes[currentVoice][sequenceStep] = lastNotePlayed;
            }
            color[x+y*5] = trigger[currentVoice][sequenceStep]?urgb_u32(5, 3, 20):urgb_u32(0,0,0);
        }
    }
    // play
    if(x==4 && y==3 && pressed)
    {
        if(holdingWrite)
        {
            liveWrite = true;
            if(!playing)
            {
                playing = true;
                CurrentStep = 0;
            }
        }
        else if(liveWrite)
        {
            liveWrite = false;
            writing = false;
            playing = true;
        }
        else
        {
            playing = !playing;
            if(playing)
                CurrentStep = 0;
            else
                UpdateAfterTrigger(-1);
        }
        if(liveWrite)
        {
            color[x+y*5] = urgb_u32(20, 0, 7);
        }
        else
        {
            color[x+y*5] = playing?urgb_u32(3, 20, 7):urgb_u32(0,0,0);
            if(!writing)
            {
                color[24] = 0;
            }
        }
    }
    // write
    if(x==4 && y==4)
    {
        holdingWrite = pressed;
        if(pressed)
        {
            liveWrite = false;
            writing = !writing;
            color[x+y*5] = writing?urgb_u32(20, 10, 12):urgb_u32(0,0,0);
        }
    }
    // sound select mode
    if(x==0 && y==0)
    {
        soundSelectMode = pressed;
    }
}
