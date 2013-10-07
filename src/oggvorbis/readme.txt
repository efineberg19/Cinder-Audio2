

Origin and arrangement of Ogg Vobis source code: http://www.xiph.org/downloads/

ogg - libogg-1.3.1
- copied .c's in src folder to oggvorbis/ogg/
- initialized  "ogg_int16_t temp = 0;" on os.h line 97 to silence warning.


vorbis - libvorbis-1.3.3
- copied .[ch]'s from lib folder into oggvorbis/vorbis/
- removed barkmel.c, and tune.c, psytune.c (utility programs)