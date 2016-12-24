/* any given program is simply a single binary operation:

   a =: 1, b =: 2, print-int a+b

   (,) is of the highest precedence of any operator by default, while (=:) is second.
   by default, all other functions are of equal precedence.  The user can make functions that are of 
   different precedence, but it is not recommended.

   'Zero-argument' functions have the lowest precedence.  It is actually a compiler error to attempt
   to define a zero-argument function at any precedence.

   Functions are right-associative: 1*2+4/5 === 1*(2+(4/5))
   This is true for the majority of functions; the only exceptions are ones like (,).

   All functions take two arguments, though if an argument is not supplied, then it is ignored:

   print-int 2

   Only one argument is supplied here, the LHS can be interpreted as null, but attempting to
   reference it in any way is an error.

   There is a specific way of evaluation that happens.  The LHS of a function is always evaluated
   prior to the evaluation of the outer expression.  The RHS, however, is not.  This makes it
   possible to use the RHS as if it were a quote as in LISP.  The (call) function can be used
   to force evaluate the RHS.

   An example of this being used is (,).  (,) simply concatenates the statements given.  This
   allows full programs to be created, where functions like (=:) can be used and their results
   used in a subsequent expression.  However, under normal evaluation rules, (=:) would be
   right-associative.  This would mean that the program would effectively be read bottom-to-top.

   Since the LHS is explicitly to be evaluated first, it is guaranteed that the program would flow
   from top to bottom.  The RHS is called afterwards with whatever the LHS does already done.
*/
