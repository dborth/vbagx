/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
 * Carl Kenner April 2009
 *
 * gamesettings.cpp
 *
 * Game specific settings, pulled from vba-over.ini
 * And palettes for monochrome games, created by Carl Kenner
 ***************************************************************************/

#include "gamesettings.h"

gamePalette CurrentPalette;

int gamePalettesCount = 32;

// 0xRRGGBB
// TITLE, 4 Background Colours (brightest to black), 4 Status Bar Colours (brightest to black),
// 3 Sprite 1 Colours (brightest to dark), 3 Sprite 2 Colours (brightest to dark)

gamePalette gamePalettes[32] = {
	{"default", 1, {0xFFFFFF,0xADADAD,0x636363,0x000000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFFFFFF,0xADADAD,0x636363,  0xFFFFFF,0xADADAD,0x636363}}
			
	,{"ALLEY WAY", 1, {0x00FF00,0xAD0000,0x000063,0x636300,  0xFFFFFF,0xAD0000,0x630000,0x000000,
			0xFF0000,0xAD0000,0x630000,  0x0000FF,0x0000AD,0x000063}}

	,{"BASEBALL", 1, {0x009000,0x006000,0x003000,0x000000,  0xFFFFFF,0xAD0000,0x630000,0x000000,
			0xFFD060,0xAD0000,0x630000,  0x0050AD,0xFFD060,0x0040A0}}
	,{"BUGSCRAZYCASTLE2", 1, {0xFFFFFF,0xA8ADA8,0x604B20,0x302010,  0xFFFF00,0xADAD00,0x636300,0x000000,  
			0xFFFFFF,0xADA8A8,0x636060,  0x0000FF,0x0000AD,0x000063}}

	,{"CAESARS PALACE", 1, {0xFFFFFF,0xC0AD00,0x680000,0x000000,  0x608FFF,0x0000B5,0x005300,0x003000,  
			0x0060FF,0x0000A8,0x000068,  0xFF00FF,0xA800AD,0x600063}}

	,{"DARK WING DUCK", 1, {0xFF7F50,0xB05530,0x000060,0x000000,  0xFF0000,0xADADAD,0x636363,0x000000,  
		0xFFFF00,0xAD00FF,0x530070,  0x00FF00,0x00B000,0x006000}}
	,{"DUCK TALES", 1, {0x80FF00,0x68AD00,0x633100,0x201000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFFFFFF,0xAD5800,0xAD5800,  0xFFFFFF,0xADADAD,0x636363}}

	,{"EARTHWORM JIM", 1, {0x809F20,0x985D38,0x006300,0x200028,  0xFFFFFF,0xADADAD,0x636363,0x000000,  
			0xDFDFDF,0xCD7890,0x0068B0,  0xFF0000,0xA80000,0x600000}}

	,{"FELIX THE CAT", 1, {0x00BFFF,0x50AD50,0x684318,0x000000,  0xCF77CF,0xADADAD,0x636363,0xFFFFFF,  
		0xFFFFFF,0xADA800,0x000000,  0x0000FF,0x0000AD,0x000063}}
	,{"THE FLINTSTONES", 1, {0xD0FFEF,0xD8C87C,0x406300,0x000000,  0xFFFFFF,0xADADAD,0x636363,0x000000,  
		0xFFEF73,0xFFBB8C,0x0080FF,  0x0000FF,0x0000AD,0x000063}}

	,{"GAUNTLET II", 1, {0xFFFF00,0xA85D40,0x684328,0x105848,  0x000000,0xADA8A8,0x636060,0xFF0000,  
		0xC87850,0xA82820,0x630000,  0x0000FF,0x0000AD,0x000063}}

	,{"INDIANA JONES", 1, {0xE8D760,0xA88548,0x803308,0x201000, 0x00C700,0x008D00,0x004300,0x000000,  
		0x8F6050,0xA56048,0x630000, 0x90604F,0x883810,0x680000}}
		
	,{"KID ICARUS", 1, {0xFFFFFF,0xADADAD,0x314278,0x003163,  0xFFFFFF,0xAD0000,0x630000,0x000000,
			0xFFFF00,0xB80000,0x436300,  0xFFFFFF,0xD4B4A4,0x633100}}

	,{"MAGNETIC SOCCER", 1, {0x009000,0x006000,0x003000,0x000000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFF0000,0xAD0000,0x630000,  0x0000FF,0x0000AD,0x000063}}
	,{"BEACH,VOLLEYBALL", 1, {0xD4B4A4,0xADAD30,0x000063,0x000020,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xB49E90,0xB00000,0x31271E,  0x0000FF,0x0000AD,0x000063}}
	,{"MARBLE MADNESS", 1, {0x00FFFF,0x00ADAD,0x006363,0x303030,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFF0000,0xAD0000,0x630000,  0xFFFF00,0xAD8000,0x634000}}
	,{"METROID2", 1, {0xFF7B30,0xAD5230,0x300063,0x000018,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0x7B6300,0xAD0000,0x000063,  0xFF00FF,0x8400AD,0x300063}}
	,{"MORTAL KOMBAT", 1, {0xADFF00,0x00AD00,0x006300,0x400000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFFFF00,0xADAD00,0x636300,  0xFFFF00,0xADAD00,0x636300}}
	,{"MORTAL KOMBAT II", 1, {0xC0B0FF,0x6890A0,0x303063,0x000000,  0xADFF00,0x00AD00,0x006300,0x400000,
			0xC9A104,0xBF5000,0xB20A00,  0x8080FF,0x5050AD,0x303063}}
	,{"MORTAL KOMBAT 3", 1, {0xFFFFFF,0x6060A0,0x535373,0x000000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xF5CCAC,0x9A7057,0x630000,  0xA87860,0x882020,0x000000}}
	,{"MR.DO!", 1, {0x38F038,0x00C800,0x705000,0x600000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFFFFFF,0xADADAD,0x636363,  0xFFFFFF,0xADADAD,0x636363}}

	,{"ROCKY BULLWINKLE", 1, {0x60C7D7,0x909090,0x683320,0x002000,  0xFFFF4F,0xAD7D15,0x63530B,0x000000,  
			0xFFFF00,0xAD6018,0x000000,  0x0000FF,0x0000AD,0x000063}}

	,{"SIMPSONS3", 1, {0x80FF40,0x00AD50,0x006300,0x402000,  0xE0D000,0xADADAD,0x636363,0x000000,
			0xF0E000,0xAD4000,0x0000FF,  0xFF0000,0xAD0000,0x630000}}
	,{"SPIDER-MAN 2", 1, {0xEFE7DF,0xA88548,0x604318,0x000000,  0xFFFFFF,0xAD0000,0x630000,0x000000,  
			0xFFFF00,0xAD0000,0x006000,  0xFF9F9F,0xFF0000,0x0050FF}}
	,{"SPIDER-MAN 3 DMG", 1, {0x00FF00,0x00AD00,0x006300,0x002000,  0xFFFFFF,0xADADAD,0x636363,0x000000,  
			0xFFB850,0xAD6838,0x630000,  0xFFAF9F,0xF00000,0x0050FF}}
	,{"STAR WARS", 1, {0xFFCF70,0xAF9538,0x705318,0x000000,  0xFFFFFF,0xADADAD,0x636363,0x000000,  
			0xFFFFFF,0xAD9800,0x006000,  0xFF0000,0x680095,0x000063}}
	,{"SUPER MARIO LAND", 1, {0x90A0FF,0x80AD00,0x636300,0x301800,  0xFFFF40,0xADAD00,0x636300,0x000000,
			0xFFE080,0x0000AD,0xFF0000,  0xFFFFFF,0xADADAD,0x636363}}

	,{"TMNT FOOT CLAN", 1, {0xFFFF80,0xADB800,0x636300,0x302000,  0x8080FF,0x0000AD,0x000063,0x000030,
			0x60A060,0x633030,0x533000,  0xFFFFFF,0xADADAD,0x636363}}
	,{"TENNIS", 1, {0x008000,0xB49E90,0x630000,0x000000,  0xFFFFFF,0xB49E90,0x630000,0x201000,
			0xB4A880,0xADAD00,0x632000,  0x0000AD,0xA48E80,0x636300}}
	,{"TETRIS", 1, {0xAD0000,0x00AD00,0x0000AD,0x000000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0x00AD00,0x0000AD,0xAD0000,  0xFFFFFF,0xADADAD,0x636363}}

	,{"", 1, {0x00FF00,0x00AD00,0x006300,0x002000,  0xFFFFFF,0xADADAD,0x636363,0x000000,
			0xFF8000,0xAD0040,0x630000,  0x00A0FF,0x2000AD,0x000063}}
};


int gameSettingsCount = 106;

gameSetting gameSettings[106] = {
	{
	"Dragon Ball Z - The Legacy of Goku II (Europe)(En,Fr,De,Es,It)",
	"ALFP",
	1,
	-1,
	-1,
	-1
	},
	{
	"Dragon Ball Z - The Legacy of Goku (Europe)(En,Fr,De,Es,It)",
	"ALGP",
	1,
	-1,
	-1,
	-1
	},
	{
	"Rocky (Europe)(En,Fr,De,Es,It)",
	"AROP",
	1,
	-1,
	-1,
	-1
	},
	{
	"Rocky (USA)(En,Fr,De,Es,It)",
	"AR8e",
	1,
	-1,
	-1,
	-1
	},
	{
	"Pokemon - Ruby Version (USA, Europe)",
	"AXVE",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Sapphire Version (USA, Europe)",
	"AXPE",
	-1,
	1,
	131072,
	-1
	},
	{
	"Super Mario Advance 4 - Super Mario Bros. 3 (Europe)(En,Fr,De,Es,It)",
	"AX4P",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Top Gun - Combat Zones (USA)(En,Fr,De,Es,It)",
	"A2YE",
	5,
	-1,
	-1,
	-1
	},
	{
	"Dragon Ball Z - Taiketsu (Europe)(En,Fr,De,Es,It)",
	"BDBP",
	1,
	-1,
	-1,
	-1
	},
	{
	"Mario vs. Donkey Kong (Europe)",
	"BM5P",
	3,
	-1,
	-1,
	-1
	},
	{
	"Pokemon - Emerald Version (USA, Europe)",
	"BPEE",
	-1,
	1,
	131072,
	-1
	},
	{
	"Yu-Gi-Oh! - Ultimate Masters - World Championship Tournament 2006 (Europe)(En,Jp,Fr,De,Es,It)",
	"BY6P",
	2,
	-1,
	-1,
	-1
	},
	{
	"Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia)",
	"B24E",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Classic NES Series - Castlevania (USA, Europe)",
	"FADE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Bomberman (USA, Europe)",
	"FBME",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Donkey Kong (USA, Europe)",
	"FDKE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Dr. Mario (USA, Europe)",
	"FDME",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Excitebike (USA, Europe)",
	"FEBE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Ice Climber (USA, Europe)",
	"FICE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Zelda II - The Adventure of Link (USA, Europe)",
	"FLBE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Metroid (USA, Europe)",
	"FMRE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Pac-Man (USA, Europe)",
	"FP7E",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Super Mario Bros. (USA, Europe)",
	"FSME",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Xevious (USA, Europe)",
	"FXVE",
	1,
	-1,
	-1,
	1
	},
	{
	"Classic NES Series - Legend of Zelda (USA, Europe)",
	"FZLE",
	1,
	-1,
	-1,
	1
	},
	{
	"Yoshi's Universal Gravitation (Europe)(En,Fr,De,Es,It)",
	"KYGP",
	4,
	-1,
	-1,
	-1
	},
	{
	"Boktai - The Sun Is in Your Hand (Europe)(En,Fr,De,Es,It)",
	"U3IP",
	-1,
	1,
	-1,
	-1
	},
	{
	"Boktai 2 - Solar Boy Django (Europe)(En,Fr,De,Es,It)",
	"U32P",
	-1,
	1,
	-1,
	-1
	},
	{
	"Golden Sun - The Lost Age (USA)",
	"AGFE",
	-1,
	1,
	0x10000,
	-1
	},
	{
	"Golden Sun (USA)",
	"AGSE",
	-1,
	1,
	0x10000,
	-1
	},
	{
	"Dragon Ball Z - The Legacy of Goku II (USA)",
	"ALFE",
	1,
	-1,
	-1,
	-1
	},
	{
	"Dragon Ball Z - The Legacy of Goku (USA)",
	"ALGE",
	1,
	-1,
	-1,
	-1
	},
	{
	"Super Mario Advance 4 - Super Mario Bros 3 - Super Mario Advance 4 v1.1 (USA)",
	"AX4E",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Dragon Ball Z - Taiketsu (USA)",
	"BDBE",
	1,
	-1,
	-1,
	-1
	},
	{
	"Dragon Ball Z - Buu's Fury (USA)",
	"BG3E",
	1,
	-1,
	-1,
	-1
	},
	{
	"2 Games in 1 - Dragon Ball Z - The Legacy of Goku I & II (USA)",
	"BLFE",
	1,
	-1,
	-1,
	-1
	},
	{
	"Pokemon - Fire Red Version (USA, Europe)",
	"BPRE",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Leaf Green Version (USA, Europe)",
	"BPGE",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Dragon Ball GT - Transformation (USA)",
	"BT4E",
	1,
	-1,
	-1,
	-1
	},
	{
	"2 Games in 1 - Dragon Ball Z - Buu's Fury + Dragon Ball GT - Transformation (USA)",
	"BUFE",
	1,
	-1,
	-1,
	-1
	},
	{
	"useBios=1",
	"BYGE",
	2,
	-1,
	-1,
	-1
	},
	{
	"Yoshi - Topsy-Turvy (USA)",
	"KYGE",
	4,
	-1,
	-1,
	-1
	},
	{
	"e-Reader (USA)",
	"PSAE",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Boktai - The Sun Is in Your Hand (USA)",
	"U3IE",
	-1,
	1,
	-1,
	-1
	},
	{
	"Boktai 2 - Solar Boy Django (USA)",
	"U32E",
	-1,
	1,
	-1,
	-1
	},
	{
	"Dragon Ball Z - The Legacy of Goku II International (Japan)",
	"ALFJ",
	1,
	-1,
	-1,
	-1
	},
	{
	"Pocket Monsters - Sapphire (Japan)",
	"AXPJ",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pocket Monsters - Ruby (Japan)",
	"AXVJ",
	-1,
	1,
	131072,
	-1
	},
	{
	"Super Mario Advance 4 (Japan)",
	"AX4J",
	-1,
	-1,
	131072,
	-1
	},
	{
	"F-Zero - Climax (Japan)",
	"BFTJ",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Game Boy Wars Advance 1+2 (Japan)",
	"BGWJ",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Sennen Kazoku (Japan)",
	"BKAJ",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pocket Monsters - Emerald (Japan)",
	"BPEJ",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pocket Monsters - Leaf Green (Japan)",
	"BPGJ",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pocket Monsters - Fire Red (Japan)",
	"BPRJ",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Digi Communication 2 - Datou! Black Gemagema Dan (Japan)",
	"BDKJ",
	1,
	-1,
	-1,
	-1
	},
	{
	"Rockman EXE 4.5 - Real Operation (Japan)",
	"BR4J",
	-1,
	1,
	-1,
	-1
	},
	{
	"Famicom Mini Vol. 01 - Super Mario Bros. (Japan)",
	"FMBJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 12 - Clu Clu Land (Japan)",
	"FCLJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 13 - Balloon Fight (Japan)",
	"FBFJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 14 - Wrecking Crew (Japan)",
	"FWCJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 15 - Dr. Mario (Japan)",
	"FDMJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 16 - Dig Dug (Japan)",
	"FDDJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 17 - Takahashi Meijin no Boukenjima (Japan)",
	"FTBJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 18 - Makaimura (Japan)",
	"FMKJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 19 - Twin Bee (Japan)",
	"FTWJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 20 - Ganbare Goemon! Karakuri Douchuu (Japan)",
	"FGGJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 21 - Super Mario Bros. 2 (Japan)",
	"FM2J",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 22 - Nazo no Murasame Jou (Japan)",
	"FNMJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 23 - Metroid (Japan)",
	"FMRJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 24 - Hikari Shinwa - Palthena no Kagami (Japan)",
	"FPTJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 25 - The Legend of Zelda 2 - Link no Bouken (Japan)",
	"FLBJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 26 - Famicom Mukashi Banashi - Shin Onigashima - Zen Kou Hen (Japan)",
	"FFMJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 27 - Famicom Tantei Club - Kieta Koukeisha - Zen Kou Hen (Japan)",
	"FTKJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 28 - Famicom Tantei Club Part II - Ushiro ni Tatsu Shoujo - Zen Kou Hen (Japan)",
	"FTUJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 29 - Akumajou Dracula (Japan)",
	"FADJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Famicom Mini Vol. 30 - SD Gundam World - Gachapon Senshi Scramble Wars (Japan)",
	"FSDJ",
	1,
	-1,
	-1,
	1
	},
	{
	"Koro Koro Puzzle - Happy Panechu! (Japan)",
	"KHPJ",
	4,
	-1,
	-1,
	-1
	},
	{
	"Yoshi no Banyuuinryoku (Japan)",
	"KYGJ",
	4,
	-1,
	-1,
	-1
	},
	{
	"Card e-Reader+ (Japan)",
	"PSAJ",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Bokura no Taiyou - Taiyou Action RPG (Japan)",
	"U3IJ",
	-1,
	1,
	-1,
	-1
	},
	{
	"Zoku Bokura no Taiyou - Taiyou Shounen Django (Japan)",
	"U32J",
	-1,
	1,
	-1,
	-1
	},
	{
	"Shin Bokura no Taiyou - Gyakushuu no Sabata (Japan)",
	"U33J",
	-1,
	1,
	-1,
	-1
	},
	{
	"Mother 3 (Japan)",
	"A3UJ",
	-1,
	-1,
	65536,
	-1
	},
	{
	"Pokemon - Version Saphir (France)",
	"AXPF",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Version Rubis (France)",
	"AXVF",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Version Emeraude (France)",
	"BPEF",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Version Vert Feuille (France)",
	"BPGF",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Version Rouge Feu (France)",
	"BPRF",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Versione Zaffiro (Italy)",
	"AXPI",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Versione Rubino (Italy)",
	"AXVI",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Versione Smeraldo (Italy)",
	"BPEI",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Versione Verde Foglia (Italy)",
	"BPGI",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Versione Rosso Fuoco (Italy)",
	"BPRI",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Saphir-Edition (Germany)",
	"AXPD",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Rubin-Edition (Germany)",
	"AXVD",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Smaragd-Edition (Germany)",
	"BPED",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Blattgruene Edition (Germany)",
	"BPGD",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Feuerrote Edition (Germany)",
	"BPRD",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Edicion Zafiro (Spain)",
	"AXPS",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Edicion Rubi (Spain)",
	"AXVS",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Edicion Esmeralda (Spain)",
	"BPES",
	-1,
	1,
	131072,
	-1
	},
	{
	"Pokemon - Edicion Verde Hoja (Spain)",
	"BPGS",
	-1,
	-1,
	131072,
	-1
	},
	{
	"Pokemon - Edicion Rojo Fuego (Spain)",
	"BPRS",
	1,
	-1,
	131072,
	-1
	},
	{
	"WarioWare - Twisted! (USA)",
	"RZWE",
	-1,
	1, // needs "RealTimeClock" (actually motion sensor and rumble)
	-1,
	-1
	},
	{
	"Mawaru Made in Wario (Japan)",
	"RZWJ",
	-1,
	1, // needs "RealTimeClock" (actually motion sensor and rumble)
	-1,
	-1
	},
};
