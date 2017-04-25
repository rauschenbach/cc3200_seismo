#ifndef _VERSION_H
#define _VERSION_H

#include "my_defs.h"

/* ������ ������ ����� */
#define __MY_VER__	0
#define __MY_REV__	28

#ifndef _WIN32			/* Embedded platform */
	static const unsigned char prog_ver = __MY_VER__; /* ������ ��������� */
	static const unsigned char prog_rev = __MY_REV__; /* ������� ��������� */
#else
	const unsigned char  prog_ver = __MY_VER__; /* ������ ��������� */
	const unsigned char prog_rev = __MY_REV__; /* ������� ��������� == */
#endif


/**
 *  ���������� ������ ������
 */
IDEF unsigned char get_version(void)
{
    return prog_ver; 	/* ������ ��������� */
}


/**
 *  ���������� ������� ������
 */
IDEF unsigned char get_revision(void)
{
    return prog_rev; 	/* ������ ��������� */
}



#endif /* version.h */

