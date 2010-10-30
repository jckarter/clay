Place the syntax/clay.vim file inside ~/.vim/syntax for syntax highlighting,
and the plugin/clay.vim file inside ~/.vim/plugin for clay-specific commands.

Add the following lines to your .vimrc file to use the syntax highlighter:
--
au BufRead,BufNewFile *.clay set filetype=clay
au! Syntax clay runtime! syntax/clay.vim
--

If you've installed Clay to a different path from /usr/local/lib, set
the g:LibClay variable to enable the plugin commands:
--
let g:LibClay = "/path/to/lib-clay"
--

The clay plugin adds the following ex-mode commands:

:ClayLibraryModule <module> [<platform>]
    Opens the clay source file for the library module <module> under
    the g:LibClay directory. <module> is specified as a dotted path, as
    in a clay "import" statement. If there are multiple platform-specific
    source files for the module, one is chosen arbitrarily.
    Example:
        :ClayLibraryModule io.files

The clay plugin adds the following command-mode keyboard commands:

\cl       short for :ClayLibraryModule<SPACE>

If you've changed the mapleader character, that character will be used instead
of backslash (\).
