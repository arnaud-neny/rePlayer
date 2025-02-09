// BME main definitions header file

#define GFX_SCANLINES 1
#define GFX_DOUBLESIZE 2
#define GFX_USE1PAGE 0
#define GFX_USE2PAGES 4
#define GFX_USE3PAGES 8
#define GFX_WAITVBLANK 16
#define GFX_FULLSCREEN 32
#define GFX_WINDOW 64
#define GFX_NOSWITCHING 128
#define GFX_USEDIBSECTION 256

#define MOUSE_ALWAYS_VISIBLE 0
#define MOUSE_FULLSCREEN_HIDDEN 1
#define MOUSE_ALWAYS_HIDDEN 2

#define MOUSEB_LEFT 1
#define MOUSEB_RIGHT 2
#define MOUSEB_MIDDLE 4

#define JOY_LEFT 1
#define JOY_RIGHT 2
#define JOY_UP 4
#define JOY_DOWN 8
#define JOY_FIRE1 16
#define JOY_FIRE2 32
#define JOY_FIRE3 64
#define JOY_FIRE4 128

#define LEFT 0
#define MIDDLE 128
#define RIGHT 255

#define B_OFF 0
#define B_SOLID 1
#define B_NOTSOLID 2

#define MONO 0
#define STEREO 1
#define EIGHTBIT 0
#define SIXTEENBIT 2

#define VM_OFF 0
#define VM_ON 1
#define VM_ONESHOT 0
#define VM_LOOP 2
#define VM_16BIT 4
// rePlayer begin
#define KEY_BACKSPACE    1
#define KEY_CAPSLOCK     2
#define KEY_ENTER        3
#define KEY_ESC          4
#define KEY_ALT          5
#define KEY_CTRL         6
#define KEY_LEFTCTRL     7
#define KEY_RIGHTALT     8
#define KEY_RIGHTCTRL    9
#define KEY_LEFTSHIFT    10
#define KEY_RIGHTSHIFT   11
#define KEY_NUMLOCK      12
#define KEY_SCROLLLOCK   13
#define KEY_SPACE        14
#define KEY_TAB          15
#define KEY_F1           16
#define KEY_F2           17
#define KEY_F3           18
#define KEY_F4           19
#define KEY_F5           20
#define KEY_F6           21
#define KEY_F7           22
#define KEY_F8           23
#define KEY_F9           24
#define KEY_F10          25
#define KEY_F11          26
#define KEY_F12          27
#define KEY_A            28
#define KEY_N            29
#define KEY_B            30
#define KEY_O            31
#define KEY_C            32
#define KEY_P            33
#define KEY_D            34
#define KEY_Q            35
#define KEY_E            36
#define KEY_R            37
#define KEY_F            38
#define KEY_S            39
#define KEY_G            40
#define KEY_T            41
#define KEY_H            42
#define KEY_U            43
#define KEY_I            44
#define KEY_V            45
#define KEY_J            46
#define KEY_W            47
#define KEY_K            48
#define KEY_X            49
#define KEY_L            50
#define KEY_Y            51
#define KEY_M            52
#define KEY_Z            53
#define KEY_1            54
#define KEY_2            55
#define KEY_3            56
#define KEY_4            57
#define KEY_5            58
#define KEY_6            59
#define KEY_7            60
#define KEY_8            61
#define KEY_9            62
#define KEY_0            63
#define KEY_MINUS        64
#define KEY_EQUAL        65
#define KEY_BRACKETL     66
#define KEY_BRACKETR     67
#define KEY_SEMICOLON    68
#define KEY_APOST1       69
#define KEY_APOST2       70
#define KEY_COMMA        71
#define KEY_COLON        72
#define KEY_PERIOD       73
#define KEY_SLASH        74
#define KEY_BACKSLASH    75
#define KEY_DEL          76
#define KEY_DOWN         77
#define KEY_END          78
#define KEY_HOME         79
#define KEY_INS          80
#define KEY_LEFT         81
#define KEY_PGDN         82
#define KEY_PGUP         83
#define KEY_RIGHT        84
#define KEY_UP           85
#define KEY_WINDOWSL     86
#define KEY_WINDOWSR     87
#define KEY_MENU         88
#define KEY_PAUSE        89
#define KEY_KPDIVIDE     90
#define KEY_KPMULTIPLY   91
#define KEY_KPPLUS       92
#define KEY_KPMINUS      93
#define KEY_KP0          94
#define KEY_KP1          95
#define KEY_KP2          96
#define KEY_KP3          97
#define KEY_KP4          98
#define KEY_KP5          99
#define KEY_KP6          100
#define KEY_KP7          101
#define KEY_KP8          102
#define KEY_KP9          103
#define KEY_KPUP         104
#define KEY_KPDOWN       105
#define KEY_KPLEFT       106
#define KEY_KPRIGHT      107
#define KEY_KPENTER      108
#define KEY_KPEQUALS     109
#define KEY_KPPERIOD     110

typedef unsigned char Uint8;
typedef char Sint8;
typedef short Sint16;
typedef unsigned int Uint32;
typedef int Sint32;
// rePlayer end

typedef struct
{
	Sint8 *start;
	Sint8 *repeat;
	Sint8 *end;
	unsigned char voicemode;
} SAMPLE;

typedef struct
{
	volatile Sint8 *pos;
	Sint8 *repeat;
	Sint8 *end;
	SAMPLE *smp;
	unsigned freq;
	volatile unsigned fractpos;
	int vol;
	int mastervol;
	unsigned panning;
	volatile unsigned voicemode;
} CHANNEL;

typedef struct
{
  unsigned rawcode;
  char *name;
} KEY;

typedef struct
{
  Sint16 xsize;
  Sint16 ysize;
  Sint16 xhot;
  Sint16 yhot;
  Uint32 offset;
} SPRITEHEADER;

typedef struct
{
  Uint32 type;
  Uint32 offset;
} BLOCKHEADER;

typedef struct
{
  Uint8 blocksname[13];
  Uint8 palettename[13];
} MAPHEADER;

typedef struct
{
  Sint32 xsize;
  Sint32 ysize;
  Uint8 xdivisor;
  Uint8 ydivisor;
  Uint8 xwrap;
  Uint8 ywrap;
} LAYERHEADER;

extern int bme_error;
