/* Print "group key" for every row of the skin editor label table; used by check_table.sh. */

#include <config.h>

#include <stdio.h>

#include "lib/global.h"

#include "src/panel-plugins/skineditor/skinedit_table.h"

int
main (void)
{
    size_t si, ri;

    for (si = 0; si < skinedit_table_count; si++)
        for (ri = 0; ri < skinedit_table[si].nrows; ri++)
            printf ("%s %s\n", skinedit_table[si].rows[ri].group, skinedit_table[si].rows[ri].key);
    return 0;
}
