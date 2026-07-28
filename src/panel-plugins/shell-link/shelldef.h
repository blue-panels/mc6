/*
   libshfs - the helper scripts compiled into the library.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 * \brief Header: built-in helper scripts
 */

#ifndef MC__SHELLDEF_H
#define MC__SHELLDEF_H

/*** typedefs(not structures) and defined constants **********************************************/

/* default 'ls' script */
#define VFS_SHELL_LS_DEF_CONTENT                                                                   \
    ""                                                                                             \
    "# shfs-helper: ls 1\n"                                                                        \
    "export LC_TIME=C\n"                                                                           \
    "ls -Qlan \"/${SHELL_FILENAME}\" 2>/dev/null | grep '^[^cbt]' | (\n"                           \
    "while read p l u g s m d y n; do\n"                                                           \
    "    echo \"P$p $u.$g\"\n"                                                                     \
    "    echo \"S$s\"\n"                                                                           \
    "    echo \"d$m $d $y\"\n"                                                                     \
    "    echo \":$n\"\n"                                                                           \
    "    echo\n"                                                                                   \
    "done\n"                                                                                       \
    ")\n"                                                                                          \
    "ls -Qlan \"/${SHELL_FILENAME}\" 2>/dev/null | grep '^[cb]' | (\n"                             \
    "while read p l u g a i m d y n; do\n"                                                         \
    "    echo \"P$p $u.$g\"\n"                                                                     \
    "    echo \"E$a$i\"\n"                                                                         \
    "    echo \"d$m $d $y\"\n"                                                                     \
    "    echo \":$n\"\n"                                                                           \
    "    echo\n"                                                                                   \
    "done\n"                                                                                       \
    ")\n"                                                                                          \
    "echo \"### 200\"\n"

/* default file exists script */
#define VFS_SHELL_EXISTS_DEF_CONTENT                                                               \
    ""                                                                                             \
    "# shfs-helper: fexists 1\n"                                                                   \
    "ls -l \"/${SHELL_FILENAME}\" >/dev/null 2>/dev/null\n"                                        \
    "echo '### '$?\n"

/* default 'mkdir' script */
#define VFS_SHELL_MKDIR_DEF_CONTENT                                                                \
    ""                                                                                             \
    "# shfs-helper: mkdir 1\n"                                                                     \
    "if mkdir \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                                          \
    "    echo \"### 000\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

/* default 'unlink' script */
#define VFS_SHELL_UNLINK_DEF_CONTENT                                                               \
    ""                                                                                             \
    "# shfs-helper: unlink 1\n"                                                                    \
    "if rm -f \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                                          \
    "    echo \"### 000\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

/* default 'chown' script */
#define VFS_SHELL_CHOWN_DEF_CONTENT                                                                \
    ""                                                                                             \
    "# shfs-helper: chown 1\n"                                                                     \
    "if chown ${SHELL_FILEOWNER}:${SHELL_FILEGROUP} \"/${SHELL_FILENAME}\"; then\n"                \
    "    echo \"### 000\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

/* default 'chmod' script */
#define VFS_SHELL_CHMOD_DEF_CONTENT                                                                \
    ""                                                                                             \
    "# shfs-helper: chmod 1\n"                                                                     \
    "if chmod ${SHELL_FILEMODE} \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                        \
    "    echo \"### 000\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

/* default 'utime' script */
#define VFS_SHELL_UTIME_DEF_CONTENT                                                                \
    ""                                                                                             \
    "# shfs-helper: utime 1\n"                                                                     \
    "#UTIME \"$SHELL_TOUCHATIME_W_NSEC\" \"$SHELL_TOUCHMTIME_W_NSEC\" $SHELL_FILENAME\n"           \
    "if TZ=UTC touch -h -m -d \"$SHELL_TOUCHMTIME_W_NSEC\" \"/${SHELL_FILENAME}\" 2>/dev/null && " \
    "\\\n"                                                                                         \
    "   TZ=UTC touch -h -a -d \"$SHELL_TOUCHATIME_W_NSEC\" \"/${SHELL_FILENAME}\" 2>/dev/null; "   \
    "then\n"                                                                                       \
    "  echo \"### 000\"\n"                                                                         \
    "elif TZ=UTC touch -h -m -t $SHELL_TOUCHMTIME \"/${SHELL_FILENAME}\" 2>/dev/null && \\\n"      \
    "     TZ=UTC touch -h -a -t $SHELL_TOUCHATIME \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"      \
    "  echo \"### 000\"\n"                                                                         \
    "elif [ -n \"$SHELL_HAVE_PERL\" ] && \\\n"                                                     \
    "   perl -e 'utime '$SHELL_FILEATIME','$SHELL_FILEMTIME',@ARGV;' \"/${SHELL_FILENAME}\" "      \
    "2>/dev/null; then\n"                                                                          \
    "  echo \"### 000\"\n"                                                                         \
    "else\n"                                                                                       \
    "  echo \"### 500\"\n"                                                                         \
    "fi\n"

/* default 'rmdir' script */
#define VFS_SHELL_RMDIR_DEF_CONTENT                                                                \
    ""                                                                                             \
    "# shfs-helper: rmdir 1\n"                                                                     \
    "if rmdir \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                                          \
    "   echo \"### 000\"\n"                                                                        \
    "else\n"                                                                                       \
    "   echo \"### 500\"\n"                                                                        \
    "fi\n"

/* default 'ln -s' symlink script */
#define VFS_SHELL_LN_DEF_CONTENT                                                                   \
    ""                                                                                             \
    "# shfs-helper: ln 1\n"                                                                        \
    "if ln -s \"/${SHELL_FILEFROM}\" \"/${SHELL_FILETO}\" 2>/dev/null; then\n"                     \
    "   echo \"### 000\"\n"                                                                        \
    "else\n"                                                                                       \
    "   echo \"### 500\"\n"                                                                        \
    "fi\n"

/* default 'mv' script */
#define VFS_SHELL_MV_DEF_CONTENT                                                                   \
    ""                                                                                             \
    "# shfs-helper: mv 1\n"                                                                        \
    "if mv \"/${SHELL_FILEFROM}\" \"/${SHELL_FILETO}\" 2>/dev/null; then\n"                        \
    "   echo \"### 000\"\n"                                                                        \
    "else\n"                                                                                       \
    "   echo \"### 500\"\n"                                                                        \
    "fi\n"

/* default 'ln' hardlink script */
#define VFS_SHELL_HARDLINK_DEF_CONTENT                                                             \
    ""                                                                                             \
    "# shfs-helper: hardlink 1\n"                                                                  \
    "if ln \"/${SHELL_FILEFROM}\" \"/${SHELL_FILETO}\" 2>/dev/null; then\n"                        \
    "   echo \"### 000\"\n"                                                                        \
    "else\n"                                                                                       \
    "   echo \"### 500\"\n"                                                                        \
    "fi\n"

/* default 'retr'  script */
#define VFS_SHELL_GET_DEF_CONTENT                                                                  \
    ""                                                                                             \
    "# shfs-helper: get 1\n"                                                                       \
    "export LC_TIME=C\n"                                                                           \
    "if dd if=\"/${SHELL_FILENAME}\" of=/dev/null bs=1 count=1 2>/dev/null ; then\n"               \
    "    ls -ln \"/${SHELL_FILENAME}\" 2>/dev/null | (\n"                                          \
    "       read p l u g s r\n"                                                                    \
    "       echo $s\n"                                                                             \
    "    )\n"                                                                                      \
    "    echo \"### 100\"\n"                                                                       \
    "    cat \"/${SHELL_FILENAME}\"\n"                                                             \
    "    echo \"### 200\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

/* default 'stor'  script */
#define VFS_SHELL_SEND_DEF_CONTENT                                                                 \
    ""                                                                                             \
    "# shfs-helper: send 1\n"                                                                      \
    "FILENAME=\"/${SHELL_FILENAME}\"\n"                                                            \
    "FILESIZE=${SHELL_FILESIZE}\n"                                                                 \
    "echo \"### 001\"\n"                                                                           \
    "{\n"                                                                                          \
    "    while [ $FILESIZE -gt 0 ]; do\n"                                                          \
    "        cnt=`expr \\( $FILESIZE + 255 \\) / 256`\n"                                           \
    "        n=`dd bs=256 count=$cnt | tee -a \"${FILENAME}\" | wc -c`\n"                          \
    "        FILESIZE=`expr $FILESIZE - $n`\n"                                                     \
    "    done\n"                                                                                   \
    "}; echo \"### 200\"\n"

/* default 'appe'  script */
#define VFS_SHELL_APPEND_DEF_CONTENT                                                               \
    ""                                                                                             \
    "# shfs-helper: append 1\n"                                                                    \
    "FILENAME=\"/${SHELL_FILENAME}\"\n"                                                            \
    "FILESIZE=${SHELL_FILESIZE}\n"                                                                 \
    "echo \"### 001\"\n"                                                                           \
    "res=`exec 3>&1\n"                                                                             \
    "(\n"                                                                                          \
    "    head -c $FILESIZE -q - || echo DD >&3\n"                                                  \
    ") 2>/dev/null | (\n"                                                                          \
    "    cat > \"${FILENAME}\"\n"                                                                  \
    "    cat > /dev/null\n"                                                                        \
    ")`; [ \"$res\" = DD ] && {\n"                                                                 \
    "    > \"${FILENAME}\"\n"                                                                      \
    "    while [ $FILESIZE -gt 0 ]\n"                                                              \
    "    do\n"                                                                                     \
    "       cnt=`expr \\( $FILESIZE + 255 \\) / 256`\n"                                            \
    "       n=`dd bs=256 count=$cnt | tee -a \"${FILENAME}\" | wc -c`\n"                           \
    "       FILESIZE=`expr $FILESIZE - $n`\n"                                                      \
    "    done\n"                                                                                   \
    "}; echo \"### 200\"\n"

/* default 'info'  script */
#define VFS_SHELL_INFO_DEF_CONTENT                                                                 \
    ""                                                                                             \
    "# shfs-helper: info 2\n"                                                                      \
    "LC_TIME=C\n"                                                                                  \
    "export LC_TIME\n"                                                                             \
    "#SHELL_HAVE_HEAD         1\n"                                                                 \
    "#SHELL_HAVE_SED          2\n"                                                                 \
    "#SHELL_HAVE_AWK          4\n"                                                                 \
    "#SHELL_HAVE_PERL         8\n"                                                                 \
    "#SHELL_HAVE_LSQ         16\n"                                                                 \
    "#SHELL_HAVE_DATE_MDYT   32\n"                                                                 \
    "#SHELL_HAVE_TAIL        64\n"                                                                 \
    "#SHELL_HAVE_SHA256     128\n"                                                                 \
    "#SHELL_HAVE_MD5        256\n"                                                                 \
    "#SHELL_HAVE_NOTRUNC    512\n"                                                                 \
    "res=0\n"                                                                                      \
    "if `echo yes| head -c 1 > /dev/null 2>&1` ; then\n"                                           \
    "    res=`expr $res + 1`\n"                                                                    \
    "fi\n"                                                                                         \
    "if `echo 1 | sed 's/1/2/' >/dev/null 2>&1` ; then\n"                                          \
    "    res=`expr $res + 2`\n"                                                                    \
    "fi\n"                                                                                         \
    "if `echo 1| awk '{print}' > /dev/null 2>&1` ; then\n"                                         \
    "    res=`expr $res + 4`\n"                                                                    \
    "fi\n"                                                                                         \
    "if `perl -v > /dev/null 2>&1` ; then\n"                                                       \
    "    res=`expr $res + 8`\n"                                                                    \
    "fi\n"                                                                                         \
    "if `ls -Q / >/dev/null 2>&1` ; then\n"                                                        \
    "    res=`expr $res + 16`\n"                                                                   \
    "fi\n"                                                                                         \
    "dat=`ls -lan / 2>/dev/null | head -n 3 | (\n"                                                 \
    "    while read p l u g s rec; do\n"                                                           \
    "      if [ -n \"$g\" ]; then\n"                                                               \
    "        if [ -n \"$l\" ]; then\n"                                                             \
    "          echo \"$rec\"\n"                                                                    \
    "        fi\n"                                                                                 \
    "      fi\n"                                                                                   \
    "    done\n"                                                                                   \
    ")`\n"                                                                                         \
    "dat=`echo $dat | cut -c1 2>/dev/null`\n"                                                      \
    "r=`echo \"0123456789\"| grep \"$dat\"`\n"                                                     \
    "if [ -z \"$r\" ]; then\n"                                                                     \
    "    res=`expr $res + 32`\n"                                                                   \
    "fi\n"                                                                                         \
    "if `echo yes| tail -c +1 - > /dev/null 2>&1` ; then\n"                                        \
    "    res=`expr $res + 64`\n"                                                                   \
    "fi\n"                                                                                         \
    "if `echo yes| sha256sum > /dev/null 2>&1` ; then\n"                                           \
    "    res=`expr $res + 128`\n"                                                                  \
    "fi\n"                                                                                         \
    "if `echo yes| md5sum > /dev/null 2>&1` ; then\n"                                              \
    "    res=`expr $res + 256`\n"                                                                  \
    "fi\n"                                                                                         \
    "if `dd if=/dev/null of=/dev/null conv=notrunc > /dev/null 2>&1` ; then\n"                     \
    "    res=`expr $res + 512`\n"                                                                  \
    "fi\n"                                                                                         \
    "echo $res\n"                                                                                  \
    "echo \"### 200\"\n"

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

/* default 'putat' script */
#define VFS_SHELL_PUTAT_DEF_CONTENT                                                                \
    ""                                                                                             \
    "# shfs-helper: putat 1\n"                                                                     \
    "LC_ALL=C\n"                                                                                   \
    "export LC_ALL\n"                                                                              \
    "FILENAME=\"/${SHELL_FILENAME}\"\n"                                                            \
    "OFFSET=${SHELL_START_OFFSET}\n"                                                               \
    "\n"                                                                                           \
    "# Write SHELL_FILESIZE bytes into FILENAME starting at OFFSET, leaving whatever\n"            \
    "# lies beyond untouched. This is what makes resuming possible without ever\n"                 \
    "# shortening the destination: the only helper that truncates is send, and it\n"               \
    "# does so because a human asked to start over.\n"                                             \
    "echo \"### 001\"\n"                                                                           \
    "{\n"                                                                                          \
    "    if [ -n \"${SHELL_HAVE_PERL}\" ]; then\n"                                                 \
    "        perl -e '\n"                                                                          \
    "my ($f, $off, $len) = @ARGV;\n"                                                               \
    "open(F, \"+<\", $f) or open(F, \">\", $f) or exit 1;\n"                                       \
    "binmode(F);\n"                                                                                \
    "binmode(STDIN);\n"                                                                            \
    "seek(F, $off, 0);\n"                                                                          \
    "my $left = $len;\n"                                                                           \
    "my $buf;\n"                                                                                   \
    "while ($left > 0) {\n"                                                                        \
    "    my $want = $left > 65536 ? 65536 : $left;\n"                                              \
    "    my $got = read(STDIN, $buf, $want);\n"                                                    \
    "    last if !defined($got) || $got == 0;\n"                                                   \
    "    print F $buf;\n"                                                                          \
    "    $left -= $got;\n"                                                                         \
    "}\n"                                                                                          \
    "close(F);\n"                                                                                  \
    "' \"${FILENAME}\" ${OFFSET} ${SHELL_FILESIZE}\n"                                              \
    "    else\n"                                                                                   \
    "        # conv=notrunc is the point: without it dd would cut the file at the\n"               \
    "        # end of what we write.\n"                                                            \
    "        dd of=\"${FILENAME}\" bs=1 seek=${OFFSET} count=${SHELL_FILESIZE} conv=notrunc "      \
    "2>/dev/null\n"                                                                                \
    "    fi\n"                                                                                     \
    "}; echo \"### 200\"\n"

/* default 'cksumrange' script */
#define VFS_SHELL_CKSUMRANGE_DEF_CONTENT                                                           \
    ""                                                                                             \
    "# shfs-helper: cksumrange 1\n"                                                                \
    "LC_ALL=C\n"                                                                                   \
    "export LC_ALL\n"                                                                              \
    "FILENAME=\"/${SHELL_FILENAME}\"\n"                                                            \
    "OFFSET=${SHELL_START_OFFSET}\n"                                                               \
    "LENGTH=${SHELL_LENGTH}\n"                                                                     \
    "\n"                                                                                           \
    "# Emit LENGTH bytes of FILENAME starting at OFFSET.\n"                                        \
    "# Same three-way choice the get helper makes, for the same reason: perl where\n"              \
    "# it exists, then the coreutils pair, then dd as the portable last resort.\n"                 \
    "shfs_slice ()\n"                                                                              \
    "{\n"                                                                                          \
    "    if [ -n \"${SHELL_HAVE_PERL}\" ]; then\n"                                                 \
    "        perl -e '\n"                                                                          \
    "my ($f, $off, $len) = @ARGV;\n"                                                               \
    "open(F, $f) or exit 1;\n"                                                                     \
    "binmode(F);\n"                                                                                \
    "seek(F, $off, 0);\n"                                                                          \
    "my $left = $len;\n"                                                                           \
    "my $buf;\n"                                                                                   \
    "while ($left > 0) {\n"                                                                        \
    "    my $want = $left > 65536 ? 65536 : $left;\n"                                              \
    "    my $got = read(F, $buf, $want);\n"                                                        \
    "    last if !defined($got) || $got == 0;\n"                                                   \
    "    print $buf;\n"                                                                            \
    "    $left -= $got;\n"                                                                         \
    "}\n"                                                                                          \
    "close(F);\n"                                                                                  \
    "' \"$1\" \"$2\" \"$3\"\n"                                                                     \
    "    elif [ -n \"${SHELL_HAVE_TAIL}\" ] && [ -n \"${SHELL_HAVE_HEAD}\" ]; then\n"              \
    "        start=`expr $2 + 1`\n"                                                                \
    "        tail -c +${start} \"$1\" | head -c $3\n"                                              \
    "    else\n"                                                                                   \
    "        dd if=\"$1\" bs=1 skip=$2 count=$3 2>/dev/null\n"                                     \
    "    fi\n"                                                                                     \
    "}\n"                                                                                          \
    "\n"                                                                                           \
    "echo \"### 001\"\n"                                                                           \
    "if [ -r \"${FILENAME}\" ]; then\n"                                                            \
    "    case \"${SHELL_DIGEST}\" in\n"                                                            \
    "    sha256)\n"                                                                                \
    "        shfs_slice \"${FILENAME}\" ${OFFSET} ${LENGTH} | sha256sum | cut -d' ' -f1\n"         \
    "        ;;\n"                                                                                 \
    "    md5)\n"                                                                                   \
    "        shfs_slice \"${FILENAME}\" ${OFFSET} ${LENGTH} | md5sum | cut -d' ' -f1\n"            \
    "        ;;\n"                                                                                 \
    "    *)\n"                                                                                     \
    "        # cksum is the POSIX floor: weaker, but present everywhere. Its two\n"                \
    "        # fields are the checksum and the byte count; take the checksum.\n"                   \
    "        shfs_slice \"${FILENAME}\" ${OFFSET} ${LENGTH} | cksum | cut -d' ' -f1\n"             \
    "        ;;\n"                                                                                 \
    "    esac\n"                                                                                   \
    "    echo \"### 200\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

/* default 'blockdigests' script */
#define VFS_SHELL_BLOCKDIGESTS_DEF_CONTENT                                                         \
    ""                                                                                             \
    "# shfs-helper: blockdigests 1\n"                                                              \
    "LC_ALL=C\n"                                                                                   \
    "export LC_ALL\n"                                                                              \
    "FILENAME=\"/${SHELL_FILENAME}\"\n"                                                            \
    "OFFSET=${SHELL_START_OFFSET}\n"                                                               \
    "LENGTH=${SHELL_LENGTH}\n"                                                                     \
    "BLOCK=${SHELL_BLOCKSIZE}\n"                                                                   \
    "\n"                                                                                           \
    "# One digest per block, in order, so that a partially written file can be\n"                  \
    "# compared against its source in a single pass instead of by repeated probing.\n"             \
    "\n"                                                                                           \
    "# Portable path: one process pair per block, so the caller is expected to ask\n"              \
    "# for few, large blocks when this is what runs.\n"                                            \
    "shfs_blocks_shell ()\n"                                                                       \
    "{\n"                                                                                          \
    "    pos=${OFFSET}\n"                                                                          \
    "    left=${LENGTH}\n"                                                                         \
    "    while [ ${left} -gt 0 ]; do\n"                                                            \
    "        want=${BLOCK}\n"                                                                      \
    "        if [ ${left} -lt ${BLOCK} ]; then\n"                                                  \
    "            want=${left}\n"                                                                   \
    "        fi\n"                                                                                 \
    "        case \"${SHELL_DIGEST}\" in\n"                                                        \
    "        sha256)\n"                                                                            \
    "            dd if=\"${FILENAME}\" bs=1 skip=${pos} count=${want} 2>/dev/null | sha256sum | "  \
    "cut -d' ' -f1\n"                                                                              \
    "            ;;\n"                                                                             \
    "        md5)\n"                                                                               \
    "            dd if=\"${FILENAME}\" bs=1 skip=${pos} count=${want} 2>/dev/null | md5sum | cut " \
    "-d' ' -f1\n"                                                                                  \
    "            ;;\n"                                                                             \
    "        *)\n"                                                                                 \
    "            dd if=\"${FILENAME}\" bs=1 skip=${pos} count=${want} 2>/dev/null | cksum | cut "  \
    "-d' ' -f1\n"                                                                                  \
    "            ;;\n"                                                                             \
    "        esac\n"                                                                               \
    "        pos=`expr ${pos} + ${want}`\n"                                                        \
    "        left=`expr ${left} - ${want}`\n"                                                      \
    "    done\n"                                                                                   \
    "}\n"                                                                                          \
    "\n"                                                                                           \
    "echo \"### 001\"\n"                                                                           \
    "if [ -r \"${FILENAME}\" ]; then\n"                                                            \
    "    done_by_perl=no\n"                                                                        \
    "    if [ -n \"${SHELL_HAVE_PERL}\" ]; then\n"                                                 \
    "        # Exits non-zero when it cannot produce the algorithm that was asked\n"               \
    "        # for, so the shell path takes over instead of the caller silently\n"                 \
    "        # receiving a digest of the wrong kind.\n"                                            \
    "        if perl -e '\n"                                                                       \
    "my ($f, $off, $len, $blk, $algo) = @ARGV;\n"                                                  \
    "my $ctor;\n"                                                                                  \
    "if ($algo eq \"sha256\") {\n"                                                                 \
    "    eval { require Digest::SHA; 1 } or exit 2;\n"                                             \
    "    $ctor = sub { Digest::SHA->new(256) };\n"                                                 \
    "} elsif ($algo eq \"md5\") {\n"                                                               \
    "    eval { require Digest::MD5; 1 } or exit 2;\n"                                             \
    "    $ctor = sub { Digest::MD5->new };\n"                                                      \
    "} else {\n"                                                                                   \
    "    exit 2;\n"                                                                                \
    "}\n"                                                                                          \
    "open(F, $f) or exit 1;\n"                                                                     \
    "binmode(F);\n"                                                                                \
    "seek(F, $off, 0);\n"                                                                          \
    "my $left = $len;\n"                                                                           \
    "while ($left > 0) {\n"                                                                        \
    "    my $want = $left > $blk ? $blk : $left;\n"                                                \
    "    my $buf;\n"                                                                               \
    "    my $got = read(F, $buf, $want);\n"                                                        \
    "    last if !defined($got) || $got == 0;\n"                                                   \
    "    my $ctx = $ctor->();\n"                                                                   \
    "    $ctx->add($buf);\n"                                                                       \
    "    print $ctx->hexdigest, \"\\n\";\n"                                                        \
    "    $left -= $got;\n"                                                                         \
    "}\n"                                                                                          \
    "close(F);\n"                                                                                  \
    "exit 0\n"                                                                                     \
    "' \"${FILENAME}\" ${OFFSET} ${LENGTH} ${BLOCK} \"${SHELL_DIGEST}\"; then\n"                   \
    "            done_by_perl=yes\n"                                                               \
    "        fi\n"                                                                                 \
    "    fi\n"                                                                                     \
    "\n"                                                                                           \
    "    if [ \"${done_by_perl}\" = no ]; then\n"                                                  \
    "        shfs_blocks_shell\n"                                                                  \
    "    fi\n"                                                                                     \
    "\n"                                                                                           \
    "    echo \"### 200\"\n"                                                                       \
    "else\n"                                                                                       \
    "    echo \"### 500\"\n"                                                                       \
    "fi\n"

#endif
