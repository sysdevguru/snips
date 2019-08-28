#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>

char jsonfn_temp[512];
char jsonfn_real[512];

int json_count;

FILE *jsonf;
#define JSON_DIR "/usr/local/iris/data/spool"

void json_init(char *prognm, char *hardcoded_val)
{
    json_init_extension(prognm, hardcoded_val, NULL);
}

void json_init_extension(char *prognm, char *hardcoded_val, char *fileExtension) {
    
    char *fileExt = "json";
    
    if (fileExtension)
        fileExt = fileExtension;
    
    snprintf(jsonfn_real, 512,  "%s/%s__snips.%ld.%s", /*get_jsondir()*/ JSON_DIR, prognm,
        time(NULL), fileExt);
   
    
    sprintf(jsonfn_temp, "%s.tmp.%d", jsonfn_real, getpid());

    if ((jsonf = fopen(jsonfn_temp, "w")) == NULL) {
      fprintf(stderr, "(error) Could not open JSON file %s for writing!\n", jsonfn_temp); exit(-1);
    }

    // XXX this is bad to hardcode snipsippingmon here: should be 'snips' + pgrognm and remove the 01,02,03 if present
    fprintf(jsonf, "{\n\t\"%s\": {\n\t\t\"%s\": {", prognm, hardcoded_val);

    json_count = 0;
}

void json_snips_log(char *parent, int count, ...) {
  va_list argp;
  char *name;
  float *value;

  int curCount = 0;
  fprintf(jsonf, "%c\n\t\t\t\"%s\": {", json_count ? ',' : ' ', parent);

  va_start(argp, count);

  while (count > 0) {
    name = va_arg(argp, char*);
    value = (float *)va_arg(argp, char *);

    if (value != 0) {
        if (curCount <= 1)
       	fprintf(jsonf, "\n\t\t\t\t\"%s\": %d%c", name, *((int *)value), count > 1 ? ',' : ' ');
        else
       	fprintf(jsonf, "\n\t\t\t\t\"%s\": %.9f%c", name, *((float *)value), count > 1 ? ',' : ' ');

    } else {
      fprintf(jsonf, "\n\t\t\t\t\"%s\": null%c", name, count > 1 ? ',' : ' ');
    }

                curCount++;
    count--;
  }

  va_end(argp);

  fprintf(jsonf, "\n\t\t\t}");
  json_count++;
}


void json_log(char *parent, int count, ...) {
  va_list argp;
  char *name;
  float *value;

  int curCount = 0;
  fprintf(jsonf, "%c\n\t\t\t\"%s\": {", json_count ? ',' : ' ', parent);

  va_start(argp, count);

  while (count > 0) {
    name = va_arg(argp, char*);
    value = (float *)va_arg(argp, char *);


    if (value != 0) {
    	if (curCount <= 1)
            fprintf(jsonf, "\n\t\t\t\t\"%s\": %d%c", name, (int) *value, count > 1 ? ',' : ' ');
        else
            fprintf(jsonf, "\n\t\t\t\t\"%s\": %.2f%c", name, *value, count > 1 ? ',' : ' ');

    } else {
      fprintf(jsonf, "\n\t\t\t\t\"%s\": null%c", name, count > 1 ? ',' : ' ');
    }

		curCount++;
    count--;
  }

  va_end(argp);

  fprintf(jsonf, "\n\t\t\t}");
  json_count++;
}

void json_close() {
  if (jsonf) {
    fprintf(jsonf, "\n\t\t}\n\t}\n}\n");

    fclose(jsonf);
  

    if (rename(jsonfn_temp, jsonfn_real)) {
      fprintf(stderr, "(error) could not complete JSON file move from %s to %s\n", 
          jsonfn_temp, jsonfn_real);
    }
  }

  jsonf = NULL;
}
