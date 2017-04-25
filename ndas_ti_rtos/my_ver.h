#ifndef _VERSION_H
#define _VERSION_H

#include "my_defs.h"

/* Меняем только здесь */
#define __MY_VER__	0
#define __MY_REV__	28

#ifndef _WIN32			/* Embedded platform */
	static const unsigned char prog_ver = __MY_VER__; /* Версия программы */
	static const unsigned char prog_rev = __MY_REV__; /* Ревизия программы */
#else
	const unsigned char  prog_ver = __MY_VER__; /* Версия программы */
	const unsigned char prog_rev = __MY_REV__; /* Ревизия программы == */
#endif


/**
 *  Возвращает версию модуля
 */
IDEF unsigned char get_version(void)
{
    return prog_ver; 	/* Версия программы */
}


/**
 *  Возвращает ревизию модуля
 */
IDEF unsigned char get_revision(void)
{
    return prog_rev; 	/* Версия программы */
}



#endif /* version.h */

