#include <string.h>

#include "Util.h"

extern "C" {
#include "common/memgzio.h"
}

static int (ZEXPORT *utilGzWriteFunc)(gzFile, const voidp, unsigned int) = NULL;
static int (ZEXPORT *utilGzReadFunc)(gzFile, voidp, unsigned int) = NULL;
static int (ZEXPORT *utilGzCloseFunc)(gzFile) = NULL;
static z_off_t (ZEXPORT *utilGzSeekFunc)(gzFile, z_off_t, int) = NULL;

extern bool cpuIsMultiBoot;

bool utilIsGBAImage(const char * file)
{
  cpuIsMultiBoot = false;
  if(strlen(file) > 4) {
    const char * p = strrchr(file,'.');

    if(p != NULL) {
      if((strcasecmp(p, ".agb") == 0) ||
         (strcasecmp(p, ".gba") == 0) ||
         (strcasecmp(p, ".bin") == 0) ||
         (strcasecmp(p, ".elf") == 0))
        return true;
      if(strcasecmp(p, ".mb") == 0) {
        cpuIsMultiBoot = true;
        return true;
      }
    }
  }

  return false;
}

bool utilIsGBImage(const char * file)
{
  if(strlen(file) > 4) {
    const char * p = strrchr(file,'.');

    if(p != NULL) {
      if((strcasecmp(p, ".dmg") == 0) ||
         (strcasecmp(p, ".gb") == 0) ||
         (strcasecmp(p, ".gbc") == 0) ||
         (strcasecmp(p, ".cgb") == 0) ||
         (strcasecmp(p, ".sgb") == 0))
        return true;
    }
  }

  return false;
}

void utilWriteInt(gzFile gzFile, int i)
{
  utilGzWrite(gzFile, &i, sizeof(int));
}

int utilReadInt(gzFile gzFile)
{
  int i = 0;
  utilGzRead(gzFile, &i, sizeof(int));
  return i;
}

void utilReadData(gzFile gzFile, variable_desc* data)
{
  while(data->address) {
    utilGzRead(gzFile, data->address, data->size);
    data++;
  }
}

void utilReadDataSkip(gzFile gzFile, variable_desc* data)
{
  while(data->address) {
    utilGzSeek(gzFile, data->size, SEEK_CUR);
    data++;
  }
}

void utilWriteData(gzFile gzFile, variable_desc *data)
{
  while(data->address) {
    utilGzWrite(gzFile, data->address, data->size);
    data++;
  }
}

gzFile utilMemGzOpen(char *memory, int available, const char *mode)
{
  utilGzWriteFunc = memgzwrite;
  utilGzReadFunc = memgzread;
  utilGzCloseFunc = memgzclose;
  utilGzSeekFunc = memgzseek;

  return memgzopen(memory, available, mode);
}

int utilGzWrite(gzFile file, const voidp buffer, unsigned int len)
{
  return utilGzWriteFunc(file, buffer, len);
}

int utilGzRead(gzFile file, voidp buffer, unsigned int len)
{
  return utilGzReadFunc(file, buffer, len);
}

int utilGzClose(gzFile file)
{
  return utilGzCloseFunc(file);
}

z_off_t utilGzSeek(gzFile file, z_off_t offset, int whence)
{
	return utilGzSeekFunc(file, offset, whence);
}

long utilGzMemTell(gzFile file)
{
  return memtell(file);
}
