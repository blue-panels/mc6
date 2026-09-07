/** \file skinedit_model.h
 *  \brief Header: skin editor model, a skin opened for editing
 */

#ifndef MC__SKINEDIT_MODEL_H
#define MC__SKINEDIT_MODEL_H

#include "lib/global.h"
#include "lib/mcconfig.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SKINEDIT_PARTS 3

/*** enums ***************************************************************************************/

typedef enum
{
    SKINEDIT_PART_FG = 0,
    SKINEDIT_PART_BG = 1,
    SKINEDIT_PART_ATTRS = 2
} skinedit_part_t;

/* where the effective value of a part comes from */
typedef enum
{
    SKINEDIT_SRC_SET,           /* written in this key */
    SKINEDIT_SRC_GROUP_DEFAULT, /* [group] _default_ */
    SKINEDIT_SRC_CORE_DEFAULT,  /* [core] _default_ */
    SKINEDIT_SRC_FALLBACK,      /* what the engine fills in for this key, mc_skin_fallbacks() */
    SKINEDIT_SRC_TERMINAL       /* nothing anywhere: the terminal's own color */
} skinedit_src_t;

typedef enum
{
    SKINEDIT_ENTRY_COLOR, /* fg;bg;attrs */
    SKINEDIT_ENTRY_CHAR,  /* one character */
    SKINEDIT_ENTRY_STRING /* spinner_sequence */
} skinedit_kind_t;

/* what a color value needs from the terminal */
typedef enum
{
    SKINEDIT_COLOR_BASIC,     /* 16 names, "default", "base" */
    SKINEDIT_COLOR_256,       /* colorN, grayN, rgbRGB */
    SKINEDIT_COLOR_TRUECOLOR, /* #rgb, #rrggbb */
    SKINEDIT_COLOR_UNKNOWN
} skinedit_color_class_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    skinedit_kind_t kind;
    char *group;
    char *key;
    const char *label;               /* translated; the key itself for an unknown entry */
    const char *description;         /* one sentence, may be NULL */
    const char *builtin;             /* CHAR/STRING: what mc uses when the key is absent */
    char *raw[SKINEDIT_PARTS];       /* the ini value, NULL = not set; CHAR/STRING use [0] */
    char *shown;                     /* CHAR/STRING: raw or the built-in, in the display charset */
    char *file_raw[SKINEDIT_PARTS];  /* the same as of the last open or save */
    char *effective[SKINEDIT_PARTS]; /* after alias and fallback, NULL = terminal */
    skinedit_src_t src[SKINEDIT_PARTS];
    char *alias[SKINEDIT_PARTS]; /* alias name the value went through, or NULL */
    gboolean known;              /* from the table, not only from the file */
} skinedit_entry_t;

typedef struct
{
    char *group; /* group of the section's entries, the first one for a mixed section */
    const char *label;
    GPtrArray *entries; /* skinedit_entry_t, owned */
} skinedit_section_t;

typedef struct
{
    char *name;                         /* skin name, "modarin256" */
    char *path;                         /* file it was read from */
    char *description;                  /* [skin] description */
    gboolean system;                    /* not in the user's skins directory */
    skinedit_color_class_t colors;      /* the skin is for 16, 256 or true colors */
    skinedit_color_class_t file_colors; /* the same as of the last open or save */
    mc_config_t *config;
    GPtrArray *sections; /* skinedit_section_t, owned */
} skinedit_model_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* open by skin name, searched where mc searches skins */
skinedit_model_t *skinedit_model_open (const char *name, GError **error);
/* open a file; @name is what the skin is called, the file's base name when NULL */
skinedit_model_t *skinedit_model_open_file (const char *path, const char *name, GError **error);
void skinedit_model_free (skinedit_model_t *m);

skinedit_entry_t *skinedit_model_find (const skinedit_model_t *m, const char *group,
                                       const char *key);
skinedit_section_t *skinedit_model_find_section (const skinedit_model_t *m, const char *group);

/* COLOR entries: @value NULL makes the part inherited; [core] _default_ gets "default" instead */
void skinedit_model_set (skinedit_model_t *m, skinedit_entry_t *e, skinedit_part_t part,
                         const char *value);
/* CHAR and STRING entries: @value is in the display charset, NULL puts the built-in back */
void skinedit_model_set_text (skinedit_model_t *m, skinedit_entry_t *e, const char *value);
/* the same with @utf8 as it goes into the file: for undo and the like, no conversion */
void skinedit_model_set_text_raw (skinedit_model_t *m, skinedit_entry_t *e, const char *utf8);

/* skin files are UTF-8, the screen may not be: between the two, a new string each */
char *skinedit_text_from_skin (const char *utf8);
char *skinedit_text_to_skin (const char *text);
void skinedit_model_reset (skinedit_model_t *m, skinedit_entry_t *e);
void skinedit_model_reset_all (skinedit_model_t *m);

gboolean skinedit_entry_changed (const skinedit_entry_t *e);
gboolean skinedit_model_dirty (const skinedit_model_t *m);
gboolean skinedit_section_changed (const skinedit_section_t *s);

/* a config with the same content, for the skin engine to take over */
mc_config_t *skinedit_model_config_copy (const skinedit_model_t *m);

/* what the colors in use ask of the terminal */
void skinedit_model_needs (const skinedit_model_t *m, gboolean *needs_256, gboolean *needs_true);
/* the declared class, raised to what the colors in use need */
skinedit_color_class_t skinedit_model_class (const skinedit_model_t *m);
/* the entry with the highest color class above @cls, its part in @part; NULL when there is none */
skinedit_entry_t *skinedit_model_over_class (const skinedit_model_t *m, skinedit_color_class_t cls,
                                             skinedit_part_t *part);

/* a skin name is a file name without a directory part or the .ini suffix */
gboolean skinedit_model_name_ok (const char *name);
/* the file a skin of this name is saved to: the user's skins directory; NULL for a bad name */
char *skinedit_model_user_path (const char *name);
/* save under @name into the user's skins directory; @description (UTF-8) NULL keeps the current
   one; the model changes only when the file is written */
gboolean skinedit_model_save (skinedit_model_t *m, const char *name, const char *description,
                              GError **error);
gboolean skinedit_model_save_to (skinedit_model_t *m, const char *path, const char *name,
                                 const char *description, GError **error);

/* helpers, public for the tests */
char *skinedit_color_join (char *const parts[SKINEDIT_PARTS]);
skinedit_color_class_t skinedit_color_classify (const char *value);

/*** inline functions ****************************************************************************/

#endif
