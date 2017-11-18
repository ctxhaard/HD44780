/*
 * hd44780.c - Controller driver for HD44780 2-lines LCD display
 * connected to GPIO lines.
 *
 * Copyright (C) 2017 Carlo Tomasin
 *
 * Author: Carlo Tomasin <c.tomasin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define DEBUG

#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define CMD_CLEAR_DISPLAY     (1 << 0)
#define CMD_RETURN_HOME       (1 << 1)
#define CMD_SET_DDRAM         (1 << 7)

#define LINE_OFFSET (0x40)
#define CMD_SET_DDRAM_LINE(x)  (CMD_SET_DDRAM | (LINE_OFFSET * (x - 1)))
#define CMD_SET_DDRAM_POS(x)  (CMD_SET_DDRAM  | x)

#define CMD_CLEAR_DISP  0x01

struct hd44780_data {

	struct platform_device *pdev;
	struct gpio_descs *gpios;
	int lines;
	int cols;
	size_t size;
	struct gpio_desc *gpio_rs;
	struct gpio_desc *gpio_en;
	struct gpio_desc *gpio_db4;
	struct gpio_desc *gpio_db5;
	struct gpio_desc *gpio_db6;
	struct gpio_desc *gpio_db7;

	dev_t devn;
	struct cdev cdev;

	spinlock_t lock;
	spinlock_t write_lock;
};

static struct class *lcd_class;
static dev_t first;
static dev_t next;

static void hd44780_pulse_enable(struct hd44780_data *pdata)
{
//	dev_dbg(&pdata->pdev->dev,"=> EN pulse\n");
	gpiod_set_value_cansleep(pdata->gpio_en, 0);
	udelay(500);
	gpiod_set_value_cansleep(pdata->gpio_en, 1);
	udelay(500);
	gpiod_set_value_cansleep(pdata->gpio_en, 0);
	udelay(500);
}

static void hd44780_write_4_bits(struct hd44780_data *pdata, u8 val)
{
//	dev_dbg(&pdata->pdev->dev,"=> 0x%02x\n", (val & 0xf));
	gpiod_set_value_cansleep(pdata->gpio_db7, val & (1 << 3));
	gpiod_set_value_cansleep(pdata->gpio_db6, val & (1 << 2));
	gpiod_set_value_cansleep(pdata->gpio_db5, val & (1 << 1));
	gpiod_set_value_cansleep(pdata->gpio_db4, val & (1 << 0));
	hd44780_pulse_enable(pdata);
}

static void hd44780_send(struct hd44780_data *pdata, u8 val, u8 mode)
{
	spin_lock(&pdata->write_lock);
	dev_dbg(&pdata->pdev->dev,"%s: 0x%02x\n", (mode ? "CHAR" : "CMD"), val);
	udelay(1);
	gpiod_set_value_cansleep(pdata->gpio_rs, mode);
	hd44780_write_4_bits(pdata, val >> 4);
	hd44780_write_4_bits(pdata, (val & 0xf));
	mdelay(1);
	spin_unlock(&pdata->write_lock);
}

static void  hd44780_command(struct hd44780_data *pdata, u8 val)
{
	hd44780_send(pdata, val, 0);
}

static void hd44780_char(struct hd44780_data *pdata, u8 val)
{
	hd44780_send(pdata, val, 1);
}

static int hd44780_write(struct hd44780_data *pdata, char *msg, size_t pos)
{
	int lcd_pos;
	int cols, lines;
	int cur_line;

	dev_dbg(&pdata->pdev->dev, "writing at: %d\n", pos);

	spin_lock(&pdata->lock);
	cols = pdata->cols;
	lines = pdata->lines;

	spin_unlock(&pdata->lock);

	lcd_pos = ((pos / pdata->cols) * LINE_OFFSET) + (pos % pdata->cols);

	hd44780_command(pdata, CMD_SET_DDRAM_POS(lcd_pos));
	cur_line = 1 + (lcd_pos / LINE_OFFSET);

	while (*msg) {
		if (pos > lines * cols)
			break;

		if(*msg == '\n') {
			++cur_line;
			if(cur_line > lines)
				break;
			hd44780_command(pdata, CMD_SET_DDRAM_LINE(cur_line));
			pos = (pos / cols) + cols;
		} else {
			int line;

			line = 1 + (pos / cols);
			if(line > lines)
				break;
			if (line != cur_line) {
				hd44780_command(pdata, CMD_SET_DDRAM_LINE(line));
				cur_line = line;
			}
			hd44780_char(pdata, *msg);
			++pos;
		}
		++msg;
	}
	return 0;

}

static int hd44780_clear(struct hd44780_data *pdata, char *msg) {
	hd44780_command(pdata,CMD_CLEAR_DISP);

	return hd44780_write(pdata, msg, 0);
}

static int hd44780_line(struct hd44780_data *pdata, char *txt, int line) {

	u8 cmd;

	switch(line) {
	case 1:
		cmd = CMD_SET_DDRAM_LINE(2);
		break;
	default:
		cmd = CMD_SET_DDRAM_LINE(1);
		break;
	}
	hd44780_command(pdata, cmd);

	while (*txt) {
		hd44780_char(pdata, *txt);
		++txt;
	}
	return 0;
}


static int hd44780_prompt(struct hd44780_data *pdata)
{
	struct new_utsname *name;

	name = utsname();

	hd44780_line(pdata, name->sysname, 0);
	hd44780_line(pdata, name->release, 1);
	return 0;
}

static int hd44780_setup(struct hd44780_data *pdata)
{
	dev_info(&pdata->pdev->dev, "setting up LCD\n");
	hd44780_command(pdata,0x33); // 110011 Initialise
	hd44780_command(pdata,0x32); // 110010 Initialise
	hd44780_command(pdata,0x06); // 000110 Cursor move direction
	hd44780_command(pdata,0x0c); // 001100 Display On,Cursor Off, Blink Off
	hd44780_command(pdata,0x28); // 101000 Data length, number of lines, font size
	hd44780_command(pdata,CMD_CLEAR_DISP); //lcd_byte(0x01,LCD_CMD) # 000001 Clear display
//	mdelay(1);
	return 0;
}

int hd44780_file_open(struct inode *inode, struct file *filp)
{
	struct hd44780_data *pdata; /* device information */
	pdata = container_of(inode->i_cdev, struct hd44780_data, cdev);

	// TODO: gestire apertura in append!!!
	filp->private_data = pdata; /* for other methods */

	return 0;
}

ssize_t hd44780_file_write (struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
	char *buffer;
	struct hd44780_data *pdata;
	size_t len;

	// TODO: gestire scrittura in append...

	pdata = filp->private_data;

	len = (pdata->cols * pdata->lines);
	if (count < len)
		len = count;

	dev_dbg(&pdata->pdev->dev, "writing %d chars at: %lld\n", len, filp->f_pos);

	if (NULL == (buffer = kmalloc((len + 1), GFP_KERNEL)))
		return -ENOMEM;

	if(copy_from_user(buffer, buff, len)) {
		return -EFAULT;
	}

	buffer[len] = 0x00;
	hd44780_write(pdata, buffer, filp->f_pos);

	kfree(buffer);

	*offp += count;

	if(*offp > pdata->size)
		pdata->size = *offp;
	return count;
}


loff_t hd44780_llseek (struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	struct hd44780_data *pdata;

	// TODO: gestire reentrant
	pdata = filp->private_data;

	switch(whence)
	{
	case 0: /* SEEK_SET */
		newpos = off;
		break;
	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;
	case 2: /* SEEK_END */
		newpos = pdata->size + off;
		break;
	default:
		return -EINVAL;
	}

	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;

	dev_dbg(&pdata->pdev->dev,"llseek new pos: %lld\n", newpos);
	return newpos;
}


struct file_operations hd44780_fops = {

		.owner = THIS_MODULE,
		.open  = hd44780_file_open,
		.write = hd44780_file_write,
		.llseek = hd44780_llseek,
};

static int hd44780_probe(struct platform_device *pdev)
{
	struct hd44780_data *pdata;
	struct pinctrl      *pinctrl;
	struct pinctrl_state *pstate;
	struct gpio_desc *pdesc;
	int retval;
	int i;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		return -ENOMEM;
	}

	spin_lock_init(&pdata->lock);
	spin_lock_init(&pdata->write_lock);

	spin_lock(&pdata->lock);
	pdata->pdev = pdev;

	pinctrl = devm_pinctrl_get(&pdata->pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "can't get pinctrl handle\n");
		retval = -EINVAL;
		goto err_probe;
	}

	pstate = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_DEFAULT);
	if (!pstate) {
		dev_err(&pdev->dev, "can't find default pinctrl state\n");
		retval = -EINVAL;
		goto err_probe;
	}

	if (pinctrl_select_state(pinctrl, pstate)) {

		dev_err(&pdev->dev, "cant't select default pinctrl state\n");
		retval = -EINVAL;
		goto err_probe;
	}

	pdata->gpios = devm_gpiod_get_array(&pdev->dev, "lcd", 0);
	if (IS_ERR(pdata->gpios) || pdata->gpios->ndescs < 6) {
		dev_err(&pdev->dev, "can't get gpios\n");
		retval = -EINVAL;
		goto err_probe;
	}
	if (pdata->gpios->ndescs < 6) {
		retval = -EINVAL;
		goto err_probe;
	}

	for (i = 0; i < pdata->gpios->ndescs; ++i) {
		pdesc = pdata->gpios->desc[i];
		gpiod_direction_output(pdesc, 0);
	}

	pdata->gpio_rs = pdata->gpios->desc[0];
	pdata->gpio_en = pdata->gpios->desc[1];
	pdata->gpio_db4 = pdata->gpios->desc[2];
	pdata->gpio_db5 = pdata->gpios->desc[3];
	pdata->gpio_db6 = pdata->gpios->desc[4];
	pdata->gpio_db7 = pdata->gpios->desc[5];

	of_property_read_u32(pdev->dev.of_node, "cols", &pdata->cols);
	if (!pdata->cols)
		pdata->cols = 16;

	of_property_read_u32(pdev->dev.of_node, "lines", &pdata->lines);
	if (!pdata->lines)
		pdata->lines = 2;

	dev_set_drvdata(&pdev->dev, pdata);

	hd44780_setup(pdata);
	//hd44780_prompt(pdata);

	// sysfs device
	if( !lcd_class) {
		dev_err(&pdev->dev, "can't get gpios\n");
		retval = -EINVAL;
		goto err_probe;
	}

	pdata->devn = next;
	next = MKDEV(MAJOR(next), MINOR(next) + 1);

	if (NULL == device_create(lcd_class, NULL, pdata->devn, NULL, "lcd%d", MINOR(pdata->devn))) {
		retval = -EIO;
		goto err_probe;
	}
	// end sysfs

	cdev_init(&pdata->cdev, &hd44780_fops);
	pdata->cdev.owner = THIS_MODULE;
	pdata->cdev.ops = &hd44780_fops;

	if (cdev_add(&pdata->cdev, pdata->devn, 1)) {
		retval = -EIO;
		dev_err(&pdev->dev, "cannot add char device\n");
		goto err_probe;
	}

	dev_info(&pdev->dev,"major: %d minor: %d\n",MAJOR(pdata->devn), MINOR(pdata->devn));
	retval = 0;

err_probe:
	spin_unlock(&pdata->lock);
	return retval;
}

static int hd44780_remove(struct platform_device *pdev)
{
	char *msg = "bye";
	struct hd44780_data *pdata;

	pdata = (struct hd44780_data *)dev_get_drvdata(&pdev->dev);

	// sysfs device
	if (lcd_class) {
		device_destroy(lcd_class, pdata->devn);
	}
	// end sysfs

	cdev_del(&pdata->cdev);

	hd44780_clear(pdata, msg);
	return 0;
}

static const struct of_device_id hd44780_match[] = {
		{
				.compatible = "hd44780",
		},
		{},
};

static struct platform_driver hd44780_platform_driver = {
		.probe = hd44780_probe,
		.remove = hd44780_remove,
		.driver = {
				.name = "HD44780",
				.owner = THIS_MODULE,
				.of_match_table = of_match_ptr(hd44780_match)
		}
};

static int __init hd44780_module_init(void)
{
	pr_info("hd4480 module init\n");

	lcd_class = class_create(THIS_MODULE,"lcd");
	alloc_chrdev_region(&first, 0, 10, "lcdchar"); // a cosa serve il nome?
	next = first;

	return platform_driver_register(&hd44780_platform_driver);
}

static void __exit hd44780_module_exit(void)
{
	pr_info("hd4480 module exit\n");
	platform_driver_unregister(&hd44780_platform_driver);

	if (lcd_class)
		class_destroy(lcd_class);

	unregister_chrdev_region(first, 10);
}

module_init(hd44780_module_init)
module_exit(hd44780_module_exit)

MODULE_AUTHOR("Carlo Tomasin <c.tomasin@gmail.com>");
MODULE_DESCRIPTION("Device driver for HD4480 2-lines LCD display");
MODULE_LICENSE("GPL");
