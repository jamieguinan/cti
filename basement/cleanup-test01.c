/*
 * This is a test of the "__cleanup__" variable attribute available in
 * GCC and LLVM/Clang. I wanted to see how variables behaved in various
 * contexts.
 *
 * The result is a confirmation that C variable lifetime lasts from
 * point of instantiation to end of enclosing scope.
 * 
 * Run with "./cleanup-test01.sh" to test at different optimization
 * levels. The output should not change.
 */

#include <stdio.h>
#include <stdlib.h>

/* Type X must have an associated "void X_free(X **)" function. */
#define localptr(_type, _var) _type * _var __attribute__ ((__cleanup__( _type ## _free )))


/* Custom type */
typedef struct {
  const char * value;
} X;


/* Allocation/init function (constructor) */
X * X_new(const char * init)
{
  X * x = malloc(sizeof(*x));
  x->value = init;
  return x;
}

/* Cleanup function (destructor) */
void X_free(X ** x)
{
  printf("%s\n", (*x)->value);
  free(*x);
  *x = NULL;
}


/* Function with parameterized behavior that exercises different local
   variable scopes. */
void f1(int i)
{
  printf("==%s\n", __func__);
  localptr(X, a) = X_new("a");
  {
    localptr(X, b) = X_new("b");
  }

  localptr(X, c) = X_new("c");

  if (i == 0) {
    localptr(X, d) = X_new("d");
    return;
  }
  else {
    localptr(X, e) = X_new("e");
  }

  localptr(X, f) = X_new("f");
}


int main()
{
  printf("==%s\n", __func__);
  localptr(X, x1) = X_new("bye");
  f1(0);
  f1(1);
  return 0;
}
