
#ifndef SHELLFM_MIX
#define SHELLFM_MIX

/*
	The volume consists of two bytes, the higher one for the right channel, the
	lower one for the left. The adjust function takes a whole volume word,
	which is added to the volume read from the mixer device, so if you call
	adjust, make sure the bytes are equal. The STEP macro below is a good value
	to increase/decrease the volume, however, anything may be used.
*/
#define STEP 0x0202

#define VOL 0
#define PCM 4

extern signed adjust(signed, int);

#endif
