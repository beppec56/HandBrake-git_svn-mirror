/***************************************************************************
 *            hb-backend.c
 *
 *  Fri Mar 28 10:38:44 2008
 *  Copyright  2008-2015  John Stebbins
 *  <john at stebbins dot name>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
#define _GNU_SOURCE
#include <limits.h>
#include <math.h>
#include "hb.h"
#include "ghbcompat.h"
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include "hb-backend.h"
#include "settings.h"
#include "callbacks.h"
#include "subtitlehandler.h"
#include "audiohandler.h"
#include "videohandler.h"
#include "x264handler.h"
#include "preview.h"
#include "values.h"
#include "lang.h"
#include "jansson.h"

typedef struct
{
    gchar *option;
    const gchar *shortOpt;
    gdouble ivalue;
    const gchar *svalue;
} options_map_t;

typedef struct
{
    gint count;
    options_map_t *map;
} combo_opts_t;

static options_map_t d_subtitle_track_sel_opts[] =
{
    {N_("None"),                                    "none",       0, "0"},
    {N_("First Track Matching Selected Languages"), "first",      1, "1"},
    {N_("All Tracks Matching Selected Languages"),  "all",        2, "2"},
};
combo_opts_t subtitle_track_sel_opts =
{
    sizeof(d_subtitle_track_sel_opts)/sizeof(options_map_t),
    d_subtitle_track_sel_opts
};

static options_map_t d_subtitle_burn_opts[] =
{
    {N_("None"),                                     "none",          0, "0"},
    {N_("Foreign Audio Subtitle Track"),             "foreign",       1, "1"},
    {N_("First Selected Track"),                     "first",         2, "2"},
    {N_("Foreign Audio, then First Selected Track"), "foreign_first", 3, "3"},
};
combo_opts_t subtitle_burn_opts =
{
    sizeof(d_subtitle_burn_opts)/sizeof(options_map_t),
    d_subtitle_burn_opts
};

static options_map_t d_audio_track_sel_opts[] =
{
    {N_("None"),                                    "none",       0, "0"},
    {N_("First Track Matching Selected Languages"), "first",      1, "1"},
    {N_("All Tracks Matching Selected Languages"),  "all",        2, "2"},
};
combo_opts_t audio_track_sel_opts =
{
    sizeof(d_audio_track_sel_opts)/sizeof(options_map_t),
    d_audio_track_sel_opts
};

static options_map_t d_point_to_point_opts[] =
{
    {N_("Chapters:"), "chapter", 0, "0"},
    {N_("Seconds:"),  "time",    1, "1"},
    {N_("Frames:"),   "frame",   2, "2"},
};
combo_opts_t point_to_point_opts =
{
    sizeof(d_point_to_point_opts)/sizeof(options_map_t),
    d_point_to_point_opts
};

static options_map_t d_when_complete_opts[] =
{
    {N_("Do Nothing"),            "nothing",  0, "0"},
    {N_("Show Notification"),     "notify",   1, "1"},
    {N_("Quit Handbrake"),        "quit",     4, "4"},
    {N_("Put Computer To Sleep"), "sleep",    2, "2"},
    {N_("Shutdown Computer"),     "shutdown", 3, "3"},
};
combo_opts_t when_complete_opts =
{
    sizeof(d_when_complete_opts)/sizeof(options_map_t),
    d_when_complete_opts
};

static options_map_t d_par_opts[] =
{
    {N_("Off"),    "0", 0, "0"},
    {N_("Strict"), "1", 1, "1"},
    {N_("Loose"),  "2", 2, "2"},
    {N_("Custom"), "3", 3, "3"},
};
combo_opts_t par_opts =
{
    sizeof(d_par_opts)/sizeof(options_map_t),
    d_par_opts
};

static options_map_t d_alignment_opts[] =
{
    {"2", "2", 2, "2"},
    {"4", "4", 4, "4"},
    {"8", "8", 8, "8"},
    {"16", "16", 16, "16"},
};
combo_opts_t alignment_opts =
{
    sizeof(d_alignment_opts)/sizeof(options_map_t),
    d_alignment_opts
};

static options_map_t d_logging_opts[] =
{
    {"0", "0", 0, "0"},
    {"1", "1", 1, "1"},
    {"2", "2", 2, "2"},
};
combo_opts_t logging_opts =
{
    sizeof(d_logging_opts)/sizeof(options_map_t),
    d_logging_opts
};

static options_map_t d_log_longevity_opts[] =
{
    {N_("Week"),     "week",     7, "7"},
    {N_("Month"),    "month",    30, "30"},
    {N_("Year"),     "year",     365, "365"},
    {N_("Immortal"), "immortal", 366, "366"},
};
combo_opts_t log_longevity_opts =
{
    sizeof(d_log_longevity_opts)/sizeof(options_map_t),
    d_log_longevity_opts
};

static options_map_t d_appcast_update_opts[] =
{
    {N_("Never"),   "never", 0, "never"},
    {N_("Daily"),   "daily", 1, "daily"},
    {N_("Weekly"),  "weekly", 2, "weekly"},
    {N_("Monthly"), "monthly", 3, "monthly"},
};
combo_opts_t appcast_update_opts =
{
    sizeof(d_appcast_update_opts)/sizeof(options_map_t),
    d_appcast_update_opts
};

static options_map_t d_vqual_granularity_opts[] =
{
    {"0.2",  "0.2",  0.2,  "0.2"},
    {"0.25", "0.25", 0.25, "0.25"},
    {"0.5",  "0.5",  0.5,  "0.5"},
    {"1",    "1",    1,    "1"},
};
combo_opts_t vqual_granularity_opts =
{
    sizeof(d_vqual_granularity_opts)/sizeof(options_map_t),
    d_vqual_granularity_opts
};

static options_map_t d_detel_opts[] =
{
    {N_("Off"),    "off",   0, ""},
    {N_("Custom"), "custom", 1, ""},
    {N_("Default"),"default",2, NULL},
};
combo_opts_t detel_opts =
{
    sizeof(d_detel_opts)/sizeof(options_map_t),
    d_detel_opts
};

static options_map_t d_decomb_opts[] =
{
    {N_("Off"),    "off",   0, ""},
    {N_("Custom"), "custom", 1, ""},
    {N_("Default"),"default",2, NULL},
    {N_("Fast"),   "fast",   3, "7:2:6:9:1:80"},
    {N_("Bob"),    "bob",    4, "455"},
};
combo_opts_t decomb_opts =
{
    sizeof(d_decomb_opts)/sizeof(options_map_t),
    d_decomb_opts
};

static options_map_t d_deint_opts[] =
{
    {N_("Off"),    "off",    0,  ""},
    {N_("Custom"), "custom", 1,  ""},
    {N_("Fast"),   "fast",   2,  "0:-1:-1:0:1"},
    {N_("Slow"),   "slow",   3,  "1:-1:-1:0:1"},
    {N_("Slower"), "slower", 4,  "3:-1:-1:0:1"},
    {N_("Bob"),    "bob",    5, "15:-1:-1:0:1"},
};
combo_opts_t deint_opts =
{
    sizeof(d_deint_opts)/sizeof(options_map_t),
    d_deint_opts
};

static options_map_t d_denoise_opts[] =
{
    {N_("Off"),     "off",     0, ""},
    {N_("NLMeans"), "nlmeans", 1, ""},
    {N_("HQDN3D"),  "hqdn3d",  2, ""},
};
combo_opts_t denoise_opts =
{
    sizeof(d_denoise_opts)/sizeof(options_map_t),
    d_denoise_opts
};

static options_map_t d_denoise_preset_opts[] =
{
    {N_("Custom"),     "custom",     1, ""},
    {N_("Ultralight"), "ultralight", 5, ""},
    {N_("Light"),      "light",      2, ""},
    {N_("Medium"),     "medium",     3, ""},
    {N_("Strong"),     "strong",     4, ""},
};
combo_opts_t denoise_preset_opts =
{
    sizeof(d_denoise_preset_opts)/sizeof(options_map_t),
    d_denoise_preset_opts
};

static options_map_t d_nlmeans_tune_opts[] =
{
    {N_("None"),        "none",       0, ""},
    {N_("Film"),        "film",       1, ""},
    {N_("Grain"),       "grain",      2, ""},
    {N_("High Motion"), "highmotion", 3, ""},
    {N_("Animation"),   "animation",  4, ""},
};
combo_opts_t nlmeans_tune_opts =
{
    sizeof(d_nlmeans_tune_opts)/sizeof(options_map_t),
    d_nlmeans_tune_opts
};

static options_map_t d_direct_opts[] =
{
    {N_("None"),      "none",     0, "none"},
    {N_("Spatial"),   "spatial",  1, "spatial"},
    {N_("Temporal"),  "temporal", 2, "temporal"},
    {N_("Automatic"), "auto",     3, "auto"},
};
combo_opts_t direct_opts =
{
    sizeof(d_direct_opts)/sizeof(options_map_t),
    d_direct_opts
};

static options_map_t d_badapt_opts[] =
{
    {N_("Off"),             "0", 0, "0"},
    {N_("Fast"),            "1", 1, "1"},
    {N_("Optimal"),         "2", 2, "2"},
};
combo_opts_t badapt_opts =
{
    sizeof(d_badapt_opts)/sizeof(options_map_t),
    d_badapt_opts
};

static options_map_t d_bpyramid_opts[] =
{
    {N_("Off"),    "none",   0, "none"},
    {N_("Strict"), "strict", 1, "strict"},
    {N_("Normal"), "normal", 2, "normal"},
};
combo_opts_t bpyramid_opts =
{
    sizeof(d_bpyramid_opts)/sizeof(options_map_t),
    d_bpyramid_opts
};

static options_map_t d_weightp_opts[] =
{
    {N_("Off"),    "0", 0, "0"},
    {N_("Simple"), "1", 1, "1"},
    {N_("Smart"),  "2", 2, "2"},
};
combo_opts_t weightp_opts =
{
    sizeof(d_weightp_opts)/sizeof(options_map_t),
    d_weightp_opts
};

static options_map_t d_me_opts[] =
{
    {N_("Diamond"),              "dia",  0, "dia"},
    {N_("Hexagon"),              "hex",  1, "hex"},
    {N_("Uneven Multi-Hexagon"), "umh",  2, "umh"},
    {N_("Exhaustive"),           "esa",  3, "esa"},
    {N_("Hadamard Exhaustive"),  "tesa", 4, "tesa"},
};
combo_opts_t me_opts =
{
    sizeof(d_me_opts)/sizeof(options_map_t),
    d_me_opts
};

static options_map_t d_subme_opts[] =
{
    {N_("0: SAD, no subpel"),          "0", 0, "0"},
    {N_("1: SAD, qpel"),               "1", 1, "1"},
    {N_("2: SATD, qpel"),              "2", 2, "2"},
    {N_("3: SATD: multi-qpel"),        "3", 3, "3"},
    {N_("4: SATD, qpel on all"),       "4", 4, "4"},
    {N_("5: SATD, multi-qpel on all"), "5", 5, "5"},
    {N_("6: RD in I/P-frames"),        "6", 6, "6"},
    {N_("7: RD in all frames"),        "7", 7, "7"},
    {N_("8: RD refine in I/P-frames"), "8", 8, "8"},
    {N_("9: RD refine in all frames"), "9", 9, "9"},
    {N_("10: QPRD in all frames"),     "10", 10, "10"},
    {N_("11: No early terminations in analysis"), "11", 11, "11"},
};
combo_opts_t subme_opts =
{
    sizeof(d_subme_opts)/sizeof(options_map_t),
    d_subme_opts
};

static options_map_t d_analyse_opts[] =
{
    {N_("Most"), "p8x8,b8x8,i8x8,i4x4", 0, "p8x8,b8x8,i8x8,i4x4"},
    {N_("None"), "none", 1, "none"},
    {N_("Some"), "i4x4,i8x8", 2, "i4x4,i8x8"},
    {N_("All"),  "all",  3, "all"},
    {N_("Custom"),  "custom",  4, "all"},
};
combo_opts_t analyse_opts =
{
    sizeof(d_analyse_opts)/sizeof(options_map_t),
    d_analyse_opts
};

static options_map_t d_trellis_opts[] =
{
    {N_("Off"),         "0", 0, "0"},
    {N_("Encode only"), "1", 1, "1"},
    {N_("Always"),      "2", 2, "2"},
};
combo_opts_t trellis_opts =
{
    sizeof(d_trellis_opts)/sizeof(options_map_t),
    d_trellis_opts
};

typedef struct
{
    const gchar *name;
    combo_opts_t *opts;
} combo_name_map_t;

combo_name_map_t combo_name_map[] =
{
    {"SubtitleTrackSelectionBehavior", &subtitle_track_sel_opts},
    {"SubtitleBurnBehavior", &subtitle_burn_opts},
    {"AudioTrackSelectionBehavior", &audio_track_sel_opts},
    {"PtoPType", &point_to_point_opts},
    {"WhenComplete", &when_complete_opts},
    {"PicturePAR", &par_opts},
    {"PictureModulus", &alignment_opts},
    {"LoggingLevel", &logging_opts},
    {"LogLongevity", &log_longevity_opts},
    {"check_updates", &appcast_update_opts},
    {"VideoQualityGranularity", &vqual_granularity_opts},
    {"PictureDeinterlace", &deint_opts},
    {"PictureDecomb", &decomb_opts},
    {"PictureDetelecine", &detel_opts},
    {"PictureDenoiseFilter", &denoise_opts},
    {"PictureDenoisePreset", &denoise_preset_opts},
    {"PictureDenoiseTune", &nlmeans_tune_opts},
    {"x264_direct", &direct_opts},
    {"x264_b_adapt", &badapt_opts},
    {"x264_bpyramid", &bpyramid_opts},
    {"x264_weighted_pframes", &weightp_opts},
    {"x264_me", &me_opts},
    {"x264_subme", &subme_opts},
    {"x264_analyse", &analyse_opts},
    {"x264_trellis", &trellis_opts},
    {NULL, NULL}
};

const gchar *srt_codeset_table[] =
{
    "ANSI_X3.4-1968",
    "ANSI_X3.4-1986",
    "ANSI_X3.4",
    "ANSI_X3.110-1983",
    "ANSI_X3.110",
    "ASCII",
    "ECMA-114",
    "ECMA-118",
    "ECMA-128",
    "ECMA-CYRILLIC",
    "IEC_P27-1",
    "ISO-8859-1",
    "ISO-8859-2",
    "ISO-8859-3",
    "ISO-8859-4",
    "ISO-8859-5",
    "ISO-8859-6",
    "ISO-8859-7",
    "ISO-8859-8",
    "ISO-8859-9",
    "ISO-8859-9E",
    "ISO-8859-10",
    "ISO-8859-11",
    "ISO-8859-13",
    "ISO-8859-14",
    "ISO-8859-15",
    "ISO-8859-16",
    "UTF-7",
    "UTF-8",
    "UTF-16",
    "UTF-16LE",
    "UTF-16BE",
    "UTF-32",
    "UTF-32LE",
    "UTF-32BE",
    NULL
};
#define SRT_TABLE_SIZE (sizeof(srt_codeset_table)/ sizeof(char*)-1)

#if 0
typedef struct iso639_lang_t
{
    char * eng_name;        /* Description in English */
    char * native_name;     /* Description in native language */
    char * iso639_1;       /* ISO-639-1 (2 characters) code */
    char * iso639_2;        /* ISO-639-2/t (3 character) code */
    char * iso639_2b;       /* ISO-639-2/b code (if different from above) */
} iso639_lang_t;
#endif

const iso639_lang_t ghb_language_table[] =
{
    { "Any", "", "zz", "und" },
    { "Afar", "", "aa", "aar" },
    { "Abkhazian", "", "ab", "abk" },
    { "Afrikaans", "", "af", "afr" },
    { "Akan", "", "ak", "aka" },
    { "Albanian", "", "sq", "sqi", "alb" },
    { "Amharic", "", "am", "amh" },
    { "Arabic", "", "ar", "ara" },
    { "Aragonese", "", "an", "arg" },
    { "Armenian", "", "hy", "hye", "arm" },
    { "Assamese", "", "as", "asm" },
    { "Avaric", "", "av", "ava" },
    { "Avestan", "", "ae", "ave" },
    { "Aymara", "", "ay", "aym" },
    { "Azerbaijani", "", "az", "aze" },
    { "Bashkir", "", "ba", "bak" },
    { "Bambara", "", "bm", "bam" },
    { "Basque", "", "eu", "eus", "baq" },
    { "Belarusian", "", "be", "bel" },
    { "Bengali", "", "bn", "ben" },
    { "Bihari", "", "bh", "bih" },
    { "Bislama", "", "bi", "bis" },
    { "Bosnian", "", "bs", "bos" },
    { "Breton", "", "br", "bre" },
    { "Bulgarian", "", "bg", "bul" },
    { "Burmese", "", "my", "mya", "bur" },
    { "Catalan", "", "ca", "cat" },
    { "Chamorro", "", "ch", "cha" },
    { "Chechen", "", "ce", "che" },
    { "Chinese", "", "zh", "zho", "chi" },
    { "Church Slavic", "", "cu", "chu" },
    { "Chuvash", "", "cv", "chv" },
    { "Cornish", "", "kw", "cor" },
    { "Corsican", "", "co", "cos" },
    { "Cree", "", "cr", "cre" },
    { "Czech", "", "cs", "ces", "cze" },
    { "Danish", "Dansk", "da", "dan" },
    { "German", "Deutsch", "de", "deu", "ger" },
    { "Divehi", "", "dv", "div" },
    { "Dzongkha", "", "dz", "dzo" },
    { "English", "English", "en", "eng" },
    { "Spanish", "Espanol", "es", "spa" },
    { "Esperanto", "", "eo", "epo" },
    { "Estonian", "", "et", "est" },
    { "Ewe", "", "ee", "ewe" },
    { "Faroese", "", "fo", "fao" },
    { "Fijian", "", "fj", "fij" },
    { "French", "Francais", "fr", "fra", "fre" },
    { "Western Frisian", "", "fy", "fry" },
    { "Fulah", "", "ff", "ful" },
    { "Georgian", "", "ka", "kat", "geo" },
    { "Gaelic (Scots)", "", "gd", "gla" },
    { "Irish", "", "ga", "gle" },
    { "Galician", "", "gl", "glg" },
    { "Manx", "", "gv", "glv" },
    { "Greek, Modern", "", "el", "ell", "gre" },
    { "Guarani", "", "gn", "grn" },
    { "Gujarati", "", "gu", "guj" },
    { "Haitian", "", "ht", "hat" },
    { "Hausa", "", "ha", "hau" },
    { "Hebrew", "", "he", "heb" },
    { "Herero", "", "hz", "her" },
    { "Hindi", "", "hi", "hin" },
    { "Hiri Motu", "", "ho", "hmo" },
    { "Croatian", "Hrvatski", "hr", "hrv", "scr" },
    { "Igbo", "", "ig", "ibo" },
    { "Ido", "", "io", "ido" },
    { "Icelandic", "Islenska", "is", "isl", "ice" },
    { "Sichuan Yi", "", "ii", "iii" },
    { "Inuktitut", "", "iu", "iku" },
    { "Interlingue", "", "ie", "ile" },
    { "Interlingua", "", "ia", "ina" },
    { "Indonesian", "", "id", "ind" },
    { "Inupiaq", "", "ik", "ipk" },
    { "Italian", "Italiano", "it", "ita" },
    { "Javanese", "", "jv", "jav" },
    { "Japanese", "", "ja", "jpn" },
    { "Kalaallisut", "", "kl", "kal" },
    { "Kannada", "", "kn", "kan" },
    { "Kashmiri", "", "ks", "kas" },
    { "Kanuri", "", "kr", "kau" },
    { "Kazakh", "", "kk", "kaz" },
    { "Central Khmer", "", "km", "khm" },
    { "Kikuyu", "", "ki", "kik" },
    { "Kinyarwanda", "", "rw", "kin" },
    { "Kirghiz", "", "ky", "kir" },
    { "Komi", "", "kv", "kom" },
    { "Kongo", "", "kg", "kon" },
    { "Korean", "", "ko", "kor" },
    { "Kuanyama", "", "kj", "kua" },
    { "Kurdish", "", "ku", "kur" },
    { "Lao", "", "lo", "lao" },
    { "Latin", "", "la", "lat" },
    { "Latvian", "", "lv", "lav" },
    { "Limburgan", "", "li", "lim" },
    { "Lingala", "", "ln", "lin" },
    { "Lithuanian", "", "lt", "lit" },
    { "Luxembourgish", "", "lb", "ltz" },
    { "Luba-Katanga", "", "lu", "lub" },
    { "Ganda", "", "lg", "lug" },
    { "Macedonian", "", "mk", "mkd", "mac" },
    { "Hungarian", "Magyar", "hu", "hun" },
    { "Marshallese", "", "mh", "mah" },
    { "Malayalam", "", "ml", "mal" },
    { "Maori", "", "mi", "mri", "mao" },
    { "Marathi", "", "mr", "mar" },
    { "Malay", "", "ms", "msa", "msa" },
    { "Malagasy", "", "mg", "mlg" },
    { "Maltese", "", "mt", "mlt" },
    { "Moldavian", "", "mo", "mol" },
    { "Mongolian", "", "mn", "mon" },
    { "Nauru", "", "na", "nau" },
    { "Navajo", "", "nv", "nav" },
    { "Dutch", "Nederlands", "nl", "nld", "dut" },
    { "Ndebele, South", "", "nr", "nbl" },
    { "Ndebele, North", "", "nd", "nde" },
    { "Ndonga", "", "ng", "ndo" },
    { "Nepali", "", "ne", "nep" },
    { "Norwegian", "Norsk", "no", "nor" },
    { "Norwegian Nynorsk", "", "nn", "nno" },
    { "Norwegian Bokmål", "", "nb", "nob" },
    { "Chichewa; Nyanja", "", "ny", "nya" },
    { "Occitan", "", "oc", "oci" },
    { "Ojibwa", "", "oj", "oji" },
    { "Oriya", "", "or", "ori" },
    { "Oromo", "", "om", "orm" },
    { "Ossetian", "", "os", "oss" },
    { "Panjabi", "", "pa", "pan" },
    { "Persian", "", "fa", "fas", "per" },
    { "Pali", "", "pi", "pli" },
    { "Polish", "", "pl", "pol" },
    { "Portuguese", "Portugues", "pt", "por" },
    { "Pushto", "", "ps", "pus" },
    { "Quechua", "", "qu", "que" },
    { "Romansh", "", "rm", "roh" },
    { "Romanian", "", "ro", "ron", "rum" },
    { "Rundi", "", "rn", "run" },
    { "Russian", "", "ru", "rus" },
    { "Sango", "", "sg", "sag" },
    { "Sanskrit", "", "sa", "san" },
    { "Serbian", "", "sr", "srp", "scc" },
    { "Sinhala", "", "si", "sin" },
    { "Slovak", "", "sk", "slk", "slo" },
    { "Slovenian", "", "sl", "slv" },
    { "Northern Sami", "", "se", "sme" },
    { "Samoan", "", "sm", "smo" },
    { "Shona", "", "sn", "sna" },
    { "Sindhi", "", "sd", "snd" },
    { "Somali", "", "so", "som" },
    { "Sotho, Southern", "", "st", "sot" },
    { "Sardinian", "", "sc", "srd" },
    { "Swati", "", "ss", "ssw" },
    { "Sundanese", "", "su", "sun" },
    { "Finnish", "Suomi", "fi", "fin" },
    { "Swahili", "", "sw", "swa" },
    { "Swedish", "Svenska", "sv", "swe" },
    { "Tahitian", "", "ty", "tah" },
    { "Tamil", "", "ta", "tam" },
    { "Tatar", "", "tt", "tat" },
    { "Telugu", "", "te", "tel" },
    { "Tajik", "", "tg", "tgk" },
    { "Tagalog", "", "tl", "tgl" },
    { "Thai", "", "th", "tha" },
    { "Tibetan", "", "bo", "bod", "tib" },
    { "Tigrinya", "", "ti", "tir" },
    { "Tonga", "", "to", "ton" },
    { "Tswana", "", "tn", "tsn" },
    { "Tsonga", "", "ts", "tso" },
    { "Turkmen", "", "tk", "tuk" },
    { "Turkish", "", "tr", "tur" },
    { "Twi", "", "tw", "twi" },
    { "Uighur", "", "ug", "uig" },
    { "Ukrainian", "", "uk", "ukr" },
    { "Urdu", "", "ur", "urd" },
    { "Uzbek", "", "uz", "uzb" },
    { "Venda", "", "ve", "ven" },
    { "Vietnamese", "", "vi", "vie" },
    { "Volapük", "", "vo", "vol" },
    { "Welsh", "", "cy", "cym", "wel" },
    { "Walloon", "", "wa", "wln" },
    { "Wolof", "", "wo", "wol" },
    { "Xhosa", "", "xh", "xho" },
    { "Yiddish", "", "yi", "yid" },
    { "Yoruba", "", "yo", "yor" },
    { "Zhuang", "", "za", "zha" },
    { "Zulu", "", "zu", "zul" },
    {NULL, NULL, NULL, NULL}
};
#define LANG_TABLE_SIZE (sizeof(ghb_language_table)/ sizeof(iso639_lang_t)-1)

static void audio_bitrate_opts_set(GtkBuilder *builder, const gchar *name);

static void
del_tree(const gchar *name, gboolean del_top)
{
    const gchar *file;

    if (g_file_test(name, G_FILE_TEST_IS_DIR))
    {
        GDir *gdir = g_dir_open(name, 0, NULL);
        file = g_dir_read_name(gdir);
        while (file)
        {
            gchar *path;
            path = g_strdup_printf("%s/%s", name, file);
            del_tree(path, TRUE);
            g_free(path);
            file = g_dir_read_name(gdir);
        }
        if (del_top)
            g_rmdir(name);
        g_dir_close(gdir);
    }
    else
    {
        g_unlink(name);
    }
}

const gchar*
ghb_version()
{
    return hb_get_version(NULL);
}

float
ghb_vquality_default(signal_user_data_t *ud)
{
    float quality;
    gint vcodec;
    vcodec = ghb_settings_video_encoder_codec(ud->settings, "VideoEncoder");

    switch (vcodec)
    {
    case HB_VCODEC_X265:
    case HB_VCODEC_X264:
        return 20;
    case HB_VCODEC_THEORA:
        return 45;
    case HB_VCODEC_FFMPEG_MPEG2:
    case HB_VCODEC_FFMPEG_MPEG4:
        return 3;
    default:
    {
        float min, max, step;
        int direction;

        hb_video_quality_get_limits(vcodec, &min, &max, &step, &direction);
        // Pick something that is 70% of max
        // Probably too low for some and too high for others...
        quality = (max - min) * 0.7;
        if (direction)
            quality = max - quality;
    }
    }
    return quality;
}

void
ghb_vquality_range(
    signal_user_data_t *ud,
    float *min,
    float *max,
    float *step,
    float *page,
    gint *digits,
    int *direction)
{
    float min_step;
    gint vcodec;
    vcodec = ghb_settings_video_encoder_codec(ud->settings, "VideoEncoder");

    *page = 10;
    *digits = 0;
    hb_video_quality_get_limits(vcodec, min, max, &min_step, direction);
    *step = ghb_settings_combo_double(ud->prefs, "VideoQualityGranularity");

    if (*step < min_step)
        *step = min_step;

    if ((int)(*step * 100) % 10 != 0)
        *digits = 2;
    else if ((int)(*step * 10) % 10 != 0)
        *digits = 1;
}

gint
find_combo_entry(combo_opts_t *opts, const GhbValue *gval)
{
    gint ii;

    if (opts == NULL)
        return 0;

    if (ghb_value_type(gval) == GHB_STRING)
    {
        const gchar *str;
        str = ghb_value_get_string(gval);
        for (ii = 0; ii < opts->count; ii++)
        {
            if (strcmp(opts->map[ii].shortOpt, str) == 0)
            {
                break;
            }
        }
        return ii;
    }
    else if (ghb_value_type(gval) == GHB_DOUBLE)
    {
        gdouble val;
        val = ghb_value_get_double(gval);
        for (ii = 0; ii < opts->count; ii++)
        {
            if (opts->map[ii].ivalue == val)
            {
                break;
            }
        }
        return ii;
    }
    else if (ghb_value_type(gval) == GHB_INT ||
             ghb_value_type(gval) == GHB_BOOL)
    {
        gint64 val;
        val = ghb_value_get_int(gval);
        for (ii = 0; ii < opts->count; ii++)
        {
            if ((gint64)opts->map[ii].ivalue == val)
            {
                break;
            }
        }
        return ii;
    }
    return opts->count;
}

static gint
lookup_generic_int(combo_opts_t *opts, const GhbValue *gval)
{
    gint ii;
    gint result = -1;

    if (opts == NULL)
        return 0;

    ii = find_combo_entry(opts, gval);
    if (ii < opts->count)
    {
        result = opts->map[ii].ivalue;
    }
    return result;
}

static gdouble
lookup_generic_double(combo_opts_t *opts, const GhbValue *gval)
{
    gint ii;
    gdouble result = -1;

    ii = find_combo_entry(opts, gval);
    if (ii < opts->count)
    {
        result = opts->map[ii].ivalue;
    }
    return result;
}

static const gchar*
lookup_generic_option(combo_opts_t *opts, const GhbValue *gval)
{
    gint ii;
    const gchar *result = "";

    ii = find_combo_entry(opts, gval);
    if (ii < opts->count)
    {
        result = opts->map[ii].option;
    }
    return result;
}

gint
ghb_find_closest_audio_samplerate(gint irate)
{
    gint result;
    const hb_rate_t *rate;

    result = 0;
    for (rate = hb_audio_samplerate_get_next(NULL); rate != NULL;
         rate = hb_audio_samplerate_get_next(rate))
    {
        if (irate <= rate->rate)
        {
            result = rate->rate;
            break;
        }
    }
    return result;
}

const iso639_lang_t* ghb_iso639_lookup_by_int(int idx)
{
    return &ghb_language_table[idx];
}

int
ghb_lookup_audio_lang(const GhbValue *glang)
{
    gint ii;
    const gchar *str;

    str = ghb_value_get_string(glang);
    for (ii = 0; ii < LANG_TABLE_SIZE; ii++)
    {
        if (ghb_language_table[ii].iso639_2 &&
            strcmp(ghb_language_table[ii].iso639_2, str) == 0)
        {
            return ii;
        }
        if (ghb_language_table[ii].iso639_2b &&
            strcmp(ghb_language_table[ii].iso639_2b, str) == 0)
        {
            return ii;
        }
        if (ghb_language_table[ii].iso639_1 &&
            strcmp(ghb_language_table[ii].iso639_1, str) == 0)
        {
            return ii;
        }
        if (ghb_language_table[ii].native_name &&
            strcmp(ghb_language_table[ii].native_name, str) == 0)
        {
            return ii;
        }
        if (ghb_language_table[ii].eng_name &&
            strcmp(ghb_language_table[ii].eng_name, str) == 0)
        {
            return ii;
        }
    }
    return -1;
}

static int
lookup_audio_lang_int(const GhbValue *glang)
{
    gint ii = ghb_lookup_audio_lang(glang);
    if (ii >= 0)
        return ii;
    return 0;
}

static const gchar*
lookup_audio_lang_option(const GhbValue *glang)
{
    gint ii = ghb_lookup_audio_lang(glang);
    if (ii >= 0)
    {
        if (ghb_language_table[ii].native_name[0] != 0)
            return ghb_language_table[ii].native_name;
        else
            return ghb_language_table[ii].eng_name;
    }
    return "Any";
}

// Handle for libhb.  Gets set by ghb_backend_init()
static hb_handle_t * h_scan = NULL;
static hb_handle_t * h_queue = NULL;
static hb_handle_t * h_live = NULL;

extern void hb_get_temporary_directory(char path[512]);

gchar*
ghb_get_tmp_dir()
{
    char dir[512];

    hb_get_temporary_directory(dir);
    return g_strdup(dir);
}

void
ghb_hb_cleanup(gboolean partial)
{
    char dir[512];

    hb_get_temporary_directory(dir);
    del_tree(dir, !partial);
}

gint
ghb_subtitle_track_source(GhbValue *settings, gint track)
{
    gint title_id, titleindex;
    const hb_title_t *title;

    if (track == -2)
        return SRTSUB;
    if (track < 0)
        return VOBSUB;
    title_id = ghb_dict_get_int(settings, "title");
    title = ghb_lookup_title(title_id, &titleindex);
    if (title == NULL)
        return VOBSUB;

    hb_subtitle_t * sub;
    sub = hb_list_item( title->list_subtitle, track);
    if (sub != NULL)
        return sub->source;
    else
        return VOBSUB;
}

const gchar*
ghb_subtitle_track_lang(GhbValue *settings, gint track)
{
    gint title_id, titleindex;
    const hb_title_t * title;

    title_id = ghb_dict_get_int(settings, "title");
    title = ghb_lookup_title(title_id, &titleindex);
    if (title == NULL)  // Bad titleindex
        goto fail;
    if (track == -1)
        return ghb_get_user_audio_lang(settings, title, 0);
    if (track < 0)
        goto fail;

    hb_subtitle_t * sub;
    sub = hb_list_item( title->list_subtitle, track);
    if (sub != NULL)
        return sub->iso639_2;

fail:
    return "und";
}

static gint
search_audio_bitrates(gint rate)
{
    const hb_rate_t *hbrate;
    for (hbrate = hb_audio_bitrate_get_next(NULL); hbrate != NULL;
         hbrate = hb_audio_bitrate_get_next(hbrate))
    {
        if (hbrate->rate == rate)
            return 1;
    }
    return 0;
}

static gboolean find_combo_item_by_int(GtkTreeModel *store, gint value, GtkTreeIter *iter);

static void
grey_combo_box_item(GtkComboBox *combo, gint value, gboolean grey)
{
    GtkListStore *store;
    GtkTreeIter iter;

    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    if (find_combo_item_by_int(GTK_TREE_MODEL(store), value, &iter))
    {
        gtk_list_store_set(store, &iter,
                           1, !grey,
                           -1);
    }
}

static void
grey_builder_combo_box_item(GtkBuilder *builder, const gchar *name, gint value, gboolean grey)
{
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    grey_combo_box_item(combo, value, grey);
}

void
ghb_mix_opts_filter(GtkComboBox *combo, gint acodec)
{
    g_debug("ghb_mix_opts_filter()\n");

    const hb_mixdown_t *mix;
    for (mix = hb_mixdown_get_next(NULL); mix != NULL;
         mix = hb_mixdown_get_next(mix))
    {
        grey_combo_box_item(combo, mix->amixdown,
                !hb_mixdown_has_codec_support(mix->amixdown, acodec));
    }
}

static void
grey_mix_opts(signal_user_data_t *ud, gint acodec, gint64 layout)
{
    g_debug("grey_mix_opts()\n");

    const hb_mixdown_t *mix;
    for (mix = hb_mixdown_get_next(NULL); mix != NULL;
         mix = hb_mixdown_get_next(mix))
    {
        grey_builder_combo_box_item(ud->builder, "AudioMixdown", mix->amixdown,
                !hb_mixdown_is_supported(mix->amixdown, acodec, layout));
    }
}

void
ghb_grey_combo_options(signal_user_data_t *ud)
{
    gint track, title_id, titleindex, acodec, fallback;
    const hb_title_t *title;
    hb_audio_config_t *aconfig = NULL;

    title_id = ghb_dict_get_int(ud->settings, "title");
    title = ghb_lookup_title(title_id, &titleindex);

    track = ghb_dict_get_int(ud->settings, "AudioTrack");
    aconfig = ghb_get_audio_info(title, track);

    const char *mux_id;
    const hb_container_t *mux;

    mux_id = ghb_dict_get_string(ud->settings, "FileFormat");
    mux = ghb_lookup_container_by_name(mux_id);

    grey_builder_combo_box_item(ud->builder, "x264_analyse", 4, TRUE);

    const hb_encoder_t *enc;
    for (enc = hb_audio_encoder_get_next(NULL); enc != NULL;
         enc = hb_audio_encoder_get_next(enc))
    {
        if (!(mux->format & enc->muxers))
        {
            grey_builder_combo_box_item(ud->builder, "AudioEncoder",
                enc->codec, TRUE);
            grey_builder_combo_box_item(ud->builder, "AudioEncoderFallback",
                enc->codec, TRUE);
        }
        else
        {
            grey_builder_combo_box_item(ud->builder, "AudioEncoder",
                enc->codec, FALSE);
            grey_builder_combo_box_item(ud->builder, "AudioEncoderFallback",
                enc->codec, FALSE);
        }
    }
    for (enc = hb_video_encoder_get_next(NULL); enc != NULL;
         enc = hb_video_encoder_get_next(enc))
    {
        if (!(mux->format & enc->muxers))
        {
            grey_builder_combo_box_item(ud->builder, "VideoEncoder",
                enc->codec, TRUE);
        }
        else
        {
            grey_builder_combo_box_item(ud->builder, "VideoEncoder",
                enc->codec, FALSE);
        }
    }

    if (aconfig && (aconfig->in.codec & HB_ACODEC_MASK) != HB_ACODEC_MP3)
    {
        grey_builder_combo_box_item(ud->builder, "AudioEncoder", HB_ACODEC_MP3_PASS, TRUE);
    }
    if (aconfig && (aconfig->in.codec & HB_ACODEC_MASK) != HB_ACODEC_FFAAC)
    {
        grey_builder_combo_box_item(ud->builder, "AudioEncoder", HB_ACODEC_AAC_PASS, TRUE);
    }
    if (aconfig && (aconfig->in.codec & HB_ACODEC_MASK) != HB_ACODEC_AC3)
    {
        grey_builder_combo_box_item(ud->builder, "AudioEncoder", HB_ACODEC_AC3_PASS, TRUE);
    }
    if (aconfig && (aconfig->in.codec & HB_ACODEC_MASK) != HB_ACODEC_DCA)
    {
        grey_builder_combo_box_item(ud->builder, "AudioEncoder", HB_ACODEC_DCA_PASS, TRUE);
    }
    if (aconfig && (aconfig->in.codec & HB_ACODEC_MASK) != HB_ACODEC_DCA_HD)
    {
        grey_builder_combo_box_item(ud->builder, "AudioEncoder", HB_ACODEC_DCA_HD_PASS, TRUE);
    }

    acodec = ghb_settings_audio_encoder_codec(ud->settings, "AudioEncoder");

    gint64 layout = aconfig != NULL ? aconfig->in.channel_layout : ~0;
    fallback = ghb_select_fallback(ud->settings, acodec);
    gint copy_mask = ghb_get_copy_mask(ud->settings);
    acodec = ghb_select_audio_codec(mux->format, aconfig, acodec,
                                    fallback, copy_mask);
    grey_mix_opts(ud, acodec, layout);
}

gint
ghb_get_best_mix(hb_audio_config_t *aconfig, gint acodec, gint mix)
{
    gint layout;
    layout = aconfig ? aconfig->in.channel_layout : AV_CH_LAYOUT_5POINT1;

    if (mix == HB_AMIXDOWN_NONE)
        mix = HB_INVALID_AMIXDOWN;

    return hb_mixdown_get_best(acodec, layout, mix);
}

// Set up the model for the combo box
void
ghb_init_combo_box(GtkComboBox *combo)
{
    GtkListStore *store;
    GtkCellRenderer *cell;

    g_debug("ghb_init_combo_box()\n");
    // First modify the combobox model to allow greying out of options
    if (combo == NULL)
        return;
    // Store contains:
    // 1 - String to display
    // 2 - bool indicating whether the entry is selectable (grey or not)
    // 3 - String that is used for presets
    // 4 - Int value determined by backend
    // 5 - String value determined by backend
    store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_BOOLEAN,
                               G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_STRING);
    gtk_combo_box_set_model(combo, GTK_TREE_MODEL(store));

    if (!gtk_combo_box_get_has_entry(combo))
    {
        gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
        cell = GTK_CELL_RENDERER(gtk_cell_renderer_text_new());
        g_object_set(cell, "max-width-chars", 60, NULL);
        g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
            "markup", 0, "sensitive", 1, NULL);
    }
    else
    { // Combo box entry
        gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo), 0);
    }
}

// Set up the model for the combo box
static void
init_combo_box(GtkBuilder *builder, const gchar *name)
{
    GtkComboBox *combo;

    g_debug("init_combo_box() %s\n", name);
    // First modify the combobox model to allow greying out of options
    combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    ghb_init_combo_box(combo);
}

void
ghb_audio_samplerate_opts_set(GtkComboBox *combo)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("audio_samplerate_opts_set ()\n");
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    // Add an item for "Same As Source"
    gtk_list_store_append(store, &iter);
    str = g_strdup_printf("<small>%s</small>", _("Same as source"));
    gtk_list_store_set(store, &iter,
                       0, str,
                       1, TRUE,
                       2, "source",
                       3, 0.0,
                       4, "source",
                       -1);
    g_free(str);

    const hb_rate_t *rate;
    for (rate = hb_audio_samplerate_get_next(NULL); rate != NULL;
         rate = hb_audio_samplerate_get_next(rate))
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf("<small>%s</small>", rate->name);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, rate->name,
                           3, (gdouble)rate->rate,
                           4, rate->name,
                           -1);
        g_free(str);
    }
}

static void
audio_samplerate_opts_set(GtkBuilder *builder, const gchar *name)
{
    g_debug("audio_samplerate_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    ghb_audio_samplerate_opts_set(combo);
}

const hb_rate_t sas_rate =
{
    .name = N_("Same as source"),
    .rate = 0,
};

const char*
ghb_audio_samplerate_get_short_name(int rate)
{
    const char *name;
    name = hb_audio_samplerate_get_name(rate);
    if (name == NULL)
        name = "source";
    return name;
}

const hb_rate_t*
ghb_lookup_audio_samplerate(const char *name)
{
    // Check for special "Same as source" value
    if (!strncmp(name, "source", 8))
        return &sas_rate;

    // First find an enabled rate
    int rate = hb_audio_samplerate_get_from_name(name);

    // Now find the matching rate info
    const hb_rate_t *hb_rate, *first;
    for (first = hb_rate = hb_audio_samplerate_get_next(NULL); hb_rate != NULL;
         hb_rate = hb_audio_samplerate_get_next(hb_rate))
    {
        if (rate == hb_rate->rate)
        {
            return hb_rate;
        }
    }
    // Return a default rate if nothing matches
    return first;
}

int
ghb_lookup_audio_samplerate_rate(const char *name)
{
    return ghb_lookup_audio_samplerate(name)->rate;
}

int
ghb_settings_audio_samplerate_rate(const GhbValue *settings, const char *name)
{
    int result;
    char *rate_id = ghb_dict_get_string_xform(settings, name);
    result = ghb_lookup_audio_samplerate_rate(rate_id);
    g_free(rate_id);
    return result;
}

const hb_rate_t*
ghb_settings_audio_samplerate(const GhbValue *settings, const char *name)
{
    const hb_rate_t *result;
    char *rate_id = ghb_dict_get_string_xform(settings, name);
    result = ghb_lookup_audio_samplerate(rate_id);
    g_free(rate_id);
    return result;
}

static void
video_framerate_opts_set(GtkBuilder *builder, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;

    g_debug("video_framerate_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    // Add an item for "Same As Source"
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, _("Same as source"),
                       1, TRUE,
                       2, "source",
                       3, 0.0,
                       4, "source",
                       -1);

    const hb_rate_t *rate;
    for (rate = hb_video_framerate_get_next(NULL); rate != NULL;
         rate = hb_video_framerate_get_next(rate))
    {
        gchar *desc = "";
        gchar *option;
        if (strcmp(rate->name, "23.976") == 0)
        {
            desc = _("(NTSC Film)");
        }
        else if (strcmp(rate->name, "25") == 0)
        {
            desc = _("(PAL Film/Video)");
        }
        else if (strcmp(rate->name, "29.97") == 0)
        {
            desc = _("(NTSC Video)");
        }
        option = g_strdup_printf ("%s %s", rate->name, desc);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, option,
                           1, TRUE,
                           2, rate->name,
                           3, (gdouble)rate->rate,
                           4, rate->name,
                           -1);
        g_free(option);
    }
}

const hb_rate_t*
ghb_lookup_video_framerate(const char *name)
{
    // Check for special "Same as source" value
    if (!strncmp(name, "source", 8))
        return &sas_rate;

    // First find an enabled rate
    int rate = hb_video_framerate_get_from_name(name);

    // Now find the matching rate info
    const hb_rate_t *hb_rate, *first;
    for (first = hb_rate = hb_video_framerate_get_next(NULL); hb_rate != NULL;
         hb_rate = hb_video_framerate_get_next(hb_rate))
    {
        if (rate == hb_rate->rate)
        {
            return hb_rate;
        }
    }
    // Return a default rate if nothing matches
    return first;
}

int
ghb_lookup_video_framerate_rate(const char *name)
{
    return ghb_lookup_video_framerate(name)->rate;
}

int
ghb_settings_video_framerate_rate(const GhbValue *settings, const char *name)
{
    int result;
    char *rate_id = ghb_dict_get_string_xform(settings, name);
    result = ghb_lookup_video_framerate_rate(rate_id);
    g_free(rate_id);
    return result;
}

const hb_rate_t*
ghb_settings_video_framerate(const GhbValue *settings, const char *name)
{
    const hb_rate_t *result;
    char *rate_id = ghb_dict_get_string_xform(settings, name);
    result = ghb_lookup_video_framerate(rate_id);
    g_free(rate_id);
    return result;
}

static void
video_encoder_opts_set(
    GtkBuilder *builder,
    const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("video_encoder_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    const hb_encoder_t *enc;
    for (enc = hb_video_encoder_get_next(NULL); enc != NULL;
         enc = hb_video_encoder_get_next(enc))
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf("<small>%s</small>", enc->name);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, enc->short_name,
                           3, (gdouble)enc->codec,
                           4, enc->short_name,
                           -1);
        g_free(str);
    }
}

const hb_encoder_t*
ghb_lookup_video_encoder(const char *name)
{
    // First find an enabled encoder
    int codec = hb_video_encoder_get_from_name(name);

    // Now find the matching encoder info
    const hb_encoder_t *enc, *first;
    for (first = enc = hb_video_encoder_get_next(NULL); enc != NULL;
         enc = hb_video_encoder_get_next(enc))
    {
        if (codec == enc->codec)
        {
            return enc;
        }
    }
    // Return a default encoder if nothing matches
    return first;
}

int
ghb_lookup_video_encoder_codec(const char *name)
{
    return ghb_lookup_video_encoder(name)->codec;
}

int
ghb_settings_video_encoder_codec(const GhbValue *settings, const char *name)
{
    const char *encoder_id = ghb_dict_get_string(settings, name);
    return ghb_lookup_video_encoder_codec(encoder_id);
}

const hb_encoder_t*
ghb_settings_video_encoder(const GhbValue *settings, const char *name)
{
    const char *encoder_id = ghb_dict_get_string(settings, name);
    return ghb_lookup_video_encoder(encoder_id);
}

void
ghb_audio_encoder_opts_set_with_mask(
    GtkComboBox *combo,
    int mask,
    int neg_mask)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("ghb_audio_encoder_opts_set_with_mask()\n");
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    const hb_encoder_t *enc;
    for (enc = hb_audio_encoder_get_next(NULL); enc != NULL;
         enc = hb_audio_encoder_get_next(enc))
    {
        if ((mask & enc->codec) && !(neg_mask & enc->codec))
        {
            gtk_list_store_append(store, &iter);
            str = g_strdup_printf("<small>%s</small>", enc->name);
            gtk_list_store_set(store, &iter,
                               0, str,
                               1, TRUE,
                               2, enc->short_name,
                               3, (gdouble)enc->codec,
                               4, enc->short_name,
                               -1);
            g_free(str);
        }
    }
}

const hb_encoder_t*
ghb_lookup_audio_encoder(const char *name)
{
    // First find an enabled encoder
    int codec = hb_audio_encoder_get_from_name(name);

    // Now find the matching encoder info
    const hb_encoder_t *enc, *first;
    for (first = enc = hb_audio_encoder_get_next(NULL); enc != NULL;
         enc = hb_audio_encoder_get_next(enc))
    {
        if (codec == enc->codec)
        {
            return enc;
        }
    }
    // Return a default encoder if nothing matches
    return first;
}

int
ghb_lookup_audio_encoder_codec(const char *name)
{
    return ghb_lookup_audio_encoder(name)->codec;
}

int
ghb_settings_audio_encoder_codec(const GhbValue *settings, const char *name)
{
    const char *encoder_id = ghb_dict_get_string(settings, name);
    return ghb_lookup_audio_encoder_codec(encoder_id);
}

const hb_encoder_t*
ghb_settings_audio_encoder(const GhbValue *settings, const char *name)
{
    const char *encoder_id = ghb_dict_get_string(settings, name);
    return ghb_lookup_audio_encoder(encoder_id);
}

static void
audio_encoder_opts_set_with_mask(
    GtkBuilder *builder,
    const gchar *name,
    int mask,
    int neg_mask)
{
    g_debug("audio_encoder_opts_set_with_mask()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    ghb_audio_encoder_opts_set_with_mask(combo, mask, neg_mask);
}

void
ghb_audio_encoder_opts_set(GtkComboBox *combo)
{
    ghb_audio_encoder_opts_set_with_mask(combo, ~0, 0);
}

static void
audio_encoder_opts_set(
    GtkBuilder *builder,
    const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    const hb_encoder_t *enc;
    for (enc = hb_audio_encoder_get_next(NULL); enc != NULL;
         enc = hb_audio_encoder_get_next(enc))
    {
        if (enc->codec != HB_ACODEC_AUTO_PASS)
        {
            gtk_list_store_append(store, &iter);
            str = g_strdup_printf("<small>%s</small>", enc->name);
            gtk_list_store_set(store, &iter,
                               0, str,
                               1, TRUE,
                               2, enc->short_name,
                               3, (gdouble)enc->codec,
                               4, enc->short_name,
                               -1);
            g_free(str);
        }
    }
}

static void
acodec_fallback_opts_set(GtkBuilder *builder, const gchar *name)
{
    audio_encoder_opts_set_with_mask(builder, name, ~0, HB_ACODEC_PASS_FLAG);
}

void
ghb_mix_opts_set(GtkComboBox *combo)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("mix_opts_set ()\n");
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    const hb_mixdown_t *mix;
    for (mix = hb_mixdown_get_next(NULL); mix != NULL;
         mix = hb_mixdown_get_next(mix))
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf("<small>%s</small>", mix->name);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, mix->short_name,
                           3, (gdouble)mix->amixdown,
                           4, mix->short_name,
                           -1);
        g_free(str);
    }
}

const hb_mixdown_t*
ghb_lookup_mixdown(const char *name)
{
    // First find an enabled mixdown
    int mix = hb_mixdown_get_from_name(name);

    // Now find the matching mixdown info
    const hb_mixdown_t *mixdown, *first;
    for (first = mixdown = hb_mixdown_get_next(NULL); mixdown != NULL;
         mixdown = hb_mixdown_get_next(mixdown))
    {
        if (mix == mixdown->amixdown)
        {
            return mixdown;
        }
    }
    // Return a default mixdown if nothing matches
    return first;
}

int
ghb_lookup_mixdown_mix(const char *name)
{
    return ghb_lookup_mixdown(name)->amixdown;
}

int
ghb_settings_mixdown_mix(const GhbValue *settings, const char *name)
{
    const char *mixdown_id = ghb_dict_get_string(settings, name);
    return ghb_lookup_mixdown_mix(mixdown_id);
}

const hb_mixdown_t*
ghb_settings_mixdown(const GhbValue *settings, const char *name)
{
    const char *mixdown_id = ghb_dict_get_string(settings, name);
    return ghb_lookup_mixdown(mixdown_id);
}

static void
mix_opts_set(GtkBuilder *builder, const gchar *name)
{
    g_debug("mix_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    ghb_mix_opts_set(combo);
}

static void
container_opts_set(
    GtkBuilder *builder,
    const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("hb_container_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    const hb_container_t *mux;
    for (mux = hb_container_get_next(NULL); mux != NULL;
         mux = hb_container_get_next(mux))
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf("<small>%s</small>", mux->name);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, mux->short_name,
                           3, (gdouble)mux->format,
                           4, mux->short_name,
                           -1);
        g_free(str);
    }
}

const hb_container_t*
ghb_lookup_container_by_name(const gchar *name)
{
    // First find an enabled muxer
    int format = hb_container_get_from_name(name);

    // Now find the matching muxer info
    const hb_container_t *mux, *first;
    for (first = mux = hb_container_get_next(NULL); mux != NULL;
         mux = hb_container_get_next(mux))
    {
        if (format == mux->format)
        {
            return mux;
        }
    }
    // Return a default container if nothing matches
    return first;
}

static void
srt_codeset_opts_set(GtkBuilder *builder, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii;

    g_debug("srt_codeset_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    for (ii = 0; ii < SRT_TABLE_SIZE; ii++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, srt_codeset_table[ii],
                           1, TRUE,
                           2, srt_codeset_table[ii],
                           3, (gdouble)ii,
                           4, srt_codeset_table[ii],
                           -1);
    }
}

static void
language_opts_set(GtkBuilder *builder, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii;

    g_debug("language_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    for (ii = 0; ii < LANG_TABLE_SIZE; ii++)
    {
        const gchar *lang;

        if (ghb_language_table[ii].native_name[0] != 0)
            lang = ghb_language_table[ii].native_name;
        else
            lang = ghb_language_table[ii].eng_name;

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, lang,
                           1, TRUE,
                           2, ghb_language_table[ii].iso639_2,
                           3, (gdouble)ii,
                           4, ghb_language_table[ii].iso639_1,
                           -1);
    }
}

const iso639_lang_t*
ghb_lookup_lang(const char *name)
{
    int ii;
    for (ii = 0; ii < LANG_TABLE_SIZE; ii++)
    {
        if (!strncmp(name, ghb_language_table[ii].iso639_2, 4) ||
            !strncmp(name, ghb_language_table[ii].iso639_1, 4) ||
            !strncmp(name, ghb_language_table[ii].iso639_2b, 4) ||
            !strncmp(name, ghb_language_table[ii].native_name, 4) ||
            !strncmp(name, ghb_language_table[ii].eng_name, 4))
        {
            return &ghb_language_table[ii];
        }
    }
    return NULL;
}

gchar*
ghb_create_title_label(const hb_title_t *title)
{
    gchar *label;

    if (title->type == HB_STREAM_TYPE || title->type == HB_FF_STREAM_TYPE)
    {
        if (title->duration != 0)
        {
            char *tmp;
            tmp  = g_strdup_printf (_("%d - %02dh%02dm%02ds - %s"),
                title->index, title->hours, title->minutes, title->seconds,
                title->name);
            label = g_markup_escape_text(tmp, -1);
            g_free(tmp);
        }
        else
        {
            char *tmp;
            tmp  = g_strdup_printf ("%d - %s",
                                    title->index, title->name);
            label = g_markup_escape_text(tmp, -1);
            g_free(tmp);
        }
    }
    else if (title->type == HB_BD_TYPE)
    {
        if (title->duration != 0)
        {
            label = g_strdup_printf(_("%d (%05d.MPLS) - %02dh%02dm%02ds"),
                title->index, title->playlist, title->hours,
                title->minutes, title->seconds);
        }
        else
        {
            label = g_strdup_printf(_("%d (%05d.MPLS) - Unknown Length"),
                title->index, title->playlist);
        }
    }
    else
    {
        if (title->duration != 0)
        {
            label  = g_strdup_printf(_("%d - %02dh%02dm%02ds"),
                title->index, title->hours, title->minutes, title->seconds);
        }
        else
        {
            label  = g_strdup_printf(_("%d - Unknown Length"),
                                    title->index);
        }
    }
    return label;
}

void
title_opts_set(GtkBuilder *builder, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    hb_list_t  * list = NULL;
    const hb_title_t * title = NULL;
    gint ii;
    gint count = 0;


    g_debug("title_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    if (h_scan != NULL)
    {
        list = hb_get_titles( h_scan );
        count = hb_list_count( list );
    }
    if( count <= 0 )
    {
        char *opt;

        // No titles.  Fill in a default.
        gtk_list_store_append(store, &iter);
        opt = g_strdup_printf("<small>%s</small>", _("No Titles"));
        gtk_list_store_set(store, &iter,
                           0, opt,
                           1, TRUE,
                           2, "none",
                           3, -1.0,
                           4, "none",
                           -1);
        g_free(opt);
        return;
    }
    for (ii = 0; ii < count; ii++)
    {
        char *title_opt, *title_index, *opt;

        title = hb_list_item(list, ii);
        title_opt = ghb_create_title_label(title);
        opt = g_strdup_printf("<small>%s</small>", title_opt);
        title_index = g_strdup_printf("%d", title->index);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, opt,
                           1, TRUE,
                           2, title_index,
                           3, (gdouble)title->index,
                           4, title_index,
                           -1);
        g_free(opt);
        g_free(title_opt);
        g_free(title_index);
    }
}

static int
lookup_title_index(hb_handle_t *h, int title_id)
{
    if (h == NULL)
        return -1;

    hb_list_t *list;
    const hb_title_t *title;
    int count, ii;

    list = hb_get_titles(h);
    count = hb_list_count(list);
    for (ii = 0; ii < count; ii++)
    {
        title = hb_list_item(list, ii);
        if (title_id == title->index)
        {
            return ii;
        }
    }
    return -1;
}

const hb_title_t*
lookup_title(hb_handle_t *h, int title_id, int *index)
{
    int ii = lookup_title_index(h, title_id);

    if (index != NULL)
        *index = ii;
    if (ii < 0)
        return NULL;

    hb_list_t *list;
    list = hb_get_titles(h);
    return hb_list_item(list, ii);
}

int
ghb_lookup_title_index(int title_id)
{
    return lookup_title_index(h_scan, title_id);
}

const hb_title_t*
ghb_lookup_title(int title_id, int *index)
{
    return lookup_title(h_scan, title_id, index);
}

int
ghb_lookup_queue_title_index(int title_id)
{
    return lookup_title_index(h_queue, title_id);
}

const hb_title_t*
ghb_lookup_queue_title(int title_id, int *index)
{
    return lookup_title(h_queue, title_id, index);
}

static void
video_tune_opts_set(signal_user_data_t *ud, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii, count = 0;

    // Check if encoder has been set yet.
    // If not, bail
    GhbValue *value = ghb_dict_get(ud->settings, "VideoEncoder");
    if (value == NULL) return;

    int encoder = ghb_get_video_encoder(ud->settings);
    const char * const *tunes;
    tunes = hb_video_encoder_get_tunes(encoder);

    while (tunes && tunes[count]) count++;
    if (count == 0) return;

    g_debug("video_tune_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(ud->builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, _("None"),
                       1, TRUE,
                       2, "none",
                       3, (gdouble)0,
                       4, "none",
                       -1);

    for (ii = 0; ii < count; ii++)
    {
        if (strcmp(tunes[ii], "fastdecode") && strcmp(tunes[ii], "zerolatency"))
        {
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, tunes[ii],
                               1, TRUE,
                               2, tunes[ii],
                               3, (gdouble)ii + 1,
                               4, tunes[ii],
                               -1);
        }
    }
}

static void
video_profile_opts_set(signal_user_data_t *ud, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii, count = 0;

    // Check if encoder has been set yet.
    // If not, bail
    GhbValue *value = ghb_dict_get(ud->settings, "VideoEncoder");
    if (value == NULL) return;

    int encoder = ghb_get_video_encoder(ud->settings);
    const char * const *profiles;
    profiles = hb_video_encoder_get_profiles(encoder);

    while (profiles && profiles[count]) count++;
    if (count == 0) return;

    g_debug("video_profile_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(ud->builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    for (ii = 0; ii < count; ii++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, profiles[ii],
                           1, TRUE,
                           2, profiles[ii],
                           3, (gdouble)ii,
                           4, profiles[ii],
                           -1);
    }
}

static void
video_level_opts_set(signal_user_data_t *ud, const gchar *name)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii, count = 0;

    // Check if encoder has been set yet.
    // If not, bail
    GhbValue *value = ghb_dict_get(ud->settings, "VideoEncoder");
    if (value == NULL) return;

    int encoder = ghb_get_video_encoder(ud->settings);
    const char * const *levels;
    levels = hb_video_encoder_get_levels(encoder);

    while (levels && levels[count]) count++;
    if (count == 0) return;

    g_debug("video_level_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(ud->builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    for (ii = 0; ii < count; ii++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, levels[ii],
                           1, TRUE,
                           2, levels[ii],
                           3, (gdouble)ii,
                           4, levels[ii],
                           -1);
    }
}

static gboolean
find_combo_item_by_int(GtkTreeModel *store, gint value, GtkTreeIter *iter)
{
    gdouble ivalue;
    gboolean foundit = FALSE;

    if (gtk_tree_model_get_iter_first (store, iter))
    {
        do
        {
            gtk_tree_model_get(store, iter, 3, &ivalue, -1);
            if (value == (gint)ivalue)
            {
                foundit = TRUE;
                break;
            }
        } while (gtk_tree_model_iter_next (store, iter));
    }
    return foundit;
}

void
audio_track_opts_set(GtkBuilder *builder, const gchar *name, const hb_title_t *title)
{
    GtkTreeIter iter;
    GtkListStore *store;
    hb_audio_config_t * audio;
    gint ii;
    gint count = 0;
    gchar *opt;

    g_debug("audio_track_opts_set ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    if (title != NULL)
    {
        count = hb_list_count( title->list_audio );
    }
    if( count <= 0 )
    {
        // No audio. set some default
        opt = g_strdup_printf("<small>%s</small>", _("No Audio"));

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, opt,
                           1, TRUE,
                           2, "none",
                           3, -1.0,
                           4, "none",
                           -1);
        g_free(opt);
        return;
    }
    for (ii = 0; ii < count; ii++)
    {
        char idx[4];
        audio = hb_list_audio_config_item(title->list_audio, ii);
        opt = g_strdup_printf("<small>%d - %s</small>",
                              ii + 1, audio->lang.description);
        snprintf(idx, 4, "%d", ii);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, opt,
                           1, TRUE,
                           2, idx,
                           3, (gdouble)ii,
                           4, idx,
                           -1);
        g_free(opt);
    }
    gtk_combo_box_set_active (combo, 0);
}

void
subtitle_track_opts_set(
    GtkBuilder *builder,
    const gchar *name,
    const hb_title_t *title)
{
    GtkTreeIter iter;
    GtkListStore *store;
    hb_subtitle_t * subtitle;
    gint ii, count = 0;

    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    if (title != NULL)
    {
        count = hb_list_count( title->list_subtitle );
    }
    if (count > 0)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, _("Foreign Audio Search"),
                           1, TRUE,
                           2, "-1",
                           3, -1.0,
                           4, "auto",
                           -1);

        for (ii = 0; ii < count; ii++)
        {
            gchar *opt;
            char idx[4];

            subtitle = hb_list_item(title->list_subtitle, ii);
            opt = g_strdup_printf("%d - %s (%s)", ii+1, subtitle->lang,
                                  hb_subsource_name(subtitle->source));
            snprintf(idx, 4, "%d", ii);

            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                        0, opt,
                        1, TRUE,
                        2, idx,
                        3, (gdouble)ii,
                        4, idx,
                        -1);
            g_free(opt);
        }
    }
    else
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, _("None"),
                           1, TRUE,
                           2, "0",
                           3, 0.0,
                           4, "none",
                           -1);
    }
    gtk_combo_box_set_active (combo, 0);
}

// Get title id of feature or longest title
gint
ghb_longest_title()
{
    hb_title_set_t * title_set;
    const hb_title_t * title;
    gint count = 0, ii, longest = -1;
    int64_t duration = 0;

    g_debug("ghb_longest_title ()\n");
    if (h_scan == NULL) return 0;
    title_set = hb_get_title_set( h_scan );
    count = hb_list_count( title_set->list_title );
    if (count < 1) return -1;

    // Check that the feature title in the title_set exists in the list
    // of titles.  If not, pick the longest.
    for (ii = 0; ii < count; ii++)
    {
        title = hb_list_item(title_set->list_title, ii);
        if (title->index == title_set->feature)
            return title_set->feature;
        if (title->duration > duration)
            longest = title->index;
    }
    return longest;
}

const gchar*
ghb_get_source_audio_lang(const hb_title_t *title, gint track)
{
    hb_audio_config_t * audio;
    const gchar *lang = "und";

    g_debug("ghb_lookup_1st_audio_lang ()\n");
    if (title == NULL)
        return lang;
    if (hb_list_count( title->list_audio ) <= track)
        return lang;

    audio = hb_list_audio_config_item(title->list_audio, track);
    if (audio == NULL)
        return lang;

    lang = audio->lang.iso639_2;
    return lang;
}

gint
ghb_find_audio_track(const hb_title_t *title, const gchar *lang, int start)
{
    hb_audio_config_t * audio;
    gint ii, count = 0;

    if (title != NULL)
    {
        count = hb_list_count(title->list_audio);
    }

    for (ii = start; ii < count; ii++)
    {
        audio = hb_list_audio_config_item(title->list_audio, ii);
        if (!strcmp(lang, audio->lang.iso639_2) || !strcmp(lang, "und"))
        {
            return ii;
        }
    }
    return -1;
}

gint
ghb_find_subtitle_track(const hb_title_t * title, const gchar * lang, int start)
{
    hb_subtitle_t * subtitle;
    gint count, ii;

    count = hb_list_count(title->list_subtitle);

    // Try to find an item that matches the preferred language
    for (ii = start; ii < count; ii++)
    {
        subtitle = (hb_subtitle_t*)hb_list_item( title->list_subtitle, ii );
        if (strcmp(lang, subtitle->iso639_2) == 0 ||
            strcmp(lang, "und") == 0)
        {
            return ii;
        }
    }
    return -1;
}

#if 0
static void
generic_opts_set(GtkBuilder *builder, const gchar *name, combo_opts_t *opts)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii;

    g_debug("generic_opts_set ()\n");
    if (name == NULL || opts == NULL) return;
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    for (ii = 0; ii < opts->count; ii++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, gettext(opts->map[ii].option),
                           1, TRUE,
                           2, opts->map[ii].shortOpt,
                           3, opts->map[ii].ivalue,
                           4, opts->map[ii].svalue,
                           -1);
    }
}
#endif

static void
small_opts_set(GtkBuilder *builder, const gchar *name, combo_opts_t *opts)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint ii;
    gchar *str;

    g_debug("small_opts_set ()\n");
    if (name == NULL || opts == NULL) return;
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);
    for (ii = 0; ii < opts->count; ii++)
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf("<small>%s</small>",
                              gettext(opts->map[ii].option));
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, opts->map[ii].shortOpt,
                           3, opts->map[ii].ivalue,
                           4, opts->map[ii].svalue,
                           -1);
        g_free(str);
    }
}

combo_opts_t*
find_combo_table(const gchar *name)
{
    gint ii;

    for (ii = 0; combo_name_map[ii].name != NULL; ii++)
    {
        if (strcmp(name, combo_name_map[ii].name) == 0)
        {
            return combo_name_map[ii].opts;
        }
    }
    return NULL;
}

gint
ghb_lookup_combo_int(const gchar *name, const GhbValue *gval)
{
    if (gval == NULL)
        return 0;
    else if (strcmp(name, "SrtLanguage") == 0)
        return lookup_audio_lang_int(gval);
    else
    {
        return lookup_generic_int(find_combo_table(name), gval);
    }
    g_warning("ghb_lookup_combo_int() couldn't find %s", name);
    return 0;
}

gdouble
ghb_lookup_combo_double(const gchar *name, const GhbValue *gval)
{
    if (gval == NULL)
        return 0;
    else if (strcmp(name, "SrtLanguage") == 0)
        return lookup_audio_lang_int(gval);
    else
    {
        return lookup_generic_double(find_combo_table(name), gval);
    }
    g_warning("ghb_lookup_combo_double() couldn't find %s", name);
    return 0;
}

const gchar*
ghb_lookup_combo_option(const gchar *name, const GhbValue *gval)
{
    if (gval == NULL)
        return NULL;
    else if (strcmp(name, "SrtLanguage") == 0)
        return lookup_audio_lang_option(gval);
    else
    {
        return lookup_generic_option(find_combo_table(name), gval);
    }
    g_warning("ghb_lookup_combo_int() couldn't find %s", name);
    return NULL;
}

void ghb_init_lang_list_box(GtkListBox *list_box)
{
    int ii;

    for (ii = 0; ii < LANG_TABLE_SIZE; ii++)
    {
        const char *lang;
        if (ghb_language_table[ii].native_name != NULL &&
            ghb_language_table[ii].native_name[0] != 0)
        {
            lang = ghb_language_table[ii].native_name;
        }
        else
        {
            lang = ghb_language_table[ii].eng_name;
        }
        GtkWidget *label = gtk_label_new(lang);
        g_object_set_data(G_OBJECT(label), "lang_idx", (gpointer)(intptr_t)ii);
        gtk_widget_show(label);
        gtk_list_box_insert(list_box, label, -1);
    }
}

void
ghb_update_ui_combo_box(
    signal_user_data_t *ud,
    const gchar *name,
    const void *user_data,
    gboolean all)
{
    GtkComboBox *combo = NULL;
    gint signal_id;
    gint handler_id = 0;

    if (name != NULL)
    {
        g_debug("ghb_update_ui_combo_box() %s\n", name);
        // Clearing a combo box causes a rash of "changed" events, even when
        // the active item is -1 (inactive).  To control things, I'm disabling
        // the event till things are settled down.
        combo = GTK_COMBO_BOX(GHB_WIDGET(ud->builder, name));
        signal_id = g_signal_lookup("changed", GTK_TYPE_COMBO_BOX);
        if (signal_id > 0)
        {
            // Valid signal id found.  This should always succeed.
            handler_id = g_signal_handler_find ((gpointer)combo, G_SIGNAL_MATCH_ID,
                                                signal_id, 0, 0, 0, 0);
            if (handler_id > 0)
            {
                // This should also always succeed
                g_signal_handler_block ((gpointer)combo, handler_id);
            }
        }
    }
    if (all)
    {
        audio_bitrate_opts_set(ud->builder, "AudioBitrate");
        audio_samplerate_opts_set(ud->builder, "AudioSamplerate");
        video_framerate_opts_set(ud->builder, "VideoFramerate");
        mix_opts_set(ud->builder, "AudioMixdown");
        video_encoder_opts_set(ud->builder, "VideoEncoder");
        audio_encoder_opts_set(ud->builder, "AudioEncoder");
        acodec_fallback_opts_set(ud->builder, "AudioEncoderFallback");
        language_opts_set(ud->builder, "SrtLanguage");
        srt_codeset_opts_set(ud->builder, "SrtCodeset");
        title_opts_set(ud->builder, "title");
        audio_track_opts_set(ud->builder, "AudioTrack", user_data);
        subtitle_track_opts_set(ud->builder, "SubtitleTrack", user_data);
        small_opts_set(ud->builder, "VideoQualityGranularity", &vqual_granularity_opts);
        small_opts_set(ud->builder, "SubtitleTrackSelectionBehavior", &subtitle_track_sel_opts);
        small_opts_set(ud->builder, "SubtitleBurnBehavior", &subtitle_burn_opts);
        small_opts_set(ud->builder, "AudioTrackSelectionBehavior", &audio_track_sel_opts);
        small_opts_set(ud->builder, "PtoPType", &point_to_point_opts);
        small_opts_set(ud->builder, "WhenComplete", &when_complete_opts);
        small_opts_set(ud->builder, "PicturePAR", &par_opts);
        small_opts_set(ud->builder, "PictureModulus", &alignment_opts);
        small_opts_set(ud->builder, "LoggingLevel", &logging_opts);
        small_opts_set(ud->builder, "LogLongevity", &log_longevity_opts);
        small_opts_set(ud->builder, "check_updates", &appcast_update_opts);
        small_opts_set(ud->builder, "PictureDeinterlace", &deint_opts);
        small_opts_set(ud->builder, "PictureDetelecine", &detel_opts);
        small_opts_set(ud->builder, "PictureDecomb", &decomb_opts);
        small_opts_set(ud->builder, "PictureDenoiseFilter", &denoise_opts);
        small_opts_set(ud->builder, "PictureDenoisePreset", &denoise_preset_opts);
        small_opts_set(ud->builder, "PictureDenoiseTune", &nlmeans_tune_opts);
        small_opts_set(ud->builder, "x264_direct", &direct_opts);
        small_opts_set(ud->builder, "x264_b_adapt", &badapt_opts);
        small_opts_set(ud->builder, "x264_bpyramid", &bpyramid_opts);
        small_opts_set(ud->builder, "x264_weighted_pframes", &weightp_opts);
        small_opts_set(ud->builder, "x264_me", &me_opts);
        small_opts_set(ud->builder, "x264_subme", &subme_opts);
        small_opts_set(ud->builder, "x264_analyse", &analyse_opts);
        small_opts_set(ud->builder, "x264_trellis", &trellis_opts);
        video_tune_opts_set(ud, "VideoTune");
        video_profile_opts_set(ud, "VideoProfile");
        video_level_opts_set(ud, "VideoLevel");
        container_opts_set(ud->builder, "FileFormat");
    }
    else
    {
        if (strcmp(name, "AudioBitrate") == 0)
            audio_bitrate_opts_set(ud->builder, "AudioBitrate");
        else if (strcmp(name, "AudioSamplerate") == 0)
            audio_samplerate_opts_set(ud->builder, "AudioSamplerate");
        else if (strcmp(name, "VideoFramerate") == 0)
            video_framerate_opts_set(ud->builder, "VideoFramerate");
        else if (strcmp(name, "AudioMixdown") == 0)
            mix_opts_set(ud->builder, "AudioMixdown");
        else if (strcmp(name, "VideoEncoder") == 0)
            video_encoder_opts_set(ud->builder, "VideoEncoder");
        else if (strcmp(name, "AudioEncoder") == 0)
            audio_encoder_opts_set(ud->builder, "AudioEncoder");
        else if (strcmp(name, "AudioEncoderFallback") == 0)
            acodec_fallback_opts_set(ud->builder, "AudioEncoderFallback");
        else if (strcmp(name, "SrtLanguage") == 0)
            language_opts_set(ud->builder, "SrtLanguage");
        else if (strcmp(name, "SrtCodeset") == 0)
            srt_codeset_opts_set(ud->builder, "SrtCodeset");
        else if (strcmp(name, "title") == 0)
            title_opts_set(ud->builder, "title");
        else if (strcmp(name, "SubtitleTrack") == 0)
            subtitle_track_opts_set(ud->builder, "SubtitleTrack", user_data);
        else if (strcmp(name, "AudioTrack") == 0)
            audio_track_opts_set(ud->builder, "AudioTrack", user_data);
        else if (strcmp(name, "VideoTune") == 0)
            video_tune_opts_set(ud, "VideoTune");
        else if (strcmp(name, "VideoProfile") == 0)
            video_profile_opts_set(ud, "VideoProfile");
        else if (strcmp(name, "VideoLevel") == 0)
            video_level_opts_set(ud, "VideoLevel");
        else if (strcmp(name, "FileFormat") == 0)
            container_opts_set(ud->builder, "FileFormat");
        else
            small_opts_set(ud->builder, name, find_combo_table(name));
    }
    if (handler_id > 0)
    {
        g_signal_handler_unblock ((gpointer)combo, handler_id);
    }
}

static void
init_ui_combo_boxes(GtkBuilder *builder)
{
    gint ii;

    init_combo_box(builder, "AudioBitrate");
    init_combo_box(builder, "AudioSamplerate");
    init_combo_box(builder, "VideoFramerate");
    init_combo_box(builder, "AudioMixdown");
    init_combo_box(builder, "SrtLanguage");
    init_combo_box(builder, "SrtCodeset");
    init_combo_box(builder, "title");
    init_combo_box(builder, "AudioTrack");
    init_combo_box(builder, "SubtitleTrack");
    init_combo_box(builder, "VideoEncoder");
    init_combo_box(builder, "AudioEncoder");
    init_combo_box(builder, "AudioEncoderFallback");
    init_combo_box(builder, "VideoTune");
    init_combo_box(builder, "VideoProfile");
    init_combo_box(builder, "VideoLevel");
    init_combo_box(builder, "FileFormat");
    for (ii = 0; combo_name_map[ii].name != NULL; ii++)
    {
        init_combo_box(builder, combo_name_map[ii].name);
    }
}

// Construct the advanced options string
// The result is allocated, so someone must free it at some point.
const gchar*
ghb_build_advanced_opts_string(GhbValue *settings)
{
    gint vcodec;
    vcodec = ghb_settings_video_encoder_codec(settings, "VideoEncoder");
    switch (vcodec)
    {
        case HB_VCODEC_X264:
            return ghb_dict_get_string(settings, "x264Option");

        default:
            return NULL;
    }
}

void ghb_set_video_encoder_opts(hb_dict_t *dict, GhbValue *js)
{
    gint vcodec = ghb_settings_video_encoder_codec(js, "VideoEncoder");
    switch (vcodec)
    {
        case HB_VCODEC_X265:
        case HB_VCODEC_X264:
        {
            if (vcodec == HB_VCODEC_X264 &&
                ghb_dict_get_bool(js, "x264UseAdvancedOptions"))
            {
                const char *opts;
                opts = ghb_dict_get_string(js, "x264Option");
                hb_dict_set(dict, "Options", hb_value_string(opts));
            }
            else
            {
                const char *preset, *tune, *profile, *level, *opts;
                GString *str = g_string_new("");
                preset = ghb_dict_get_string(js, "VideoPreset");
                tune = ghb_dict_get_string(js, "VideoTune");
                profile = ghb_dict_get_string(js, "VideoProfile");
                level = ghb_dict_get_string(js, "VideoLevel");
                opts = ghb_dict_get_string(js, "VideoOptionExtra");
                char *tunes;

                g_string_append_printf(str, "%s", tune);
                if (vcodec == HB_VCODEC_X264)
                {
                    if (ghb_dict_get_bool(js, "x264FastDecode"))
                    {
                        g_string_append_printf(str, "%s%s", str->str[0] ? "," : "", "fastdecode");
                    }
                    if (ghb_dict_get_bool(js, "x264ZeroLatency"))
                    {
                        g_string_append_printf(str, "%s%s", str->str[0] ? "," : "", "zerolatency");
                    }
                }
                tunes = g_string_free(str, FALSE);

                if (preset != NULL)
                    hb_dict_set(dict, "Preset", hb_value_string(preset));
                if (tunes != NULL && strcasecmp(tune, "none"))
                    hb_dict_set(dict, "Tune", hb_value_string(tunes));
                if (profile != NULL && strcasecmp(profile, "auto"))
                    hb_dict_set(dict, "Profile", hb_value_string(profile));
                if (level != NULL && strcasecmp(level, "auto"))
                    hb_dict_set(dict, "Level", hb_value_string(level));
                if (opts != NULL)
                    hb_dict_set(dict, "Options", hb_value_string(opts));

                g_free(tunes);
            }
        } break;

        case HB_VCODEC_FFMPEG_MPEG2:
        case HB_VCODEC_FFMPEG_MPEG4:
        case HB_VCODEC_FFMPEG_VP8:
        {
            const char *opts;
            opts = ghb_dict_get_string(js, "VideoOptionExtra");
            if (opts != NULL && opts[0])
            {
                hb_dict_set(dict, "Options", hb_value_string(opts));
            }
        } break;

        case HB_VCODEC_THEORA:
        default:
        {
        } break;
    }
}

void
ghb_part_duration(const hb_title_t *title, gint sc, gint ec, gint *hh, gint *mm, gint *ss)
{
    hb_chapter_t * chapter;
    gint count, c;
    gint64 duration;

    *hh = *mm = *ss = 0;
    if (title == NULL) return;

    *hh = title->hours;
    *mm = title->minutes;
    *ss = title->seconds;

    count = hb_list_count(title->list_chapter);
    if (sc > count) sc = count;
    if (ec > count) ec = count;

    if (sc == 1 && ec == count)
        return;

    duration = 0;
    for (c = sc; c <= ec; c++)
    {
        chapter = hb_list_item(title->list_chapter, c-1);
        duration += chapter->duration;
    }

    *hh =   duration / 90000 / 3600;
    *mm = ((duration / 90000) % 3600) / 60;
    *ss =  (duration / 90000) % 60;
}

gint64
ghb_get_chapter_duration(const hb_title_t *title, gint chap)
{
    hb_chapter_t * chapter;
    gint count;

    if (title == NULL) return 0;
    count = hb_list_count( title->list_chapter );
    if (chap >= count) return 0;
    chapter = hb_list_item(title->list_chapter, chap);
    if (chapter == NULL) return 0;
    return chapter->duration;
}

gint64
ghb_get_chapter_start(const hb_title_t *title, gint chap)
{
    hb_chapter_t * chapter;
    gint count, ii;
    gint64 start = 0;

    if (title == NULL) return 0;
    count = hb_list_count( title->list_chapter );
    if (chap > count) return chap = count;
    for (ii = 0; ii < chap; ii++)
    {
        chapter = hb_list_item(title->list_chapter, ii);
        start += chapter->duration;
    }
    return start;
}

GhbValue*
ghb_get_chapters(const hb_title_t *title)
{
    hb_chapter_t * chapter;
    gint count, ii;
    GhbValue *chapters = NULL;

    chapters = ghb_array_new();

    if (title == NULL) return chapters;
    count = hb_list_count( title->list_chapter );
    for (ii = 0; ii < count; ii++)
    {
        chapter = hb_list_item(title->list_chapter, ii);
        if (chapter == NULL) break;
        if (chapter->title == NULL || chapter->title[0] == 0)
        {
            gchar *str;
            str = g_strdup_printf (_("Chapter %2d"), ii+1);
            ghb_array_append(chapters, ghb_string_value_new(str));
            g_free(str);
        }
        else
        {
            ghb_array_append(chapters, ghb_string_value_new(chapter->title));
        }
    }
    return chapters;
}

gboolean
ghb_ac3_in_audio_list(const GhbValue *audio_list)
{
    gint count, ii;

    count = ghb_array_len(audio_list);
    for (ii = 0; ii < count; ii++)
    {
        GhbValue *asettings;
        gint acodec;

        asettings = ghb_array_get(audio_list, ii);
        acodec = ghb_settings_audio_encoder_codec(asettings, "AudioEncoder");
        if (acodec & HB_ACODEC_AC3)
            return TRUE;
    }
    return FALSE;
}

static char custom_audio_bitrate_str[8];
static hb_rate_t custom_audio_bitrate =
{
    .name = custom_audio_bitrate_str,
    .rate = 0
};

static void
audio_bitrate_opts_add(GtkBuilder *builder, const gchar *name, gint rate)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("audio_bitrate_opts_add ()\n");

    if (rate >= 0 && rate < 8) return;

    custom_audio_bitrate.rate = rate;
    if (rate < 0)
    {
        snprintf(custom_audio_bitrate_str, 8, _("N/A"));
    }
    else
    {
        snprintf(custom_audio_bitrate_str, 8, "%d", rate);
    }

    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    if (!find_combo_item_by_int(GTK_TREE_MODEL(store), rate, &iter))
    {
        str = g_strdup_printf ("<small>%s</small>", custom_audio_bitrate.name);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, custom_audio_bitrate.name,
                           3, (gdouble)rate,
                           4, custom_audio_bitrate.name,
                           -1);
        g_free(str);
    }
}

void
ghb_audio_bitrate_opts_filter(
    GtkComboBox *combo,
    gint first_rate,
    gint last_rate)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gdouble ivalue;
    gboolean done = FALSE;

    g_debug("audio_bitrate_opts_filter ()\n");
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(store), &iter))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 3, &ivalue, -1);
            if ((int)ivalue < first_rate || (int)ivalue > last_rate)
            {
                gtk_list_store_set(store, &iter, 1, FALSE, -1);
            }
            else
            {
                gtk_list_store_set(store, &iter, 1, TRUE, -1);
            }
            done = !gtk_tree_model_iter_next (GTK_TREE_MODEL(store), &iter);
        } while (!done);
    }
}

static void
audio_bitrate_opts_update(
    GtkBuilder *builder,
    const gchar *name,
    gint first_rate,
    gint last_rate,
    gint extra_rate)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gdouble ivalue;
    gboolean done = FALSE;

    g_debug("audio_bitrate_opts_clean ()\n");
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    ghb_audio_bitrate_opts_filter(combo, first_rate, last_rate);

    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(store), &iter))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 3, &ivalue, -1);
            if (!search_audio_bitrates(ivalue) && (ivalue != extra_rate))
            {
                done = !gtk_list_store_remove(store, &iter);
            }
            else
            {
                done = !gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
            }
        } while (!done);
    }
    if (extra_rate > 0 && !search_audio_bitrates(extra_rate))
    {
        audio_bitrate_opts_add(builder, name, extra_rate);
    }
    else
    {
        custom_audio_bitrate.rate = 0;
        custom_audio_bitrate_str[0] = 0;
    }
}

void
ghb_audio_bitrate_opts_set(GtkComboBox *combo, gboolean extra)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gchar *str;

    g_debug("audio_bitrate_opts_set ()\n");
    store = GTK_LIST_STORE(gtk_combo_box_get_model (combo));
    gtk_list_store_clear(store);

    const hb_rate_t *rate;
    for (rate = hb_audio_bitrate_get_next(NULL); rate != NULL;
         rate = hb_audio_bitrate_get_next(rate))
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf ("<small>%s</small>", rate->name);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, rate->name,
                           3, (gdouble)rate->rate,
                           4, rate->name,
                           -1);
        g_free(str);
    }
    if (extra && custom_audio_bitrate.rate != 0)
    {
        gtk_list_store_append(store, &iter);
        str = g_strdup_printf ("<small>%s</small>", custom_audio_bitrate.name);
        gtk_list_store_set(store, &iter,
                           0, str,
                           1, TRUE,
                           2, custom_audio_bitrate.name,
                           3, (gdouble)custom_audio_bitrate.rate,
                           4, custom_audio_bitrate.name,
                           -1);
        g_free(str);
    }
}

static void
audio_bitrate_opts_set(GtkBuilder *builder, const gchar *name)
{
    GtkComboBox *combo = GTK_COMBO_BOX(GHB_WIDGET(builder, name));
    ghb_audio_bitrate_opts_set(combo, TRUE);
}

void
ghb_set_bitrate_opts(
    GtkBuilder *builder,
    gint first_rate,
    gint last_rate,
    gint extra_rate)
{
    audio_bitrate_opts_update(builder, "AudioBitrate", first_rate, last_rate, extra_rate);
}

const char*
ghb_audio_bitrate_get_short_name(int rate)
{
    if (rate == custom_audio_bitrate.rate)
    {
        return custom_audio_bitrate.name;
    }

    const hb_rate_t *hb_rate, *first;
    for (first = hb_rate = hb_audio_bitrate_get_next(NULL); hb_rate != NULL;
         hb_rate = hb_audio_bitrate_get_next(hb_rate))
    {
        if (rate == hb_rate->rate)
        {
            return hb_rate->name;
        }
    }
    return first->name;
}

const hb_rate_t*
ghb_lookup_audio_bitrate(const char *name)
{
    // Now find the matching rate info
    const hb_rate_t *hb_rate, *first;
    for (first = hb_rate = hb_audio_bitrate_get_next(NULL); hb_rate != NULL;
         hb_rate = hb_audio_bitrate_get_next(hb_rate))
    {
        if (!strncmp(name, hb_rate->name, 8))
        {
            return hb_rate;
        }
    }
    // Return a default rate if nothing matches
    return first;
}

int
ghb_lookup_audio_bitrate_rate(const char *name)
{
    return ghb_lookup_audio_bitrate(name)->rate;
}

int
ghb_settings_audio_bitrate_rate(const GhbValue *settings, const char *name)
{
    int result;
    char *rate_id = ghb_dict_get_string_xform(settings, name);
    result = ghb_lookup_audio_bitrate_rate(rate_id);
    g_free(rate_id);
    return result;
}

const hb_rate_t*
ghb_settings_audio_bitrate(const GhbValue *settings, const char *name)
{
    const hb_rate_t *result;
    char *rate_id = ghb_dict_get_string_xform(settings, name);
    result = ghb_lookup_audio_bitrate(rate_id);
    g_free(rate_id);
    return result;
}

static ghb_status_t hb_status;

void
ghb_combo_init(signal_user_data_t *ud)
{
    // Set up the list model for the combos
    init_ui_combo_boxes(ud->builder);
    // Populate all the combos
    ghb_update_ui_combo_box(ud, NULL, NULL, TRUE);
}

void
ghb_backend_init(gint debug)
{
    /* Init libhb */
    h_scan = hb_init( debug, 0 );
    h_queue = hb_init( debug, 0 );
    h_live = hb_init( debug, 0 );
}

void
ghb_backend_close()
{
    hb_close(&h_live);
    hb_close(&h_queue);
    hb_close(&h_scan);
    hb_global_close();
}

void ghb_backend_scan_stop()
{
    hb_scan_stop( h_scan );
}

void
ghb_backend_scan(const gchar *path, gint titleindex, gint preview_count, uint64_t min_duration)
{
    hb_scan( h_scan, path, titleindex, preview_count, 1, min_duration );
    hb_status.scan.state |= GHB_STATE_SCANNING;
    // initialize count and cur to something that won't cause FPE
    // when computing progress
    hb_status.scan.title_count = 1;
    hb_status.scan.title_cur = 0;
    hb_status.scan.preview_count = 1;
    hb_status.scan.preview_cur = 0;
    hb_status.scan.progress = 0;
}

void
ghb_backend_queue_scan(const gchar *path, gint titlenum)
{
    g_debug("ghb_backend_queue_scan()");
    hb_scan( h_queue, path, titlenum, 10, 0, 0 );
    hb_status.queue.state |= GHB_STATE_SCANNING;
}

gint
ghb_get_scan_state()
{
    return hb_status.scan.state;
}

gint
ghb_get_queue_state()
{
    return hb_status.queue.state;
}

void
ghb_clear_scan_state(gint state)
{
    hb_status.scan.state &= ~state;
}

void
ghb_clear_live_state(gint state)
{
    hb_status.live.state &= ~state;
}

void
ghb_clear_queue_state(gint state)
{
    hb_status.queue.state &= ~state;
}

void
ghb_set_scan_state(gint state)
{
    hb_status.scan.state |= state;
}

void
ghb_set_queue_state(gint state)
{
    hb_status.queue.state |= state;
}

void
ghb_get_status(ghb_status_t *status)
{
    memcpy(status, &hb_status, sizeof(ghb_status_t));
}

static void
update_status(hb_state_t *state, ghb_instance_status_t *status)
{
    switch( state->state )
    {
#define p state->param.scanning
        case HB_STATE_SCANNING:
        {
            status->state |= GHB_STATE_SCANNING;
            status->title_count = p.title_count;
            status->title_cur = p.title_cur;
            status->preview_count = p.preview_count;
            status->preview_cur = p.preview_cur;
            status->progress = p.progress;
        } break;
#undef p

        case HB_STATE_SCANDONE:
        {
            status->state &= ~GHB_STATE_SCANNING;
            status->state |= GHB_STATE_SCANDONE;
        } break;

#define p state->param.working
        case HB_STATE_WORKING:
            if (status->state & GHB_STATE_SCANNING)
            {
                status->state &= ~GHB_STATE_SCANNING;
                status->state |= GHB_STATE_SCANDONE;
            }
            status->state |= GHB_STATE_WORKING;
            status->state &= ~GHB_STATE_PAUSED;
            status->state &= ~GHB_STATE_SEARCHING;
            status->pass = p.pass;
            status->pass_count = p.pass_count;
            status->pass_id = p.pass_id;
            status->progress = p.progress;
            status->rate_cur = p.rate_cur;
            status->rate_avg = p.rate_avg;
            status->hours = p.hours;
            status->minutes = p.minutes;
            status->seconds = p.seconds;
            status->unique_id = p.sequence_id & 0xFFFFFF;
            break;

        case HB_STATE_SEARCHING:
            status->state |= GHB_STATE_SEARCHING;
            status->state &= ~GHB_STATE_WORKING;
            status->state &= ~GHB_STATE_PAUSED;
            status->pass = p.pass;
            status->pass_count = p.pass_count;
            status->pass_id = p.pass_id;
            status->progress = p.progress;
            status->rate_cur = p.rate_cur;
            status->rate_avg = p.rate_avg;
            status->hours = p.hours;
            status->minutes = p.minutes;
            status->seconds = p.seconds;
            status->unique_id = p.sequence_id & 0xFFFFFF;
            break;
#undef p

        case HB_STATE_PAUSED:
            status->state |= GHB_STATE_PAUSED;
            break;

        case HB_STATE_MUXING:
        {
            status->state |= GHB_STATE_MUXING;
        } break;

#define p state->param.workdone
        case HB_STATE_WORKDONE:
        {
            status->state |= GHB_STATE_WORKDONE;
            status->state &= ~GHB_STATE_MUXING;
            status->state &= ~GHB_STATE_PAUSED;
            status->state &= ~GHB_STATE_WORKING;
            status->state &= ~GHB_STATE_SEARCHING;
            switch (p.error)
            {
            case HB_ERROR_NONE:
                status->error = GHB_ERROR_NONE;
                break;
            case HB_ERROR_CANCELED:
                status->error = GHB_ERROR_CANCELED;
                break;
            default:
                status->error = GHB_ERROR_FAIL;
                break;
            }
        } break;
#undef p
    }
}

void
ghb_track_status()
{
    hb_state_t state;

    if (h_scan == NULL) return;
    hb_get_state( h_scan, &state );
    update_status(&state, &hb_status.scan);
    hb_get_state( h_queue, &state );
    update_status(&state, &hb_status.queue);
    hb_get_state( h_live, &state );
    update_status(&state, &hb_status.live);
}

hb_audio_config_t*
ghb_get_audio_info(const hb_title_t *title, gint track)
{
    if (title == NULL) return NULL;
    if (!hb_list_count(title->list_audio))
    {
        return NULL;
    }
    return hb_list_audio_config_item(title->list_audio, track);
}

hb_subtitle_t*
ghb_get_subtitle_info(const hb_title_t *title, gint track)
{
    if (title == NULL) return NULL;
    if (!hb_list_count(title->list_subtitle))
    {
        return NULL;
    }
    return hb_list_item(title->list_subtitle, track);
}

hb_list_t *
ghb_get_title_list()
{
    if (h_scan == NULL) return NULL;
    return hb_get_titles( h_scan );
}

gboolean
ghb_audio_is_passthru(gint acodec)
{
    g_debug("ghb_audio_is_passthru () \n");
    return (acodec & HB_ACODEC_PASS_FLAG) != 0;
}

gboolean
ghb_audio_can_passthru(gint acodec)
{
    g_debug("ghb_audio_can_passthru () \n");
    return (acodec & HB_ACODEC_PASS_MASK) != 0;
}

gint
ghb_get_default_acodec()
{
    return HB_ACODEC_FFAAC;
}

void
ghb_picture_settings_deps(signal_user_data_t *ud)
{
    gboolean autoscale, keep_aspect, enable_keep_aspect;
    gboolean enable_scale_width, enable_scale_height;
    gboolean enable_disp_width, enable_disp_height, enable_par;
    gint pic_par;
    GtkWidget *widget;

    pic_par = ghb_settings_combo_int(ud->settings, "PicturePAR");
    enable_keep_aspect = (pic_par != HB_ANAMORPHIC_STRICT &&
                          pic_par != HB_ANAMORPHIC_LOOSE);
    keep_aspect = ghb_dict_get_bool(ud->settings, "PictureKeepRatio");
    autoscale = ghb_dict_get_bool(ud->settings, "autoscale");

    enable_scale_width = enable_scale_height =
                         !autoscale && (pic_par != HB_ANAMORPHIC_STRICT);
    enable_disp_width = (pic_par == HB_ANAMORPHIC_CUSTOM) && !keep_aspect;
    enable_par = (pic_par == HB_ANAMORPHIC_CUSTOM) && !keep_aspect;
    enable_disp_height = FALSE;

    widget = GHB_WIDGET(ud->builder, "PictureModulus");
    gtk_widget_set_sensitive(widget, pic_par != HB_ANAMORPHIC_STRICT);
    widget = GHB_WIDGET(ud->builder, "PictureLooseCrop");
    gtk_widget_set_sensitive(widget, pic_par != HB_ANAMORPHIC_STRICT);
    widget = GHB_WIDGET(ud->builder, "scale_width");
    gtk_widget_set_sensitive(widget, enable_scale_width);
    widget = GHB_WIDGET(ud->builder, "scale_height");
    gtk_widget_set_sensitive(widget, enable_scale_height);
    widget = GHB_WIDGET(ud->builder, "PictureDisplayWidth");
    gtk_widget_set_sensitive(widget, enable_disp_width);
    widget = GHB_WIDGET(ud->builder, "PictureDisplayHeight");
    gtk_widget_set_sensitive(widget, enable_disp_height);
    widget = GHB_WIDGET(ud->builder, "PicturePARWidth");
    gtk_widget_set_sensitive(widget, enable_par);
    widget = GHB_WIDGET(ud->builder, "PicturePARHeight");
    gtk_widget_set_sensitive(widget, enable_par);
    widget = GHB_WIDGET(ud->builder, "PictureKeepRatio");
    gtk_widget_set_sensitive(widget, enable_keep_aspect);
    widget = GHB_WIDGET(ud->builder, "autoscale");
    gtk_widget_set_sensitive(widget, pic_par != HB_ANAMORPHIC_STRICT);
}

void
ghb_limit_rational( gint *num, gint *den, gint limit )
{
    if (*num < limit && *den < limit)
        return;

    if (*num > *den)
    {
        gdouble factor = (double)limit / *num;
        *num = limit;
        *den = factor * *den;
    }
    else
    {
        gdouble factor = (double)limit / *den;
        *den = limit;
        *num = factor * *num;
    }
}

void
ghb_set_scale_settings(GhbValue *settings, gint mode)
{
    gboolean keep_aspect;
    gint pic_par;
    gboolean autocrop, autoscale, loosecrop;
    gint crop[4] = {0,};
    gint width, height;
    gint crop_width, crop_height;
    gboolean keep_width = (mode & GHB_PIC_KEEP_WIDTH);
    gboolean keep_height = (mode & GHB_PIC_KEEP_HEIGHT);
    gint mod;
    gint max_width = 0;
    gint max_height = 0;

    g_debug("ghb_set_scale ()\n");

    pic_par = ghb_settings_combo_int(settings, "PicturePAR");
    if (pic_par == HB_ANAMORPHIC_STRICT)
    {
        ghb_dict_set_bool(settings, "autoscale", TRUE);
        ghb_dict_set_int(settings, "PictureModulus", 2);
        ghb_dict_set_bool(settings, "PictureLooseCrop", TRUE);
    }
    if (pic_par == HB_ANAMORPHIC_STRICT || pic_par == HB_ANAMORPHIC_LOOSE)
    {
        ghb_dict_set_bool(settings, "PictureKeepRatio", TRUE);
    }

    int title_id, titleindex;
    const hb_title_t * title;

    title_id = ghb_dict_get_int(settings, "title");
    title = ghb_lookup_title(title_id, &titleindex);
    if (title == NULL) return;

    hb_geometry_t srcGeo, resultGeo;
    hb_geometry_settings_t uiGeo;

    srcGeo.width   = title->geometry.width;
    srcGeo.height  = title->geometry.height;
    srcGeo.par     = title->geometry.par;

    // First configure widgets
    mod = ghb_settings_combo_int(settings, "PictureModulus");
    if (mod <= 0)
        mod = 16;
    keep_aspect = ghb_dict_get_bool(settings, "PictureKeepRatio");
    autocrop = ghb_dict_get_bool(settings, "PictureAutoCrop");
    autoscale = ghb_dict_get_bool(settings, "autoscale");
    // "PictureLooseCrop" is a flag that says we prefer to crop extra to
    // satisfy alignment constraints rather than scaling to satisfy them.
    loosecrop = ghb_dict_get_bool(settings, "PictureLooseCrop");
    // Align dimensions to either 16 or 2 pixels
    // The scaler crashes if the dimensions are not divisible by 2
    // x264 also will not accept dims that are not multiple of 2
    if (autoscale)
    {
        keep_width = FALSE;
        keep_height = FALSE;
    }

    if (autocrop)
    {
        crop[0] = title->crop[0];
        crop[1] = title->crop[1];
        crop[2] = title->crop[2];
        crop[3] = title->crop[3];
        ghb_dict_set_int(settings, "PictureTopCrop", crop[0]);
        ghb_dict_set_int(settings, "PictureBottomCrop", crop[1]);
        ghb_dict_set_int(settings, "PictureLeftCrop", crop[2]);
        ghb_dict_set_int(settings, "PictureRightCrop", crop[3]);
    }
    else
    {
        crop[0] = ghb_dict_get_int(settings, "PictureTopCrop");
        crop[1] = ghb_dict_get_int(settings, "PictureBottomCrop");
        crop[2] = ghb_dict_get_int(settings, "PictureLeftCrop");
        crop[3] = ghb_dict_get_int(settings, "PictureRightCrop");
        // Prevent manual crop from creating too small an image
        if (title->geometry.height - crop[0] < crop[1] + 16)
        {
            crop[0] = title->geometry.height - crop[1] - 16;
        }
        if (title->geometry.width - crop[2] < crop[3] + 16)
        {
            crop[2] = title->geometry.width - crop[3] - 16;
        }
    }
    if (loosecrop)
    {
        gint need1, need2;

        // Adjust the cropping to accomplish the desired width and height
        crop_width = title->geometry.width - crop[2] - crop[3];
        crop_height = title->geometry.height - crop[0] - crop[1];
        width = MOD_DOWN(crop_width, mod);
        height = MOD_DOWN(crop_height, mod);

        need1 = EVEN((crop_height - height) / 2);
        need2 = crop_height - height - need1;
        crop[0] += need1;
        crop[1] += need2;
        need1 = EVEN((crop_width - width) / 2);
        need2 = crop_width - width - need1;
        crop[2] += need1;
        crop[3] += need2;
        ghb_dict_set_int(settings, "PictureTopCrop", crop[0]);
        ghb_dict_set_int(settings, "PictureBottomCrop", crop[1]);
        ghb_dict_set_int(settings, "PictureLeftCrop", crop[2]);
        ghb_dict_set_int(settings, "PictureRightCrop", crop[3]);
    }
    uiGeo.crop[0] = crop[0];
    uiGeo.crop[1] = crop[1];
    uiGeo.crop[2] = crop[2];
    uiGeo.crop[3] = crop[3];

    crop_width = title->geometry.width - crop[2] - crop[3];
    crop_height = title->geometry.height - crop[0] - crop[1];
    if (autoscale)
    {
        width = crop_width;
        height = crop_height;
    }
    else
    {
        width = ghb_dict_get_int(settings, "scale_width");
        height = ghb_dict_get_int(settings, "scale_height");
        if (mode & GHB_PIC_USE_MAX)
        {
            max_width = MOD_DOWN(
                ghb_dict_get_int(settings, "PictureWidth"), mod);
            max_height = MOD_DOWN(
                ghb_dict_get_int(settings, "PictureHeight"), mod);
        }
    }
    g_debug("max_width %d, max_height %d\n", max_width, max_height);

    if (width < 16)
        width = 16;
    if (height < 16)
        height = 16;

    width = MOD_ROUND(width, mod);
    height = MOD_ROUND(height, mod);

    uiGeo.mode = pic_par;
    uiGeo.keep = 0;
    if (keep_width)
        uiGeo.keep |= HB_KEEP_WIDTH;
    if (keep_height)
        uiGeo.keep |= HB_KEEP_HEIGHT;
    if (keep_aspect)
        uiGeo.keep |= HB_KEEP_DISPLAY_ASPECT;
    uiGeo.itu_par = 0;
    uiGeo.modulus = mod;
    uiGeo.geometry.width = width;
    uiGeo.geometry.height = height;
    uiGeo.geometry.par = title->geometry.par;
    uiGeo.maxWidth = max_width;
    uiGeo.maxHeight = max_height;
    if (pic_par != HB_ANAMORPHIC_NONE)
    {
        if (pic_par == HB_ANAMORPHIC_CUSTOM && !keep_aspect)
        {
            if (mode & GHB_PIC_KEEP_PAR)
            {
                uiGeo.geometry.par.num =
                    ghb_dict_get_int(settings, "PicturePARWidth");
                uiGeo.geometry.par.den =
                    ghb_dict_get_int(settings, "PicturePARHeight");
            }
            else if (mode & (GHB_PIC_KEEP_DISPLAY_HEIGHT |
                             GHB_PIC_KEEP_DISPLAY_WIDTH))
            {
                uiGeo.geometry.par.num =
                        ghb_dict_get_int(settings, "PictureDisplayWidth");
                uiGeo.geometry.par.den = width;
            }
        }
        else
        {
            uiGeo.keep |= HB_KEEP_DISPLAY_ASPECT;
        }
    }
    // hb_set_anamorphic_size will adjust par, dar, and width/height
    // to conform to job parameters that have been set, including
    // maxWidth and maxHeight
    hb_set_anamorphic_size2(&srcGeo, &uiGeo, &resultGeo);

    ghb_dict_set_int(settings, "scale_width", resultGeo.width);
    ghb_dict_set_int(settings, "scale_height", resultGeo.height);

    gint disp_width;

    disp_width = ((gdouble)resultGeo.par.num / resultGeo.par.den) *
                 resultGeo.width + 0.5;

    ghb_dict_set_int(settings, "PicturePARWidth", resultGeo.par.num);
    ghb_dict_set_int(settings, "PicturePARHeight", resultGeo.par.den);
    ghb_dict_set_int(settings, "PictureDisplayWidth", disp_width);
    ghb_dict_set_int(settings, "PictureDisplayHeight", resultGeo.height);
}

void
ghb_update_display_aspect_label(signal_user_data_t *ud)
{
    gint disp_width, disp_height, dar_width, dar_height;
    gchar *str;

    disp_width = ghb_dict_get_int(ud->settings, "PictureDisplayWidth");
    disp_height = ghb_dict_get_int(ud->settings, "PictureDisplayHeight");
    hb_reduce(&dar_width, &dar_height, disp_width, disp_height);
    gint iaspect = dar_width * 9 / dar_height;
    if (dar_width > 2 * dar_height)
    {
        str = g_strdup_printf("%.2f : 1", (gdouble)dar_width / dar_height);
    }
    else if (iaspect <= 16 && iaspect >= 15)
    {
        str = g_strdup_printf("%.2f : 9", (gdouble)dar_width * 9 / dar_height);
    }
    else if (iaspect <= 12 && iaspect >= 11)
    {
        str = g_strdup_printf("%.2f : 3", (gdouble)dar_width * 3 / dar_height);
    }
    else
    {
        str = g_strdup_printf("%d : %d", dar_width, dar_height);
    }
    ghb_ui_update(ud, "display_aspect", ghb_string_value(str));
    g_free(str);
}

void
ghb_set_scale(signal_user_data_t *ud, gint mode)
{
    if (ud->scale_busy) return;
    ud->scale_busy = TRUE;

    ghb_set_scale_settings(ud->settings, mode);
    ghb_picture_settings_deps(ud);

    // Step needs to be at least 2 because odd widths cause scaler crash
    // subsampled chroma requires even crop values.
    GtkWidget *widget;
    int mod = ghb_settings_combo_int(ud->settings, "PictureModulus");
    widget = GHB_WIDGET (ud->builder, "scale_width");
    gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), mod, 16);
    widget = GHB_WIDGET (ud->builder, "scale_height");
    gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), mod, 16);

    // "PictureLooseCrop" is a flag that says we prefer to crop extra to
    // satisfy alignment constraints rather than scaling to satisfy them.
    gboolean loosecrop = ghb_dict_get_bool(ud->settings, "PictureLooseCrop");
    if (loosecrop)
    {
        widget = GHB_WIDGET (ud->builder, "PictureTopCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), mod, 16);
        widget = GHB_WIDGET (ud->builder, "PictureBottomCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), mod, 16);
        widget = GHB_WIDGET (ud->builder, "PictureLeftCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), mod, 16);
        widget = GHB_WIDGET (ud->builder, "PictureRightCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), mod, 16);
    }
    else
    {
        widget = GHB_WIDGET (ud->builder, "PictureTopCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), 2, 16);
        widget = GHB_WIDGET (ud->builder, "PictureBottomCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), 2, 16);
        widget = GHB_WIDGET (ud->builder, "PictureLeftCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), 2, 16);
        widget = GHB_WIDGET (ud->builder, "PictureRightCrop");
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON(widget), 2, 16);
    }

    ghb_ui_update_from_settings(ud, "autoscale", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureModulus", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureLooseCrop", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureKeepRatio", ud->settings);

    ghb_ui_update_from_settings(ud, "PictureTopCrop", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureBottomCrop", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureLeftCrop", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureRightCrop", ud->settings);

    ghb_ui_update_from_settings(ud, "scale_width", ud->settings);
    ghb_ui_update_from_settings(ud, "scale_height", ud->settings);

    ghb_ui_update_from_settings(ud, "PicturePARWidth", ud->settings);
    ghb_ui_update_from_settings(ud, "PicturePARHeight", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureDisplayWidth", ud->settings);
    ghb_ui_update_from_settings(ud, "PictureDisplayHeight", ud->settings);
    ghb_update_display_aspect_label(ud);
    ud->scale_busy = FALSE;
}

static void
get_preview_geometry(signal_user_data_t *ud, const hb_title_t *title,
                     hb_geometry_t *srcGeo, hb_geometry_settings_t *uiGeo)
{
    srcGeo->width  = title->geometry.width;
    srcGeo->height = title->geometry.height;
    srcGeo->par    = title->geometry.par;

    uiGeo->mode = ghb_settings_combo_int(ud->settings, "PicturePAR");
    uiGeo->keep = ghb_dict_get_bool(ud->settings, "PictureKeepRatio") ||
                                        uiGeo->mode == HB_ANAMORPHIC_STRICT  ||
                                        uiGeo->mode == HB_ANAMORPHIC_LOOSE;
    uiGeo->itu_par = 0;
    uiGeo->modulus = ghb_settings_combo_int(ud->settings, "PictureModulus");
    uiGeo->crop[0] = ghb_dict_get_int(ud->settings, "PictureTopCrop");
    uiGeo->crop[1] = ghb_dict_get_int(ud->settings, "PictureBottomCrop");
    uiGeo->crop[2] = ghb_dict_get_int(ud->settings, "PictureLeftCrop");
    uiGeo->crop[3] = ghb_dict_get_int(ud->settings, "PictureRightCrop");
    uiGeo->geometry.width = ghb_dict_get_int(ud->settings, "scale_width");
    uiGeo->geometry.height = ghb_dict_get_int(ud->settings, "scale_height");
    uiGeo->geometry.par.num = ghb_dict_get_int(ud->settings, "PicturePARWidth");
    uiGeo->geometry.par.den = ghb_dict_get_int(ud->settings, "PicturePARHeight");
    uiGeo->maxWidth = 0;
    uiGeo->maxHeight = 0;
    if (ghb_dict_get_bool(ud->prefs, "preview_show_crop"))
    {
        gdouble xscale = (gdouble)uiGeo->geometry.width /
                  (title->geometry.width - uiGeo->crop[2] - uiGeo->crop[3]);
        gdouble yscale = (gdouble)uiGeo->geometry.height /
                  (title->geometry.height - uiGeo->crop[0] - uiGeo->crop[1]);

        uiGeo->geometry.width += xscale * (uiGeo->crop[2] + uiGeo->crop[3]);
        uiGeo->geometry.height += yscale * (uiGeo->crop[0] + uiGeo->crop[1]);
        uiGeo->crop[0] = 0;
        uiGeo->crop[1] = 0;
        uiGeo->crop[2] = 0;
        uiGeo->crop[3] = 0;
        uiGeo->modulus = 2;
    }
}

gboolean
ghb_validate_filter_string(const gchar *str, gint max_fields)
{
    gint fields = 0;
    gchar *end;

    if (str == NULL || *str == 0) return TRUE;
    while (*str)
    {
        g_strtod(str, &end);
        if (str != end)
        { // Found a numeric value
            fields++;
            // negative max_fields means infinate
            if (max_fields >= 0 && fields > max_fields) return FALSE;
            if (*end == 0)
                return TRUE;
            if (*end != ':')
                return FALSE;
            str = end + 1;
        }
        else
            return FALSE;
    }
    return FALSE;
}

gboolean
ghb_validate_filters(GhbValue *settings, GtkWindow *parent)
{
    const gchar *str;
    gint index;
    gchar *message;

    gboolean decomb_deint = ghb_dict_get_bool(settings, "PictureDecombDeinterlace");
    // deinte
    index = ghb_settings_combo_int(settings, "PictureDeinterlace");
    if (!decomb_deint && index == 1)
    {
        str = ghb_dict_get_string(settings, "PictureDeinterlaceCustom");
        if (!ghb_validate_filter_string(str, -1))
        {
            message = g_strdup_printf(
                        _("Invalid Deinterlace Settings:\n\n%s\n"), str);
            ghb_message_dialog(parent, GTK_MESSAGE_ERROR,
                               message, _("Cancel"), NULL);
            g_free(message);
            return FALSE;
        }
    }
    // detel
    index = ghb_settings_combo_int(settings, "PictureDetelecine");
    if (index == 1)
    {
        str = ghb_dict_get_string(settings, "PictureDetelecineCustom");
        if (!ghb_validate_filter_string(str, -1))
        {
            message = g_strdup_printf(
                        _("Invalid Detelecine Settings:\n\n%s\n"), str);
            ghb_message_dialog(parent, GTK_MESSAGE_ERROR,
                               message, _("Cancel"), NULL);
            g_free(message);
            return FALSE;
        }
    }
    // decomb
    index = ghb_settings_combo_int(settings, "PictureDecomb");
    if (decomb_deint && index == 1)
    {
        str = ghb_dict_get_string(settings, "PictureDecombCustom");
        if (!ghb_validate_filter_string(str, -1))
        {
            message = g_strdup_printf(
                        _("Invalid Decomb Settings:\n\n%s\n"), str);
            ghb_message_dialog(parent, GTK_MESSAGE_ERROR,
                               message, _("Cancel"), NULL);
            g_free(message);
            return FALSE;
        }
    }
    // denoise
    // TODO
    return TRUE;
}

gboolean
ghb_validate_video(GhbValue *settings, GtkWindow *parent)
{
    gint vcodec;
    gchar *message;
    const char *mux_id;
    const hb_container_t *mux;

    mux_id = ghb_dict_get_string(settings, "FileFormat");
    mux = ghb_lookup_container_by_name(mux_id);

    vcodec = ghb_settings_video_encoder_codec(settings, "VideoEncoder");
    if ((mux->format & HB_MUX_MASK_MP4) && (vcodec == HB_VCODEC_THEORA))
    {
        // mp4/theora combination is not supported.
        message = g_strdup_printf(
                    _("Theora is not supported in the MP4 container.\n\n"
                    "You should choose a different video codec or container.\n"
                    "If you continue, FFMPEG will be chosen for you."));
        if (!ghb_message_dialog(parent, GTK_MESSAGE_WARNING,
                                message, _("Cancel"), _("Continue")))
        {
            g_free(message);
            return FALSE;
        }
        g_free(message);
        vcodec = hb_video_encoder_get_default(mux->format);
        ghb_dict_set_string(settings, "VideoEncoder",
                                hb_video_encoder_get_short_name(vcodec));
    }
    return TRUE;
}

gboolean
ghb_validate_subtitles(GhbValue *settings, GtkWindow *parent)
{
    gint title_id, titleindex;
    const hb_title_t * title;
    gchar *message;

    title_id = ghb_dict_get_int(settings, "title");
    title = ghb_lookup_title(title_id, &titleindex);
    if (title == NULL)
    {
        /* No valid title, stop right there */
        g_message(_("No title found.\n"));
        return FALSE;
    }

    const GhbValue *slist, *subtitle;
    gint count, ii, source, track;
    gboolean burned, one_burned = FALSE;

    slist = ghb_dict_get_value(settings, "subtitle_list");
    count = ghb_array_len(slist);
    for (ii = 0; ii < count; ii++)
    {
        subtitle = ghb_array_get(slist, ii);
        track = ghb_dict_get_int(subtitle, "SubtitleTrack");
        source = ghb_dict_get_int(subtitle, "SubtitleSource");
        burned = track != -1 &&
                 ghb_dict_get_bool(subtitle, "SubtitleBurned");
        if (burned && one_burned)
        {
            // MP4 can only handle burned vobsubs.  make sure there isn't
            // already something burned in the list
            message = g_strdup_printf(
            _("Only one subtitle may be burned into the video.\n\n"
                "You should change your subtitle selections.\n"
                "If you continue, some subtitles will be lost."));
            if (!ghb_message_dialog(parent, GTK_MESSAGE_WARNING,
                                    message, _("Cancel"), _("Continue")))
            {
                g_free(message);
                return FALSE;
            }
            g_free(message);
            break;
        }
        else if (burned)
        {
            one_burned = TRUE;
        }
        if (source == SRTSUB)
        {
            const gchar *filename;

            filename = ghb_dict_get_string(subtitle, "SrtFile");
            if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR))
            {
                message = g_strdup_printf(
                _("Srt file does not exist or not a regular file.\n\n"
                    "You should choose a valid file.\n"
                    "If you continue, this subtitle will be ignored."));
                if (!ghb_message_dialog(parent, GTK_MESSAGE_WARNING, message,
                    _("Cancel"), _("Continue")))
                {
                    g_free(message);
                    return FALSE;
                }
                g_free(message);
                break;
            }
        }
    }
    return TRUE;
}

gboolean
ghb_validate_audio(GhbValue *settings, GtkWindow *parent)
{
    gint title_id, titleindex;
    const hb_title_t * title;
    gchar *message;

    title_id = ghb_dict_get_int(settings, "title");
    title = ghb_lookup_title(title_id, &titleindex);
    if (title == NULL)
    {
        /* No valid title, stop right there */
        g_message(_("No title found.\n"));
        return FALSE;
    }

    const char *mux_id;
    const hb_container_t *mux;

    mux_id = ghb_dict_get_string(settings, "FileFormat");
    mux = ghb_lookup_container_by_name(mux_id);

    const GhbValue *audio_list;
    gint count, ii;

    audio_list = ghb_dict_get_value(settings, "audio_list");
    count = ghb_array_len(audio_list);
    for (ii = 0; ii < count; ii++)
    {
        GhbValue *asettings;
        hb_audio_config_t *aconfig;
        int track, codec;

        asettings = ghb_array_get(audio_list, ii);
        track = ghb_dict_get_int(asettings, "AudioTrack");
        codec = ghb_settings_audio_encoder_codec(asettings, "AudioEncoder");
        if (codec == HB_ACODEC_AUTO_PASS)
            continue;

        aconfig = (hb_audio_config_t *) hb_list_audio_config_item(
                                            title->list_audio, track );
        if ( ghb_audio_is_passthru(codec) &&
            !(ghb_audio_can_passthru(aconfig->in.codec) &&
             (aconfig->in.codec & codec)))
        {
            // Not supported.  AC3 is passthrough only, so input must be AC3
            message = g_strdup_printf(
                        _("The source does not support Pass-Thru.\n\n"
                        "You should choose a different audio codec.\n"
                        "If you continue, one will be chosen for you."));
            if (!ghb_message_dialog(parent, GTK_MESSAGE_WARNING,
                                    message, _("Cancel"), _("Continue")))
            {
                g_free(message);
                return FALSE;
            }
            g_free(message);
            if ((codec & HB_ACODEC_AC3) ||
                (aconfig->in.codec & HB_ACODEC_MASK) == HB_ACODEC_DCA)
            {
                codec = HB_ACODEC_AC3;
            }
            else if (mux->format & HB_MUX_MASK_MKV)
            {
                codec = HB_ACODEC_LAME;
            }
            else
            {
                codec = HB_ACODEC_FFAAC;
            }
            const char *name = hb_audio_encoder_get_short_name(codec);
            ghb_dict_set_string(asettings, "AudioEncoder", name);
        }
        gchar *a_unsup = NULL;
        gchar *mux_s = NULL;
        if (mux->format & HB_MUX_MASK_MP4)
        {
            mux_s = "MP4";
            // mp4/vorbis|DTS combination is not supported.
            if (codec == HB_ACODEC_VORBIS)
            {
                a_unsup = "Vorbis";
                codec = HB_ACODEC_FFAAC;
            }
        }
        if (a_unsup)
        {
            message = g_strdup_printf(
                        _("%s is not supported in the %s container.\n\n"
                        "You should choose a different audio codec.\n"
                        "If you continue, one will be chosen for you."), a_unsup, mux_s);
            if (!ghb_message_dialog(parent, GTK_MESSAGE_WARNING,
                                    message, _("Cancel"), _("Continue")))
            {
                g_free(message);
                return FALSE;
            }
            g_free(message);
            const char *name = hb_audio_encoder_get_short_name(codec);
            ghb_dict_set_string(asettings, "AudioEncoder", name);
        }

        const hb_mixdown_t *mix;
        mix = ghb_settings_mixdown(asettings, "AudioMixdown");

        const gchar *mix_unsup = NULL;
        if (!hb_mixdown_is_supported(mix->amixdown, codec, aconfig->in.channel_layout))
        {
            mix_unsup = mix->name;
        }
        if (mix_unsup)
        {
            message = g_strdup_printf(
                        _("The source audio does not support %s mixdown.\n\n"
                        "You should choose a different mixdown.\n"
                        "If you continue, one will be chosen for you."), mix_unsup);
            if (!ghb_message_dialog(parent, GTK_MESSAGE_WARNING,
                                    message, _("Cancel"), _("Continue")))
            {
                g_free(message);
                return FALSE;
            }
            g_free(message);
            int amixdown = ghb_get_best_mix(aconfig, codec, mix->amixdown);
            ghb_dict_set_string(asettings, "AudioMixdown",
                                    hb_mixdown_get_short_name(amixdown));
        }
    }
    return TRUE;
}

static void
add_job(hb_handle_t *h, GhbValue *js, gint unique_id)
{
    hb_dict_t * dict;
    json_error_t error;
    int ii, count;

    // Assumes that the UI has reduced geometry settings to only the
    // necessary PAR value

    const char *mux_name;
    const hb_container_t *mux;
    int mux_id;

    mux_name = ghb_dict_get_string(js, "FileFormat");
    mux = ghb_lookup_container_by_name(mux_name);

    mux_id = mux->format;

    int p_to_p = -1, range_seek_points = 0, chapter_markers = 0;
    int64_t range_start = 0, range_end = 0;
    range_start = ghb_dict_get_int(js, "start_frame") + 1;
    const char *range_type = "chapter";
    if (range_start != 0)
    {
        range_type = "preview";
        GhbValue *prefs = ghb_dict_get_value(js, "Preferences");
        range_seek_points = ghb_dict_get_int(prefs, "preview_count");
        range_end = ghb_dict_get_int(prefs, "live_duration") * 90000LL;
    }
    else
    {
        chapter_markers = ghb_dict_get_bool(js, "ChapterMarkers");
        p_to_p = ghb_settings_combo_int(js, "PtoPType");
        switch (p_to_p)
        {
            default:
            case 0: // Chapter range
            {
                range_type = "chapter";
                range_start = ghb_dict_get_int(js, "start_point");
                range_end  = ghb_dict_get_int(js, "end_point");
                if (range_start == range_end)
                    chapter_markers = 0;
            } break;
            case 1: // PTS range
            {
                double start, end;
                range_type = "time";
                start = ghb_dict_get_double(js, "start_point");
                end   = ghb_dict_get_double(js, "end_point");
                range_start = (int64_t)start * 90000;
                range_end  = (int64_t)end   * 90000 - range_start;
            } break;
            case 2: // Frame range
            {
                range_type = "frame";
                range_start = ghb_dict_get_int(js, "start_point") - 1;
                range_end  = ghb_dict_get_int(js, "end_point")   - 1 -
                              range_start;
            } break;
        }
    }

    const char *path = ghb_dict_get_string(js, "source");
    int title_id = ghb_dict_get_int(js, "title");

    int angle = ghb_dict_get_int(js, "angle");

    hb_rational_t par;
    par.num = ghb_dict_get_int(js, "PicturePARWidth");
    par.den = ghb_dict_get_int(js, "PicturePARHeight");

    int vcodec, acodec_copy_mask, acodec_fallback, grayscale;
    vcodec = ghb_settings_video_encoder_codec(js, "VideoEncoder");
    acodec_copy_mask = ghb_get_copy_mask(js);
    acodec_fallback = ghb_settings_audio_encoder_codec(js, "AudioEncoderFallback");
    grayscale   = ghb_dict_get_bool(js, "VideoGrayScale");

    dict = json_pack_ex(&error, 0,
    "{"
    // SequenceID
    "s:o,"
    // Destination {Mux, ChapterMarkers, ChapterList}
    "s:{s:o, s:o, s[]},"
    // Source {Path, Title, Angle}
    "s:{s:o, s:o, s:o,},"
    // PAR {Num, Den}
    "s:{s:o, s:o},"
    // Video {Codec}
    "s:{s:o},"
    // Audio {CopyMask, FallbackEncoder, AudioList []}
    "s:{s:o, s:o, s:[]},"
    // Subtitles {Search {Enable}, SubtitleList []}
    "s:{s:{s:o}, s:[]},"
    // Metadata
    "s:{},"
    // Filters {Grayscale, FilterList []}
    "s:{s:o, s:[]}"
    "}",
        "SequenceID",           hb_value_int(unique_id),
        "Destination",
            "Mux",              hb_value_int(mux_id),
            "ChapterMarkers",   hb_value_bool(chapter_markers),
            "ChapterList",
        "Source",
            "Path",             hb_value_string(path),
            "Title",            hb_value_int(title_id),
            "Angle",            hb_value_int(angle),
        "PAR",
            "Num",              hb_value_int(par.num),
            "Den",              hb_value_int(par.den),
        "Video",
            "Encoder",          hb_value_int(vcodec),
        "Audio",
            "CopyMask",         hb_value_int(acodec_copy_mask),
            "FallbackEncoder",  hb_value_int(acodec_fallback),
            "AudioList",
        "Subtitle",
            "Search",
                "Enable",       hb_value_bool(FALSE),
            "SubtitleList",
        "Metadata",
        "Filters",
            "Grayscale",        hb_value_bool(grayscale),
            "FilterList"
    );
    if (dict == NULL)
    {
        g_warning("json pack job failure: %s", error.text);
        return;
    }
    const char *dest = ghb_dict_get_string(js, "destination");
    hb_dict_t *dest_dict = hb_dict_get(dict, "Destination");
    if (dest != NULL)
    {
        hb_dict_set(dest_dict, "File", hb_value_string(dest));
    }
    if (mux_id & HB_MUX_MASK_MP4)
    {
        int mp4_optimize, ipod_atom = 0;
        mp4_optimize = ghb_dict_get_bool(js, "Mp4HttpOptimize");
        if (vcodec == HB_VCODEC_X264)
        {
            ipod_atom = ghb_dict_get_bool(js, "Mp4iPodCompatible");
        }
        hb_dict_t *mp4_dict;
        mp4_dict = json_pack_ex(&error, 0, "{s:o, s:o}",
            "Mp4Optimize",      hb_value_bool(mp4_optimize),
            "IpodAtom",         hb_value_bool(ipod_atom));
        if (mp4_dict == NULL)
        {
            g_warning("json pack mp4 failure: %s", error.text);
            return;
        }
        hb_dict_set(dest_dict, "Mp4Options", mp4_dict);
    }
    hb_dict_t *source_dict = hb_dict_get(dict, "Source");
    if (range_start || range_end)
    {
        hb_dict_t *range_dict = hb_dict_init();
        hb_dict_set(range_dict, "Type", hb_value_string(range_type));
        if (range_start)
            hb_dict_set(range_dict, "Start", hb_value_int(range_start));
        if (range_end)
            hb_dict_set(range_dict, "End",   hb_value_int(range_end));
        if (range_seek_points)
        {
            hb_dict_set(range_dict, "SeekPoints",
                    hb_value_int(range_seek_points));
        }
        hb_dict_set(source_dict, "Range", range_dict);
    }

    hb_dict_t *video_dict = hb_dict_get(dict, "Video");
    if (ghb_dict_get_bool(js, "vquality_type_constant"))
    {
        double vquality = ghb_dict_get_double(js, "VideoQualitySlider");
        hb_dict_set(video_dict, "Quality", hb_value_double(vquality));
    }
    else if (ghb_dict_get_bool(js, "vquality_type_bitrate"))
    {
        int vbitrate, twopass, fastfirstpass;
        vbitrate = ghb_dict_get_int(js, "VideoAvgBitrate");
        twopass = ghb_dict_get_bool(js, "VideoTwoPass");
        fastfirstpass = ghb_dict_get_bool(js, "VideoTurboTwoPass");
        hb_dict_set(video_dict, "Bitrate", hb_value_int(vbitrate));
        hb_dict_set(video_dict, "TwoPass", hb_value_bool(twopass));
        hb_dict_set(video_dict, "Turbo", hb_value_bool(fastfirstpass));
    }
    ghb_set_video_encoder_opts(video_dict, js);

    hb_dict_t *meta_dict = hb_dict_get(dict, "Metadata");
    const char * meta;

    meta = ghb_dict_get_string(js, "MetaName");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "Name", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaArtist");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "Artist", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaAlbumArtist");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "AlbumArtist", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaReleaseDate");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "ReleaseDate", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaComment");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "Comment", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaGenre");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "Genre", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaDescription");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "Description", hb_value_string(meta));
    }
    meta = ghb_dict_get_string(js, "MetaLongDescription");
    if (meta && *meta)
    {
        hb_dict_set(meta_dict, "LongDescription", hb_value_string(meta));
    }

    // process chapter list
    if (chapter_markers)
    {
        hb_value_array_t *chapter_list = hb_dict_get(dest_dict, "ChapterList");
        GhbValue *chapters;
        GhbValue *chapter;
        gint chap;
        gint count;

        chapters = ghb_dict_get_value(js, "chapter_list");
        count = ghb_array_len(chapters);
        for(chap = 0; chap < count; chap++)
        {
            hb_dict_t *chapter_dict;
            gchar *name;

            name = NULL;
            chapter = ghb_array_get(chapters, chap);
            name = ghb_value_get_string_xform(chapter);
            if (name == NULL)
            {
                name = g_strdup_printf (_("Chapter %2d"), chap+1);
            }
            chapter_dict = json_pack_ex(&error, 0, "{s:o}",
                                    "Name", hb_value_string(name));
            if (chapter_dict == NULL)
            {
                g_warning("json pack chapter failure: %s", error.text);
                return;
            }
            hb_value_array_append(chapter_list, chapter_dict);
            g_free(name);
        }
    }

    // Create filter list
    hb_dict_t *filters_dict = hb_dict_get(dict, "Filters");
    hb_value_array_t *filter_list = hb_dict_get(filters_dict, "FilterList");
    hb_dict_t *filter_dict;
    char *filter_str;

    // Crop scale filter
    int width, height, crop[4];
    width = ghb_dict_get_int(js, "scale_width");
    height = ghb_dict_get_int(js, "scale_height");

    crop[0] = ghb_dict_get_int(js, "PictureTopCrop");
    crop[1] = ghb_dict_get_int(js, "PictureBottomCrop");
    crop[2] = ghb_dict_get_int(js, "PictureLeftCrop");
    crop[3] = ghb_dict_get_int(js, "PictureRightCrop");

    filter_str = g_strdup_printf("%d:%d:%d:%d:%d:%d",
                            width, height, crop[0], crop[1], crop[2], crop[3]);
    filter_dict = json_pack_ex(&error, 0, "{s:o, s:o}",
                            "ID",       hb_value_int(HB_FILTER_CROP_SCALE),
                            "Settings", hb_value_string(filter_str));
    if (filter_dict == NULL)
    {
        g_warning("json pack scale filter failure: %s", error.text);
        return;
    }
    hb_value_array_append(filter_list, filter_dict);
    g_free(filter_str);

    // detelecine filter
    gint detel = ghb_settings_combo_int(js, "PictureDetelecine");
    if (detel)
    {
        const char *filter_str = NULL;
        if (detel != 1)
        {
            if (detel_opts.map[detel].svalue != NULL)
                filter_str = detel_opts.map[detel].svalue;
        }
        else
        {
            filter_str = ghb_dict_get_string(js, "PictureDetelecineCustom");
        }
        filter_dict = json_pack_ex(&error, 0, "{s:o}",
                                "ID", hb_value_int(HB_FILTER_DETELECINE));
        if (filter_dict == NULL)
        {
            g_warning("json pack detelecine filter failure: %s", error.text);
            return;
        }
        if (filter_str != NULL)
        {
            hb_dict_set(filter_dict, "Settings", hb_value_string(filter_str));
        }
        hb_value_array_append(filter_list, filter_dict);
    }

    // Decomb filter
    gboolean decomb_deint;
    gint decomb, deint;
    decomb_deint = ghb_dict_get_bool(js, "PictureDecombDeinterlace");
    decomb = ghb_settings_combo_int(js, "PictureDecomb");
    deint = ghb_settings_combo_int(js, "PictureDeinterlace");
    if (decomb_deint && decomb)
    {
        const char *filter_str = NULL;
        if (decomb != 1)
        {
            if (decomb_opts.map[decomb].svalue != NULL)
                filter_str = decomb_opts.map[decomb].svalue;
        }
        else
        {
            filter_str = ghb_dict_get_string(js, "PictureDecombCustom");
        }
        filter_dict = json_pack_ex(&error, 0, "{s:o}",
                                "ID", hb_value_int(HB_FILTER_DECOMB));
        if (filter_dict == NULL)
        {
            g_warning("json pack decomb filter failure: %s", error.text);
            return;
        }
        if (filter_str != NULL)
        {
            hb_dict_set(filter_dict, "Settings", hb_value_string(filter_str));
        }
        hb_value_array_append(filter_list, filter_dict);
    }

    // Deinterlace filter
    if ( !decomb_deint && deint )
    {
        const char *filter_str = NULL;
        if (deint != 1)
        {
            if (deint_opts.map[deint].svalue != NULL)
                filter_str = deint_opts.map[deint].svalue;
        }
        else
        {
            filter_str = ghb_dict_get_string(js, "PictureDeinterlaceCustom");
        }
        filter_dict = json_pack_ex(&error, 0, "{s:o}",
                                "ID", hb_value_int(HB_FILTER_DEINTERLACE));
        if (filter_dict == NULL)
        {
            g_warning("json pack deinterlace filter failure: %s", error.text);
            return;
        }
        if (filter_str != NULL)
        {
            hb_dict_set(filter_dict, "Settings", hb_value_string(filter_str));
        }
        hb_value_array_append(filter_list, filter_dict);
    }

    // Denoise filter
    if (strcmp(ghb_dict_get_string(js, "PictureDenoiseFilter"), "off"))
    {
        int filter_id = HB_FILTER_HQDN3D;
        if (!strcmp(ghb_dict_get_string(js, "PictureDenoiseFilter"), "nlmeans"))
            filter_id = HB_FILTER_NLMEANS;

        if (!strcmp(ghb_dict_get_string(js, "PictureDenoisePreset"), "custom"))
        {
            const char *filter_str;
            filter_str = ghb_dict_get_string(js, "PictureDenoiseCustom");
            filter_dict = json_pack_ex(&error, 0, "{s:o, s:o}",
                                "ID",       hb_value_int(filter_id),
                                "Settings", hb_value_string(filter_str));
            if (filter_dict == NULL)
            {
                g_warning("json pack denoise filter failure: %s", error.text);
                return;
            }
            hb_value_array_append(filter_list, filter_dict);
        }
        else
        {
            const char *preset, *tune;
            preset = ghb_dict_get_string(js, "PictureDenoisePreset");
            tune = ghb_dict_get_string(js, "PictureDenoiseTune");
            filter_str = hb_generate_filter_settings(filter_id, preset, tune);
            filter_dict = json_pack_ex(&error, 0, "{s:o, s:o}",
                                "ID",       hb_value_int(filter_id),
                                "Settings", hb_value_string(filter_str));
            if (filter_dict == NULL)
            {
                g_warning("json pack denoise filter failure: %s", error.text);
                return;
            }
            hb_value_array_append(filter_list, filter_dict);
            g_free(filter_str);
        }
    }

    // Deblock filter
    gint deblock = ghb_dict_get_int(js, "PictureDeblock");
    if( deblock >= 5 )
    {
        filter_str = g_strdup_printf("%d", deblock);
        filter_dict = json_pack_ex(&error, 0, "{s:o, s:o}",
                            "ID",       hb_value_int(HB_FILTER_DEBLOCK),
                            "Settings", hb_value_string(filter_str));
        if (filter_dict == NULL)
        {
            g_warning("json pack deblock filter failure: %s", error.text);
            return;
        }
        hb_value_array_append(filter_list, filter_dict);
        g_free(filter_str);
    }

    // VFR filter
    gint vrate_den = ghb_settings_video_framerate_rate(js, "VideoFramerate");
    gint cfr;
    if (ghb_dict_get_bool(js, "VideoFrameratePFR"))
        cfr = 2;
    else if (ghb_dict_get_bool(js, "VideoFramerateCFR"))
        cfr = 1;
    else
        cfr = 0;

    // x264 zero latency requires CFR encode
    if (ghb_dict_get_bool(js, "x264ZeroLatency"))
    {
        cfr = 1;
        ghb_log("zerolatency x264 tune selected, forcing constant framerate");
    }

    if (vrate_den == 0)
    {
        filter_str = g_strdup_printf("%d", cfr);
    }
    else
    {
        filter_str = g_strdup_printf("%d:%d:%d", cfr, 27000000, vrate_den);
    }
    filter_dict = json_pack_ex(&error, 0, "{s:o, s:o}",
                        "ID",       hb_value_int(HB_FILTER_VFR),
                        "Settings", hb_value_string(filter_str));
    if (filter_dict == NULL)
    {
        g_warning("json pack vfr filter failure: %s", error.text);
        return;
    }
    hb_value_array_append(filter_list, filter_dict);
    g_free(filter_str);

    // Create audio list
    hb_dict_t *audios_dict = hb_dict_get(dict, "Audio");
    hb_value_array_t *json_audio_list = hb_dict_get(audios_dict, "AudioList");
    const GhbValue *audio_list;

    audio_list = ghb_dict_get_value(js, "audio_list");
    count = ghb_array_len(audio_list);
    for (ii = 0; ii < count; ii++)
    {
        hb_dict_t *audio_dict;
        GhbValue *asettings;
        int track, acodec, mixdown, samplerate;
        const char *aname;
        double gain, drc, quality;

        asettings = ghb_array_get(audio_list, ii);
        track = ghb_dict_get_int(asettings, "AudioTrack");
        aname = ghb_dict_get_string(asettings, "AudioTrackName");
        acodec = ghb_settings_audio_encoder_codec(asettings, "AudioEncoder");
        audio_dict = json_pack_ex(&error, 0,
            "{s:o, s:o}",
            "Track",                hb_value_int(track),
            "Encoder",              hb_value_int(acodec));
        if (audio_dict == NULL)
        {
            g_warning("json pack audio failure: %s", error.text);
            return;
        }
        if (aname != NULL && aname[0] != 0)
        {
            hb_dict_set(audio_dict, "Name", hb_value_string(aname));
        }

        // It would be better if this were done in libhb for us, but its not yet.
        if (!ghb_audio_is_passthru(acodec))
        {
            gain = ghb_dict_get_double(asettings, "AudioTrackGainSlider");
            if (gain > 0)
                hb_dict_set(audio_dict, "Gain", hb_value_double(gain));
            drc = ghb_dict_get_double(asettings, "AudioTrackDRCSlider");
            if (drc < 1.0)
                drc = 0.0;
            if (drc > 0)
                hb_dict_set(audio_dict, "DRC", hb_value_double(drc));

            mixdown = ghb_settings_mixdown_mix(asettings, "AudioMixdown");
            hb_dict_set(audio_dict, "Mixdown", hb_value_int(mixdown));

            samplerate = ghb_settings_audio_samplerate_rate(
                                            asettings, "AudioSamplerate");
            hb_dict_set(audio_dict, "Samplerate", hb_value_int(samplerate));
            gboolean qe;
            qe = ghb_dict_get_bool(asettings, "AudioTrackQualityEnable");
            quality = ghb_dict_get_double(asettings, "AudioTrackQuality");
            if (qe && quality != HB_INVALID_AUDIO_QUALITY)
            {
                hb_dict_set(audio_dict, "Quality", hb_value_double(quality));
            }
            else
            {
                int bitrate =
                    ghb_settings_audio_bitrate_rate(asettings, "AudioBitrate");
                bitrate = hb_audio_bitrate_get_best(
                                        acodec, bitrate, samplerate, mixdown);
                hb_dict_set(audio_dict, "Bitrate", hb_value_int(bitrate));
            }
        }

        hb_value_array_append(json_audio_list, audio_dict);
    }

    // Create subtitle list
    hb_dict_t *subtitles_dict = hb_dict_get(dict, "Subtitle");
    hb_value_array_t *json_subtitle_list = hb_dict_get(subtitles_dict, "SubtitleList");
    const GhbValue *subtitle_list;

    subtitle_list = ghb_dict_get_value(js, "subtitle_list");
    count = ghb_array_len(subtitle_list);
    for (ii = 0; ii < count; ii++)
    {
        hb_dict_t *subtitle_dict;
        gint track;
        gboolean force, burned, def, one_burned = FALSE;
        GhbValue *ssettings;
        gint source;

        ssettings = ghb_array_get(subtitle_list, ii);

        force = ghb_dict_get_bool(ssettings, "SubtitleForced");
        burned = ghb_dict_get_bool(ssettings, "SubtitleBurned");
        def = ghb_dict_get_bool(ssettings, "SubtitleDefaultTrack");
        source = ghb_dict_get_int(ssettings, "SubtitleSource");

        if (source == SRTSUB)
        {
            const gchar *filename, *lang, *code;
            int offset;
            filename = ghb_dict_get_string(ssettings, "SrtFile");
            if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR))
            {
                continue;
            }
            offset = ghb_dict_get_int(ssettings, "SrtOffset");
            lang = ghb_dict_get_string(ssettings, "SrtLanguage");
            code = ghb_dict_get_string(ssettings, "SrtCodeset");
            if (burned && !one_burned && hb_subtitle_can_burn(SRTSUB))
            {
                // Only allow one subtitle to be burned into the video
                one_burned = TRUE;
            }
            else
            {
                burned = FALSE;
            }
            subtitle_dict = json_pack_ex(&error, 0,
                "{s:o, s:o, s:o, s:{s:o, s:o, s:o}}",
                "Default",  hb_value_bool(def),
                "Burn",     hb_value_bool(burned),
                "Offset",   hb_value_int(offset),
                "SRT",
                    "Filename", hb_value_string(filename),
                    "Language", hb_value_string(lang),
                    "Codeset",  hb_value_string(code));
            if (subtitle_dict == NULL)
            {
                g_warning("json pack srt failure: %s", error.text);
                return;
            }
            hb_value_array_append(json_subtitle_list, subtitle_dict);
        }

        track = ghb_dict_get_int(ssettings, "SubtitleTrack");
        if (track == -1)
        {
            hb_dict_t *search = hb_dict_get(subtitles_dict, "Search");
            if (burned && !one_burned)
            {
                // Only allow one subtitle to be burned into the video
                one_burned = TRUE;
            }
            else
            {
                burned = FALSE;
            }
            hb_dict_set(search, "Enable", hb_value_bool(TRUE));
            hb_dict_set(search, "Forced", hb_value_bool(force));
            hb_dict_set(search, "Default", hb_value_bool(def));
            hb_dict_set(search, "Burn", hb_value_bool(burned));
        }
        else if (track >= 0)
        {
            if (burned && !one_burned && hb_subtitle_can_burn(source))
            {
                // Only allow one subtitle to be burned into the video
                one_burned = TRUE;
            }
            else
            {
                burned = FALSE;
            }

            subtitle_dict = json_pack_ex(&error, 0,
            "{s:o, s:o, s:o, s:o}",
                "Track",    hb_value_int(track),
                "Default",  hb_value_bool(def),
                "Force",    hb_value_bool(force),
                "Burn",     hb_value_bool(burned));
            if (subtitle_dict == NULL)
            {
                g_warning("json pack subtitle failure: %s", error.text);
                return;
            }
            hb_value_array_append(json_subtitle_list, subtitle_dict);
        }
    }

    char *json_job = hb_value_get_json(dict);
    hb_value_free(&dict);

    hb_add_json(h, json_job);
    free(json_job);
}

void
ghb_add_job(GhbValue *js, gint unique_id)
{
    add_job(h_queue, js, unique_id);
}

void
ghb_add_live_job(GhbValue *js, gint unique_id)
{
    add_job(h_live, js, unique_id);
}

void
ghb_remove_job(gint unique_id)
{
    hb_job_t * job;
    gint ii;

    // Multiples passes all get the same id
    // remove them all.
    // Go backwards through list, so reordering doesn't screw me.
    ii = hb_count(h_queue) - 1;
    while ((job = hb_job(h_queue, ii--)) != NULL)
    {
        if ((job->sequence_id & 0xFFFFFF) == unique_id)
            hb_rem(h_queue, job);
    }
}

void
ghb_start_queue()
{
    hb_start( h_queue );
}

void
ghb_stop_queue()
{
    hb_stop( h_queue );
}

void
ghb_start_live_encode()
{
    hb_start( h_live );
}

void
ghb_stop_live_encode()
{
    hb_stop( h_live );
}

void
ghb_pause_queue()
{
    hb_state_t s;
    hb_get_state2( h_queue, &s );

    if( s.state == HB_STATE_PAUSED )
    {
        hb_status.queue.state &= ~GHB_STATE_PAUSED;
        hb_resume( h_queue );
    }
    else
    {
        hb_status.queue.state |= GHB_STATE_PAUSED;
        hb_pause( h_queue );
    }
}

static void
vert_line(
    GdkPixbuf * pb,
    guint8 r,
    guint8 g,
    guint8 b,
    gint x,
    gint y,
    gint len,
    gint width)
{
    guint8 *pixels = gdk_pixbuf_get_pixels (pb);
    guint8 *dst;
    gint ii, jj;
    gint channels = gdk_pixbuf_get_n_channels (pb);
    gint stride = gdk_pixbuf_get_rowstride (pb);

    for (jj = 0; jj < width; jj++)
    {
        dst = pixels + y * stride + (x+jj) * channels;
        for (ii = 0; ii < len; ii++)
        {
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst += stride;
        }
    }
}

static void
horz_line(
    GdkPixbuf * pb,
    guint8 r,
    guint8 g,
    guint8 b,
    gint x,
    gint y,
    gint len,
    gint width)
{
    guint8 *pixels = gdk_pixbuf_get_pixels (pb);
    guint8 *dst;
    gint ii, jj;
    gint channels = gdk_pixbuf_get_n_channels (pb);
    gint stride = gdk_pixbuf_get_rowstride (pb);

    for (jj = 0; jj < width; jj++)
    {
        dst = pixels + (y+jj) * stride + x * channels;
        for (ii = 0; ii < len; ii++)
        {
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst += channels;
        }
    }
}

static void
hash_pixbuf(
    GdkPixbuf * pb,
    gint        x,
    gint        y,
    gint        w,
    gint        h,
    gint        step,
    gint        orientation)
{
    gint ii, jj;
    gint line_width = 8;
    struct
    {
        guint8 r;
        guint8 g;
        guint8 b;
    } c[4] =
    {{0x80, 0x80, 0x80},{0xC0, 0x80, 0x70},{0x80, 0xA0, 0x80},{0x70, 0x80, 0xA0}};

    if (!orientation)
    {
        // vertical lines
        for (ii = x, jj = 0; ii+line_width < x+w; ii += step, jj++)
        {
            vert_line(pb, c[jj&3].r, c[jj&3].g, c[jj&3].b, ii, y, h, line_width);
        }
    }
    else
    {
        // horizontal lines
        for (ii = y, jj = 0; ii+line_width < y+h; ii += step, jj++)
        {
            horz_line(pb, c[jj&3].r, c[jj&3].g, c[jj&3].b, x, ii, w, line_width);
        }
    }
}

GdkPixbuf*
ghb_get_preview_image(
    const hb_title_t *title,
    gint index,
    signal_user_data_t *ud,
    gint *out_width,
    gint *out_height)
{
    hb_geometry_t srcGeo, resultGeo;
    hb_geometry_settings_t uiGeo;

    if( title == NULL ) return NULL;

    gboolean deinterlace;
    if (ghb_dict_get_bool(ud->settings, "PictureDecombDeinterlace"))
    {
        deinterlace = ghb_settings_combo_int(ud->settings, "PictureDecomb")
                      == 0 ? 0 : 1;
    }
    else
    {
        deinterlace = ghb_settings_combo_int(ud->settings, "PictureDeinterlace")
                      == 0 ? 0 : 1;
    }
    // Get the geometry settings for the preview.  This will disable
    // cropping if the setting to show the cropped region is enabled.
    get_preview_geometry(ud, title, &srcGeo, &uiGeo);

    // hb_get_preview doesn't compensate for anamorphic, so lets
    // calculate scale factors
    hb_set_anamorphic_size2(&srcGeo, &uiGeo, &resultGeo);

    // Rescale preview dimensions to adjust for screen PAR and settings PAR
    ghb_par_scale(ud, &uiGeo.geometry.width, &uiGeo.geometry.height,
                      resultGeo.par.num, resultGeo.par.den);
    uiGeo.geometry.par.num = 1;
    uiGeo.geometry.par.den = 1;

    GdkPixbuf *preview;
    hb_image_t *image;
    image = hb_get_preview2(h_scan, title->index, index, &uiGeo, deinterlace);

    if (image == NULL)
    {
        preview = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                 title->geometry.width, title->geometry.height);
        return preview;
    }

    // Create an GdkPixbuf and copy the libhb image into it, converting it from
    // libhb's format something suitable.
    // The image data returned by hb_get_preview is 4 bytes per pixel,
    // BGRA format. Alpha is ignored.
    preview = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                             image->width, image->height);
    guint8 *pixels = gdk_pixbuf_get_pixels(preview);

    guint8 *src_line = image->data;
    guint8 *dst = pixels;

    gint ii, jj;
    gint channels = gdk_pixbuf_get_n_channels(preview);
    gint stride = gdk_pixbuf_get_rowstride(preview);
    guint8 *tmp;

    for (ii = 0; ii < image->height; ii++)
    {
        guint32 *src = (guint32*)src_line;
        tmp = dst;
        for (jj = 0; jj < image->width; jj++)
        {
            tmp[0] = src[0] >> 16;
            tmp[1] = src[0] >> 8;
            tmp[2] = src[0] >> 0;
            tmp += channels;
            src++;
        }
        src_line += image->plane[0].stride;
        dst += stride;
    }
    gint w = ghb_dict_get_int(ud->settings, "scale_width");
    gint h = ghb_dict_get_int(ud->settings, "scale_height");
    ghb_par_scale(ud, &w, &h, resultGeo.par.num, resultGeo.par.den);

    gint c0, c1, c2, c3;
    c0 = ghb_dict_get_int(ud->settings, "PictureTopCrop");
    c1 = ghb_dict_get_int(ud->settings, "PictureBottomCrop");
    c2 = ghb_dict_get_int(ud->settings, "PictureLeftCrop");
    c3 = ghb_dict_get_int(ud->settings, "PictureRightCrop");

    gdouble xscale = (gdouble)w / (gdouble)(title->geometry.width - c2 - c3);
    gdouble yscale = (gdouble)h / (gdouble)(title->geometry.height - c0 - c1);

    *out_width = w;
    *out_height = h;

    int previewWidth = image->width;
    int previewHeight = image->height;

    // If the preview is too large to fit the screen, reduce it's size.
    if (ghb_dict_get_bool(ud->prefs, "reduce_hd_preview"))
    {
        GdkScreen *ss;
        gint s_w, s_h;
        gint factor = 80;

        if (ghb_dict_get_bool(ud->prefs, "preview_fullscreen"))
        {
            factor = 100;
        }
        ss = gdk_screen_get_default();
        s_w = gdk_screen_get_width(ss);
        s_h = gdk_screen_get_height(ss);

        if (previewWidth > s_w * factor / 100 ||
            previewHeight > s_h * factor / 100)
        {
            GdkPixbuf *scaled_preview;
            int orig_w = previewWidth;
            int orig_h = previewHeight;

            if (previewWidth > s_w * factor / 100)
            {
                previewWidth = s_w * factor / 100;
                previewHeight = previewHeight * previewWidth / orig_w;
            }
            if (previewHeight > s_h * factor / 100)
            {
                previewHeight = s_h * factor / 100;
                previewWidth = orig_w * previewHeight / orig_h;
            }
            xscale *= (gdouble)previewWidth / orig_w;
            yscale *= (gdouble)previewHeight / orig_h;
            w *= (gdouble)previewWidth / orig_w;
            h *= (gdouble)previewHeight / orig_h;
            scaled_preview = gdk_pixbuf_scale_simple(preview,
                            previewWidth, previewHeight, GDK_INTERP_HYPER);
            g_object_unref(preview);
            preview = scaled_preview;
        }
    }
    if (ghb_dict_get_bool(ud->prefs, "preview_show_crop"))
    {
        c0 *= yscale;
        c1 *= yscale;
        c2 *= xscale;
        c3 *= xscale;
        // Top
        hash_pixbuf(preview, c2, 0, w, c0, 32, 0);
        // Bottom
        hash_pixbuf(preview, c2, previewHeight-c1, w, c1, 32, 0);
        // Left
        hash_pixbuf(preview, 0, c0, c2, h, 32, 1);
        // Right
        hash_pixbuf(preview, previewWidth-c3, c0, c3, h, 32, 1);
    }
    hb_image_close(&image);
    return preview;
}

static void
sanitize_volname(gchar *name)
{
    gchar *a, *b;

    a = b = name;
    while (*b)
    {
        switch(*b)
        {
        case '<':
            b++;
            break;
        case '>':
            b++;
            break;
        default:
            *a = *b & 0x7f;
            a++; b++;
            break;
        }
    }
    *a = 0;
}

gchar*
ghb_dvd_volname(const gchar *device)
{
    gchar *name;
    name = hb_dvd_name((gchar*)device);
    if (name != NULL && name[0] != 0)
    {
        name = g_strdup(name);
        sanitize_volname(name);
        return name;
    }
    return NULL;
}
