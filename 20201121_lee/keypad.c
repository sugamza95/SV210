// 모듈의 헤더파일 선언
#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/types.h>

#define DRIVER_AUTHOR "hanback"
#define DRIVER_DESC "KEYPAD program"

#define KEYPAD_NAME "KEYPAD"
#define KEYPAD_MODULE_VERSION "KEYPAD V0.1" // 디바이스 버전
#define KEYPAD_PHY_ADDR 0x88000000          // keypad의 물리주소
#define KEYPAD_ADDR_RANGE 0x1000
////////////////////////////
#define TIMER_INTERVAL 5
///////////////////////////
// Global variable
static int keypad_usage = 0;
static struct timer_list mytimer;
static unsigned short value;
static unsigned long keypad_ioremap;
static unsigned short *keypad_row_addr, *keypad_col_addr;
static unsigned short *buzzer_addr;
char keypad_fpga_keycode[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

static struct input_dev *idev;
void mypollingfunction(unsigned long data);

// define functions...
// 응용 프로그램에서 디바이스를 처음 사용하는 경우를 처리하는 함수
int keypad_open(struct inode *minode, struct file *mfile) {
  // 디바이스가 열려 있는지 확인.
  if (keypad_usage != 0)
    return -EBUSY;

  // keypad의 가상 주소 매핑
  keypad_ioremap = (unsigned long)ioremap(KEYPAD_PHY_ADDR, KEYPAD_ADDR_RANGE);

  // 등록할 수 있는 I/O 영역인지 확인
  if (!check_mem_region(keypad_ioremap, KEYPAD_ADDR_RANGE)) {
    // I/O 메모리 영역을 등록
    request_mem_region(keypad_ioremap, KEYPAD_ADDR_RANGE, KEYPAD_NAME);
  } else {
    printk("driver: unable to register this!\n");
    return -1;
  }

  // keypad 의 col, row 와 buzzer 의 주소 설정
  keypad_col_addr = (unsigned short *)(keypad_ioremap + 0x70);
  keypad_row_addr = (unsigned short *)(keypad_ioremap + 0x72);
  buzzer_addr = (unsigned short *)(keypad_ioremap + 0x50);

  init_timer(&mytimer);

  mytimer.expires = get_jiffies_64() + TIMER_INTERVAL;
  mytimer.function = &mypollingfunction;

  add_timer(&mytimer);
  keypad_usage = 1;
  return 0;
}

// 응용 프로그램에서 디바이스를 더이상 사용하지 않아서 닫기를 구현하는 함수
int keypad_release(struct inode *minode, struct file *mfile) {
  // 커널 타이머 목록에서 등록된 것을 제거
  del_timer(&mytimer);

  // 매핑된 가상주소를 해제
  iounmap((unsigned long *)keypad_ioremap);

  // 등록된 I/O 메모리 영역을 해제
  release_mem_region(keypad_ioremap, KEYPAD_ADDR_RANGE);

  keypad_usage = 0;
  return 0;
}

// 커널 타이머에서 호출 되는 함수
void mypollingfunction(unsigned long data) {
  int i, j = 1, k; //, row=1;
  int function_key = 0;
  unsigned char tmp[4] = {0x1, 0x2, 0x4, 0x8};
  value = 0;
  for (i = 0; i < 4; i++) {
    *keypad_row_addr = tmp[i];
    value = *keypad_col_addr & 0x0f;
    if (value > 0) {
      for (k = 0; k < 4; k++) {
        if (value == tmp[k]) {
          value = j + (i * 4);
          function_key = keypad_fpga_keycode[value - 1];
          // printk("function_key = %d\n", function_key);
          if (value != 0x00)
            goto stop_poll;
        }
        j++;
      }
    }
  }
stop_poll:
  if (value > 0) {
    input_report_key(idev, function_key, 1);
    input_report_key(idev, function_key, 0);
    input_sync(idev);
    *buzzer_addr = 0x1; // buzzer 를 켠다. mdelay(30);
    *buzzer_addr = 0x0; // buzzer 를 끈다.
  } else {
    *keypad_row_addr = 0x00;
  }
  mytimer.expires = get_jiffies_64() + TIMER_INTERVAL;
  add_timer(&mytimer);
}

// 디바이스 드라이버의 읽기를 구현하는 함수
ssize_t keypad_read(struct file *inode, char *gdata, size_t length,
                    loff_t *off_what) {
  int ret;
  // value 가 가리키는 커널 메모리 데이터를 gdata 가 가리키는 사용자 // 메모리
  // 데이터에 n 바이트 만큼 써넣는다.
  ret = copy_to_user(gdata, &value, 1);
  if (ret < 0)
    return -1;
  return length;
}
// 파일 오퍼레이션 구조체
// 파일을 열때 open()을 사용한다. open()는 시스템 콜을 호출하여 커널 내부로
// 들어간다. // 해당 시스템 콜과 관련된 파일 연산자 구조체 내부의 open 에
// 해당하는 필드가 드라이버 내에서 // keypad_open()으로 정의되어 있으므로
// keypad_open()가 호출된다. read, release도 마찬가지로 동작한다.
struct file_operations keypad_fops = {
    .owner = THIS_MODULE,
    .open = keypad_open,
    .read = keypad_read,
    .release = keypad_release,
};

// 모듈을 커널 내부로 삽입
// 모듈 프로그램의 핵심적인 목적은 커널 내부로 들어가서 서비스를 제공받는
// 것이므로 // 커널 내부로 들어가는 init()을 먼저 시작한다. 응용 프로그램은 소스
// 내부에서 정의되지 않은 많은 함수를 사용한다. 그것은 외부 // 라이브러리가
// 컴파일 과정에서 링크되어 사용되기 때문이다. 모듈 프로그램은 커널 //
// 내부하고만 링크되기 때문에 커널에서 정의하고 허용하는 함수만을 사용할 수
// 있다.
int keypad_init(void) {
  int result;
  int i = 0;

  idev = input_allocate_device();

  set_bit(EV_KEY, idev->evbit);
  set_bit(EV_KEY, idev->keybit);

  for (i = 0; i < 30; i++)
    set_bit(i & KEY_MAX, idev->keybit);

  idev->name = "keypad";
  idev->id.vendor = 0x1002;
  idev->id.product = 0x1002;
  idev->id.bustype = BUS_HOST;
  idev->phys = "keypad/input";
  idev->open = keypad_open;
  idev->close = keypad_release;
  result = input_register_device(idev);

  if (result < 0) {
    printk(KERN_WARNING "Can't get any major\n");
    return result;
  }
  return 0;
}

// 모듈을 커널에서 제거
void keypad_exit(void) {

  // 문자 디바이스 드라이버를 제거한다.
  input_unregister_device(idev);

  printk("driver: %s DRIVER EXIT\n", KEYPAD_NAME);
}

module_init(keypad_init);
module_exit(keypad_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC); // 모듈에 대한 설명
MODULE_LICENSE("Dual BSD/GPL");  // 모듈의 라이선스 등록
