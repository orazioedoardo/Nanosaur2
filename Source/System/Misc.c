/****************************/
/*      MISC ROUTINES       */
/* By Brian Greenstone      */
/* (c)2003 Pangea Software  */
/* (c)2022 Iliyas Jorio     */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include "game.h"


/****************************/
/*    CONSTANTS             */
/****************************/

#define	MAX_FPS				300		// mac original was 190
#define	DEFAULT_FPS			13

#define	PTRCOOKIE_SIZE		16


/**********************/
/*     VARIABLES      */
/**********************/

long	gRAMAlloced = 0;

uint32_t 	gSeed0 = 0, gSeed1 = 0, gSeed2 = 0;

float	gFramesPerSecond = DEFAULT_FPS;
float	gFramesPerSecondFrac = 1.0f / DEFAULT_FPS;

int		gNumPointers = 0;


/**********************/
/*     PROTOTYPES     */
/**********************/


/*********************** DO ALERT *******************/

void DoAlert(const char* format, ...)
{
	Enter2D();

	char message[1024];
	va_list args;
	va_start(args, format);
	SDL_vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	SDL_Log("Nanosaur 2 Alert: %s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Nanosaur 2", message, /*gSDLWindow*/NULL);

	Exit2D();
}


/*********************** DO FATAL ALERT *******************/

void DoFatalAlert(const char* format, ...)
{
	Enter2D();

	char message[1024];
	va_list args;
	va_start(args, format);
	SDL_vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	SDL_Log("Nanosaur 2 Fatal Alert: %s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Nanosaur 2", message, /*gSDLWindow*/NULL);

	Exit2D();
	CleanQuit();
}


/************ CLEAN QUIT ***************/

void CleanQuit(void)
{
static Boolean	beenHere = false;


	if (!beenHere)
	{
		beenHere = true;

		SavePrefs();									// save prefs before bailing

		DeleteAllObjects();
		DisposeTerrain();								// dispose of any memory allocated by terrain manager
		DisposeAllBG3DContainers();						// nuke all models
		DisposeAllSpriteGroups();						// nuke all sprites
		DisposeAllSpriteAtlases();

		if (gGameViewInfoPtr)							// see if need to dispose this
			OGL_DisposeGameView();

		OGL_Shutdown();

		ShutdownSound();								// cleanup sound stuff
	}

	SDL_ShowCursor();
	MyFlushEvents();

	ExitToShell();
}



#pragma mark -


/******************** MY RANDOM LONG **********************/
//
// My own random number generator that returns a LONG
//
// NOTE: call this instead of MyRandomShort if the value is going to be
//		masked or if it just doesnt matter since this version is quicker
//		without the 0xffff at the end.
//

uint32_t MyRandomLong(void)
{
  return gSeed2 ^= (((gSeed1 ^= (gSeed2>>5)*1568397607UL)>>7)+
                   (gSeed0 = (gSeed0+1)*3141592621UL))*2435386481UL;
}


/************************* RANDOM RANGE *************************/
//
// THE RANGE *IS* INCLUSIVE OF MIN AND MAX
//

uint16_t	RandomRange(unsigned short min, unsigned short max)
{
uint16_t		qdRdm;											// treat return value as 0-65536
uint32_t		range, t;

	qdRdm = MyRandomLong();
	range = max+1 - min;
	t = (qdRdm * range)>>16;	 							// now 0 <= t <= range

	return( t+min );
}



/************** RANDOM FLOAT ********************/
//
// returns a random float between 0 and 1
//

float RandomFloat(void)
{
unsigned long	r;
float	f;

	r = MyRandomLong() & 0xfff;
	if (r == 0)
		return(0);

	f = (float)r;							// convert to float
	f = f * (1.0f/(float)0xfff);			// get # between 0..1
	return(f);
}


/************** RANDOM FLOAT 2 ********************/
//
// returns a random float between -1 and +1
//

float RandomFloat2(void)
{
unsigned long	r;
float	f;

	r = MyRandomLong() & 0xfff;
	if (r == 0)
		return(0);

	f = (float)r;							// convert to float
	f = f * (2.0f/(float)0xfff);			// get # between 0..2
	f -= 1.0f;								// get -1..+1
	return(f);
}



/**************** SET MY RANDOM SEED *******************/

void SetMyRandomSeed(uint32_t seed)
{
	gSeed0 = seed;
	gSeed1 = 0;
	gSeed2 = 0;

}

/**************** INIT MY RANDOM SEED *******************/

void InitMyRandomSeed(void)
{
	gSeed0 = 0x2a80ce30;
	gSeed1 = 0;
	gSeed2 = 0;
}


#pragma mark -


/****************** ALLOC PTR ********************/

void *AllocPtr(long size)
{
	GAME_ASSERT(size >= 0);
	GAME_ASSERT(size <= 0x7FFFFFFF);

	size += PTRCOOKIE_SIZE;						// make room for our cookie & whatever else (also keep to 16-byte alignment!)
	Ptr p = SDL_malloc(size);
	GAME_ASSERT(p);

	uint32_t* cookiePtr = (uint32_t *)p;
	cookiePtr[0] = 'FACE';
	cookiePtr[1] = (uint32_t) size;
	cookiePtr[2] = 'PTR3';
	cookiePtr[3] = 'PTR4';

	gNumPointers++;
	gRAMAlloced += size;

	return p + PTRCOOKIE_SIZE;
}


/****************** ALLOC PTR CLEAR ********************/

void *AllocPtrClear(long size)
{
	GAME_ASSERT(size >= 0);
	GAME_ASSERT(size <= 0x7FFFFFFF);

	size += PTRCOOKIE_SIZE;						// make room for our cookie & whatever else (also keep to 16-byte alignment!)
	Ptr p = SDL_calloc(1, size);
	GAME_ASSERT(p);

	uint32_t* cookiePtr = (uint32_t *)p;
	cookiePtr[0] = 'FACE';
	cookiePtr[1] = (uint32_t) size;
	cookiePtr[2] = 'PTC3';
	cookiePtr[3] = 'PTC4';

	gNumPointers++;
	gRAMAlloced += size;

	return p + PTRCOOKIE_SIZE;
}


/****************** REALLOC PTR ********************/

void* ReallocPtr(void* initialPtr, long newSize)
{
	GAME_ASSERT(newSize >= 0);
	GAME_ASSERT(newSize <= 0x7FFFFFFF);

	if (initialPtr == NULL)
	{
		return AllocPtr(newSize);
	}

	Ptr p = ((Ptr)initialPtr) - PTRCOOKIE_SIZE;	// back up pointer to cookie
	newSize += PTRCOOKIE_SIZE;					// make room for our cookie & whatever else (also keep to 16-byte alignment!)

	p = SDL_realloc(p, newSize);				// reallocate it
	GAME_ASSERT(p);

	uint32_t* cookiePtr = (uint32_t *)p;
	GAME_ASSERT(cookiePtr[0] == 'FACE');		// realloc shouldn't have touched our cookie

	uint32_t initialSize = cookiePtr[1];		// update heap size metric
	gRAMAlloced += newSize - initialSize;

	cookiePtr[0] = 'FACE';						// rewrite cookie
	cookiePtr[1] = (uint32_t) newSize;
	cookiePtr[2] = 'REA3';
	cookiePtr[3] = 'REA4';

	return p + PTRCOOKIE_SIZE;
}


/***************** SAFE DISPOSE PTR ***********************/

void SafeDisposePtr(void *ptr)
{
	if (ptr == NULL)
	{
		return;
	}

	Ptr p = ((Ptr)ptr) - PTRCOOKIE_SIZE;			// back up to pt to cookie

	uint32_t* cookiePtr = (uint32_t *)p;
	GAME_ASSERT(cookiePtr[0] == 'FACE');
	gRAMAlloced -= cookiePtr[1];					// deduct ptr size from heap size

	cookiePtr[0] = 'DEAD';							// zap cookie

	SDL_free(p);

	gNumPointers--;
}



#pragma mark -


/******************* VERIFY SYSTEM ******************/

void VerifySystem(void)
{
}


#pragma mark -



/************** CALC FRAMES PER SECOND *****************/
//
// This version uses UpTime() which is only available on PCI Macs.
//

void CalcFramesPerSecond(void)
{
static UnsignedWide time;
UnsignedWide currTime;
unsigned long deltaTime;
float		fps;

int				i;
static int		sampIndex = 0;
static float	sampleList[16] = {60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60};

wait:
	Microseconds(&currTime);

	if (gTimeDemo)
	{
		fps = 40;
	}
	else
	{
		deltaTime = currTime.lo - time.lo;

		if (deltaTime == 0)
		{
			fps = DEFAULT_FPS;
		}
		else
		{
			fps = 1000000.0f / deltaTime;

			if (fps < DEFAULT_FPS)					// (avoid divide by 0's later)
			{
				fps = DEFAULT_FPS;
			}
			else if (fps > MAX_FPS)					// limit to avoid issue
			{
				if (fps - MAX_FPS > 1000)			// try to sneak in some sleep if we have 1 ms to spare
				{
					SDL_Delay(1);
				}
				goto wait;
			}
		}

#if _DEBUG
		if (GetKeyState(SDL_SCANCODE_KP_PLUS))		// debug speed-up with KP_PLUS
			fps = DEFAULT_FPS;
#endif
	}

			/* ADD TO LIST */

	sampleList[sampIndex] = fps;
	sampIndex++;
	sampIndex &= 0x7;


			/* CALC AVERAGE */

	gFramesPerSecond = 0;
	for (i = 0; i < 8; i++)
		gFramesPerSecond += sampleList[i];
	gFramesPerSecond *= 1.0f / 8.0f;



	if (gFramesPerSecond < DEFAULT_FPS)			// (avoid divide by 0's later)
		gFramesPerSecond = DEFAULT_FPS;
	gFramesPerSecondFrac = 1.0f/gFramesPerSecond;		// calc fractional for multiplication


	time = currTime;	// reset for next time interval
}


/********************* IS POWER OF 2 ****************************/

Boolean IsPowerOf2(int num)
{
int		i;

	i = 2;
	do
	{
		if (i == num)				// see if this power of 2 matches
			return(true);
		i *= 2;						// next power of 2
	}while(i <= num);				// search until power is > number

	return(false);
}

#pragma mark-

/******************* MY FLUSH EVENTS **********************/
//
// This call makes damed sure that there are no events anywhere in any event queue.
//

void MyFlushEvents(void)
{
}


#pragma mark -

/********************* SWIZZLE SHORT **************************/

int16_t SwizzleShort(const int16_t *shortPtr)
{
	return (int16_t) SwizzleUShort((const uint16_t*) shortPtr);
}


/********************* SWIZZLE USHORT **************************/

uint16_t SwizzleUShort(const uint16_t *shortPtr)
{
uint16_t	theShort = *shortPtr;

#if __LITTLE_ENDIAN__

	uint32_t	b1 = theShort & 0xff;
	uint32_t	b2 = (theShort & 0xff00) >> 8;

	theShort = (b1 << 8) | b2;
#endif

	return(theShort);
}



/********************* SWIZZLE LONG **************************/

int32_t SwizzleLong(const int32_t *longPtr)
{
	return (int32_t) SwizzleULong((const uint32_t*) longPtr);

}


/********************* SWIZZLE U LONG **************************/

uint32_t SwizzleULong(const uint32_t *longPtr)
{
uint32_t	theLong = *longPtr;

#if __LITTLE_ENDIAN__

	uint32_t	b1 = theLong & 0xff;
	uint32_t	b2 = (theLong & 0xff00) >> 8;
	uint32_t	b3 = (theLong & 0xff0000) >> 16;
	uint32_t	b4 = (theLong & 0xff000000) >> 24;

	theLong = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;

#endif

	return(theLong);
}


/********************* SWIZZLE FLOAT **************************/

float SwizzleFloat(const float *floatPtr)
{
	const void* blob = floatPtr;

	uint32_t theLong = SwizzleULong((const uint32_t *) blob);

	blob = &theLong;

	return *((const float *) blob);
}
