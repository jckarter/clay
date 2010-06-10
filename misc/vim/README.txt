Place the clay.vim file inside ~/.vim/syntax

Add the following lines to your .vimrc file

au BufRead,BufNewFile *.clay set filetype=clay
au! Syntax clay runtime! syntax/clay.vim
