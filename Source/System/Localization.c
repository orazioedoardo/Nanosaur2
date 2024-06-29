// LOCALIZATION.C
// (C) 2022 Iliyas Jorio
// This file is part of Nanosaur 2. https://github.com/jorio/nanosaur2

#include "game.h"
#include <string.h>
#include <SDL.h>

#define CSV_PATH ":System:strings.csv"

#define MAX_STRINGS (NUM_LOCALIZED_STRINGS + 1)

static GameLanguageID	gCurrentStringsLanguage = LANGUAGE_ILLEGAL;
static Ptr				gStringsBuffer = nil;
static const char*		gStringsTable[MAX_STRINGS];

static const char kLanguageCodesISO639_1[NUM_LANGUAGES][3] =
{
	[LANGUAGE_ENGLISH	] = "en",
	[LANGUAGE_FRENCH	] = "fr",
	[LANGUAGE_GERMAN	] = "de",
	[LANGUAGE_SPANISH	] = "es",
	[LANGUAGE_ITALIAN	] = "it",
	[LANGUAGE_SWEDISH	] = "sv",
	[LANGUAGE_DUTCH		] = "nl",
	[LANGUAGE_RUSSIAN	] = "ru",
};

void LoadLocalizedStrings(GameLanguageID languageID)
{
	// Don't bother reloading strings if we've already loaded this language
	if (languageID == gCurrentStringsLanguage)
	{
		return;
	}

	// Free previous strings buffer
	DisposeLocalizedStrings();

	GAME_ASSERT(languageID >= 0);
	GAME_ASSERT(languageID < NUM_LANGUAGES);

	long count = 0;
	gStringsBuffer = LoadTextFile(CSV_PATH, &count);

	for (int i = 0; i < MAX_STRINGS; i++)
		gStringsTable[i] = nil;
	gStringsTable[STR_NULL] = "???";
	_Static_assert(STR_NULL == 0, "STR_NULL must be 0!");

	int row = 1;	// start row at 1, so that 0 is an illegal index (STR_NULL)

	char* csvReader = gStringsBuffer;
	while (csvReader != NULL)
	{
		char* myPhrase = NULL;
		bool eol = false;

		for (int x = 0; !eol; x++)
		{
			char* phrase = CSVIterator(&csvReader, &eol);

			if (phrase &&
				phrase[0] &&
				(x == languageID || !myPhrase))
			{
				myPhrase = phrase;
			}
		}

		if (myPhrase != NULL)
		{
			gStringsTable[row] = myPhrase;
			row++;
		}
	}
}

const char* Localize(LocStrID stringID)
{
	if (!gStringsBuffer)
		return "STRINGS NOT LOADED";

	if (stringID < 0 || stringID >= MAX_STRINGS)
		return "ILLEGAL STRING ID";

	if (!gStringsTable[stringID])
		return "";

	return gStringsTable[stringID];
}

int LocalizeWithPlaceholder(LocStrID stringID, char* buf0, size_t bufSize, const char* format, ...)
{
	char* buf = buf0;
	const char* localizedString = Localize(stringID);

	const char* placeholder = strchr(localizedString, '#');

	if (!placeholder)
	{
		goto fail;
	}

	size_t bytesBeforePlaceholder = placeholder - localizedString;

	if (bytesBeforePlaceholder + 1 >= bufSize)		// +1 for nul terminator
	{
		goto fail;
	}

	memcpy(buf, localizedString, bytesBeforePlaceholder);
	buf += bytesBeforePlaceholder;
	bufSize -= bytesBeforePlaceholder;

	va_list args;
	va_start(args, format);
	int rc = vsnprintf(buf, bufSize, format, args);
	va_end(args);

	if (rc >= 0)
	{
		buf += rc;
		bufSize -= rc;

		rc = snprintf(buf, bufSize, "%s", placeholder + 1);
		if (rc >= 0)
		{
			buf += rc;
			bufSize -= rc;
		}
	}

	return (int) (buf - buf0);

fail:
	return snprintf(buf, bufSize, "%s", localizedString);
}

bool IsNativeEnglishSystem(void)
{
	bool prefersEnglish = true;

#if SDL_VERSION_ATLEAST(2,0,14)
	SDL_Locale* localeList = SDL_GetPreferredLocales();

	if (NULL != localeList
		&& 0 != localeList[0].language
		&& (0 != strncmp(localeList[0].language, kLanguageCodesISO639_1[LANGUAGE_ENGLISH], 2)))
	{
		prefersEnglish = false;
	}

	SDL_free(localeList);
#endif

	return prefersEnglish;
}

GameLanguageID GetBestLanguageIDFromSystemLocale(void)
{
	GameLanguageID languageID = LANGUAGE_ENGLISH;

#if !(SDL_VERSION_ATLEAST(2,0,14))
	#warning Please upgrade to SDL 2.0.14 or later for SDL_GetPreferredLocales. Will default to English for now.
#else
	SDL_Locale* localeList = SDL_GetPreferredLocales();
	if (!localeList)
		return languageID;

	for (SDL_Locale* locale = localeList; locale->language; locale++)
	{
		for (int i = 0; i < NUM_LANGUAGES; i++)
		{
			if (0 == strncmp(locale->language, kLanguageCodesISO639_1[i], 2))
			{
				languageID = i;
				goto foundLocale;
			}
		}
	}

foundLocale:
	SDL_free(localeList);
#endif

	return languageID;
}

void DisposeLocalizedStrings(void)
{
	if (gStringsBuffer != nil)
	{
		SafeDisposePtr(gStringsBuffer);
		gStringsBuffer = nil;
	}
}
