
;(autoload 'perl-mode "cperl-mode" "alternate mode for editing Perl programs" t)

;;; You can either fine-tune the bells and whistles of this mode or
;;; bulk enable them by putting
;(setq cperl-hairy t)
(global-unset-key "\C-h")
(global-set-key "\C-h" 'delete-backward-char)
;(load "/usr/share/emacs/site-lisp/rng-auto.el")

(require 'cc-mode)
(defun my-build-tab-stop-list (width)
  (let ((num-tab-stops (/ 80 width))
        (counter 1)
        (ls nil))
    (while (<= counter num-tab-stops)
      (setq ls (cons (* width counter) ls))
      (setq counter (1+ counter)))
    (set (make-local-variable 'tab-stop-list) (nreverse ls))))
(defun my-c-mode-common-hook ()
  (setq tab-width 4) ;; change this to taste, this is what K&R uses :)
  (my-build-tab-stop-list tab-width)
  (setq c-basic-offset tab-width)
  (setq indent-tabs-mode t)) ;; force only spaces for indentation
(add-hook 'c-mode-common-hook 'my-c-mode-common-hook)



(add-hook 'c-mode-common-hook
  (lambda()
    (local-set-key (kbd "C-c <right>") 'hs-show-block)
    (local-set-key (kbd "C-c <left>")  'hs-hide-block)
    (local-set-key (kbd "C-c <up>")    'hs-hide-all)
    (local-set-key (kbd "C-c <down>")  'hs-show-all)
    (hs-minor-mode t)))


;; replace C-s with C-\ in a much more general way so that C-\ can be typed
;; for every instance of C-s. It is at such a low level that emacs even thinks
;; that you typed a C-s.  replace C-s with C-\  ,  globally
;; this disables any command which uses C-\ I believe there are two
;; Note That position \034(octal) which is C-\ is switched with \023(octal)
;; which is C-s

 (setq keyboard-translate-table "\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\023\035\036\037 !\042#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\134]^_`abcdefghijklmnopqrstuvwxyz{|}~\177")


        ;; change ctrl-q so it can be used as flow control
        ;; remap C-q tp ESC `

;; (global-unset-key "\C-q")
;; (global-set-key "\e`" 'quoted-insert)


	;; don't make backup files
        ;; (setq make-backup-files nil)
(setq require-final-newline t)


	;; change input mod to CBREAK mode for C-s C-q
	;; emacs will never see C-s, C-q
        ;; (set-input-mode nil t)

	;; change help command from C-h to ESC ?
	;; so we can use C-h for backspace
(global-unset-key "\C-h")
(global-set-key "\C-h" 'delete-backward-char)

(global-set-key "\e?" 'help-command)
(global-set-key "\C-x\C-x" 'mail-send)
	;; set backspace to delete a char same as DEL

        ;; (global-unset-key "\d")
        ;; (global-set-key "\d" 'delete-char)
(global-set-key "\C-cg" 'goto-line)
	;; set up the ispell spelling checker interface
(global-unset-key "\C-o")
(global-set-key "\C-o" 'undo)
(global-set-key "\M-\]" 'copy-region-as-kill)
(global-set-key "\C-\\" 'mark-word)
(global-unset-key "\C-f" )
(global-set-key "\C-f" 'forward-word)
(global-unset-key "\C-b" )
(global-set-key "\C-b" 'backward-word)
(global-unset-key "\M-f" )


(global-set-key "\M-f" 'find-file)
(global-set-key "\M-o" 'find-file-other-window)
(global-set-key "\M-\\" 'mark-word)
(global-set-key "\M-{" 'mark-whole-buffer)
;(global-unset-key "\M-}")



(global-set-key "\M-}" 'indent-region)
(global-set-key "\C-x\C-m" 'save-buffer)
(global-set-key "\C-c\C-m" 'delete-other-windows)
(global-set-key "\C-c\'" 'split-window-vertically)
(global-set-key "\C-c\;" 'split-window-horizontally)
(global-set-key "\C-x\z" 'yank)



(autoload 'ispell-word "ispell"
 "Check the spelling of word in buffer." t)
(autoload 'ispell-complete-word "ispell" "Complete word at or before point" t)
(autoload 'ispell-region "ispell" 
  "Check spelling of every word in the region" t)
(autoload 'ispell-buffer "ispell" 
  "Check spelling of every word in the buffer" t)

(global-set-key "\e$" 'ispell-word)
(global-unset-key "\C-t")
(global-set-key "\C-t" 'forward-word)
;; (global-unset-key "\C-&")
;; (global-set-key "\C-&" 'backward-word)
(global-set-key "\C-cs" 'ispell-region)

(global-set-key "\C-c," 'backward-paragraph)
(global-set-key "\C-c." 'forward-paragraph)
(global-set-key "\C-c\C-c" 'compile)
(global-set-key "\C-c\/" 'compile)
(global-set-key "\C-c\]" 'replace-string)
(global-set-key "\C-ce" 'exchange-dot-and-mark)
(global-set-key "\C-cs" 'shrink-window)

;; THE FOLLOWING ARE CUSTOMIZATIONS YOU CAN ADD IF YOU WANT THEM
;; YOU WILL HAVE TO EDIT THIS FILE TO DO SO

;; AUTO FILL
;;  If you want emacs to automatically wrap when you reach the
;;  end of a line (i.e. you don't have to type in the RETURN at the
;;  end of each line, you just keep typing) remove the semicolons
;;  from the two line after this paragraph (the ones with setq).
;;  Set the default major mode to text mode and turn on auto fill


;;(setq default-major-mode 'text-mode)
(setq text-mode-hook 'turn-on-auto-fill)
;; (setq load-path ("/usr/local/lib/emacs/lisp/" "/usr/local/test/lib/emacs/site-lisp" "/home/anthm/.lisp"))
(setq term-file-prefix (concat (car load-path) "/term/"))
(setq term-setup-hook '(lambda nil
	(if (fboundp 'enable-arrow-keys) (enable-arrow-keys))))

; (autoload 'html-mode "/home/anthm/.lisp/html-mode" "HTML major mode." t)
 ;(or (assoc "\\.html$" auto-mode-alist)
  ; (setq auto-mode-alist (cons '("\\.html$" . html-mode) 
   ;                            auto-mode-alist)))



  (setq auto-mode-alist (cons '("\\.html$" . html-helper-mode) auto-mode-alist))
  (setq auto-mode-alist (cons '("\\.cgi$" . cperl-mode) auto-mode-alist))
  (setq auto-mode-alist (cons '("\\.p[ml]$" . cperl-mode) auto-mode-alist))

  (setq auto-mode-alist (cons '("\\.xml$" . nxml-mode) auto-mode-alist))

(setq html-helper-do-write-file-hooks t)
(setq html-helper-build-new-buffer t)


(add-hook 'cperl-mode-hook 'n-cperl-mode-hook t)
(defun n-cperl-mode-hook ()
;  (setq cperl-indent-level 4)
;  (setq cperl-continued-statement-offset 0)
;  (setq cperl-extra-newline-before-brace t)
  (set-face-background 'cperl-array-face "black")
  (set-face-background 'cperl-hash-face "black")
  )





(cond ((fboundp 'global-font-lock-mode)
       ;; Customize face attributes
       (setq font-lock-face-attributes
             ;; Symbol-for-Face Foreground Background Bold Italic Underline
             '((font-lock-comment-face       "green")
	       (font-lock-preprocessor-face       "gray")
               (font-lock-string-face        "Sienna")
               (font-lock-keyword-face       "purple")
               (font-lock-function-name-face "limegreen")
               (font-lock-variable-name-face "Yellow")
               (font-lock-type-face          "Yellow")
               (font-lock-reference-face     "Purple")

	       (font-lock-builtin-face "limegreen")
	       (font-lock-constant-face "yellow")
	       (font-lock-doc-face "limegreen")
	       (font-lock-highlighting-face "limegreen")
	       (font-lock-warning-face "limegreen")

               ))
       ;; Load the font-lock package.
       (require 'font-lock)
       ;; Maximum colors
       (setq font-lock-maximum-decoration t)
       ;; Turn on font-lock in all modes that support it
       (global-font-lock-mode t)))




 


;; Fix for arrow key disease

 
(define-key function-key-map "\eOA" [up])
(define-key function-key-map "\e[A" [up])
(define-key function-key-map "\eOB" [down])
(define-key function-key-map "\e[B" [down])
(define-key function-key-map "\eOC" [right])
(define-key function-key-map "\e[C" [right])
(define-key function-key-map "\eOD" [left])
(define-key function-key-map "\e[D" [left])







(set-background-color "black")
(set-foreground-color "white")
(set-cursor-color "white")

