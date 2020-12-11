/**
 * Checks to see whether we're running genuine GNU readline and not some other
 * library emulating it.
 *
 * Some readline emulators, e.g., editline, have a bug that makes color prompts
 * not work correctly.  So, unless we know we're using genuine GNU readline,
 * use this function to disable color prompts.
 *
 * @return Returns `true` only if we're running genuine GNU readline.
 *
 * @sa [The GNU Readline Library](https://tiswww.case.edu/php/chet/readline/rltop.html)
 * @sa http://stackoverflow.com/a/31333315/99089
 */
bool have_genuine_gnu_readline( void );
