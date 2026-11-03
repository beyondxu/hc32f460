"""Run the real print loop with a scripted SPI DMA/GPIO environment."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'source/main.c').read_bytes().decode('latin1')
start = source.index('void HC32_Realtime_Print_Engine(void)')
engine = source[start:source.index('void HC32_Standalone_Hardware_Test(void);', start)]
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "printer_control.h"
#define DISABLE 0
#define ENABLE 1
#define RESET 0
#define SET 1
#define PIN_RESET 0
#define PIN_SET 1
#define SYNC_PORT 1
#define SYNC_PIN 13
#define READY_PORT 2
#define READY_PIN 2
#define TPH_COLUMN_BYTES 100
#define __DMB() ((void)0)
static uint8_t u8SelfTestTxBuf[100], u8PrintBuffer[2][100];
static uint8_t u8CurrentBufIdx;
static int enDmaRxDone, ready, arms, fires, event_idx, stop_after, event_count;
static uint8_t *dma_buffer;
static jmp_buf stop;
struct event {int sync, done;};
static struct event events[32];
static int mode, power;
static void TPH_M0333N_Power(int state) {power=state;}
static void Slave_Arm_SPI_DMA(uint8_t *tx, uint8_t *rx, uint16_t len) {
    assert(len==100); enDmaRxDone=RESET; dma_buffer=rx; ++arms;
}
static void GPIO_SetPins(int port, int pin) {
    ready=1;
    if (arms==stop_after) longjmp(stop,1);
}
static void GPIO_ResetPins(int port, int pin) {ready=0;}
static int GPIO_ReadInputPins(int port, int pin) {
    assert(event_idx < event_count);
    struct event e=events[event_idx++];
    if(e.done) {
        memset(dma_buffer,arms,100);
        if (mode != 2 && (arms==1 || arms==4)) {
            memset(dma_buffer,0,100); dma_buffer[0]=PRINTER_CONTROL_TAG;
            dma_buffer[1]=arms==1?PRINTER_CONTROL_ON:PRINTER_CONTROL_OFF;
            memcpy(dma_buffer+2,PRINTER_CONTROL_MAGIC,PRINTER_CONTROL_MAGIC_LEN);
            if(mode==3) dma_buffer[99]=1; /* malformed control must not power on */
        }
        if(mode==4 && arms==2) dma_buffer[50]=0xC0; /* reserved data bits */
        enDmaRxDone=SET;
    }
    return e.sync;
}
static void TPH_FireOneColumn(const uint8_t *p) {
    assert(ready==0 && enDmaRxDone==SET && power==ENABLE);
    assert(p==u8PrintBuffer[u8CurrentBufIdx]);
    for(int i=0;i<100;i++) assert(p[i]==arms);
    ++fires;
}
ENGINE
static void run(int scenario) {
    mode=scenario; arms=fires=event_idx=0;
    if(mode==1) {
        events[0]=(struct event){0,0};
        events[1]=(struct event){1,0};
        events[2]=(struct event){1,0};
        event_count=3; stop_after=2;
    } else {
        for(int frame=0;frame<5;frame++) {
            events[2*frame]=(struct event){0,1};
            events[2*frame+1]=(struct event){1,0};
        }
        event_count=10; stop_after=6;
    }
    if(!setjmp(stop)) HC32_Realtime_Print_Engine();
    assert(fires==(mode==0?2:mode==4?1:0));
    assert(arms==stop_after && ready==1);
    assert(power==DISABLE);
}
int main(void) {
    for(int i=0;i<5;i++) run(i);
    puts("PASS: power controls, DMA completion, alternating buffers, cancel, unpowered/invalid data rejection");
}
'''.replace('ENGINE', engine)
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    (root/'test.c').write_text(harness)
    subprocess.run(['cc', '-std=c99', '-I', str(Path(__file__).resolve().parents[1]/'source'), str(root/'test.c'), '-o', str(root/'test')], check=True)
    subprocess.run([str(root/'test')], check=True)
