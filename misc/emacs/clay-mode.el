;;; clay-mode-el -- Major mode for editing Clay source files

;; Author: Prafulla Kiran <prafulla@tachyontech.net>
;; Created: 19 May 2010
;; Keywords: Clay major-mode

;; Copyright (C) 2010 Prafulla Kiran <prafulla@tachyontech.net>

;; This program is free software; you can redistribute it and/or
;; modify it under the terms of the GNU General Public License as
;; published by the Free Software Foundation; either version 2 of
;; the License, or (at your option) any later version.

;; This program is distributed in the hope that it will be
;; useful, but WITHOUT ANY WARRANTY; without even the implied
;; warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;; PURPOSE.  See the GNU General Public License for more details.

;; You should have received a copy of the GNU General Public
;; License along with this program; if not, write to the Free
;; Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
;; MA 02111-1307 USA

;;; Commentary:
;;
;; This mode is designed to make development in Clay easier for Emacs users

;;; Code:

(defvar clay-mode-hook nil)
(defvar clay-mode-map
  (let ((clay-mode-map (make-keymap)))
    clay-mode-map)
  "Keymap for Clay major mode")

(add-to-list 'auto-mode-alist '("\\.clay\\'" . clay-mode))

(defconst clay-quoted-string-re "\\(\".*?[^\\]\"\\|'.*?[^\\]'\\)")
(defconst clay-quoted-string-or-regex-re "\\(/.*?[^\\]/\\w*\\|\".*?[^\\]\"\\|'.*?[^\\]'\\)")

(defconst clay-font-lock-keywords-1
  ;; TODO: Add - "as" "..."? "rvalue" "lvalue" "<--"? "goto" "lambda" "block" "new"
  (list
   ;; --- Begin handling quoted strings (lifted-from javascript.el) ---

   ;; detect literal strings following a + operator
   (list (concat "+[ \t]*" clay-quoted-string-re)  1 font-lock-string-face)

   ;; detect literal strings used in "literal object" keys
   (list (concat "[,{][ \t]*" clay-quoted-string-re "[ \t]*:" ) 1 font-lock-string-face)

   ;; detects strings and regexes when assigned, passed, returned
   ;; used as an object key string (i.e. bla["some string"]), when used
   ;; as a literal object value (i.e. key: "string"), used as an array
   ;; element, or when they appear as the first expression on a line
   ;; and a few other hairy cases
   (list (concat "[=(:,;[][ \t]*" clay-quoted-string-or-regex-re)  1 font-lock-string-face)
   (list (concat "^[ \t]*"      clay-quoted-string-or-regex-re) 1 font-lock-string-face)
   (list (concat "return[ \t]*" clay-quoted-string-or-regex-re) 1 font-lock-string-face)

   ;; ;; detect "autoquoted" object properties... clases with "switch { ...  default: }"
   ;; ;; may not be worth the trouble
   ;; (list "\\(^[ \t]*\\|[,{][ \t]*\\)\\(\\w+\\):" 2 font-lock-string-face)
   ;; --- End handling of quoted strings ---

   '("^\\(\\[.*?\\]\\) *" 1 font-lock-doc-face)

   '("\\<\\(?:a\\(?:nd\\|s\\)\\|\\(?:b\\(?:loc\\|rea\\)k\\)\\|continue\\|e\\(?:num\\|lse\\|xternal\\)\\|for\\|goto\\|i\\(?:mport\\|nlined\\|[fn]\\)\\|l\\(?:ambda\\|value\\)\\|n\\(?:ew\\|ot\\)\\|o\\(?:r\\|verload\\(?:able\\)?\\)\\|p\\(?:rivate\\|ublic\\)\\|r\\(?:value\\|\\(e\\(?:cord\\|f\\|turn\\)\\)\\)\\|static\\|var\\|while\\)\\>" . font-lock-keyword-face)

   '("\\<\\(true\\|false\\)\\>" . font-lock-constant-face)

   '("\\(^[ \t]*var[ \t]*\\)\\(\\w+\\)[ \t]*=" 2 font-lock-variable-name-face))
  "Keyword highlighting for Clay mode.")

;; Moar syntax highlighting to be done - comments, builtins, strings et cetera
;; builtins - print, println, printTo, printlnTo, assert, range, reserve
;; TODO: Disabled builtin functions, uncomment following fn to enable
;; (defconst clay-font-lock-keywords-2
;;   (append clay-font-lock-keywords-1
;;           (list
;;            '("\\<\\(assert\\|\\(?:print\\(?:To\\|ln\\(?:To\\)?\\)?\\)\\|r\\(?:ang\\|eserv\\)e\\)\\((\\)" 1 font-lock-builtin-face)))
;;   "Built-in functions")

(defvar clay-font-lock-keywords clay-font-lock-keywords-1
  "Default highlighting expressions for Clay mode.")


(defun clay-indent-line ()
  ;; TODO: handle indentation for lines ending with comma, (
  "Indent current line as clay code"
  (interactive)
  (beginning-of-line)
  (if (bobp)
      (indent-line-to 0)
    (let ((not-indented t) cur-indent)
      (if (looking-at "^[ \t]*}") ; If the line we are looking at is the end of a block, then decrease the indentation
          (progn
            (save-excursion
              (forward-line -1)
              (setq cur-indent (- (current-indentation) default-tab-width)))
            (if (< cur-indent 0) ; We can't indent past the left margin
                (setq cur-indent 0)))
        (save-excursion
          (setq loop-count 0)
          (while not-indented ; Iterate backwards until we find an indentation hint
            (setq loop-count (+ loop-count 1))
            (forward-line -1)
            (if (looking-at "^[ \t]*}") ; This hint indicates that we need to indent at the level of the } token
                (progn
                  (setq cur-indent (current-indentation))
                  (setq not-indented nil))
              ;; TODO: Clean up repeated usage of same regexp
              (if (looking-at ".*?{[ ]*$\\|^[ \t]*\\(else\\|for\\|if\\|while\\)") ; This hint indicates we need to indent an extra level
                  (progn
                    (setq use-current-indentation (and (> loop-count 1)
                                                       (not (looking-at ".*?{[ ]*$"))))
                    (setq cur-indent (if use-current-indentation
                          (current-indentation)
                          (+ (current-indentation) default-tab-width))) ; Do the actual indenting
                    (setq not-indented nil))
                (if (bobp)
                    (setq not-indented nil)))))))
      (if cur-indent
          (indent-line-to cur-indent)
        (indent-line-to 0))))) ; If we didn't see an indentation hint, then allow no indentation

;; Syntax-table
(defvar clay-mode-syntax-table
  (let ((clay-mode-syntax-table (make-syntax-table)))
    ; Comment styles - // and C-style
    (modify-syntax-entry ?/ ". 124b" clay-mode-syntax-table)
    (modify-syntax-entry ?* ". 23" clay-mode-syntax-table)
    (modify-syntax-entry ?\n "> b" clay-mode-syntax-table)
    clay-mode-syntax-table)
  "Syntaxt table for clay-mode")

(defun clay-mode ()
  (interactive)
  (kill-all-local-variables)
  (use-local-map clay-mode-map)
  (set-syntax-table clay-mode-syntax-table)
  ;; Set up font-lock
  (set (make-local-variable 'font-lock-defaults) '(clay-font-lock-keywords))
  ;; Setting comment-start, comment-start-skip and comment-end - C-style
  (set (make-local-variable 'comment-start) "/*")
  (set (make-local-variable 'comment-start-skip) "/\\*+ *")
  (set (make-local-variable 'comment-end) "*/")
  ;; Setting default tab width to 4
  (set (make-local-variable 'default-tab-width) 4)
  ;; Register our indentation function
  (set (make-local-variable 'indent-line-function) 'clay-indent-line)
  (setq major-mode 'clay-mode)
  (setq mode-name "Clay")
  (run-hooks 'clay-mode-hook))

(provide 'clay-mode)

;;; clay-mode.el ends here
