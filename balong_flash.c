#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "hdlcio.h"

//******************************************************
//*  поиск символического имени раздела по его коду
//* Rechercher le nom symbolique d'une section sur son code
//******************************************************

void  find_pname(unsigned int id,unsigned char* pname) {

unsigned int j;
struct {
  char name[20];
  int code;
} pcodes[]={ 
  {"M3Boot",0x20000}, 
  {"M3Boot-ptable",0x10000}, 
  {"M3Boot_R11",0x200000}, 
  {"Ptable",0x10000},
  {"Fastboot",0x110000},
  {"Logo",0x130000},
  {"Kernel",0x30000},
  {"Kernel_R11",0x90000},
  {"VxWorks",0x40000},
  {"VxWorks_R11",0x220000},
  {"M3Image",0x50000},
  {"M3Image_R11",0x230000},
  {"DSP",0x60000},
  {"DSP_R11",0x240000},
  {"Nvdload",0x70000},
  {"Nvdload_R11",0x250000},
  {"Nvimg",0x80000},
  {"System",0x590000},
  {"System",0x100000},
  {"APP",0x570000}, 
  {"APP",0x5a0000}, 
  {"Oeminfo",0xa0000},
  {"CDROMISO",0xb0000},
  {"Oeminfo",0x550000},
  {"Oeminfo",0x510000},
  {"Oeminfo",0x1a0000},
  {"WEBUI",0x560000},
  {"WEBUI",0x5b0000},
  {"Wimaxcfg",0x170000},
  {"Wimaxcrf",0x180000},
  {"Userdata",0x190000},
  {"Online",0x1b0000},
  {"Online",0x5d0000},
  {"Online",0x5e0000},
  {0,0}
};

for(j=0;pcodes[j].code != 0;j++) {
  if(pcodes[j].code == id) break;
}
if (pcodes[j].code != 0) strcpy(pname,pcodes[j].name); // le nom de la copie trouvée dans la structure - имя найдено - копируем его в структуру 
else sprintf(pname,"U%08x",id); // le nom n'est pas trouvé-substitut au format Uxxxxxxxx -имя не найдено - подставляем псевдоимя Uxxxxxxxx в тупоконечном формате
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc, char* argv[]) {

unsigned int i,j,res,opt,npart=0,iolen,part,blk,blksize;
FILE* in;
FILE* out;

struct {
  unsigned int offset;    // position de l'image de la partition - позиция образа раздела
  unsigned int hdoffset;  // position de la section d'en-tête - позиция заголовка раздела
  unsigned int size;      // la section de taille d'image - размер образа раздела
  unsigned int hdsize;    // la taille de l'en-tête - размер заголовка раздела
  unsigned int code;      // L'ID de partition - ID раздела
  unsigned char pname[20];    // un nom de clé littéral - буквенное имя раздела
  unsigned char filename[50]; // le nom du fichier qui correspond à la section - имя файла, соответствующее разделу
}ptable[100];

unsigned char buf[40960];
unsigned char devname[50]="/dev/ttyUSB0";
unsigned char replybuf[4096];
unsigned char datamodecmd[]="AT^DATAMODE";
unsigned char resetcmd[]="AT^RESET";
unsigned int err;

unsigned char OKrsp[]={0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};
unsigned char NAKrsp[]={0x03, 0x00, 0x02, 0xba, 0x0a, 0x7e};

unsigned int  dpattern=0xa55aaa55;
unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0;
unsigned char filename [100];

unsigned char fdir[40];   // Répertoire pour le firmware multifonction - каталог для мультифайловой прошивки

unsigned char cmdver[7]={0x0c};           // la version du protocole - версия протокола
unsigned char cmddone[7]={0x1};           // commande de quitter le HDLC - команда выхода из HDLC
unsigned char cmd_reset[7]={0xa};           // l'équipe de quitter le HDLC - команда выхода из HDLC
unsigned char cmd_dload_init[15]={0x41};  // l'équipe a commencé la section - команда начала раздела
unsigned char cmd_data_packet[11000]={0x42};  // l'équipe a commencé le paquet - команда начала пакета
unsigned char cmd_dload_end[30]={0x43};       // article de fin de commande - команда конца раздела
// Codes de type de partition - Коды типов разделов
//-d       - une tentative de changer le modem de l'HDLC en mode AT- попытка переключить модем из режима HDLC в АТ-режим\n\       

while ((opt = getopt(argc, argv, "hp:mersn")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n L'outil est conçu pour les firmwares de modems de la famille E3372S\n\n\
%s [options] <le nom du fichier à télécharger, ou un nom de répertoire avec les fichiers>\n\n\
 Les options valides sont:\n\n\
-p <tty> - port série pour la communication avec le chargeur (par défault /dev/ttyUSB0)\n\
-n       - firmware mode mul′tifajlovoj du répertoire spécifié\n\
-m       - afficher la sortie et le fichier du firmware carte\n\
-e       - Démonter le fichier du firmware en sections sans en-têtes\n\
-s       - Démonter le fichier du firmware en sections avec rubriques\n\
-r       - quittez et redémarrez le firmware du modem\n\
\n",argv[0]);
    return;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'm':
     mflag=1;
     break;
     
   case 'n':
     nflag=1;
     break;
     
   case 'r':
     rflag=1;
     break;
     
   case 'e':
     eflag=1;
     break;

   case 's':
     sflag=1;
     break;

   case '?':
   case ':':  
     return;
  }
}  

if (eflag&sflag) {
  printf("\n les options -s et -e sont incompatibles\n");
  return;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf("\n l'option -n est incompatible avec les options -s, -m et -e\n");
  return;
}  
  

// ------  reboot sans spécifier de fichier - перезагрузка без указания файла
//--------------------------------------------
if ((optind>=argc)&rflag) goto sio; 


// Ouverture du fichier d'entrée - Открытие входного файла
//--------------------------------------------
if (optind>=argc) {
  if (nflag)
    printf("\n - Ne spécifiez pas un répertoire avec des fichiers\n");
  else 
    printf("\n - Ne spécifiez pas un nom de fichier à charger\n");
  return;
}  

if (nflag) 
  // pour-n- il suffit de copier le préfixe
  strncpy(fdir,argv[optind],39);
else {
  // pour les opérations de fichier unique - для однофайловых операций
in=fopen(argv[optind],"r");
if (in == 0) {
  printf("\n Erreur d'ouverture %s",argv[optind]);
  return;
}
}


// Rechercher dans les sections du fichier - Поиск разделов внутри файла
//--------------------------------------------

if (!nflag) {
  printf("\n Rechercher dans les sections du fichier...");
  while (fread(&i,1,4,in) == 4) {
    if (i != dpattern) continue; // recherche du code splitter - ищем разделитель
    
    // Mettez en surbrillance la section options - Выделяем параметры раздела
    ptable[npart].hdoffset=ftell(in);  // position de l'en-tête de début - позиция начала заголовка раздела
    fread(buf,1,96,in); // en-tête - заголовок
    ptable[npart].hdsize=*((unsigned int*)&buf[0])-4;  // la taille de l'en-tête - размер заголовка
    ptable[npart].offset=ptable[npart].hdoffset+ptable[npart].hdsize; // le décalage de la section de corps - смещение до тела раздела 
    ptable[npart].size=*((unsigned int*)&buf[20]); // la taille de la partition - размер раздела
    ptable[npart].code=*((unsigned int*)&buf[16]); // le type de partition - тип раздела
    
    // Vous cherchez le nom symbolique de la table de partition - Ищем символическое имя раздела по таблице 
    find_pname(ptable[npart].code,ptable[npart].pname);
  // augmenter les sections de comptoir - увеличиваем счетчик разделов 
    npart++; 
  }
  if (npart == 0) {
    printf("\nSujets non trouvés !");
    return ;
  }  
}  

// Rechercher les fichiers du firmware dans le répertoire spécifié
//--------------------------------------------
else {
  printf("\n Image de rechercher des fichiers sections.\n\n ##  Nom de fichier taille ID\n-----------------------------------------------------------------\n");
  for (npart=0;npart<30;npart++) {
    if (find_file(npart, fdir, ptable[npart].filename, &ptable[npart].code, &ptable[npart].size) == 0) break; // fin de la recherche-section avec ce ID introuvable
    // obtenir le nom symbolique de la section
    find_pname(ptable[npart].code,ptable[npart].pname);
    printf("\n %02i  %8i  %08x  %-8.8s  %s",npart,ptable[npart].size,ptable[npart].code,ptable[npart].pname,ptable[npart].filename);
  }
}

printf("\n Trouvé des sections %i",npart);

  
//------ Fichier de firmware sortie mode carte
//--------------------------------------------
  
if (mflag) {
 printf("\n Table de partition dans le fichier :\n\n ##Compensés taille nom\n-------------------------------------");
 for (i=0;i<npart;i++) 
     printf("\n %02i %08x %8i  %s",i,ptable[i].offset,ptable[i].size,ptable[i].pname); 
 printf("\n");
 return;
}


//-------Mode de coupe un fichier du firmware
//--------------------------------------------
if (eflag|sflag) {
 printf("\n Partitionnement du fichier du firmware :\n\n ## Compensés taille nom\n-------------------------------------");
 for (i=0;i<npart;i++) {  
   printf("\n %02i %08x %8i  %s",i,ptable[i].offset,ptable[i].size,ptable[i].pname); 
   // nom de fichier de forme
   sprintf(filename,"%02i-%08x-%s.%s",i,ptable[i].code,ptable[i].pname,(eflag?"bin":"fw"));
   out=fopen(filename,"w");
   
   if(sflag) {
     // l'enregistrement d'en-tête
     fwrite(&dpattern,1,4,out); // en-tête de bloc marqueur-magie
     fseek(in,ptable[i].hdoffset,SEEK_SET); // aller au début du titre
     fread(buf,1,ptable[i].offset-ptable[i].hdoffset,in);
     fwrite(buf,1,ptable[i].offset-ptable[i].hdoffset,out);
   }
   // écrire le corps
   fseek(in,ptable[i].offset,SEEK_SET); //aller au début
   for(j=0;j<ptable[i].size;j+=4) {
     fread(buf,4,1,in);
     fwrite(buf,4,1,out);
   }
   fclose(out);
 }
printf("\n");
return;
}


sio:
//--------- Base mode-écriture firmware
//--------------------------------------------

// Configuration de SIO

if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Port série %s n'est pas ouvert\n", devname); 
#else
   printf("\n - Serial COM port %s ne s'ouvre pas\n", devname); 
#endif
   return; 
}


tcflush(siofd,TCIOFLUSH);  // Videz la mémoire tampon de sortie

// quitter le mode HDLC-si le modem était déjà dedans
port_timeout(1);
send_cmd(cmddone,1,replybuf);



// Entrer en mode HDLC
printf("\n Entrer dans le mode HDLC...");
port_timeout(100);


for (err=0;err<10;err++) {

if (err == 10) {
  printf("\n Vous avez dépassé le nombre de tentatives pour entrer en mode programmation\n");
  return;
}  
  
write(siofd,datamodecmd,strlen(datamodecmd));
res=read(siofd,replybuf,6);
if (res != 6) {
  printf("\nLongueur de la réponse incorrecte sur ^DATAMODE, réessayer...");
  continue;
}  
if (memcmp(replybuf,OKrsp,6) != 0) {
  printf("\n Commande ^DATAMODE rejetée...");
  continue;
}  

iolen=send_cmd(cmdver,1,replybuf);
if ((iolen == 0)||(replybuf[1] != 0x0d)) {
  printf("\n Impossible d'obtenir la version du protocole, réessayer...");
  continue;
}  
break;
}
  
i=replybuf[2];
replybuf[3+i]=0;
printf("ok");
printf("\n Version de protocole : %s",replybuf+3);
printf("\n");

if ((optind>=argc)&rflag) goto reset; // reboot sans spécifier de fichier


// Les sections enregistrement de boucle principale
for(part=0;part<npart;part++) {
  printf("\r Article record %i - %s\n",part,ptable[part].pname);
  
  // Remplissez votre paquet d'équipe
  *((unsigned int*)&cmd_dload_init[1])=htonl(ptable[part].code);  
  *((unsigned int*)&cmd_dload_init[5])=htonl(ptable[part].size);  
  // Envoyer la commande
  iolen=send_cmd(cmd_dload_init,12,replybuf);
  if ((iolen == 0) || (replybuf[1] != 2)) {
    printf("\n Le titre de l'article n'est pas adopté, le code d'erreur = %02x %02x %02x\n",replybuf[1],replybuf[2],replybuf[3]);
//    dump(cmd_dload_init,13,0);
    return;
  }  

  //  Préparations pour le cycle de pobločnomu
  blksize=4096; // la valeur initiale de la taille du bloc
  if (!nflag)   
  // aller au début de la section en mode odnofajlovom
    fseek(in,ptable[part].offset,SEEK_SET);
  else 
  // Ouvrez le fichier dans un mode multi-fichier 
    in=fopen(ptable[part].filename,"r");

  // Cycle de Pobločnyj
  for(blk=0;blk<((ptable[part].size+4095)/4096);blk++) {
    printf("\r Блок %i из %i",blk,(ptable[part].size+4095)/4096); fflush(stdout);
    res=ptable[part].size+ptable[part].offset-ftell(in);  // la taille de la pièce restante jusqu'à la fin du fichier
    if (res<4096) blksize=res;  // ajuster la taille du dernier bloc
    *(unsigned int*)&cmd_data_packet[1]=htonl(blk+1);  // # paquet
    *(unsigned short*)&cmd_data_packet[5]=htons(blksize);  // taille de bloc
    fread(cmd_data_packet+7,1,blksize,in); // lire le prochain morceau tampon de commande partition
    iolen=send_cmd(cmd_data_packet,blksize+7,replybuf); //Envoyer la commande
    if ((iolen == 0) || (replybuf[1] != 2)) {
      printf("\n La première partition bloquer % ne pas acceptée, code d'erreur = %02x %02x %02x\n",blk,replybuf[1],replybuf[2],replybuf[3]);
//      dump(cmd_data_packet,blksize+7,0);
      return;
    }  
   }
   
   // Fermer un fichier en mode fichier multi
   if (nflag) fclose(in);
    
   // Fermer un flux
   *((unsigned int*)&cmd_dload_end[1])=htonl(ptable[part].size);     
   *((unsigned int*)&cmd_dload_end[8])=htonl(ptable[part].code);
   iolen=send_cmd(cmd_dload_end,24,replybuf); //Envoyer la commande
  if ((iolen == 0) || (replybuf[1] != 2)) {
    printf("\n Erreur fermeture section, code d'erreur = %02x %02x %02x\n",replybuf[1],replybuf[2],replybuf[3]);
//     dump(replybuf,iolen,0);
    return;
  }  
   
}   
// La fin de la boucle principale  
printf("\n");


port_timeout(1);
// quitter le mode HDLC et redémarrage
reset:

if (rflag) {
  printf("\n Touches de modem...\n");
  send_cmd(cmd_reset,1,replybuf);
  write(siofd,resetcmd,strlen(resetcmd));
}  
else send_cmd(cmddone,1,replybuf);
} 
