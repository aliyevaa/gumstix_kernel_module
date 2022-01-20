/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include<linux/vmalloc.h>
#include<linux/string.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <asm/param.h>
#include <linux/gfp.h>
#include <linux/pid.h>

#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/gpio.h>

MODULE_LICENSE("Dual BSD/GPL");
int proc_ind;
static int fasync_example_fasync(int fd, struct file *filp, int mode);
static int fasync_example_open(struct inode *inode, struct file *filp);
static int fasync_example_release(struct inode *inode, struct file *filp);
static ssize_t fasync_example_write(struct file *filp, const char *buf, size_t size, loff_t *f_pos);
static ssize_t fasync_example_read(struct file *filp, char *buf, size_t size, loff_t *f_pos);
static int fasync_example_fasync(int fd, struct file *filp, int mode);
static int fasync_example_init(void);
static void fasync_example_exit(void);
static void timer_handler(unsigned long data);
static int mygpio_len;
/* Declaration of the init and exit functions */
module_init(fasync_example_init);
module_exit(fasync_example_exit);

struct file_operations fasync_example_fops = {
        write: fasync_example_write,
        open: fasync_example_open,
        release: fasync_example_release,
        read: fasync_example_read
		//read:proc_gpio_read
};


static int mytimer_major = 61;
//static char *mytimer_buffer;
struct timer_list *Timer, *Timer_new;

static int mode = 0; //O=>up mode, 1=>down mode 
static int pause = 1; //0->do count, 1=> pause
static int freq = 2; //standard frequency
static int count=15;	
static int btn0 = 17;
static int btn1 = 101;
static int btn2 = 113;
static int LEDs[4]={28,29,30,31};
void  timer_init(void);
void  timer_init_new(void);
#define BTN0 17
#define BTN1 101
#define BTN2 113 

int flag_count=0;
static int pressed0 = 0;
static int pressed1 = 0;
static int pressed2 = 0;


int pwm_modes=3;

irqreturn_t gpio_irq_btn0(int irq, void *dev_id, struct pt_regs *regs)
{
	pressed0=1;
	printk("Button0 released...\n");
	if (pressed1==0){
		timer_init_new();
	}	
	return IRQ_HANDLED;               
}

irqreturn_t gpio_irq_btn1(int irq, void *dev_id, struct pt_regs *regs)
{
        printk("Button IRQ2\n");
	pressed1=1;
	if (pressed0==0){timer_init_new();}
        return IRQ_HANDLED;
}

void pwm_brightness_control(void)
{
	pxa_set_cken(CKEN0_PWM0, 1);
	PWM_CTRL0 = 0;
	PWM_PERVAL0 = 0x20;
	switch(pwm_modes)
	{
		case 1:			
			PWM_PWDUTY0 = 0x01;
			break;
		case 2:			
			PWM_PWDUTY0 = 0x0F;
			break;
		case 3:		
			PWM_PWDUTY0 = 0x20;
			break;
		default:
			pxa_set_cken(CKEN0_PWM0, 0);
			PWM_PWDUTY0 = 0x20;
			PWM_PERVAL0 = 0x20;
			printk("Invalid value of pwm selector. Switching pwm off\n");
			break;
	}
}


irqreturn_t gpio_irq_btn2(int irq, void *dev_id, struct pt_regs *regs)
{       
	pressed3=1;
	pwm_modes++;
	if(pwm_modes==4)
		pwm_modes = 1;

	if(flag_count == 1)
		pwm_brightness_control();
        return IRQ_HANDLED;
}


int pxa_gpio_get_value(unsigned gpio)
{
	return __gpio_get_value(gpio);
}

/*
 * Set output GPIO level
 */
void pxa_gpio_set_value(unsigned gpio, int value)
{
	__gpio_set_value(gpio, value);
}


void  timer_init(void)
{
    init_timer(Timer);
    Timer->expires=jiffies + HZ*freq/2;/////???????
    Timer->function = timer_handler;  
    add_timer(Timer);
}

void  timer_init_new(void)
{
    init_timer(Timer_new);
    Timer_new->expires=jiffies + HZ*freq/2;/////???????
    Timer_new->function = timer_handler_new;
    add_timer(Timer_new);
}
static void timer_handler_new(unsigned long data) {
	if (pressed0 && pressed1){
		count=15;
		flag_count=1;
		int c = count;
        	int i;
		for (i=3;i>=0;i--){
                	int rem = c%2;
                	c = c/2;
                	pxa_gpio_set_value(LEDs[i],rem);
        	}
		mode=1;
	}
	else if(pressed0==1){
		if(flag_count==0){
			flag_count=1;
		}
		else{
			flag_count=0;
		}
	}
	else if (pressed1==1){
		if (mode==0){
			mode=1;
		}
		else {
			mode=0;
		}
	}
	pressed0=0;
	pressed1=1;
}
static int __init  fasync_example_init(void){
        int result;

        result = register_chrdev(mytimer_major, "mygpio",&fasync_example_fops);
        if (result < 0)
        {
                printk(KERN_ALERT
                        "mygpio: cannot obtain major number %d\n", mytimer_major);
                return result;
        }

	        /* Allocating buffers */
        //mytimer_buffer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
		/* Allocating buffers */
	Timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);

        /* Check if all right */
        if (!Timer)
        {
                printk(KERN_ALERT "Insufficient kernel memory\n");
                result = -ENOMEM;
                goto fail;
        }
	mygpio_len = 0;
	//memset(mytimer_buffer, 0, capacity);
	//printk(KERN_ALERT "hello from  init\n");
	gpio_direction_input( btn0);
	gpio_direction_input( btn1);
	//printk(KERN_ALERT "buttons inited\n");
	gpio_direction_output(LEDs[0],0);
	gpio_direction_output(LEDs[1],0);
	gpio_direction_output(LEDs[2],0);
	gpio_direction_output(LEDs[3],0);
	pxa_gpio_set_value(LEDs[2],1);


	pxa_gpio_mode(GPIO16_PWM0_MD);
	pxa_set_cken(CKEN0_PWM0, 0);
	PWM_CTRL0 = 0;			//Setting pin to full duty cycle
	PWM_PWDUTY0 = 0x0A;
	PWM_PERVAL0 = 0x0A;

	
        int irq_btn0 = IRQ_GPIO(BTN0);
        if (request_irq(irq_btn0, &gpio_irq_btn0, SA_INTERRUPT | SA_TRIGGER_RISING, "mygpio", NULL) != 0 ) {
                printk ( "irq not acquired \n" );
                return -1;
        }else{
	
                printk ( "irq %d acquired successfully \n", irq_btn0 );
        }
	int irq_btn1 = IRQ_GPIO(BTN1);
        if (request_irq(irq_btn1, &gpio_irq_btn1, SA_INTERRUPT | SA_TRIGGER_RISING,
                                "mygpio", NULL) != 0 ) {

		printk ( "irq not acquired \n" );
                return -1;
        }else{

                printk ( "irq %d acquired successfully \n", irq_btn1 );
        }
	
	int irq_btn2 = IRQ_GPIO(BTN2);
        if (request_irq(irq_btn2, &gpio_irq_btn2, SA_INTERRUPT | SA_TRIGGER_RISING,
                                "mygpio", NULL) != 0 ) {
               
		printk ( "irq not acquired \n" );
                return -1;
        }else{

                printk ( "irq %d acquired successfully \n", irq_btn2 );
        }
	/*************************/

    	printk(KERN_ALERT "mygpio loaded.\n");
	return result;
fail:
        fasync_example_exit();
        return result;

}

static void fasync_example_exit(void){
	/* Freeing the major number */
	unregister_chrdev(mytimer_major, "mytimer");
	//printk(KERN_ALERT "fasync_example_exit..\n");
	//Free timer
	if (Timer) {
		del_timer(Timer);
		kfree(Timer);
	}
    	unregister_chrdev(mytimer_major, "mygpio");
	
	gpio_direction_output(LEDs[0],0);
	gpio_direction_output(LEDs[1],0);
	gpio_direction_output(LEDs[2],0);
	gpio_direction_output(LEDs[3],0);
	int i;
	for (i=0;i<4;i++){
		gpio_free(LEDs[i]);
	}
	gpio_free(btn0);
	gpio_free(btn1);
	gpio_free(btn2);
	//free_irq(IRQ_GPIO(MYGPIO), NULL);
    	printk(KERN_ALERT "Removing mygpio module\n");
}


static void timer_handler(unsigned long data) {
	if ((pressed1+pressed2)==2)	{
		printk("Both Pressed!!!\n");
		count = 15;
	}
	/*Change count to binary*/	
	int c = count;
	int i;
	for (i=3;i>=0;i--){	
		int rem = c%2;
		c = c/2;
		//binary = binary * 10 + rem;
    		pxa_gpio_set_value(LEDs[i],rem);             
	}

	/*Check to count or pause*/
	if (pxa_gpio_get_value(btn0) != 0) {
		/*Button0 is pressed => count chages*/
		if (mode != 0) {
			/*count down mode..*/
			count -= 1;
			if (count == 0) count=15;
		} else {
			/*count up mode..*/
			count += 1;
			if (count == 16) count=1;
		}	
	}
	del_timer(Timer);  
	timer_init();
}

static int fasync_example_open(struct inode *inode, struct file *filp) {
		//printk(KERN_ALERT "fasync_example_open\n");
        return 0;
}

static int fasync_example_release(struct inode *inode, struct file *filp) {
       //printk(KERN_ALERT "fasync_example_release\n");
        return 0;
}


static ssize_t fasync_example_write(struct file *filp, const char *buf, size_t size, loff_t *f_pos){
	//printk(KERN_ALERT "fasync_example_write..: %s, size: %d\n", buf, size);
	if (size!=3) {
		*f_pos += size;
		return size; //only run things for this
	}
	/*Change counter period..*/
	int old_count;
	old_count = count;
	if (buf[0]=='f') {
		int range = buf[1]-'0';
		if (range>0 && range<9) {
			/*Update timer*/
			freq = range;
		}
	}
   
	/*Setting counter values..*/ 
	else if (buf[0]=='v') {
		int num = buf[1]-'0';
		if (num>=1 && num<=9) { //is a number
			count = num;
			del_timer(Timer);  
			timer_init();
		} else if (num>=49 && num<=54) {//a to f
			num -= 48; //a->1, b->2,....f->6
			count = 9 + num;
			del_timer(Timer);  
			timer_init();		
		} 
	}
	//printk(KERN_ALERT "old count: %d, new_count: %d, new_freq: %d\n", old_count, count, freq);
	*f_pos += size;
	return size;
}


static ssize_t fasync_example_read(struct file *filp, char *buf, size_t size, loff_t *f_pos){
	printk(KERN_ALERT "<<<<< fasync_example_read..: %s\n", buf);
	char bptr [256];
	char *p=bptr;
	
	//Write counter value
	p += sprintf(p, "Counter value: %d\n", count);
	//Write period
	p += sprintf(p, "Period: %d/2\n", freq);
	//Write counter direction
	if (pxa_gpio_get_value(btn1) == 0) 
		p += sprintf(p, "Counter direction: DOWN\n");
	else 
		p += sprintf(p, "Counter direction: UP\n");
	//Write counter state
	if (pxa_gpio_get_value(btn0) == 0) 
		p += sprintf(p, "Counter state: STOPPED\n");
	else 
		p += sprintf(p, "Counter state: RUNNING\n");
	(*p) = 0;
	int len = strlen(bptr);
    if (*f_pos>=len){
		return 0;
	}

	if(size > len - *f_pos){
		size=len - *f_pos;
	}
	printk("len=%d",strlen(bptr) );
	//printk(KERN_ALERT "PRINTK= %s\n",bptr);
	if (copy_to_user(buf, bptr +*f_pos, size))
	{
		//memset(bptr,0,256);
		return -EFAULT;
	}

 
	*f_pos += size;	
    return size;
}

