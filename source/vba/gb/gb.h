#ifndef GB_H
#define GB_H

#define C_FLAG 0x10
#define H_FLAG 0x20
#define N_FLAG 0x40
#define Z_FLAG 0x80

typedef union {
  struct {
    u8 B1, B0;
  } B;
  u16 W;
} gbRegister;

extern gbRegister AF, BC, DE, HL, SP, PC;
extern u16 IFF;
int gbDis(char *, u16);

bool gbUpdateSizes();
void gbEmulate(int);
void gbWriteMemory(u16, u8);
void gbDrawLine();
bool gbIsGameboyRom(const char *);
void gbGetHardwareType();
void gbReset();
void gbCleanUp();
bool gbWriteMemSaveState(char *, int);
bool gbReadSaveState(const char *);
bool gbReadMemSaveState(char *, int);
void gbSgbRenderBorder();

extern int gbHardware;

extern struct EmulatedSystem GBSystem;

// For VBA-GX
bool MemgbReadBatteryFile(char * membuffer, int read);
int MemgbWriteBatteryFile(char * membuffer);

#endif // GB_H
