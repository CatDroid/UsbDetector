/*
 * Main.c
 *
 *  Created on: 2015-5-3
 *      Author: tom
 */

#include <stdio.h>

extern int MainExt();

int main(int argc ,char** argv)
{
	printf("main\o");
	printf("ret = %d\n" , MainExt() );
	return 0;
}


