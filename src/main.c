#include <stdio.h>
#include <string.h>
#include "si5351.h"
#include "i2c.h"
#include "ssd1306.h"
#include "ec11.h"
#include "utils.h"
#include "flash.h"



#define CONFIG_MAGIC 0x12345678
#define BACK_NAME "<--"
#define N_CHANNELS 3

typedef void (*callback_t)(void);

typedef struct {
    int frequency;
    int strength;
    int power;
} Channel;

typedef struct{
    uint32_t magic;
    Channel channels[N_CHANNELS];
} Config;

typedef struct MenuItem {
    const char *name;
    struct MenuItem *parent;
    struct MenuItem *children;
    uint8_t num_children;
    void (*action)(void); // NULL if submenu
    void (*render)(void);
} MenuItem;

void si5351Restart();
void renderFreq();
void renderAmp();
void renderPower();
void selectChannel();
void configWrite();
void actionFactorMHZ();
void actionFactorKHZ();
void actionFactorHZ();
void actionSelectFreqBack();
void renderSelectFreq();
void actionAmp();
void actionPower();
void renderDash();
void actionSave();
void drawMenu();

extern MenuItem mainMenu;
extern MenuItem commandMenuItems[];
extern MenuItem freqMenuItems[];
extern MenuItem dashMenuItems[];
extern MenuItem selectFreqMenuItems[];
extern MenuItem powerMenuItems[];



Channel channels[N_CHANNELS]={
    {.frequency = 1000000, .power = SI5351_DRIVE_STRENGTH_8MA, .power = 0},
    {.frequency = 500000, .power = SI5351_DRIVE_STRENGTH_8MA, .power = 0},
    {.frequency = 1000, .power = SI5351_DRIVE_STRENGTH_8MA, .power = 0},
};

MenuItem selectFreqMenuItems[]={
    {0, &freqMenuItems[0], 0, 0, actionSelectFreqBack, renderSelectFreq},
};

int currentFreqFactor = 1;
void actionFactor100M(){
    currentFreqFactor = 100000000;
}

void actionFactor10M(){
    currentFreqFactor = 10000000;
}

void actionFactor1M(){
    currentFreqFactor = 1000000;
}

void actionFactor100K(){
    currentFreqFactor = 100000;
}

void actionFactor10K(){
    currentFreqFactor = 10000;
}

void actionFactor1K(){
    currentFreqFactor = 1000;
}

void actionFactor1H(){
    currentFreqFactor = 1;
}

void actionFactor10H(){
    currentFreqFactor = 10;
}

void actionFactor100H(){
    currentFreqFactor = 100;
}


MenuItem freqMenuItems[]={
    {"100M", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor100M, renderSelectFreq},
    {" 10M", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor10M, renderSelectFreq},
    {"  1M", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor1M, renderSelectFreq},
    {"100K", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor100K, renderSelectFreq},
    {" 10K", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor10K, renderSelectFreq},
    {"  1K", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor1K, renderSelectFreq},
    {"100H", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor100H, renderSelectFreq},
    {" 10H", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor10H, renderSelectFreq},
    {"  1H", &dashMenuItems[0], selectFreqMenuItems, sizeof(selectFreqMenuItems)/sizeof(selectFreqMenuItems), actionFactor1H, renderSelectFreq},
    {BACK_NAME, &dashMenuItems[0], 0, 0, 0, renderFreq},    
};

MenuItem ampMenuItems[]={
    {"2.2dBm", &dashMenuItems[0], 0, 0, actionAmp, 0},
    {"7.2dBm", &dashMenuItems[0], 0, 0, actionAmp, 0},
    {"9.2dBm", &dashMenuItems[0], 0, 0, actionAmp, 0},
    {"10.2dBm", &dashMenuItems[0], 0, 0, actionAmp, 0},    
    {BACK_NAME, &dashMenuItems[0], 0, 0, 0, 0},    
};

MenuItem powerMenuItems[]={
    {"OFF", &dashMenuItems[0], 0, 0, actionPower, 0},
    {"ON", &dashMenuItems[0], 0, 0, actionPower, 0},
    {BACK_NAME, &dashMenuItems[0], 0, 0, 0, 0},    
};
void actionSave(){
    configWrite();

    char buf[50];
    sprintf(buf, "[done]");
    ssd1306_SetCursor(60, 0);
    ssd1306_WriteString(buf, Font_7x10, White);
}

MenuItem commandMenuItems[]={
    {"Freq", &mainMenu, freqMenuItems, sizeof(freqMenuItems)/sizeof(MenuItem), 0, renderFreq},
    {"Amp", &mainMenu, ampMenuItems, sizeof(ampMenuItems)/sizeof(MenuItem), 0, renderAmp},
    {"Power", &mainMenu, powerMenuItems, sizeof(powerMenuItems)/sizeof(MenuItem), 0, renderPower},
    {"Save", &mainMenu, 0, 0, actionSave, 0},
    {BACK_NAME, &mainMenu, 0, 0, 0, 0},
};

MenuItem dashMenuItems[]={
    {0, &mainMenu, commandMenuItems, sizeof(commandMenuItems)/sizeof(MenuItem), selectChannel, 0},
    {0, &mainMenu, commandMenuItems, sizeof(commandMenuItems)/sizeof(MenuItem), selectChannel, 0},
    {0, &mainMenu, commandMenuItems, sizeof(commandMenuItems)/sizeof(MenuItem), selectChannel, 0},
};

MenuItem mainMenu={
    .children = dashMenuItems, 
    .num_children = sizeof(dashMenuItems)/sizeof(MenuItem), 
    .parent = NULL,
    .render = renderDash,
};


int currentChannel = 0;
int currentIndex = 0;

MenuItem *currentMenu = &mainMenu;

void actionSelectFreqBack(){
    currentIndex = 0;
    if (currentMenu->parent){
        currentMenu = &currentMenu->parent->children[0];
    }

    si5351Restart();
}

void renderSelectFreq(){
    if (direction){
        channels[currentChannel].frequency+=direction*currentFreqFactor;
        if (channels[currentChannel].frequency<0){
            channels[currentChannel].frequency = 0;
        }
    }

    char buf[50];
    sprintf(buf, "[%3.3d.%3.3d.%3.3d]", channels[currentChannel].frequency/1000000,(channels[currentChannel].frequency/1000)%1000, channels[currentChannel].frequency%1000);
    ssd1306_SetCursor(36, 0);
    ssd1306_WriteString(buf, Font_7x10, White);

    si5351Restart();
}

void actionAmp(){
    channels[currentChannel].strength = currentIndex;
    si5351Restart();
}

void actionPower(){
    channels[currentChannel].power = currentIndex;
    si5351Restart();
}

void renderDash(){
    char buf[50];
    sprintf(buf, "(%s) %3.3d.%3.3d.%3.3d",channels[0].power? "+": "-", channels[0].frequency/1000000,(channels[0].frequency/1000)%1000, channels[0].frequency%1000);
    ssd1306_SetCursor(10, 0);
    ssd1306_WriteString(buf, Font_7x10, White);

    sprintf(buf, "(%s) %3.3d.%3.3d.%3.3d",channels[1].power? "+": "-", channels[1].frequency/1000000,(channels[1].frequency/1000)%1000, channels[1].frequency%1000);
    ssd1306_SetCursor(10, 10);
    ssd1306_WriteString(buf, Font_7x10, White);

    sprintf(buf, "(%s) %3.3d.%3.3d.%3.3d",channels[2].power? "+": "-", channels[2].frequency/1000000,(channels[2].frequency/1000)%1000, channels[2].frequency%1000);
    ssd1306_SetCursor(10, 20);
    ssd1306_WriteString(buf, Font_7x10, White);

}

void drawMenu(){
    //ssd1306_Fill(Black);
    if (currentMenu->render){
        currentMenu->render();
    }

    if (currentMenu == &mainMenu){
        for (int i = 0, j = 0; (i < currentMenu->num_children) && (j<3); i++, j++) {
            if (i==currentIndex){
                ssd1306_SetCursor(0, i * 10);
                ssd1306_WriteString(">", Font_7x10, White);
            }

            ssd1306_SetCursor(10, j * 10);
            if (currentMenu->children[i].name){
                ssd1306_WriteString(currentMenu->children[i].name, Font_7x10, White);
            }
        }
    }else{
        ssd1306_SetCursor(0, 0 * 10);
        ssd1306_WriteString(">", Font_7x10, White);
        for (int i = currentIndex, j = 0; (i < currentMenu->num_children) && (j<3); i++, j++) {
            ssd1306_SetCursor(10, j * 10);
            if (currentMenu->children[i].name){
                ssd1306_WriteString(currentMenu->children[i].name, Font_7x10, White);
            }
        }
    }
    
    ssd1306_UpdateScreen();
}

void inputHandler(){
    if (!direction && !button){
        return ;
    }

    ssd1306_Fill(Black);

    if (direction){
        currentIndex += direction;

        if (currentIndex >= currentMenu->num_children){
            currentIndex = currentMenu->num_children-1;
        }
        
        if (currentIndex<0){
            currentIndex = 0;
        }
    }

    if (button){
        MenuItem *selected = &currentMenu->children[currentIndex];
        if (strcmp(selected->name, BACK_NAME)==0){
            if (selected->parent) {
                currentMenu = selected->parent;
                currentIndex = 0;
            }            
        }else{
            if (selected->action) {
                selected->action();
            }

            if (selected->children) {
                currentMenu = selected;
                currentIndex = 0;
            }
        }       
        
    }

    drawMenu();

    direction = 0;
    button = 0;
}

void selectChannel(){
    currentChannel = currentIndex;
}

void renderAmp(){
    char buf[50];
    sprintf(buf, "[%s]", ampMenuItems[channels[currentChannel].strength].name);
    ssd1306_SetCursor(60, 0);
    ssd1306_WriteString(buf, Font_7x10, White);
}

void renderPower(){
    char buf[50];
    sprintf(buf, "[%3s]", powerMenuItems[channels[currentChannel].power].name);
    ssd1306_SetCursor(60, 0);
    ssd1306_WriteString(buf, Font_7x10, White);
}

void renderFreq(){
    char buf[50];
    sprintf(buf, "[%3.3d.%3.3d.%3.3d]", channels[currentChannel].frequency/1000000,(channels[currentChannel].frequency/1000)%1000, channels[currentChannel].frequency%1000);
    ssd1306_SetCursor(36, 0);
    ssd1306_WriteString(buf, Font_7x10, White);
}

void configWrite(){
    Config config;
    config.magic = CONFIG_MAGIC;
    memcpy(config.channels, channels, sizeof(channels));
    Flash_Write(&config, sizeof(config));
}

int configRead(){
    Config config;
    Flash_Read(&config, sizeof(config));
    if (config.magic == CONFIG_MAGIC){
        memcpy(channels, config.channels, sizeof(channels));
        return 1;
    }
    return 0;
}

void si5351Restart(){
    const int32_t correction = 978;
    si5351_Init(correction);

	si5351PLLConfig_t pll_confA;
	si5351OutputConfig_t out_confA;

	si5351_Calc(channels[0].frequency, &pll_confA, &out_confA);
	si5351_SetupOutput(0, SI5351_PLL_A, channels[0].strength, &out_confA, 0);
    

    si5351PLLConfig_t pll_conf;
    si5351OutputConfig_t out_conf;
    int32_t Fclk = channels[1].frequency;
    si5351_CalcIQ(Fclk, &pll_conf, &out_conf);

    /*
    * `phaseOffset` is a 7bit value, calculated from Fpll, Fclk and desired phase shift.
    * To get N° phase shift the value should be round( (N/360)*(4*Fpll/Fclk) )
    * Two channels should use the same PLL to make it work. There are other restrictions.
    * Please see AN619 for more details.
    *
    * si5351_CalcIQ() chooses PLL and MS parameters so that:
    *   Fclk in [1.4..100] MHz
    *   out_conf.div in [9..127]
    *   out_conf.num = 0
    *   out_conf.denum = 1
    *   Fpll = out_conf.div * Fclk.
    * This automatically gives 90° phase shift between two channels if you pass
    * 0 and out_conf.div as a phaseOffset for these channels.
    */
    uint8_t phaseOffset = (uint8_t)out_conf.div;
    si5351_SetupOutput(1, SI5351_PLL_B, channels[1].strength, &out_conf, 0);
    si5351_SetupOutput(2, SI5351_PLL_B, channels[1].strength, &out_conf, phaseOffset);

    /*
    * The order is important! Setup the channels first, then setup the PLL.
    * Alternatively you could reset the PLL after setting up PLL and channels.
    * However since _SetupPLL() always resets the PLL this would only cause
    * sending extra I2C commands.
    */

    si5351_SetupPLL(SI5351_PLL_A, &pll_confA);
    si5351_SetupPLL(SI5351_PLL_B, &pll_conf);
    si5351_SetupPLL(SI5351_PLL_B, &pll_conf);
    
    int enable = (channels[0].power<<0)|(channels[2].power<<2)|(channels[1].power<<1);
    si5351_EnableOutputs(enable);
}

void si5351Restart_bk(){
    si5351_SetupCLK0(channels[0].frequency, channels[0].strength);
    si5351_SetupCLK1(channels[1].frequency, channels[1].strength);
    si5351_SetupCLK2(channels[2].frequency, channels[2].strength);
    int enable = (channels[0].power<<0)|(channels[2].power<<2)|(channels[1].power<<1);
    si5351_EnableOutputs(enable);
}

int main() {

    clock_init();
    
    I2C1_GPIO_Config();
    I2C1_Config();
    EC11_Init();
    delay_ms(100);
    configRead();

    const int32_t correction = 978;
    si5351_Init(correction);

    si5351Restart();

    ssd1306_Init();

    ssd1306_Fill(Black);
    drawMenu();

    while (1){
        inputHandler();
    }

    return 0;
}
