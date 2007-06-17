AC_DEFUN([SHELL_FM_SUMMARY],[
cat << EOT

-------------------------------------------------------------------
package    . . . . . . . . . . . . .  $PACKAGE_NAME
version    . . . . . . . . . . . . .  $PACKAGE_VERSION
with libao . . . . . . . . . . . . .  $shell_fm_have_libao
debug      . . . . . . . . . . . . .  $shell_fm_enable_debug

C compiler . . . . . . . . . . . . .  $CC
DEFS       . . . . . . . . . . . . .  $DEFS
CPPFLAGS   . . . . . . . . . . . . .  $CPPFLAGS
CFLAGS     . . . . . . . . . . . . .  $CFLAGS
LDFLAGS    . . . . . . . . . . . . .  $LDFLAGS
LIBS       . . . . . . . . . . . . .  $LIBS
-------------------------------------------------------------------

EOT
])
