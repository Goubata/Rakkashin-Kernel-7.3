/* Execute only reviewed constructor instructions against private buffers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void ctor_original(const unsigned char *,unsigned char *);
extern void ctor_candidate(const unsigned char *,unsigned char *);
static int check(const char *path,const char *wanted,void (*fn)(const unsigned char *,unsigned char *))
{
 unsigned char literal[22],obj[512];FILE *f=fopen(path,"rb");size_t i,n=strlen(wanted);
 if(!f)return 1;
 if(fseek(f,0x6239,SEEK_SET) || fread(literal,1,sizeof(literal),f)!=sizeof(literal)){fclose(f);return 2;}
 fclose(f);memset(obj,0xa5,sizeof(obj));fn(literal,obj);
 if(obj[0xc0]!=(n<<1) || memcmp(obj+0xc1,wanted,n) || obj[0xc1+n] || strlen((char *)obj+0xc1)!=n)return 3;
 for(i=0;i<sizeof(obj);i++)if((i<0xc0 || i>0xd6) && obj[i]!=0xa5)return 4;
 return 0;
}
int main(int argc,char **argv)
{
 int a,b;if(argc!=3)return 64;
 a=check(argv[1],"pmih010x-2s-ibat-lvl0",ctor_original);b=check(argv[2],"pmih010x-2s-ibat0",ctor_candidate);
 if(a||b){printf("constructor error original=%d candidate=%d\n",a,b);return 1;}
 puts("AVR_THERMAL_CTOR_PASS cases=2 scope=selected-aarch64-string-constructor-private-buffers-only");return 0;
}
