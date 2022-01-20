 /* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <asm/param.h> /* include HZ */
#include <linux/string.h> /* string operations */
#include <linux/timer.h> /* timer gizmos */
#include <linux/list.h> /* include list data struct */
#include <asm/gpio.h>
#include <linux/interrupt.h>
#include <asm/arch/pxa-regs.h>
#include <asm-arm/arch/hardware.h>


MODULE_LICENSE("Dual BSD/GPL");

/* Declaration of memory.c functions */
static int mycar_open(struct inode *inode, struct file *filp);
static int mycar_release(struct inode *inode, struct file *filp);
static ssize_t mycar_read(struct file *filp,char *buf, size_t count, loff_t *f_pos);
static ssize_t mycar_write(struct file *filp,const char *buf, size_t count, loff_t *f_pos);
static void mycar_exit(void);
static int mycar_init(void);

static void MOTORSetter(char d);


static void timer_handler(unsigned long data);


struct file_operations mycar_fops = {
	read: mycar_read,
	write: mycar_write,
	open: mycar_open,
	release: mycar_release
};

/* Declaration of the init and exit functions */
module_init(mycar_init);
module_exit(mycar_exit);

static unsigned capacity = 2560;
static unsigned bite = 2560;
module_param(capacity, uint, S_IRUGO);
module_param(bite, uint, S_IRUGO);

/* Global variables of the driver */
/* Major number */
static int mycar_major = 61;

/* Buffer to store data */
static char *mycar_buffer;
/* length of the current message */
static int mycar_len;




/* timer pointer */
static struct timer_list the_timer[1];

unsigned MOTOR[4];
int timePer = 500;

char command = 's';
char speed = 'H';

//int command=4;
//int speed;



static int mycar_init(void)
{
	int result, ret;
	MOTOR[0] = 46;
	MOTOR[1] = 47;
	MOTOR[2] = 16;
	MOTOR[3] = 17;

	gpio_direction_output(MOTOR[0],0); 
	gpio_direction_output(MOTOR[1],0);
	gpio_direction_output(MOTOR[2],0);
	gpio_direction_output(MOTOR[3],0);

	pxa_gpio_set_value(MOTOR[0],1); //
	pxa_gpio_set_value(MOTOR[1],1); //
	pxa_gpio_set_value(MOTOR[2],1); //
	pxa_gpio_set_value(MOTOR[3],1); //


	result = register_chrdev(mycar_major, "mycar", &mycar_fops);
	if (result < 0)
	{
		printk(KERN_ALERT "mycar: cannot obtain major number %d\n", mycar_major);
		return result;
	}

	
	mycar_buffer = kmalloc(capacity, GFP_KERNEL);
	/* Check if all right */
	if (!mycar_buffer)
	{ 
		printk(KERN_ALERT "Insufficient kernel memory\n"); 
		result = -ENOMEM;
		goto fail; 
	} 
	
	memset(mycar_buffer, 0, capacity);
	mycar_len = 0;

	setup_timer(&the_timer[0], timer_handler, 0);
	ret = mod_timer(&the_timer[0], jiffies + msecs_to_jiffies(timePer/2));


	pxa_set_cken(CKEN0_PWM0, 1);
	pxa_set_cken(CKEN1_PWM1, 1);

	pxa_gpio_mode(GPIO16_PWM0_MD);
	pxa_gpio_mode(GPIO17_PWM1_MD);

	PWM_CTRL0   = 0x00000020;
//	PWM_PWDUTY0 = 0x00000fff;
	PWM_PERVAL0 = 0x000003ff;

	PWM_PWDUTY0 = 0x000002cf;
	PWM_PWDUTY1 = 0x000002cf;
	PWM_CTRL1   = 0x00000020;
	//PWM_PWDUTY1 = 0x00000fff;
	PWM_PERVAL1 = 0x000003ff;

return 0;

fail: 
	mycar_exit(); 
	return result;
}

static void mycar_exit(void)
{

	gpio_direction_output(MOTOR[0],0); 
	gpio_direction_output(MOTOR[1],0);
	gpio_direction_output(MOTOR[2],0);
	gpio_direction_output(MOTOR[3],0);

	PWM_PWDUTY0 = 0x00000000;
	PWM_PWDUTY1 = 0x00000000;

	/* Freeing the major number */
	unregister_chrdev(mycar_major, "mycar");

	/* Freeing buffer memory */
	if (mycar_buffer)
		kfree(mycar_buffer);
	if (timer_pending(&the_timer[0]))
		del_timer(&the_timer[0]);

	printk(KERN_ALERT "Removing mycar module\n");

}

static int mycar_open(struct inode *inode, struct file *filp)
{

	return 0;
}

static int mycar_release(struct inode *inode, struct file *filp)
{

	return 0;
}



static ssize_t mycar_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int temp;
	char tbuf[256];


	printk(KERN_ALERT "In write\n");
		
	
	if (*f_pos >= capacity)
	{
		return -ENOSPC;
	}

	if (count > bite) count = bite;

	if (count > capacity - *f_pos)
		count = capacity - *f_pos;

	if (copy_from_user(mycar_buffer + *f_pos, buf, count))
	{
		return -EFAULT;
	}


	temp = *f_pos;

	if (mycar_buffer[temp] == 's' && count < 2) 
	{
		command = 's';
	}
	else if (mycar_buffer[temp] == 'e' && count < 6) 
	{
		command = 's';
	}	
	else if (mycar_buffer[temp] == 'f' && mycar_buffer[temp+1] == 'H' && count < 3) 
	{
		command = 'f';
		speed = 'H';
	}
	else if (mycar_buffer[temp] == 'f' && mycar_buffer[temp+1] == 'M' && count < 3) 
	{
		command = 'f';
		speed = 'M';
		printk("%c %c\n",command,speed);
	}
	else if (mycar_buffer[temp] == 'f' && mycar_buffer[temp+1] == 'L' && count < 3) 
	{
		command = 'f';
		speed = 'L';
	}
	else if (mycar_buffer[temp] == 'b' && mycar_buffer[temp+1] == 'H' && count < 3) 
	{
		command = 'b';
		speed = 'M';
	}
	else if (mycar_buffer[temp] == 'b' && mycar_buffer[temp+1] == 'M' && count < 3) 
	{
		command = 'b';
		speed = 'M';
	}
	else if (mycar_buffer[temp] == 'b' && mycar_buffer[temp+1] == 'L' && count < 3) 
	{
		command = 'b';
		speed = 'L';
	}
	//right
	else if (mycar_buffer[temp] == 'r' && mycar_buffer[temp+1] == 'H' && count < 3) 
	{
		command = 'r';
		speed = 'H';
	}
	else if (mycar_buffer[temp] == 'r' && mycar_buffer[temp+1] == 'M' && count < 3) 
	{
		command = 'r';
		speed = 'M';
	}
	//left
	else if (mycar_buffer[temp] == 'l' && mycar_buffer[temp+1] == 'H' && count < 3) 
	{
		command = 'l';
		speed = 'H';
	}
	else if (mycar_buffer[temp] == 'l' && mycar_buffer[temp+1] == 'M' && count < 3) 
	{
		command = 'l';
		speed = 'M';
	}


	*f_pos += count;
	mycar_len = *f_pos;
	return count;
}
static ssize_t mycar_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{ 
	
	char tbuf[1012];

	//sprintf(tbuf,"%d\t%d\t%s\t%s\t%s", countVal, period, state, dir, bLevel);
	sprintf(tbuf,"done\n");

	strcpy(mycar_buffer,tbuf);

	//do not go over then end
	if (count > 2560 - *f_pos)
		count = 2560 - *f_pos;
	
	if (copy_to_user(buf,mycar_buffer + *f_pos, count))
	{
		return -EFAULT;	
	}
	
	// Changing reading position as best suits 
	*f_pos += count; 
	return count; 
}




static void timer_handler(unsigned long data) 
{
	MOTORSetter(command);
	mod_timer(&the_timer[0],jiffies + msecs_to_jiffies(timePer/2));

}	


static void MOTORSetter(char d)
{
	//printk("command = %c, speed = %c\n",d,speed);

	if (d == 'r')
	{
		pxa_gpio_set_value(MOTOR[0],0);
		pxa_gpio_set_value(MOTOR[1],1);
		pxa_gpio_set_value(MOTOR[2],0);
		pxa_gpio_set_value(MOTOR[3],1);
	}
	else if (d == 'l')
	{
		pxa_gpio_set_value(MOTOR[0],1);
		pxa_gpio_set_value(MOTOR[1],0);
		pxa_gpio_set_value(MOTOR[2],1);
		pxa_gpio_set_value(MOTOR[3],0);
	}
	else if (d == 'b')
	{
		pxa_gpio_set_value(MOTOR[0],0);
		pxa_gpio_set_value(MOTOR[1],1);
		pxa_gpio_set_value(MOTOR[2],1);
		pxa_gpio_set_value(MOTOR[3],0);
	}
	else if (d == 'f')
	{
		pxa_gpio_set_value(MOTOR[0],1);
		pxa_gpio_set_value(MOTOR[1],0);
		pxa_gpio_set_value(MOTOR[2],0);
		pxa_gpio_set_value(MOTOR[3],1);
	}
	else if (d == 's')
	{
		pxa_gpio_set_value(MOTOR[0],0);
		pxa_gpio_set_value(MOTOR[1],0);
		pxa_gpio_set_value(MOTOR[2],0);
		pxa_gpio_set_value(MOTOR[3],0);
	}

	if (speed == 'L')
	{
		PWM_PWDUTY0 = 0x0000018f;
		PWM_PWDUTY1 = 0x0000018f;
	}
	else if (speed == 'M')
	{
		PWM_PWDUTY0 = 0x000002cf;
		PWM_PWDUTY1 = 0x000002cf;
	}
	else if (speed == 'H')
	{
		PWM_PWDUTY0 = 0x000003ff;
		PWM_PWDUTY1 = 0x000003ff;
	}
}




