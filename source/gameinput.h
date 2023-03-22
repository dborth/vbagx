/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner Febuary 2009
 *
 * gameinput.h
 *
 * Wii controls for individual games
 ***************************************************************************/

#ifndef _GAMEINPUT_H_
#define _GAMEINPUT_H_

#define gid(a,b,c) (a|(b<<8)|(c<<16))

#define BOKTAI1			gid('U','3','I')
#define BOKTAI2			gid('U','3','2')
#define BOKTAI3			gid('U','3','3')

#define ZELDA1			gid('F','Z','L')
#define ZELDA2			gid('F','L','B')
#define ALINKTOTHEPAST  gid('A','Z','L')
#define LINKSAWAKENING	0xFF0001
#define ORACLEOFSEASONS	gid('A','Z','7')
#define ORACLEOFAGES	gid('A','Z','8')
#define MINISHCAP		gid('B','Z','M')

#define METROID0		gid('B','M','X')
#define METROID1		gid('F','M','R')
#define METROID2		0xFF0009
#define METROID4		gid('A','M','T')

#define MARIO1CLASSIC	gid('F','S','M')
#define MARIO1DX		gid('A','H','Y')
#define MARIO2CLASSIC 	gid('F','M','2')
#define MARIO2ADV 		gid('A','M','Z')
#define MARIO3ADV		gid('A','X','4')
#define MARIOWORLD		gid('A','A','2')
#define YOSHIISLAND 	gid('A','3','A')
#define MARIOLAND1		0xFF0007
#define MARIOLAND2		0xFF0008

#define HARRYPOTTER1	gid('A','H','R')
#define HARRYPOTTER1GBC	gid('B','H','V')
#define HARRYPOTTER2	gid('A','7','H')
#define HARRYPOTTER2GBC	gid('B','H','6')
#define HARRYPOTTER3	gid('B','H','T')
#define HARRYPOTTER4 	gid('B','H','8')
#define HARRYPOTTER5 	gid('B','J','X')
#define QUIDDITCH		gid('B','H','P')

#define TMNT1			0xFF000B
#define TMNT2			0xFF000C
#define TMNT3			0xFF000D
#define TMNTGBA			gid('B','N','T')
#define TMNTGBA2		gid('B','T','2')
#define TMNT 			gid('B','E','X')

#define RESIDENTEVIL	gid('B','I','O')

#define MOHUNDERGROUND	gid('A','U','G')
#define MOHINFILTRATOR	gid('B','M','H')

#define KOROKORO		gid('K','H','P')
#define YOSHIUG			gid('K','Y','G')
#define KIRBYTNT		gid('K','T','N')
#define KIRBYTNTJ		gid('K','K','K')
#define TWISTED			gid('R','Z','W')

#define MARIOGOLF		gid('A','W','X')
#define MARIOTENNIS		gid('B','M','8')

#define MARIOKART		gid('A','M','K')

#define BIONICLEHEROES	gid('B','I','H')
#define LSW1			gid('B','L','W')
#define LSW2			gid('B','L','7')

#define SWOBIWAN		gid('B','O','W')
#define SWJPB			gid('A','S','W')
#define SWEP2			gid('A','S','2')
#define SWEP3			gid('B','E','3')
#define SWEP4			0xFF0011
#define SWEP5			0xFF0012
#define SWEP6			0xFF0013
#define SWTRILOGY		gid('B','C','K')
#define SWNDA			gid('A','2','W')
#define SWYODA			gid('A','Y','D')

#define MK1				0xFF0002
#define MK2				0xFF0003
#define MK12			0xFF0004
#define MK3				0xFF0005
#define MK4				0xFF0006
#define MKA				gid('A','M','5')
#define MKDA			gid('A','X','D')
#define MKTE			gid('A','W','4')

#define CORVETTE		gid('A','V','C')

#define ONEPIECE		gid('B','O','N')

#define HOBBIT			gid('A','H','9')
#define LOTR1			gid('A','L','O')
#define LOTR2			gid('A','L','P')
#define LOTR3			gid('B','L','R')
#define LOTR3RDAGE		gid('B','3','A')

#define CVADVENTURE		0xFF000E
#define CVBELMONT		0xFF000F
#define CVLEGENDS		0xFF0010
#define CVCIRCLEMOON	gid('A','A','M')
#define CVHARMONY		gid('A','C','H')
#define CVARIA			gid('A','2','C')
#define CVCLASSIC		gid('F','A','D')
#define CVDOUBLE		gid('B','X','K')

#define KIDDRACULA		0xFF0014

#define MARBLEMADNESS   0xFF000A

u8 gbReadMemory(u16 address);
void gbWriteMemory(u16 address, u8 value);

u32 StandardDPad(unsigned short pad);
u32 StandardMovement(unsigned short pad);
u32 StandardSideways(unsigned short pad);
u32 StandardClassic(unsigned short pad);
u32 StandardGamecube(unsigned short pad);
u32 DecodeWiimote(unsigned short pad);
u32 DecodeClassic(unsigned short pad);
u32 DecodeGamecube(unsigned short pad);
u32 DecodeNunchuk(unsigned short pad);

// For developers who don't have gamecube pads but need to test gamecube input
u32 PAD_ButtonsDownFake(unsigned short pad);
u32 PAD_ButtonsHeldFake(unsigned short pad);
u32 PAD_ButtonsUpFake(unsigned short pad);

u32 MarioLand1Input(unsigned short pad);
u32 MarioLand2Input(unsigned short pad);
u32 LegoStarWars1Input(unsigned short pad);
u32 LegoStarWars2Input(unsigned short pad);
u32 SWObiWanInput(unsigned short pad);
u32 SWEpisode2Input(unsigned short pad);
u32 SWNDAInput(unsigned short pad);
u32 SWEpisode3Input(unsigned short pad);
u32 SWEpisode4Input(unsigned short pad);
u32 SWEpisode5Input(unsigned short pad);
u32 SWEpisode6Input(unsigned short pad);
u32 SWJediPowerBattlesInput(unsigned short pad);
u32 SWTrilogyInput(unsigned short pad);
u32 SWYodaStoriesInput(unsigned short pad);
u32 MarioKartInput(unsigned short pad);
u32 MK1Input(unsigned short pad);
u32 MK12Input(unsigned short pad);
u32 MK2Input(unsigned short pad);
u32 MK3Input(unsigned short pad);
u32 MK4Input(unsigned short pad);
u32 MKAInput(unsigned short pad);
u32 MKDAInput(unsigned short pad);
u32 MKTEInput(unsigned short pad);
u32 Zelda1Input(unsigned short pad);
u32 Zelda2Input(unsigned short pad);
u32 ALinkToThePastInput(unsigned short pad);
u32 LinksAwakeningInput(unsigned short pad);
u32 OracleOfAgesInput(unsigned short pad);
u32 OracleOfSeasonsInput(unsigned short pad);
u32 MinishCapInput(unsigned short pad);
u32 MetroidZeroInput(unsigned short pad);
u32 Metroid1Input(unsigned short pad);
u32 Metroid2Input(unsigned short pad);
u32 MetroidFusionInput(unsigned short pad);
u32 TMNTInput(unsigned short pad);
u32 TMNT1Input(unsigned short pad);
u32 TMNT2Input(unsigned short pad);
u32 TMNT3Input(unsigned short pad);
u32 TMNTGBAInput(unsigned short pad);
u32 TMNTGBA2Input(unsigned short pad);
u32 HarryPotter1Input(unsigned short pad);
u32 HarryPotter1GBCInput(unsigned short pad);
u32 HarryPotter2Input(unsigned short pad);
u32 HarryPotter2GBCInput(unsigned short pad);
u32 HarryPotter3Input(unsigned short pad);
u32 HarryPotter4Input(unsigned short pad);
u32 HarryPotter5Input(unsigned short pad);
u32 Mario1ClassicInput(unsigned short pad);
u32 Mario1DXInput(unsigned short pad);
u32 Mario2Input(unsigned short pad);
u32 Mario3Input(unsigned short pad);
u32 MarioWorldInput(unsigned short pad);
u32 YoshiIslandInput(unsigned short pad);
u32 UniversalGravitationInput(unsigned short pad);
u32 MohUndergroundInput(unsigned short pad);
u32 MohInfiltratorInput(unsigned short pad);
u32 TwistedInput(unsigned short pad);
u32 KirbyTntInput(unsigned short pad);
u32 BoktaiInput(unsigned short pad);
u32 Boktai2Input(unsigned short pad);
u32 OnePieceInput(unsigned short pad);
u32 HobbitInput(unsigned short pad);
u32 FellowshipOfTheRingInput(unsigned short pad);
u32 ReturnOfTheKingInput(unsigned short pad);
u32 CastlevaniaAdventureInput(unsigned short pad);
u32 CastlevaniaBelmontInput(unsigned short pad);
u32 CastlevaniaLegendsInput(unsigned short pad);
u32 CastlevaniaCircleMoonInput(unsigned short pad);
u32 KidDraculaInput(unsigned short pad);

#endif

