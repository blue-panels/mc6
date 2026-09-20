m       make
        TMPFILE=`mktemp ${MC_TMPDIR:-/tmp}/up.XXXXXX` || exit 1
        make 2> $TMPFILE
        mcedit6 $TMPFILE
        rm $TMPFILE
