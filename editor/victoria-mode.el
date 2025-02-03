;;; victoria-mode.el -- Major mode for Victoria -*- lexical-bingind: t -*-

(require 'go-mode)

(defconst victoria-mode-syntax-table
  (with-syntax-table (copy-syntax-table)
    ;; Comments.
    (modify-syntax-entry ?# "<")
    (modify-syntax-entry ?\n ">")
    (syntax-table))
  "Syntax table for victoria-mode")

(define-derived-mode victoria-mode go-mode "vic"
  "Major mode for Victoria"
  (setq comment-start "#")
  (setq indent-tabs-mode nil)
  (setq tab-width 4)
  :syntax-table 'victoria-mode-syntax-table)

(provide 'victoria-mode)

;;; victoria-mode.el ends here.
