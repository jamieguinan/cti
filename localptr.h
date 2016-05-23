#ifndef _CTI_LOCAL_PTR_H_
#define _CTI_LOCAL_PTR_H_

/* Type X must have an associated X_free function. */
#define localptr(_type, _var) _type * _var __attribute__ ((__cleanup__( _type ## _free )))

#endif
