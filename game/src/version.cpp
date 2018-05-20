#include <stdio.h>
#include <stdlib.h>

void WriteVersion()
{
#ifndef __WIN32__
	FILE* fp(fopen("version.txt", "w"));

	if (NULL != fp)
	{
		fprintf(fp, "game version: %s\n", __GAME_VERSION__);
		fprintf(fp, "%s@%s:%s\n", __USER__, __HOSTNAME__, __PWD__);
		fprintf(fp, "Statu || Finch || PixeRochelle || ProximaCenturia && Suky || Rolof || xSuki Server Files");
		fprintf(fp, "Github Tarafindan Lisanslanmistir. Kopyalanmasi GNU/LINUX Nebzinde Suc teskil etmektedir.");
		fprintf(fp, "Paylasilmasi Icın : RamsayBolton'a Ozel Izın Verilmistir Kanitlari Icın Iletisime Geciniz.");
		fclose(fp);
	}
	else
	{
		fprintf(stderr, "cannot open version.txt\n");
		exit(0);
	}
#endif
}
