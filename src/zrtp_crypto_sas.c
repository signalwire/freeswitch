/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

/* These 3-syllable words are no longer than 11 characters. */
extern uint8_t hash_word_list_odd[256][12];

/* These 2-syllable words are no longer than 9 characters. */
extern uint8_t hash_word_list_even[256][10];

/*----------------------------------------------------------------------------*/
/*
 * copyright 2002, 2003 Bryce "Zooko" Wilcox-O'Hearn
 * mailto:zooko@zooko.com
 *
 * See the end of this file for the free software, open source license (BSD-style).
 */

/**
 * Copyright (c) 2002 Bryce "Zooko" Wilcox-O'Hearn
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software to deal in this software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of this software, and to permit
 * persons to whom this software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of this software.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THIS SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THIS SOFTWARE.
 */

zrtp_status_t b2a(zrtp_stringn_t *os, zrtp_stringn_t *result)
{
    static const char chars[]="ybndrfg8ejkmcpqxot1uwisza345h769";
	
    if (!os || !result) {
		return zrtp_status_bad_param;
    } else { 
		/* pointer into the os buffer, initially pointing to the "one-past-the-end" octet */
		const uint8_t* osp = (uint8_t*)os->buffer + os->length;
		/* pointer into the result buffer, initially pointing to the "one-past-the-end" quintet */
		uint8_t* resp;
		/* to hold up to 32 bits worth of the input */
		uint32_t x = 0;
		
		result->length = os->length*8;
		result->length = (result->length % 5) ? ((result->length/5) + 1) : result->length/5;
		
		/* pointer into the result buffer, initially pointing to the "one-past-the-end" quintet */
		resp = (uint8_t*)result->buffer + result->length;
		
		/* Now this is a real live Duff's device.  You gotta love it. */
		switch ((osp - (uint8_t*)os->buffer) % 5) {
			case 0:
				do {
					x = *--osp;
					*--resp = chars[x % 32]; /* The least sig 5 bits go into the final quintet. */
					x /= 32; /* ... now we have 3 bits worth in x... */
				case 4:
					x |= ((uint32_t)(*--osp)) << 3; /* ... now we have 11 bits worth in x... */
					*--resp = chars[x % 32];
					x /= 32; /* ... now we have 6 bits worth in x... */
					*--resp = chars[x % 32];
					x /= 32; /* ... now we have 1 bits worth in x... */
				case 3:
					/* The 8 bits from the 2-indexed octet.  So now we have 9 bits worth in x... */
					x |= ((uint32_t)(*--osp)) << 1;
					*--resp = chars[x % 32];
					x /= 32; /* ... now we have 4 bits worth in x... */
				case 2:
					/* The 8 bits from the 1-indexed octet.  So now we have 12 bits worth in x... */
					x |= ((uint32_t)(*--osp)) << 4;
					*--resp = chars[x%32];
					x /= 32; /* ... now we have 7 bits worth in x... */
					*--resp = chars[x%32];
					x /= 32; /* ... now we have 2 bits worth in x... */
				case 1:
					/* The 8 bits from the 0-indexed octet.  So now we have 10 bits worth in x... */
					x |= ((uint32_t)(*--osp)) << 2;
					*--resp = chars[x%32];
					x /= 32; /* ... now we have 5 bits worth in x... */
					*--resp = chars[x];
				} while (osp > (const uint8_t *)os->buffer);
		} /* switch ((osp - os.buf) % 5) */
		
		return zrtp_status_ok;
	}
}

/*----------------------------------------------------------------------------*/
static zrtp_status_t SAS32_compute( zrtp_sas_scheme_t *self,
								    zrtp_stream_t *stream,
									zrtp_hash_t *hash,									
									uint8_t is_transferred )
{
	zrtp_session_t *session = stream->session;
	static const zrtp_string16_t sas_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_SAS_STR);
	
    zrtp_string64_t sas_digest = ZSTR_INIT_EMPTY(sas_digest);
    zrtp_string8_t vad		   = ZSTR_INIT_EMPTY(vad);

	ZSTR_SET_EMPTY(session->sas1);
	ZSTR_SET_EMPTY(session->sas2);
	
	if (!is_transferred && !stream->protocol) {
		return zrtp_status_bad_param;
	}

	/*
	 * Generate SAS source as:
	 * sashash = KDF(ZRTPSess, "SAS", (ZIDi | ZIDr), 256)
	 */
	if (!is_transferred) {
		_zrtp_kdf( stream,
				  ZSTR_GV(stream->protocol->cc->s0),
				  ZSTR_GV(sas_label),
				  ZSTR_GV(stream->protocol->cc->kdf_context),
				  ZRTP_HASH_SIZE,
				  ZSTR_GV(sas_digest));
		

		/* Binary sas value is the leftmost ZRTP_SAS_DIGEST_LENGTH bytes */
		zrtp_zstrncpy(ZSTR_GV(session->sasbin), ZSTR_GV(sas_digest), ZRTP_SAS_DIGEST_LENGTH);	
	} else {
		zrtp_zstrcpy(ZSTR_GV(sas_digest), ZSTR_GV(session->sasbin));
	}

	/* Take the leftmost 20 bits from sas source and render bas32 value */
	sas_digest.length = 3;
	sas_digest.buffer[2] &= 0xF0;
    if (zrtp_status_ok == b2a(ZSTR_GV(sas_digest), ZSTR_GV(vad)) && vad.length >= 4) {
		zrtp_zstrncpy(ZSTR_GV(session->sas1), ZSTR_GV(vad), 4);
		return zrtp_status_ok;
    }

    return zrtp_status_fail;
}

/*----------------------------------------------------------------------------*/
static zrtp_status_t SAS256_compute( zrtp_sas_scheme_t *self,
									 zrtp_stream_t *stream,
									 zrtp_hash_t *hash,									
									 uint8_t is_transferred )
{
	zrtp_session_t *session = stream->session;
	ZSTR_SET_EMPTY(session->sas1);
	ZSTR_SET_EMPTY(session->sas2);
	
	if (!is_transferred && !stream->protocol) {
		return zrtp_status_bad_param;
	}

	/*
	 * Generate SAS source as:
	 * sashash = KDF(ZRTPSess, "SAS", (ZIDi | ZIDr), 256)
	 */
	if (!is_transferred)
	{
		static const zrtp_string16_t sas_label	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_SAS_STR);
		zrtp_string64_t sas_digest	= ZSTR_INIT_EMPTY(sas_digest);		
		
		_zrtp_kdf( stream,
				  ZSTR_GV(stream->protocol->cc->s0),
				  ZSTR_GV(sas_label),
				  ZSTR_GV(stream->protocol->cc->kdf_context),
				  ZRTP_HASH_SIZE,
				  ZSTR_GV(sas_digest));
		
		/* Binary sas value is last ZRTP_SAS_DIGEST_LENGTH bytes */
		zrtp_zstrncpy(ZSTR_GV(session->sasbin), ZSTR_GV(sas_digest), ZRTP_SAS_DIGEST_LENGTH);
	}
	
	zrtp_zstrcpyc(ZSTR_GV(session->sas1), (const char *)hash_word_list_even[(uint8_t)session->sasbin.buffer[0]]);
	zrtp_zstrcpyc(ZSTR_GV(session->sas2), (const char *)hash_word_list_odd[(uint8_t)session->sasbin.buffer[1]]);
	
    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_defaults_sas(zrtp_global_t* zrtp)
{
    zrtp_sas_scheme_t* base32 = zrtp_sys_alloc(sizeof(zrtp_sas_scheme_t));
	zrtp_sas_scheme_t* base256 = zrtp_sys_alloc(sizeof(zrtp_sas_scheme_t));

    if (!base32 || !base256) {
		if (base32) {
			zrtp_sys_free(base32);
		}
		if (base256) {
			zrtp_sys_free(base256);
		}
		return zrtp_status_alloc_fail;
    }

    zrtp_memset(base32, 0, sizeof(zrtp_sas_scheme_t));
    zrtp_memcpy(base32->base.type, ZRTP_B32, ZRTP_COMP_TYPE_SIZE);
	base32->base.id				= ZRTP_SAS_BASE32;
    base32->base.zrtp	= zrtp;
    base32->compute				= SAS32_compute;

    zrtp_memset(base256, 0, sizeof(zrtp_sas_scheme_t));
    zrtp_memcpy(base256->base.type, ZRTP_B256, ZRTP_COMP_TYPE_SIZE);
	base256->base.id			= ZRTP_SAS_BASE256;
    base256->base.zrtp	= zrtp;
    base256->compute			= SAS256_compute;

	zrtp_comp_register(ZRTP_CC_SAS, base32, zrtp);
    zrtp_comp_register(ZRTP_CC_SAS, base256, zrtp);
	
    return zrtp_status_ok;
}


uint8_t hash_word_list_odd[256][12] = {
    "adroitness",
    "adviser",
    "aftermath",
    "aggregate",
    "alkali",
    "almighty",
    "amulet",
    "amusement",
    "antenna",
    "applicant",
    "Apollo",
    "armistice",
    "article",
    "asteroid",
    "Atlantic",
    "atmosphere",
    "autopsy",
    "Babylon",
    "backwater",
    "barbecue",
    "belowground",
    "bifocals",
    "bodyguard",
    "bookseller",
    "borderline",
    "bottomless",
    "Bradbury",
    "bravado",
    "Brazilian",
    "breakaway",
    "Burlington",
    "businessman",
    "butterfat",
    "Camelot",
    "candidate",
    "cannonball",
    "Capricorn",
    "caravan",
    "caretaker",
    "celebrate",
    "cellulose",
    "certify",
    "chambermaid",
    "Cherokee",
    "Chicago",
    "clergyman",
    "coherence",
    "combustion",
    "commando",
    "company",
    "component",
    "concurrent",
    "confidence",
    "conformist",
    "congregate",
    "consensus",
    "consulting",
    "corporate",
    "corrosion",
    "councilman",
    "crossover",
    "crucifix",
    "cumbersome",
    "customer",
    "Dakota",
    "decadence",
    "December",
    "decimal",
    "designing",
    "detector",
    "detergent",
    "determine",
    "dictator",
    "dinosaur",
    "direction",
    "disable",
    "disbelief",
    "disruptive",
    "distortion",
    "document",
    "embezzle",
    "enchanting",
    "enrollment",
    "enterprise",
    "equation",
    "equipment",
    "escapade",
    "Eskimo",
    "everyday",
    "examine",
    "existence",
    "exodus",
    "fascinate",
    "filament",
    "finicky",
    "forever",
    "fortitude",
    "frequency",
    "gadgetry",
    "Galveston",
    "getaway",
    "glossary",
    "gossamer",
    "graduate",
    "gravity",
    "guitarist",
    "hamburger",
    "Hamilton",
    "handiwork",
    "hazardous",
    "headwaters",
    "hemisphere",
    "hesitate",
    "hideaway",
    "holiness",
    "hurricane",
    "hydraulic",
    "impartial",
    "impetus",
    "inception",
    "indigo",
    "inertia",
    "infancy",
    "inferno",
    "informant",
    "insincere",
    "insurgent",
    "integrate",
    "intention",
    "inventive",
    "Istanbul",
    "Jamaica",
    "Jupiter",
    "leprosy",
    "letterhead",
    "liberty",
    "maritime",
    "matchmaker",
    "maverick",
    "Medusa",
    "megaton",
    "microscope",
    "microwave",
    "midsummer",
    "millionaire",
    "miracle",
    "misnomer",
    "molasses",
    "molecule",
    "Montana",
    "monument",
    "mosquito",
    "narrative",
    "nebula",
    "newsletter",
    "Norwegian",
    "October",
    "Ohio",
    "onlooker",
    "opulent",
    "Orlando",
    "outfielder",
    "Pacific",
    "pandemic",
    "Pandora",
    "paperweight",
    "paragon",
    "paragraph",
    "paramount",
    "passenger",
    "pedigree",
    "Pegasus",
    "penetrate",
    "perceptive",
    "performance",
    "pharmacy",
    "phonetic",
    "photograph",
    "pioneer",
    "pocketful",
    "politeness",
    "positive",
    "potato",
    "processor",
    "provincial",
    "proximate",
    "puberty",
    "publisher",
    "pyramid",
    "quantity",
    "racketeer",
    "rebellion",
    "recipe",
    "recover",
    "repellent",
    "replica",
    "reproduce",
    "resistor",
    "responsive",
    "retraction",
    "retrieval",
    "retrospect",
    "revenue",
    "revival",
    "revolver",
    "sandalwood",
    "sardonic",
    "Saturday",
    "savagery",
    "scavenger",
    "sensation",
    "sociable",
    "souvenir",
    "specialist",
    "speculate",
    "stethoscope",
    "stupendous",
    "supportive",
    "surrender",
    "suspicious",
    "sympathy",
    "tambourine",
    "telephone",
    "therapist",
    "tobacco",
    "tolerance",
    "tomorrow",
    "torpedo",
    "tradition",
    "travesty",
    "trombonist",
    "truncated",
    "typewriter",
    "ultimate",
    "undaunted",
    "underfoot",
    "unicorn",
    "unify",
    "universe",
    "unravel",
    "upcoming",
    "vacancy",
    "vagabond",
    "vertigo",
    "Virginia",
    "visitor",
    "vocalist",
    "voyager",
    "warranty",
    "Waterloo",
    "whimsical",
    "Wichita",
    "Wilmington",
    "Wyoming",
    "yesteryear",
    "Yucatan"
    };

uint8_t hash_word_list_even[256][10] = {
    "aardvark",
    "absurd",
    "accrue",
    "acme",
    "adrift",
    "adult",
    "afflict",
    "ahead",
    "aimless",
    "Algol",
    "allow",
    "alone",
    "ammo",
    "ancient",
    "apple",
    "artist",
    "assume",
    "Athens",
    "atlas",
    "Aztec",
    "baboon",
    "backfield",
    "backward",
    "banjo",
    "beaming",
    "bedlamp",
    "beehive",
    "beeswax",
    "befriend",
    "Belfast",
    "berserk",
    "billiard",
    "bison",
    "blackjack",
    "blockade",
    "blowtorch",
    "bluebird",
    "bombast",
    "bookshelf",
    "brackish",
    "breadline",
    "breakup",
    "brickyard",
    "briefcase",
    "Burbank",
    "button",
    "buzzard",
    "cement",
    "chairlift",
    "chatter",
    "checkup",
    "chisel",
    "choking",
    "chopper",
    "Christmas",
    "clamshell",
    "classic",
    "classroom",
    "cleanup",
    "clockwork",
    "cobra",
    "commence",
    "concert",
    "cowbell",
    "crackdown",
    "cranky",
    "crowfoot",
    "crucial",
    "crumpled",
    "crusade",
    "cubic",
    "dashboard",
    "deadbolt",
    "deckhand",
    "dogsled",
    "dragnet",
    "drainage",
    "dreadful",
    "drifter",
    "dropper",
    "drumbeat",
    "drunken",
    "Dupont",
    "dwelling",
    "eating",
    "edict",
    "egghead",
    "eightball",
    "endorse",
    "endow",
    "enlist",
    "erase",
    "escape",
    "exceed",
    "eyeglass",
    "eyetooth",
    "facial",
    "fallout",
    "flagpole",
    "flatfoot",
    "flytrap",
    "fracture",
    "framework",
    "freedom",
    "frighten",
    "gazelle",
    "Geiger",
    "glitter",
    "glucose",
    "goggles",
    "goldfish",
    "gremlin",
    "guidance",
    "hamlet",
    "highchair",
    "hockey",
    "indoors",
    "indulge",
    "inverse",
    "involve",
    "island",
    "jawbone",
    "keyboard",
    "kickoff",
    "kiwi",
    "klaxon",
    "locale",
    "lockup",
    "merit",
    "minnow",
    "miser",
    "Mohawk",
    "mural",
    "music",
    "necklace",
    "Neptune",
    "newborn",
    "nightbird",
    "Oakland",
    "obtuse",
    "offload",
    "optic",
    "orca",
    "payday",
    "peachy",
    "pheasant",
    "physique",
    "playhouse",
    "Pluto",
    "preclude",
    "prefer",
    "preshrunk",
    "printer",
    "prowler",
    "pupil",
    "puppy",
    "python",
    "quadrant",
    "quiver",
    "quota",
    "ragtime",
    "ratchet",
    "rebirth",
    "reform",
    "regain",
    "reindeer",
    "rematch",
    "repay",
    "retouch",
    "revenge",
    "reward",
    "rhythm",
    "ribcage",
    "ringbolt",
    "robust",
    "rocker",
    "ruffled",
    "sailboat",
    "sawdust",
    "scallion",
    "scenic",
    "scorecard",
    "Scotland",
    "seabird",
    "select",
    "sentence",
    "shadow",
    "shamrock",
    "showgirl",
    "skullcap",
    "skydive",
    "slingshot",
    "slowdown",
    "snapline",
    "snapshot",
    "snowcap",
    "snowslide",
    "solo",
    "southward",
    "soybean",
    "spaniel",
    "spearhead",
    "spellbind",
    "spheroid",
    "spigot",
    "spindle",
    "spyglass",
    "stagehand",
    "stagnate",
    "stairway",
    "standard",
    "stapler",
    "steamship",
    "sterling",
    "stockman",
    "stopwatch",
    "stormy",
    "sugar",
    "surmount",
    "suspense",
    "sweatband",
    "swelter",
    "tactics",
    "talon",
    "tapeworm",
    "tempest",
    "tiger",
    "tissue",
    "tonic",
    "topmost",
    "tracker",
    "transit",
    "trauma",
    "treadmill",
    "Trojan",
    "trouble",
    "tumor",
    "tunnel",
    "tycoon",
    "uncut",
    "unearth",
    "unwind",
    "uproot",
    "upset",
    "upshot",
    "vapor",
    "village",
    "virus",
    "Vulcan",
    "waffle",
    "wallet",
    "watchword",
    "wayside",
    "willow",
    "woodlark",
    "Zulu"
    };
