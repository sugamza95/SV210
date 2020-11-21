//#include "select.h"
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
///////////////////////////////////
#include <sys/stat.h>
#include <fcntl.h>
//#include <linux/delay.h>
///////////////////////////////

#define EVENT_BUF_NUM 1
////////////////////////
#define TEXTLCD_BASE            0xbc
#define TEXTLCD_COMMAND_SET     _IOW(TEXTLCD_BASE,0,int)
#define TEXTLCD_FUNCTION_SET    _IOW(TEXTLCD_BASE,1,int)
#define TEXTLCD_DISPLAY_CONTROL _IOW(TEXTLCD_BASE,2,int)
#define TEXTLCD_CURSOR_SHIFT    _IOW(TEXTLCD_BASE,3,int)
#define TEXTLCD_ENTRY_MODE_SET  _IOW(TEXTLCD_BASE,4,int)
#define TEXTLCD_RETURN_HOME     _IOW(TEXTLCD_BASE,5,int)
#define TEXTLCD_CLEAR           _IOW(TEXTLCD_BASE,6,int)
#define TEXTLCD_DD_ADDRESS      _IOW(TEXTLCD_BASE,7,int)
#define TEXTLCD_WRITE_BYTE      _IOW(TEXTLCD_BASE,8,int)

struct strcommand_varible {
        char rows;
        char nfonts;
        char display_enable;
        char cursor_enable;
        char nblink;
        char set_screen;
        char set_rightshit;
        char increase;
        char nshift;
        char pos;
        char command;
        char strlength;
        char buf[16];
};

struct strcommand_varible strcommand;


void RED_key(int text_LCD,int * quit);


int main(void) {
  int i, quit = 1;
  int key_fd = -1;       /* the file descriptor for the device */
  size_t read_bytes; // 몇 bytes read 했느냐
  struct input_event event_buf[EVENT_BUF_NUM]; //
  int text_dev, dot_dev, temp=0;
  int value=0;////////
   
        strcommand.rows = 0;            
        strcommand.nfonts = 0;          
        strcommand.display_enable = 1;          
        strcommand.cursor_enable = 0;           
        strcommand.nblink = 0;          
        strcommand.set_screen = 0;
        strcommand.set_rightshit = 1;           
        strcommand.increase = 1;
        strcommand.nshift = 0;          
        strcommand.pos = 10;            
        strcommand.command = 1;         
        strcommand.strlength = 16;
  

  dot_dev = open("/dev/dotmatrix", O_WRONLY);
  text_dev = open("/dev/textlcd", O_WRONLY|O_NDELAY );
  if ((key_fd = open("/dev/input/event3", O_RDONLY)) < 0) {
    printf("application : keypad driver open fail!\n");
    exit(1);
  }

  printf("press the key button!\n");
  printf("press the key 26 to exit!\n");
  while (quit) {
	read_bytes=
        read(key_fd, event_buf, (sizeof(struct input_event) * EVENT_BUF_NUM));

    if (read_bytes < sizeof(struct input_event)) {
      printf("application : read error!!");
      exit(1);
    }

    for (i = 0; i < (read_bytes / sizeof(struct input_event)); i++) {

      // event type : KEY
      // event value : 0(pressed)
      if ((event_buf[i].type == EV_KEY) && (event_buf[i].value == 0)) {
        value = event_buf[i].code;
	 write(text_dev, &value, 1);  
      
	 switch(event_buf[i].code){
		case 1 : //RED
			RED_key(text_dev,&quit);
			//
			break;
		default:
			break;
	
        }
      }
    }
  }
  close(key_fd);
  close(dot_dev);
  close(text_dev);
  return 0;
}




void RED_key(int text_dev, int *quit){
        char Red[16]   = "Red             ";

	sleep(1);
        ioctl(text_dev,TEXTLCD_CLEAR,strcommand,32);
        printf("RED\n");        
        strcommand.pos = 0;     
        ioctl(text_dev,TEXTLCD_DD_ADDRESS,strcommand,32);
        write(text_dev,Red,16);
//        sleep(1);
        quit = 1;
}



