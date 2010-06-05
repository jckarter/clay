Steps to install clay-mode in emacs

1) Copy clay-mode.el to a directory of your choice (E.g. ~/emacs.d/clay)

2) Add the following two pieces of lines to your .emacs file -
   (add-to-list 'load-path "~/.emacs.d/clay/")
   (require 'clay-mode)
