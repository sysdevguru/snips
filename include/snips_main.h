#ifndef SNIPS_MAIN_H
#define SNIPS_MAIN_H

void snips_main(int ac, char **av);
void set_help_function(int(*f)());
void set_readconfig_function(int(*f)());
void set_polldevices_function(int(*f)());
void set_test_function(u_long (*f)());

#endif
