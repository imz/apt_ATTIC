;;; Directory Local Variables
;;; For more information see (info "(emacs) Directory Variables")
((nil
  ;; for indenting braces after "if", "for" etc.:
  (c-file-style . "bsd")
  (c-basic-offset . 3)
  (c-file-offsets . (;; for indenting "public:", "protected:", etc.:
		     (access-label . 0))))

 (c++-mode
  ;; Don't mix tabs and spaces (e.g., when indenting to pos 9):
  (indent-tabs-mode . nil))

 ;; For switching the mode in .h headers:
 (c-mode
  (mode . c++)
  (indent-tabs-mode . nil)))
