Place the syntax/clay.vim file inside ~/.vim/syntax for syntax highlighting,
and the plugin/clay.vim file inside ~/.vim/plugin for clay-specific commands.

Add the following line to your .vimrc file to use the syntax highlighter:
--
au BufRead,BufNewFile *.clay set filetype=clay
--

If you've installed Clay to a different path from /usr/local/lib, set
the g:LibClay variable for the plugin:
--
let g:LibClay = "/path/to/lib-clay"
--

The clay plugin adds the following ex-mode commands:

:LibClayModule <module> [<platform>]
    Opens the clay source file for the library module <module> under
    the g:LibClay directory. <module> is specified as a dotted path, as
    in a clay "import" statement. If there are multiple platform-specific
    source files for the module, one is chosen arbitrarily.
    Example:
        :LibClayModule io.files
:LibClayAlternate <n>
    Sets g:LibClay to the <n>th element of the g:LibClayAlternates
    variable. This makes it easy to navigate between different Clay
    projects.
    Example:
        :let g:LibClayAlternates = ["/usr/local/lib/lib-clay", "~/clayproject"]
        :LibClayAlternate 1
        ...do work in ~/clayproject
        :LibClayAlternate 0

The clay plugin adds the following command-mode keyboard commands:

\cl       short for :LibClayModule<SPACE>
\c<n>     short for :LibClayAlternate <n> (for <n> between 0 and 9)

If you've changed the mapleader character, that character will be used instead
of backslash (\) for the above keyboard commands.
