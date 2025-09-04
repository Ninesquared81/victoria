;;; victoria-mode.el -- Major mode for Victoria -*- lexical-binding: t -*-

(require 'odin-mode)

(defconst victoria-mode-syntax-table
  (with-syntax-table (copy-syntax-table)
    ;; Comments.
    (modify-syntax-entry ?# "<")
    (modify-syntax-entry ?\n ">")
    ;; Operators.
    (modify-syntax-entry ?& ".")
    (modify-syntax-entry ?@ ".")
    (modify-syntax-entry ?! ".")
    (modify-syntax-entry ?^ ".")
    (modify-syntax-entry ?, ".")
    (modify-syntax-entry ?: ".")
    (modify-syntax-entry ?$ ".")
    (modify-syntax-entry ?. ".")
    (modify-syntax-entry ?= ".")
    (modify-syntax-entry ?> ".")
    (modify-syntax-entry ?< ".")
    (modify-syntax-entry ?- ".")
    (modify-syntax-entry ?% ".")
    (modify-syntax-entry ?+ ".")
    (modify-syntax-entry ?? ".")
    (modify-syntax-entry ?\; ".")
    (modify-syntax-entry ?/ ".")
    (modify-syntax-entry ?* ".")
    (modify-syntax-entry ?~ ".")
    (modify-syntax-entry ?| ".")
    (syntax-table))
  "Syntax table for victoria-mode")

(defconst victoria-keywords
  '("and" "as" "const" "else" "enum" "external"
    "for" "func" "if" "loop" "module" "mut" "or" "out"
    "package" "record" "return" "then" "to" "type"
    "union" "val" "var" "when" "while")
  "Victoria keywords (except types and constants)")

(defconst victoria-types
  '("bool" "c_string" "complex32_32" "complex64_64"
    "f32" "f64" "i8" "i16" "i32" "i64" "int"
    "string" "u8" "u16" "u32" "u64" "uint")
  "Built-in Victoria data types")

(defconst victoria-builtins
  '("count_of" "type_of" "size_of")
  "Built-in (magic) Victoria functions")

(defconst victoria-constants
  '("false" "null" "true" "uninit")
  "Victoria constants")

(defconst victoria-string-prefix-regexp
  '("\\<c\"")
  "Regexps for string prefixes in Victoria")

(defconst victoria-highlighting
  `((,(regexp-opt victoria-builtins 'symbols)  . font-lock-builtin-face)
    (,(regexp-opt victoria-keywords 'symbols)  . font-lock-keyword-face)
    (,(regexp-opt victoria-types 'symbols)     . font-lock-type-face)
    (,(regexp-opt victoria-constants 'symbols) . font-lock-constant-face))
    "Syntax highlighting for victoria-mode")

;;;###autoload
(define-derived-mode victoria-mode odin-mode "vic"
  "Major mode for Victoria"
  (setq comment-start "#")
  (setq indent-tabs-mode nil)
  (setq tab-width 4)
  (setq font-lock-defaults '(victoria-highlighting))
  :syntax-table 'victoria-mode-syntax-table)

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.vic\\'" . victoria-mode))

(provide 'victoria-mode)

;;; victoria-mode.el ends here.
