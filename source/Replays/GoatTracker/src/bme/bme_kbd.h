// BME keyboard functions header file

int kbd_init(void);
void kbd_uninit(void);
int kbd_waitkey(void);
int kbd_getkey(void);
int kbd_checkkey(int rawcode);
int kbd_getascii(void);
unsigned kbd_getvirtualkey(void);
char *kbd_getkeyname(int rawcode);
