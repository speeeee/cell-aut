#lang racket
(provide (all-defined-out))

; TODO: __very__ messy implementation so far; needs to be cleaned up.

; convert input program split by whitespace to bytecode w/ symboltable.

; program for creating the image of a program.

; TODO: allow file imports (all written to same output image).
; NOTE: unfortunately, for right now this compiles imported files almost the same as C
;     : does, in that it just copies the definitions into the output file.
;     : however, there will be checks for double-imports, so there is no need for manual checking of
;     : that.

(struct Literal (val type) #:transparent)
(struct Function (name id) #:transparent)
(struct Macro (name f))

(define (bytes-drop x n) (list->bytes (drop (bytes->list x) n)))
(define (symbolize x) (bytes->string/latin-1 (bytes-drop x 5)))

(define *OUT* '())

(define *IMP* '())
(define *FUNS* (list (Function "print-int" 0) (Function "[" 1) (Function "NO_USE" 2)
                     (Function "^RR" 3) (Function "^<>" 4) (Function "DEFINE" 5)
                     (Function "print-flt" 6) (Function "print" 7) (Function "read-char" 8)
                     (Function "curry" 9) (Function "if" 10) (Function "=" 11)
                     (Function "+" 12) (Function "-" 13) (Function "*" 14) (Function "/" 15)
                     (Function "swap" 16) (Function "dup" 17) (Function "drop" 18)
                     (Function "pick" 19) (Function "FROM-BACK" 20)
                     (Function "scope" 21) (Function "=scope?" 22) (Function "recur" 23)
                     (Function "false" 24) (Function "push-bottom" 25) (Function "stack-empty?" 26)))
(define *MAC*
  (list (Macro ":d" (lambda (x out)
    (set! *FUNS* (append *FUNS* (list (Function (symbolize (car x)) (length *FUNS*)))))
    (cons (bytes 2 5 0 0 0) (cdr x))))

    (Macro "import" (lambda (x out) (let ([a (symbolize (car x))])
       (if (member a *IMP*) (cdr x)
          (begin (set! *IMP* (cons a *IMP*)) (out-parse! (string-join (file->lines a)) out)
                 (cdr x))))))))

; tokenize : string -> (any, type)
; very simple one-to-one bytecode converter.
(define (tokenize in) (map (lambda (x) (cond
  [(string->number x) (let ([a (string->number x)]) (if (integer? a) (Literal a 'int32)
                                                                     (Literal a 'float64)))]
  [(member x (map Function-name *FUNS*))
    (let ([a (findf (lambda (y) (equal? x (Function-name y))) *FUNS*)]) (Literal a 'fun))]
  [(member x (map Macro-name *MAC*)) (let ([a (findf (lambda (y) (equal? x (Macro-name y))) *MAC*)])
     (Literal a 'macro))]
  [else (Literal x 'sym)])) in))

; parse : (any, type) -> file -> [byte-string]
(define (parse e out) (foldl (lambda (x n) (case (Literal-type x)
  [(int32) (cons (bytes-append (bytes 0) (integer->integer-bytes (Literal-val x) 4 #t)) n)]
  [(float64) (cons (bytes-append (bytes 1) (real->floating-point-bytes (Literal-val x) 8 #t)) n)]
  [(macro) ((Macro-f (Literal-val x)) n out)]
  ; make following less repetitive.
  [(fun) (cons (bytes-append (bytes 2) (integer->integer-bytes (Function-id (Literal-val x)) 4 #t)) n)]
  [else (if (member (Literal-val x) (map Function-name *FUNS*))
              (let ([a (findf (lambda (y) (equal? (Literal-val x) (Function-name y))) *FUNS*)])
                (cons (bytes-append (bytes 2) (integer->integer-bytes (Function-id a) 4 #t)) n))
            (cons (bytes-append (bytes 3) (integer->integer-bytes (string-length (Literal-val x)) 4 #f)
                                (string->bytes/latin-1 (Literal-val x))) n))])) '() e))

(define (out-parse! in out)
  (map (lambda (x) (write-bytes x out)) (reverse (parse (tokenize (string-split in)) out))))

(define (main)
  (let* ([args (vector->list (current-command-line-arguments))] #| 2 |#
         [in (string-join (file->lines (cadr args)))]
         [out (open-output-file (car args) #:exists 'replace)])
    (out-parse! in out) (close-output-port out)))

(main)
