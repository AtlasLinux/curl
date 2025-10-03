#include "curl.h"
#include <stdio.h>
#include <string.h>

int main(int argc,char**argv){
    if(argc<2){ fprintf(stderr,"Usage: %s <url> [-o file]\n",argv[0]); return 1; }
    const char*url=argv[1]; const char*outf=NULL;
    for(int i=2;i<argc;i++){ if(strcmp(argv[i],"-o")==0 && i+1<argc){ outf=argv[++i]; } }

    FILE*out=stdout; if(outf){ out=fopen(outf,"wb"); if(!out){ perror("fopen"); return 1; } }

    curl_global_init(0);
    CURL*h=curl_easy_init();
    curl_easy_setopt(h,CURLOPT_URL,(void*)url);
    curl_easy_setopt(h,CURLOPT_WRITEDATA,out);
    curl_easy_setopt(h,CURLOPT_VERBOSE,(void*)(intptr_t)1);

    int rc=curl_easy_perform(h);
    curl_easy_cleanup(h);
    curl_global_cleanup();
    if(outf) fclose(out);
    return rc==CURLE_OK?0:1;
}
